#!/usr/bin/env python3
"""Launch the real Lightpad binary headless and make sure it stays up.

Unit tests construct widgets in isolation; this catches what they cannot:
static-init order, resource/plugin registration, settings load and the
MainWindow constructor all running together. Usage: app_launch_smoke_test.py
<path-to-Lightpad> [seconds]. Exit 2 (ctest SKIP) if the binary is missing.
"""
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time


def main():
    if len(sys.argv) < 2 or not os.path.exists(sys.argv[1]):
        print("Lightpad binary not found; skipping")
        return 2
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 5.0

    with tempfile.TemporaryDirectory(prefix="lightpad-smoke-") as tmp:
        root = pathlib.Path(tmp)
        for name in ("home", "config", "data", "cache"):
            (root / name).mkdir()
        sample = root / "sample.py"
        sample.write_text("def main():\n    print('hello')\n")

        env = dict(os.environ)
        env.update(
            QT_QPA_PLATFORM="offscreen",
            HOME=str(root / "home"),
            XDG_CONFIG_HOME=str(root / "config"),
            XDG_DATA_HOME=str(root / "data"),
            XDG_CACHE_HOME=str(root / "cache"),
        )
        proc = subprocess.Popen(
            [sys.argv[1], str(sample)],
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        deadline = time.time() + seconds
        while time.time() < deadline:
            if proc.poll() is not None:
                out = proc.stdout.read().decode(errors="replace")
                print(f"FAIL: exited early with {proc.returncode}\n{out}")
                return 1
            time.sleep(0.1)

        proc.send_signal(signal.SIGTERM)  # by PID only, never by name
        try:
            out, _ = proc.communicate(timeout=15)
        except subprocess.TimeoutExpired:
            proc.kill()
            out, _ = proc.communicate()
        text = out.decode(errors="replace")
        if "Segmentation fault" in text or "ASAN" in text:
            print(f"FAIL: crash output\n{text}")
            return 1
        print(f"OK: stayed up for {seconds:.0f}s")
        return 0


if __name__ == "__main__":
    sys.exit(main())
