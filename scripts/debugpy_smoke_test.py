#!/usr/bin/env python3
import importlib.util
import json
import os
import pathlib
import select
import shutil
import socket
import subprocess
import sys
import tempfile
import textwrap
import time


def find_python_interpreter():
    current = pathlib.Path.cwd()
    for directory in (current, *current.parents):
        for env_name in (".venv", "venv"):
            candidate = directory / env_name / "bin" / "python"
            if candidate.exists() and os.access(candidate, os.X_OK):
                return str(candidate)
    return shutil.which("python3") or shutil.which("python")


class DapSession:
    def __init__(self, argv):
        self.proc = subprocess.Popen(
            argv,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.seq = 1
        self.buffer = b""
        self.pending = []

    def send_request(self, command, arguments=None):
        message = {"seq": self.seq, "type": "request", "command": command}
        if arguments is not None:
            message["arguments"] = arguments
        payload = json.dumps(message, separators=(",", ":")).encode("utf-8")
        header = f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii")
        assert self.proc.stdin is not None
        self.proc.stdin.write(header)
        self.proc.stdin.write(payload)
        self.proc.stdin.flush()
        self.seq += 1
        return message["seq"]

    def _read_message(self, timeout):
        deadline = time.time() + timeout
        assert self.proc.stdout is not None
        while time.time() < deadline:
            header_end = self.buffer.find(b"\r\n\r\n")
            if header_end >= 0:
                header = self.buffer[:header_end].decode("ascii", errors="replace")
                content_length = None
                for line in header.split("\r\n"):
                    if line.lower().startswith("content-length:"):
                        content_length = int(line.split(":", 1)[1].strip())
                        break
                if content_length is None:
                    raise RuntimeError(f"Malformed DAP header: {header!r}")
                total = header_end + 4 + content_length
                if len(self.buffer) >= total:
                    payload = self.buffer[header_end + 4 : total]
                    self.buffer = self.buffer[total:]
                    return json.loads(payload.decode("utf-8"))

            remaining = max(0.0, deadline - time.time())
            ready, _, _ = select.select([self.proc.stdout], [], [], remaining)
            if not ready:
                break

            chunk = os.read(self.proc.stdout.fileno(), 65536)
            if not chunk:
                break
            self.buffer += chunk

        raise TimeoutError("Timed out waiting for DAP message")

    def wait_for(self, matcher, timeout=15.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            for index, message in enumerate(self.pending):
                if matcher(message):
                    return self.pending.pop(index)

            message = self._read_message(timeout=max(0.1, deadline - time.time()))
            if matcher(message):
                return message
            self.pending.append(message)

        raise TimeoutError("Timed out waiting for matching DAP message")

    def wait_for_response(self, command, request_seq, success=True, timeout=15.0):
        return self.wait_for(
            lambda message: message.get("type") == "response"
            and message.get("command") == command
            and message.get("request_seq") == request_seq
            and (success is None or message.get("success") == success),
            timeout=timeout,
        )

    def wait_for_event(self, event, timeout=15.0):
        return self.wait_for(
            lambda message: message.get("type") == "event"
            and message.get("event") == event,
            timeout=timeout,
        )

    def close(self):
        if self.proc.poll() is None:
            try:
                self.proc.terminate()
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def skip(message):
    print(f"SKIP: {message}", file=sys.stderr)
    return 2


def can_open_localhost_socket():
    sock = None
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.bind(("127.0.0.1", 0))
        return True
    except OSError:
        return False
    finally:
        try:
            sock.close()
        except Exception:
            pass


def main():
    python = find_python_interpreter()
    if not python:
        return skip("python interpreter not installed")
    if not can_open_localhost_socket():
        return skip("localhost sockets are unavailable in this environment")
    probe = subprocess.run(
        [python, "-c", "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('debugpy') else 1)"],
        capture_output=True,
        text=True,
    )
    if probe.returncode != 0:
        return skip(f"debugpy not installed for {python}")

    with tempfile.TemporaryDirectory(prefix="lightpad-debugpy-smoke-") as tempdir:
        program = pathlib.Path(tempdir) / "sample.py"
        program.write_text(
            textwrap.dedent(
                """
                import time

                value = 7
                while True:
                    time.sleep(1)
                    if value == 8:
                        break
                """
            ).strip()
            + "\n",
            encoding="utf-8",
        )

        session = DapSession([python, "-m", "debugpy.adapter"])
        try:
            init_seq = session.send_request(
                "initialize",
                {
                    "clientID": "lightpad-smoke",
                    "clientName": "Lightpad Smoke Test",
                    "adapterID": "debugpy",
                    "locale": "en-US",
                    "linesStartAt1": True,
                    "columnsStartAt1": True,
                    "pathFormat": "path",
                    "supportsRunInTerminalRequest": True,
                },
            )
            session.wait_for_response("initialize", init_seq, success=True, timeout=20.0)

            launch_seq = session.send_request(
                "launch",
                {
                    "type": "debugpy",
                    "request": "launch",
                    "program": str(program),
                    "cwd": tempdir,
                    "console": "internalConsole",
                    "python": [python],
                    "justMyCode": False,
                },
            )
            session.wait_for_event("initialized", timeout=20.0)
            configuration_seq = session.send_request("configurationDone", {})
            session.wait_for_response("launch", launch_seq, success=True, timeout=20.0)
            session.wait_for_response(
                "configurationDone", configuration_seq, success=True, timeout=20.0
            )

            print("PASS: debugpy DAP smoke test launched")
            return 0
        finally:
            session.close()


if __name__ == "__main__":
    sys.exit(main())
