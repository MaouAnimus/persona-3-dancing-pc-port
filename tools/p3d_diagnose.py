#!/usr/bin/env python3
"""Run an owned, bounded P3D probe and record Windows thread evidence.

Examples (run from any directory with 64-bit Python):
    py tools/p3d_diagnose.py --check-imports --seconds 30
    py tools/p3d_diagnose.py --seconds 15 --label startup

Only the subprocess created by this invocation is inspected or terminated. There
is deliberately no attach/PID option. Thread snapshots are observations, not an
unwound call stack: RBP chains and stack pointer candidates have separate labels.
Uses the Python standard library; does not alter game files or the runtime.
"""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "docs/evidence/codex-20261017"
LOW_MIN = 0x0800000000  # runtime_memory.c:668; allocation can fall back elsewhere.
CONTEXT_AMD64_CONTROL_INTEGER = 0x100003
CONTEXT_SIZE = 1232
CONTEXT_OFFSETS = {
    "rax": 120, "rcx": 128, "rdx": 136, "rbx": 144,
    "rsp": 152, "rbp": 160, "rsi": 168, "rdi": 176,
    "r8": 184, "r9": 192, "r10": 200, "r11": 208,
    "r12": 216, "r13": 224, "r14": 232, "r15": 240, "rip": 248,
}


def address(value: int) -> str:
    return f"0x{value:x}"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_boot(path: Path) -> dict:
    """Read the probe layout without importing or executing vendor scripts."""
    with path.open("rb") as stream:
        def words(count: int) -> tuple[int, ...]:
            raw = stream.read(count * 8)
            if len(raw) != count * 8:
                raise ValueError("truncated boot metadata")
            return struct.unpack("<" + "Q" * count, raw)

        magic = stream.read(8)
        if magic not in [f"G3PROBE{i}".encode() for i in range(1, 6)]:
            raise ValueError("unsupported boot file signature")
        size, entry, ns, nr, ni = words(5)
        capabilities = words(1)[0] if magic != b"G3PROBE1" else 0
        if not (0 < size <= 512 * 1024 * 1024 and entry < size
                and 0 < ns <= 64 and nr <= 1000000 and ni <= 100000):
            raise ValueError("boot metadata exceeds loader limits")
        linked = magic in (b"G3PROBE3", b"G3PROBE4", b"G3PROBE5")
        modules = []
        if magic == b"G3PROBE5":
            words(5)  # procparam and main TLS metadata
            count = words(1)[0]
            if not 0 < count <= 16:
                raise ValueError("invalid linked module count")
            for index in range(count):
                base, length, init, tls, tls_size, tls_file, tls_id = words(7)
                modules.append({"name": f"linked-module-{index}",
                                "base": base, "size": length})
            bindings = words(1)[0]
        elif linked:
            base, length, init, tls, tls_size, tls_file, bindings, procparam = words(8)
            modules.append({"name": "libc.prx", "base": base, "size": length})
            if magic == b"G3PROBE4":
                words(4)
        else:
            bindings = 0
        if bindings > ni:
            raise ValueError("invalid native binding count")
        stream.seek(bindings * 24, 1)
        segments = []
        for _ in range(ns):
            start, length, flags = words(3)
            if start > size or length > size - start:
                raise ValueError("segment outside boot image")
            segments.append({"offset": start, "size": length, "flags": flags})
        image_offset = stream.tell() + ni * 128 + nr * 32
        if image_offset + size != path.stat().st_size:
            raise ValueError("boot file image size does not match metadata")
        stream.seek(image_offset + entry)
        entry_bytes = stream.read(min(32, size - entry))

    # Companion linker evidence supplies filenames only when base and size match.
    companion = path.with_name("link.json")
    if companion.is_file():
        try:
            names = json.loads(companion.read_text(encoding="utf-8"))["modules"]
            for module in modules:
                for item in names:
                    if int(item["base"], 0) == module["base"] and item["size"] == module["size"]:
                        module["name"] = item["file"]
        except (OSError, ValueError, KeyError, TypeError):
            pass
    return {"format": magic.decode(), "image_size": size, "entry": entry,
            "capabilities": capabilities, "imports": ni, "relocations": nr,
            "segments": segments, "modules": modules, "entry_bytes": entry_bytes}


class THREADENTRY32(ctypes.Structure):
    _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
                ("th32ThreadID", wintypes.DWORD), ("th32OwnerProcessID", wintypes.DWORD),
                ("tpBasePri", wintypes.LONG), ("tpDeltaPri", wintypes.LONG),
                ("dwFlags", wintypes.DWORD)]


class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wintypes.DWORD), ("th32ModuleID", wintypes.DWORD),
                ("th32ProcessID", wintypes.DWORD), ("GlblcntUsage", wintypes.DWORD),
                ("ProccntUsage", wintypes.DWORD), ("modBaseAddr", ctypes.c_void_p),
                ("modBaseSize", wintypes.DWORD), ("hModule", wintypes.HMODULE),
                ("szModule", wintypes.WCHAR * 256), ("szExePath", wintypes.WCHAR * 260)]


class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [("BaseAddress", ctypes.c_void_p), ("AllocationBase", ctypes.c_void_p),
                ("AllocationProtect", wintypes.DWORD), ("PartitionId", wintypes.WORD),
                ("RegionSize", ctypes.c_size_t), ("State", wintypes.DWORD),
                ("Protect", wintypes.DWORD), ("Type", wintypes.DWORD)]


class ChildInspector:
    def __init__(self, child: subprocess.Popen, boot: dict, candidate_base: int):
        # Reuse the process handle returned by CreateProcess; never open another PID.
        self.child = child
        self.handle = wintypes.HANDLE(int(child._handle))
        self.boot = boot
        self.candidate_base = candidate_base
        self.guest_base = None
        self.modules = []
        self.api = ctypes.WinDLL("kernel32", use_last_error=True)
        signatures = {
            "CreateToolhelp32Snapshot": ([wintypes.DWORD, wintypes.DWORD], wintypes.HANDLE),
            "Thread32First": ([wintypes.HANDLE, ctypes.POINTER(THREADENTRY32)], wintypes.BOOL),
            "Thread32Next": ([wintypes.HANDLE, ctypes.POINTER(THREADENTRY32)], wintypes.BOOL),
            "Module32FirstW": ([wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32W)], wintypes.BOOL),
            "Module32NextW": ([wintypes.HANDLE, ctypes.POINTER(MODULEENTRY32W)], wintypes.BOOL),
            "OpenThread": ([wintypes.DWORD, wintypes.BOOL, wintypes.DWORD], wintypes.HANDLE),
            "GetProcessIdOfThread": ([wintypes.HANDLE], wintypes.DWORD),
            "SuspendThread": ([wintypes.HANDLE], wintypes.DWORD),
            "ResumeThread": ([wintypes.HANDLE], wintypes.DWORD),
            "GetThreadContext": ([wintypes.HANDLE, ctypes.c_void_p], wintypes.BOOL),
            "ReadProcessMemory": ([wintypes.HANDLE, ctypes.c_void_p, ctypes.c_void_p,
                                   ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)], wintypes.BOOL),
            "VirtualQueryEx": ([wintypes.HANDLE, ctypes.c_void_p,
                                ctypes.POINTER(MEMORY_BASIC_INFORMATION), ctypes.c_size_t], ctypes.c_size_t),
            "CloseHandle": ([wintypes.HANDLE], wintypes.BOOL),
            "LocalFree": ([ctypes.c_void_p], ctypes.c_void_p),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.api, name)
            function.argtypes, function.restype = args, result
        self.get_description = getattr(self.api, "GetThreadDescription", None)
        if self.get_description:
            self.get_description.argtypes = [wintypes.HANDLE, ctypes.POINTER(ctypes.c_void_p)]
            self.get_description.restype = wintypes.LONG

    def error(self, action: str) -> str:
        code = ctypes.get_last_error()
        return f"{action}: Win32 {code}: {ctypes.FormatError(code).strip()}"

    def read(self, pointer: int, count: int) -> bytes:
        if not pointer or count <= 0:
            return b""
        buffer = ctypes.create_string_buffer(count)
        copied = ctypes.c_size_t()
        self.api.ReadProcessMemory(self.handle, ctypes.c_void_p(pointer), buffer,
                                   count, ctypes.byref(copied))
        return buffer.raw[:copied.value]

    def region(self, pointer: int) -> MEMORY_BASIC_INFORMATION | None:
        info = MEMORY_BASIC_INFORMATION()
        if not self.api.VirtualQueryEx(self.handle, ctypes.c_void_p(pointer), ctypes.byref(info), ctypes.sizeof(info)):
            return None
        return info

    def detect_guest_base(self) -> dict:
        expected = self.boot["entry_bytes"]
        candidates = [self.candidate_base]
        # The first low mapping normally holds the image. If its requested address
        # was unavailable, inspect allocation starts in the following 4 GiB.
        pointer, limit = LOW_MIN, LOW_MIN + 0x100000000
        regions = 0
        while pointer < limit and regions < 4096:
            info = self.region(pointer)
            if not info or not info.RegionSize:
                break
            if info.State == 0x1000 and info.AllocationBase:
                candidates.append(info.AllocationBase)
            following = (info.BaseAddress or pointer) + info.RegionSize
            if following <= pointer:
                break
            pointer, regions = following, regions + 1
        for candidate in dict.fromkeys(candidates):
            if self.read(candidate + self.boot["entry"], len(expected)) == expected:
                self.guest_base = candidate
                return {"base": address(candidate), "validated": True,
                        "method": "original entry bytes match boot image",
                        "entry_bytes_compared": len(expected)}
        return {"base": None, "candidate": address(self.candidate_base), "validated": False,
                "method": "entry bytes did not match; guest addresses left unclassified"}

    def host_modules(self) -> dict:
        snapshot = self.api.CreateToolhelp32Snapshot(0x8 | 0x10, self.child.pid)
        if snapshot == ctypes.c_void_p(-1).value:
            return {"error": self.error("module snapshot"), "modules": []}
        result = []
        try:
            entry = MODULEENTRY32W()
            entry.dwSize = ctypes.sizeof(entry)
            more = self.api.Module32FirstW(snapshot, ctypes.byref(entry))
            while more:
                result.append({"name": entry.szModule, "path": entry.szExePath,
                               "base": entry.modBaseAddr or 0, "size": entry.modBaseSize})
                more = self.api.Module32NextW(snapshot, ctypes.byref(entry))
        finally:
            self.api.CloseHandle(snapshot)
        self.modules = result
        return {"modules": [{**item, "base": address(item["base"])} for item in result]}

    def symbolize(self, pointer: int) -> dict:
        result = {"address": address(pointer), "kind": "unknown"}
        if self.guest_base is not None and 0 <= pointer - self.guest_base < self.boot["image_size"]:
            offset = pointer - self.guest_base
            result.update(kind="guest", image_offset=address(offset), module="eboot", module_offset=address(offset))
            for item in self.boot["modules"]:
                if item["base"] <= offset < item["base"] + item["size"]:
                    result.update(module=item["name"], module_offset=address(offset - item["base"]))
                    break
            result["executable"] = any(segment["flags"] & 1 and segment["offset"] <= offset
                                       < segment["offset"] + segment["size"] for segment in self.boot["segments"])
        else:
            for item in self.modules:
                if item["base"] <= pointer < item["base"] + item["size"]:
                    result.update(kind="host", module=item["name"], module_offset=address(pointer - item["base"]))
                    break
        return result

    def thread_ids(self) -> list[int]:
        snapshot = self.api.CreateToolhelp32Snapshot(0x4, 0)
        if snapshot == ctypes.c_void_p(-1).value:
            raise OSError(self.error("thread snapshot"))
        result = []
        try:
            entry = THREADENTRY32()
            entry.dwSize = ctypes.sizeof(entry)
            more = self.api.Thread32First(snapshot, ctypes.byref(entry))
            while more:
                if entry.th32OwnerProcessID == self.child.pid:
                    result.append(entry.th32ThreadID)
                more = self.api.Thread32Next(snapshot, ctypes.byref(entry))
        finally:
            self.api.CloseHandle(snapshot)
        return result

    def thread(self, tid: int, frames: int, stack_bytes: int, max_hits: int) -> dict:
        result = {"tid": tid, "name": None, "errors": []}
        handle = self.api.OpenThread(0x2 | 0x8 | 0x40 | 0x800, False, tid)
        if not handle:
            result["errors"].append(self.error("OpenThread"))
            return result
        suspended = False
        try:
            # A thread may exit and its ID may be reused after enumeration.
            if self.api.GetProcessIdOfThread(handle) != self.child.pid:
                result["errors"].append("Thread no longer belongs to the owned child; skipped")
                return result
            if self.get_description:
                pointer = ctypes.c_void_p()
                if self.get_description(handle, ctypes.byref(pointer)) == 0 and pointer.value:
                    try:
                        result["name"] = ctypes.wstring_at(pointer)
                    finally:
                        self.api.LocalFree(pointer)
            previous = self.api.SuspendThread(handle)
            if previous == 0xFFFFFFFF:
                result["errors"].append(self.error("SuspendThread"))
                return result
            suspended = True
            result["previous_suspend_count"] = previous
            # CONTEXT must be 16-byte aligned. AMD64 register offsets follow WinNT.h.
            storage = ctypes.create_string_buffer(CONTEXT_SIZE + 15)
            aligned = (ctypes.addressof(storage) + 15) & ~15
            ctypes.c_uint32.from_address(aligned + 48).value = CONTEXT_AMD64_CONTROL_INTEGER
            if not self.api.GetThreadContext(handle, ctypes.c_void_p(aligned)):
                result["errors"].append(self.error("GetThreadContext"))
                return result
            raw = ctypes.string_at(aligned, CONTEXT_SIZE)
            registers = {name: struct.unpack_from("<Q", raw, offset)[0]
                         for name, offset in CONTEXT_OFFSETS.items()}
            result["registers"] = {name: address(value) for name, value in registers.items()}
            result["instruction"] = self.symbolize(registers["rip"])
            result["instruction_bytes"] = self.read(registers["rip"], 32).hex()
            chain, rbp = [], registers["rbp"]
            rsp = registers["rsp"]
            for depth in range(frames):
                if rbp < rsp or rbp - rsp > 16 * 1024 * 1024 or rbp % 8:
                    break
                data = self.read(rbp, 16)
                if len(data) != 16:
                    break
                following, returned = struct.unpack("<QQ", data)
                chain.append({"frame": depth, "rbp": address(rbp), "return": self.symbolize(returned)})
                if following <= rbp or following - rbp > 16 * 1024 * 1024:
                    break
                rbp = following
            result["rbp_chain"] = chain
            region = self.region(rsp)
            readable = stack_bytes
            if region:
                readable = min(readable, max(0, (region.BaseAddress or 0) + region.RegionSize - rsp))
            stack = self.read(rsp, readable)
            hits = []
            for index in range(0, len(stack) - 7, 8):
                value = struct.unpack_from("<Q", stack, index)[0]
                symbol = self.symbolize(value)
                if symbol["kind"] == "guest":
                    hits.append({"stack_address": address(rsp + index),
                                 "stack_offset": address(index), "value": symbol})
                    if len(hits) >= max_hits:
                        break
            result["stack_bytes_read"] = len(stack)
            result["stack_guest_pointer_candidates"] = hits
            result["stack_candidates_truncated"] = len(hits) >= max_hits
        except Exception as error:
            result["errors"].append(f"{type(error).__name__}: {error}")
        finally:
            # Exactly one resume balances our successful suspend, including on
            # GetThreadContext/ReadProcessMemory failures or KeyboardInterrupt.
            if suspended:
                resumed = self.api.ResumeThread(handle)
                result["resume_succeeded"] = resumed != 0xFFFFFFFF
                if resumed == 0xFFFFFFFF:
                    result["errors"].append(self.error("ResumeThread"))
            self.api.CloseHandle(handle)
        return result

    def snapshot(self, args: argparse.Namespace) -> dict:
        result = {"captured_utc": datetime.now(timezone.utc).isoformat(),
                  "method": "individual thread suspension; snapshots are not simultaneous",
                  "guest_image": self.detect_guest_base(), "host": self.host_modules()}
        result["threads"] = [self.thread(tid, args.frames, args.stack_bytes, args.max_stack_hits)
                             for tid in self.thread_ids()]
        return result


def bounded_seconds(text: str) -> float:
    value = float(text)
    if not math.isfinite(value) or not 0.1 <= value <= 600:
        raise argparse.ArgumentTypeError("seconds must be finite and between 0.1 and 600")
    return value


def positive_int(text: str) -> int:
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return value


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--seconds", type=bounded_seconds, default=15,
                        help="maximum startup wait before snapshot and owned-child termination (default: 15)")
    parser.add_argument("--check-imports", action="store_true", help="check imports without entering original game code")
    parser.add_argument("--probe", "--runtime", type=Path, default=ROOT / "bin/p3d_engine.exe")
    parser.add_argument("--boot", "--image", type=Path, default=ROOT / "out/p3d_test/boot-linked.bin")
    parser.add_argument("--app0", type=Path, default=ROOT / "CUSA12636")
    parser.add_argument("--user", type=Path, default=DEFAULT_OUTPUT / "user")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--label", default="startup", help="short label in unique output filenames")
    parser.add_argument("--dll-dir", action="append", type=Path, default=[], help="additional runtime DLL search directory")
    parser.add_argument("--mount", action="append", default=[], metavar="GUEST=HOST")
    parser.add_argument("--cpu-only", action="store_true", help="omit GPU; import coverage is then incomplete")
    parser.add_argument("--strict-imports", action="store_true")
    parser.add_argument("--guest-base", type=lambda value: int(value, 0), default=LOW_MIN,
                        help="candidate original image base; validated against entry bytes (default: 0x800000000)")
    parser.add_argument("--frames", type=positive_int, default=32, help="maximum RBP chain entries per thread")
    parser.add_argument("--stack-bytes", type=positive_int, default=8192, help="maximum bytes scanned above RSP per thread")
    parser.add_argument("--max-stack-hits", type=positive_int, default=96)
    args = parser.parse_args()
    if os.name != "nt" or ctypes.sizeof(ctypes.c_void_p) != 8:
        parser.error("this diagnostic requires 64-bit Python on Windows")
    if args.frames > 256 or args.stack_bytes > 1024 * 1024 or args.max_stack_hits > 8192:
        parser.error("snapshot limits: frames <= 256, stack-bytes <= 1048576, max-stack-hits <= 8192")
    for name in ("probe", "boot", "app0", "user", "output_dir"):
        setattr(args, name, getattr(args, name).resolve())
    for name in ("probe", "boot"):
        if not getattr(args, name).is_file():
            parser.error(f"{name} does not exist: {getattr(args, name)}")
    if not args.app0.is_dir():
        parser.error(f"app0 does not exist: {args.app0}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    args.user.mkdir(parents=True, exist_ok=True)
    label = re.sub(r"[^A-Za-z0-9_-]+", "-", args.label).strip("-")[:48] or "startup"
    stem = f"p3d-{label}-{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')}"
    log_path, report_path = args.output_dir / (stem + ".log"), args.output_dir / (stem + ".json")
    boot = read_boot(args.boot)
    command = [str(args.probe), str(args.boot), "--app0", str(args.app0),
               "--user", str(args.user), "--timeout", "0"]
    for option in ("check_imports", "cpu_only", "strict_imports"):
        if getattr(args, option):
            command.append("--" + option.replace("_", "-"))
    for mount in args.mount:
        command.extend(["--mount", mount])
    env = os.environ.copy()
    env["GOW3_UPSCALER"] = "off"
    env["GOW3_CONFIG"] = str(args.user / "p3d-diagnostic.ini")
    env["GOW3_GPU_USER_DIR"] = str(args.user)
    dll_dirs = [path.resolve() for path in args.dll_dir]
    default_dll = Path("C:/msys64/clang64/bin")
    if default_dll.is_dir():
        dll_dirs.append(default_dll)
    if dll_dirs:
        env["PATH"] = os.pathsep.join(map(str, dll_dirs)) + os.pathsep + env.get("PATH", "")
    # These can launch another executable or carry unrelated saved GOW3 options.
    for key in ("GOW3_RESTART_COMMAND", "GOW3_PATCHES", "GOW3_GAME_DIR"):
        env.pop(key, None)
    report = {"schema": "p3d-owned-process-diagnostic-v1",
              "started_utc": datetime.now(timezone.utc).isoformat(), "command": command,
              "cwd": str(ROOT), "seconds": args.seconds,
              "check_imports": args.check_imports, "cpu_only": args.cpu_only,
              "probe_sha256": sha256(args.probe), "boot_sha256": sha256(args.boot),
              "log": str(log_path), "report": str(report_path),
              "user": str(args.user), "dll_dirs": list(map(str, dll_dirs)),
              "boot": {key: value for key, value in boot.items() if key != "entry_bytes"},
              "limitations": ["RBP frames are heuristic and can stop at non-frame-pointer code.",
                              "Stack candidates are raw pointers, not proven return addresses.",
                              "Host addresses identify module and offset, without debug symbols.",
                              "Only this invocation's child process is inspected and terminated."]}
    child = None
    started = time.monotonic()
    result = 1
    try:
        with log_path.open("xb") as log:
            child = subprocess.Popen(command, cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
                                     stdout=log, stderr=subprocess.STDOUT,
                                     creationflags=subprocess.CREATE_NO_WINDOW)
            report["pid"] = child.pid
            print(f"Owned child PID {child.pid}; log: {log_path}", flush=True)
            try:
                child.wait(timeout=args.seconds)
                report["deadline_reached"] = False
            except subprocess.TimeoutExpired:
                report["deadline_reached"] = True
                if child.poll() is None:
                    report["snapshot"] = ChildInspector(child, boot, args.guest_base).snapshot(args)
            finally:
                if child.poll() is None:
                    report["termination"] = "terminated only the child created by this invocation"
                    child.kill()
                    child.wait(timeout=10)
                report["exit_code"] = child.returncode
            result = 124 if report.get("deadline_reached") else (child.returncode or 0)
    except KeyboardInterrupt:
        report["error"] = "interrupted by user"
        result = 130
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        result = 1
    finally:
        # Also covers exceptions raised while taking the snapshot or opening files.
        if child is not None and child.poll() is None:
            child.kill()
            child.wait(timeout=10)
            report["termination"] = "terminated owned child during cleanup"
            report["exit_code"] = child.returncode
        report["elapsed_seconds"] = round(time.monotonic() - started, 3)
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        if log_path.exists():
            log_text = log_path.read_text(encoding="utf-8", errors="replace")
            report["missing_imports"] = [line for line in log_text.splitlines() if line.startswith("MISSING:")]
            report["original_entry_observed"] = "Entering original x86-64 code" in log_text
            report["last_log_lines"] = log_text.splitlines()[-40:]
        with report_path.open("x", encoding="utf-8") as output:
            json.dump(report, output, indent=2, ensure_ascii=False)
            output.write("\n")
        print(f"Report: {report_path}", flush=True)
    return result


if __name__ == "__main__":
    raise SystemExit(main())
