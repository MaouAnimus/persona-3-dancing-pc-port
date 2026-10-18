#!/usr/bin/env python3
"""Capture exceptions from a Windows subprocess created by this invocation.

    py tools/p3d_crash_capture.py --seconds 600
    py tools/p3d_crash_capture.py --raw --probe C:/Windows/System32/cmd.exe -- /c exit 7

There is no PID/attach option. The deadline detaches the owned process without
terminating it. First-chance exceptions continue to the application's handlers;
only the initial debugger breakpoint is consumed. No game/save files are edited.
"""

from __future__ import annotations

import argparse
from collections import Counter, deque
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import subprocess
import time

from p3d_diagnose import ChildInspector, LOW_MIN, ROOT, address, read_boot


DEBUG_ONLY_THIS_PROCESS = 0x2
DBG_CONTINUE = 0x00010002
DBG_EXCEPTION_NOT_HANDLED = 0x80010001
STATUS_BREAKPOINT = 0x80000003
STATUS_ACCESS_VIOLATION = 0xC0000005
EVENT_NAMES = {1: "exception", 2: "thread_create", 3: "process_create",
               4: "thread_exit", 5: "process_exit", 6: "dll_load",
               7: "dll_unload", 8: "debug_string", 9: "rip"}


class EXCEPTION_RECORD(ctypes.Structure):
    _fields_ = [("code", wintypes.DWORD), ("flags", wintypes.DWORD),
                ("nested", ctypes.c_void_p), ("location", ctypes.c_void_p),
                ("parameter_count", wintypes.DWORD),
                ("information", ctypes.c_size_t * 15)]


class EXCEPTION_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("record", EXCEPTION_RECORD), ("first_chance", wintypes.DWORD)]


class CREATE_PROCESS_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("hFile", wintypes.HANDLE), ("hProcess", wintypes.HANDLE),
                ("hThread", wintypes.HANDLE), ("base", ctypes.c_void_p),
                ("debug_offset", wintypes.DWORD), ("debug_size", wintypes.DWORD),
                ("thread_local", ctypes.c_void_p), ("start", ctypes.c_void_p),
                ("image_name", ctypes.c_void_p), ("unicode", wintypes.WORD)]


class LOAD_DLL_DEBUG_INFO(ctypes.Structure):
    _fields_ = [("hFile", wintypes.HANDLE), ("base", ctypes.c_void_p),
                ("debug_offset", wintypes.DWORD), ("debug_size", wintypes.DWORD),
                ("image_name", ctypes.c_void_p), ("unicode", wintypes.WORD)]


class DEBUG_STRING_INFO(ctypes.Structure):
    _fields_ = [("data", ctypes.c_void_p), ("unicode", wintypes.WORD),
                ("length", wintypes.WORD)]


class EVENT_UNION(ctypes.Union):
    _fields_ = [("exception", EXCEPTION_DEBUG_INFO),
                ("create_process", CREATE_PROCESS_DEBUG_INFO),
                ("load_dll", LOAD_DLL_DEBUG_INFO),
                ("debug_string", DEBUG_STRING_INFO),
                ("exit_code", wintypes.DWORD), ("base", ctypes.c_void_p)]


class DEBUG_EVENT(ctypes.Structure):
    _fields_ = [("code", wintypes.DWORD), ("pid", wintypes.DWORD),
                ("tid", wintypes.DWORD), ("data", EVENT_UNION)]


def utc() -> str:
    return datetime.now(timezone.utc).isoformat()


def seconds(value: str) -> float:
    parsed = float(value)
    if not math.isfinite(parsed) or not 0.1 <= parsed <= 7200:
        raise argparse.ArgumentTypeError("seconds must be between 0.1 and 7200")
    return parsed


def winapi():
    api = ctypes.WinDLL("kernel32", use_last_error=True)
    for name, args, result in [
        ("WaitForDebugEvent", [ctypes.POINTER(DEBUG_EVENT), wintypes.DWORD], wintypes.BOOL),
        ("ContinueDebugEvent", [wintypes.DWORD, wintypes.DWORD, wintypes.DWORD], wintypes.BOOL),
        ("DebugSetProcessKillOnExit", [wintypes.BOOL], wintypes.BOOL),
        ("DebugActiveProcessStop", [wintypes.DWORD], wintypes.BOOL),
        ("CloseHandle", [wintypes.HANDLE], wintypes.BOOL),
    ]:
        function = getattr(api, name)
        function.argtypes, function.restype = args, result
    return api


def capture(inspector: ChildInspector, tid: int, all_threads: bool) -> dict:
    started = time.monotonic()
    result = {"captured_utc": utc(), "faulting_tid": tid,
              "method": "debug event freezes owned child; balanced additional thread suspensions",
              "host": inspector.host_modules()}
    if inspector.boot["entry_bytes"]:
        result["guest_image"] = inspector.detect_guest_base()
    tids = inspector.thread_ids() if all_threads else [tid]
    result["threads"] = [inspector.thread(item, 40, 8192, 96) for item in tids]
    result["capture_seconds"] = round(time.monotonic() - started, 4)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--seconds", type=seconds, default=600)
    parser.add_argument("--probe", type=Path, default=ROOT / "bin/p3d_engine.exe")
    parser.add_argument("--boot", type=Path, default=ROOT / "out/p3d_test/boot-linked.bin")
    parser.add_argument("--app0", type=Path, default=ROOT / "CUSA12636")
    parser.add_argument("--user", type=Path, help="optional existing runtime user directory")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "docs/evidence/crash-20261017")
    parser.add_argument("--label", default="manual")
    parser.add_argument("--raw", action="store_true", help="run probe with only trailing arguments, for harness checks")
    parser.add_argument("--guest-base", type=lambda value: int(value, 0), default=LOW_MIN)
    parser.add_argument("--dll-dir", type=Path, action="append", default=[])
    parser.add_argument("arguments", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if os.name != "nt" or ctypes.sizeof(ctypes.c_void_p) != 8:
        parser.error("requires 64-bit Python on Windows")
    if ctypes.sizeof(DEBUG_EVENT) != 176 or DEBUG_EVENT.data.offset != 16:
        parser.error("unexpected native Windows DEBUG_EVENT structure layout")
    args.probe = args.probe.resolve()
    args.boot = args.boot.resolve()
    args.app0 = args.app0.resolve()
    args.output_dir = args.output_dir.resolve()
    if not args.probe.is_file():
        parser.error(f"probe does not exist: {args.probe}")
    if not args.raw and (not args.boot.is_file() or not args.app0.is_dir()):
        parser.error("boot must exist and app0 must be a directory")
    boot = read_boot(args.boot) if not args.raw else {
        "entry_bytes": b"", "entry": 0, "image_size": 0, "modules": [], "segments": []}
    trailing = args.arguments[1:] if args.arguments[:1] == ["--"] else args.arguments
    command = [str(args.probe)]
    if not args.raw:
        command += [str(args.boot), "--app0", str(args.app0), "--timeout", "0"]
        if args.user:
            command += ["--user", str(args.user.resolve())]
    command += trailing
    env = os.environ.copy()
    env["GOW3_GPU_LOG"] = "info"
    env.setdefault("GOW3_UPSCALER", "none")
    dll_dirs = [path.resolve() for path in args.dll_dir]
    default_dll = Path("C:/msys64/clang64/bin")
    if default_dll.is_dir():
        dll_dirs.append(default_dll)
    if dll_dirs:
        env["PATH"] = os.pathsep.join(map(str, dll_dirs)) + os.pathsep + env.get("PATH", "")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    label = re.sub(r"[^A-Za-z0-9_-]+", "-", args.label).strip("-")[:48] or "manual"
    stem = f"p3d-{label}-{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')}"
    log_path = args.output_dir / (stem + ".log")
    event_path = args.output_dir / (stem + ".events.jsonl")
    report_path = args.output_dir / (stem + ".json")
    report = {"schema": "p3d-owned-child-crash-v1", "started_utc": utc(),
              "command": command, "cwd": str(ROOT), "seconds": args.seconds,
              "log": str(log_path), "events": str(event_path), "report": str(report_path),
              "environment": {key: env.get(key) for key in ("GOW3_GPU_LOG", "GOW3_UPSCALER")},
              "exit_code": None, "limitations": [
                  "First chance exceptions can be handled normally by the runtime.",
                  "RBP frames and stack pointers are observations, not a full unwind.",
                  "Host frames identify modules and offsets without symbols.",
                  "First chance write AVs used by GPU page tracking do not take snapshots.",
                  "The deadline detaches the owned child and leaves it running."]}
    recent = deque(maxlen=256)
    counts = Counter()
    snapshots = deque(maxlen=32)
    child = None
    pending = None
    pending_status = DBG_CONTINUE
    debug_active = False
    first_breakpoint = True
    api = winapi()
    started = time.monotonic()
    result = 0
    log = events = None
    try:
        log = log_path.open("xb", buffering=0)
        events = event_path.open("x", encoding="utf-8", buffering=1)
        child = subprocess.Popen(command, cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
                                 stdout=log, stderr=subprocess.STDOUT,
                                 creationflags=DEBUG_ONLY_THIS_PROCESS | subprocess.CREATE_NO_WINDOW)
        debug_active = True
        report["pid"] = child.pid
        if not api.DebugSetProcessKillOnExit(False):
            raise ctypes.WinError(ctypes.get_last_error())
        inspector = ChildInspector(child, boot, args.guest_base)
        print(f"Owned child PID {child.pid}; log: {log_path}", flush=True)
        print(f"Advance the game normally; capturing up to {args.seconds:g} seconds.", flush=True)
        while time.monotonic() - started < args.seconds:
            event = DEBUG_EVENT()
            if not api.WaitForDebugEvent(ctypes.byref(event), 100):
                error = ctypes.get_last_error()
                if error in (0, 121, 258):
                    if child.poll() is not None:
                        break
                    continue
                if child.poll() is not None:
                    break
                raise ctypes.WinError(error)
            pending = event
            pending_status = DBG_EXCEPTION_NOT_HANDLED if event.code == 1 else DBG_CONTINUE
            detail = {"utc": utc(), "event": EVENT_NAMES.get(event.code, str(event.code)),
                      "tid": event.tid, "elapsed_seconds": round(time.monotonic() - started, 4)}
            counts[detail["event"]] += 1
            try:
                if event.pid != child.pid:
                    raise RuntimeError("debug event does not belong to the owned child")
                if event.code == 1:
                    exception = event.data.exception
                    record = exception.record
                    first = bool(exception.first_chance)
                    detail.update(code=address(record.code), first_chance=first,
                                  location=address(record.location or 0),
                                  parameters=[address(record.information[index])
                                              for index in range(min(record.parameter_count, 15))])
                    if record.code == STATUS_BREAKPOINT and first and first_breakpoint:
                        pending_status = DBG_CONTINUE
                        first_breakpoint = False
                        detail["startup_breakpoint_consumed"] = True
                    else:
                        access = record.information[0] if record.parameter_count >= 2 else None
                        is_av = record.code == STATUS_ACCESS_VIOLATION
                        if is_av:
                            detail["access"] = {0: "read", 1: "write", 8: "execute"}.get(access, str(access))
                            detail["target"] = address(record.information[1]) if record.parameter_count >= 2 else None
                        if not first or (is_av and access in (0, 8)):
                            try:
                                snapshot = capture(inspector, event.tid, all_threads=not first)
                                detail["snapshot"] = snapshot
                                snapshots.append({"utc": detail["utc"], "code": detail["code"],
                                                  "location": detail["location"], "first_chance": first,
                                                  **snapshot})
                                print(f"Captured {'first' if first else 'second'} chance {detail['code']} at {detail['location']}", flush=True)
                            except Exception as error:
                                detail["snapshot_error"] = f"{type(error).__name__}: {error}"
                    detail["continue_status"] = address(pending_status)
                elif event.code == 3:
                    detail["image_base"] = address(event.data.create_process.base or 0)
                elif event.code in (4, 5):
                    detail["exit_code"] = int(event.data.exit_code)
                    if event.code == 5:
                        report["exit_code"] = int(event.data.exit_code)
                        report["process_exit_observed"] = True
                elif event.code in (6, 7):
                    base = event.data.load_dll.base if event.code == 6 else event.data.base
                    detail["base"] = address(base or 0)
                elif event.code == 8:
                    info = event.data.debug_string
                    raw = inspector.read(info.data or 0, min(int(info.length), 4096) * (2 if info.unicode else 1))
                    detail["text"] = raw.decode("utf-16-le" if info.unicode else "utf-8", errors="replace").rstrip("\0")
            except Exception as error:
                detail["event_error"] = f"{type(error).__name__}: {error}"
            finally:
                if event.code in (3, 6):
                    handle = event.data.create_process.hFile if event.code == 3 else event.data.load_dll.hFile
                    if handle:
                        api.CloseHandle(handle)
                # Resume before serializing evidence so disk I/O never holds the game.
                if not api.ContinueDebugEvent(event.pid, event.tid, pending_status):
                    raise ctypes.WinError(ctypes.get_last_error())
                pending = None
            brief = {key: value for key, value in detail.items() if key != "snapshot"}
            recent.append(brief)
            events.write(json.dumps(detail, ensure_ascii=False) + "\n")
            if event.code == 5:
                debug_active = False
                child.wait(timeout=2)
                break
        report["deadline_reached"] = not report.get("process_exit_observed", False) and time.monotonic() - started >= args.seconds
    except KeyboardInterrupt:
        report["error"] = "interrupted by user; owned child detached"
        result = 130
    except Exception as error:
        report["error"] = f"{type(error).__name__}: {error}"
        result = 1
    finally:
        if pending is not None:
            report["pending_event_continued"] = bool(api.ContinueDebugEvent(pending.pid, pending.tid, pending_status))
        if child is not None:
            if debug_active and child.poll() is None:
                detached = bool(api.DebugActiveProcessStop(child.pid))
                report["detached_owned_child"] = detached
                if not detached:
                    report["detach_error"] = ctypes.get_last_error()
                    result = 1
            status = child.poll()
            if status is not None:
                report["exit_code"] = status
            report["child_still_running"] = status is None
        report["elapsed_seconds"] = round(time.monotonic() - started, 3)
        report["finished_utc"] = utc()
        report["event_counts"] = dict(counts)
        report["recent_events"] = list(recent)
        report["snapshots"] = list(snapshots)
        if events:
            events.close()
        if log:
            status_line = f"\n[crash_capture] exit_code={report['exit_code']} child_still_running={report.get('child_still_running')}\n"
            log.write(status_line.encode("utf-8"))
            log.close()
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"Report: {report_path}", flush=True)
        print(f"Exit code: {report['exit_code']}; child running: {report.get('child_still_running')}", flush=True)
    return result


if __name__ == "__main__":
    raise SystemExit(main())
