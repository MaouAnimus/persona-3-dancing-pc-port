"""Bounded dialogue regression using the runtime's scripted controller input."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=float, default=70)
    parser.add_argument("--label", default="dialogue")
    parser.add_argument("--validation", action="store_true")
    parser.add_argument("--skip-intro", action="store_true")
    parser.add_argument("--capture-at", type=float)
    parser.add_argument("--env", action="append", default=[], metavar="NAME=VALUE")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 600:
        parser.error("seconds must be between 1 and 600")
    label = "".join(c for c in args.label if c.isalnum() or c in "-_") or "dialogue"
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output = ROOT / "docs/evidence/crash-20261017" / f"{label}-{stamp}"
    output.mkdir(parents=True, exist_ok=False)
    test_user = output / "user"
    test_user.mkdir()
    saves = ROOT / "user/savedata"
    if saves.is_dir():
        shutil.copytree(saves, test_user / "savedata")
    pad = output / "pad.txt"
    pad.write_text("", encoding="ascii")
    executable = ROOT / "bin/p3d_engine.exe"
    env = os.environ.copy()
    env.update(GOW3_PAD_FILE=str(pad), GOW3_GPU_LOG="info", GOW3_UPSCALER="none",
               GOW3_GPU_USER_DIR=str(ROOT / "user"))
    capture_trigger = output / "capture.request"
    if args.capture_at is not None:
        env.update(GOW3_FRAME_DUMP_TRIGGER=str(capture_trigger), GOW3_DUMP_DIR=str(output))
    env["PATH"] = str(ROOT / "bin") + ";C:/msys64/clang64/bin;" + env.get("PATH", "")
    if args.validation:
        env.update(VK_LAYER_PATH="C:/msys64/clang64/bin", GOW3_VK_VALIDATION="1",
                   GOW3_VK_VALIDATION_SYNC="1")
    for item in args.env:
        name, value = item.split("=", 1)
        env[name] = value
    command = [str(executable), str(ROOT / "out/p3d_test/boot-linked.bin"),
               "--app0", str(ROOT / "CUSA12636"), "--user", str(test_user), "--timeout", "0"]
    report = {"command": command, "executable_sha256": hashlib.file_digest(executable.open("rb"), "sha256").hexdigest(),
              "environment": {k: v for k, v in env.items() if k.startswith(("GOW3_", "P3D_", "VK_LAYER"))},
              "started_utc": datetime.now(timezone.utc).isoformat(), "output": str(output), "pulses": 0}
    started = time.monotonic()
    with (output / "runtime.log").open("wb") as log:
        child = subprocess.Popen(command, cwd=ROOT, env=env, stdin=subprocess.DEVNULL,
                                 stdout=log, stderr=subprocess.STDOUT,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        report["pid"] = child.pid
        print(f"PID {child.pid}; log: {output / 'runtime.log'}", flush=True)
        next_press = started + 8
        release = None
        capture_requested = False
        try:
            while child.poll() is None and time.monotonic() - started < args.seconds:
                now = time.monotonic()
                if release is not None and now >= release:
                    pad.write_text("", encoding="ascii")
                    release = None
                if now >= next_press:
                    buttons = "l1 r1" if args.skip_intro and report["pulses"] == 0 else "cross"
                    pad.write_text(buttons, encoding="ascii")
                    report["pulses"] += 1
                    release = now + 0.2
                    next_press = now + 1.4
                if args.capture_at is not None and not capture_requested and now - started >= args.capture_at:
                    capture_trigger.write_text("capture", encoding="ascii")
                    capture_requested = True
                time.sleep(0.05)
            report["observed_exit_code"] = child.poll()
            report["deadline_reached"] = child.poll() is None
        finally:
            pad.write_text("", encoding="ascii")
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=10)
                report["cleanup"] = "terminated only the owned regression process at the deadline"
            report["elapsed_seconds"] = round(time.monotonic() - started, 3)
            (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: report.get(k) for k in ("output", "observed_exit_code", "deadline_reached", "pulses", "elapsed_seconds")}), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
