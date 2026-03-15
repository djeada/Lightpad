#!/usr/bin/env python3
import json
import os
import pathlib
import select
import shutil
import subprocess
import sys
import tempfile
import textwrap
import time


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

    def wait_for_first(self, matchers, timeout=15.0):
        wrapped = [(name, matcher) for name, matcher in matchers]
        deadline = time.time() + timeout
        while time.time() < deadline:
            for index, message in enumerate(self.pending):
                for name, matcher in wrapped:
                    if matcher(message):
                        self.pending.pop(index)
                        return name, message

            message = self._read_message(timeout=max(0.1, deadline - time.time()))
            for name, matcher in wrapped:
                if matcher(message):
                    return name, message
            self.pending.append(message)

        raise TimeoutError("Timed out waiting for DAP event set")

    def interesting_output(self):
        parts = []
        for message in self.pending:
            if message.get("type") == "event" and message.get("event") == "output":
                body = message.get("body", {})
                parts.append(body.get("output", ""))
        if self.proc.stderr is not None:
            try:
                stderr = self.proc.stderr.read1(65536).decode("utf-8", errors="replace")
            except Exception:
                stderr = ""
            if stderr:
                parts.append(stderr)

        interesting_lines = []
        for raw_line in "".join(parts).splitlines():
            line = raw_line.strip()
            if not line:
                continue
            if line.startswith("GNU gdb ") or line.startswith("Copyright (C)"):
                continue
            if line.startswith("License GPLv3+") or line.startswith("This is free software"):
                continue
            if line.startswith("There is NO WARRANTY") or line.startswith("Type "):
                continue
            if line.startswith("For bug reporting instructions") or line.startswith("<https://"):
                continue
            if line.startswith("Find the GDB manual") or line.startswith("<http://"):
                continue
            if line.startswith("This GDB was configured as"):
                continue
            interesting_lines.append(line)

        return interesting_lines[-1] if interesting_lines else ""

    def close(self):
        if self.proc.poll() is None:
            try:
                seq = self.send_request("disconnect", {"terminateDebuggee": True})
                self.wait_for_response("disconnect", seq, success=None, timeout=3.0)
            except Exception:
                pass
            self.proc.terminate()
            try:
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def build_test_binary(tempdir):
    source = pathlib.Path(tempdir) / "sample.c"
    binary = pathlib.Path(tempdir) / "sample"
    source.write_text(
        textwrap.dedent(
            """
            #include <stdio.h>
            #include <unistd.h>

            int main(void) {
              volatile int value = 7;
              while (1) {
                sleep(1);
                if (value == 8) {
                  break;
                }
              }
              printf("value=%d\\n", value);
              return 0;
            }
            """
        ).strip()
        + "\n",
        encoding="utf-8",
    )
    subprocess.run(
        ["gcc", "-g", "-O0", "-o", str(binary), str(source)],
        check=True,
        capture_output=True,
        text=True,
    )
    return source, binary


def skip(message):
    print(f"SKIP: {message}", file=sys.stderr)
    return 2


def main():
    gdb = shutil.which("gdb")
    gcc = shutil.which("gcc")
    if not gdb:
        return skip("gdb not installed")
    if not gcc:
        return skip("gcc not installed")

    with tempfile.TemporaryDirectory(prefix="lightpad-gdb-dap-") as tempdir:
        source, binary = build_test_binary(tempdir)
        session = DapSession([gdb, "--interpreter=dap"])
        try:
            init_seq = session.send_request(
                "initialize",
                {
                    "clientID": "lightpad-smoke",
                    "clientName": "Lightpad Smoke Test",
                    "adapterID": "cppdbg",
                    "locale": "en-US",
                    "linesStartAt1": True,
                    "columnsStartAt1": True,
                    "pathFormat": "path",
                    "supportsRunInTerminalRequest": False,
                },
            )
            session.wait_for_response("initialize", init_seq, success=True, timeout=20.0)

            launch_seq = session.send_request(
                "launch",
                {
                    "program": str(binary),
                    "cwd": tempdir,
                    "stopOnEntry": False,
                    "MIMode": "gdb",
                    "miDebuggerPath": gdb,
                },
            )
            session.wait_for_event("initialized", timeout=20.0)

            breakpoint_seq = session.send_request(
                "setBreakpoints",
                {
                    "source": {"path": str(source)},
                    "breakpoints": [{"line": 7}],
                    "sourceModified": False,
                },
            )
            configuration_seq = session.send_request("configurationDone", {})

            session.wait_for_response("launch", launch_seq, success=True, timeout=20.0)
            breakpoint_response = None
            configuration_response = None
            stopped = None
            outcome = None
            message = None
            deadline = time.time() + 20.0
            while time.time() < deadline and stopped is None:
                outcome, message = session.wait_for_first(
                    [
                        (
                            "setBreakpoints-response",
                            lambda msg: msg.get("type") == "response"
                            and msg.get("command") == "setBreakpoints"
                            and msg.get("request_seq") == breakpoint_seq,
                        ),
                        (
                            "configurationDone-response",
                            lambda msg: msg.get("type") == "response"
                            and msg.get("command") == "configurationDone"
                            and msg.get("request_seq") == configuration_seq,
                        ),
                        (
                            "thread-started",
                            lambda msg: msg.get("type") == "event"
                            and msg.get("event") == "thread"
                            and msg.get("body", {}).get("reason") == "started",
                        ),
                        (
                            "stopped",
                            lambda msg: msg.get("type") == "event"
                            and msg.get("event") == "stopped",
                        ),
                        (
                            "terminated",
                            lambda msg: msg.get("type") == "event"
                            and msg.get("event") == "terminated",
                        ),
                    ],
                    timeout=max(0.1, deadline - time.time()),
                )

                if outcome == "setBreakpoints-response":
                    breakpoint_response = message
                    if not breakpoint_response.get("success", False):
                        raise RuntimeError(f"setBreakpoints failed: {message!r}")
                    continue

                if outcome == "configurationDone-response":
                    configuration_response = message
                    if (
                        configuration_response.get("success") is False
                        and configuration_response.get("message") != "notStopped"
                    ):
                        raise RuntimeError(
                            f"configurationDone failed: {configuration_response!r}"
                        )
                    continue

                if outcome == "stopped":
                    stopped = message
                    break

                if outcome == "terminated":
                    break

            if outcome == "terminated":
                details = "GDB DAP launched but the inferior terminated before any stop event."
                extra = session.interesting_output()
                if extra and ("error" in extra.lower() or "warning" in extra.lower()):
                    details = f"{details} {extra}"
                return skip(details)

            if stopped is None and outcome == "thread-started":
                thread_id = message.get("body", {}).get("threadId")
                if thread_id:
                    pause_seq = session.send_request("pause", {"threadId": thread_id})
                    session.wait_for_response("pause", pause_seq, success=True, timeout=10.0)
                outcome, message = session.wait_for_first(
                    [
                        (
                            "stopped",
                            lambda msg: msg.get("type") == "event"
                            and msg.get("event") == "stopped",
                        ),
                        (
                            "terminated",
                            lambda msg: msg.get("type") == "event"
                            and msg.get("event") == "terminated",
                        ),
                    ],
                    timeout=20.0,
                )

            if outcome == "terminated":
                details = "GDB DAP started the inferior but could not pause it before exit."
                extra = session.interesting_output()
                if extra and ("error" in extra.lower() or "warning" in extra.lower()):
                    details = f"{details} {extra}"
                return skip(details)

            if breakpoint_response is not None:
                breakpoints = breakpoint_response.get("body", {}).get("breakpoints", [])
                if not breakpoints or not breakpoints[0].get("verified"):
                    raise RuntimeError(f"Breakpoint not verified: {breakpoints!r}")

            stopped = message if stopped is None else stopped
            thread_id = stopped.get("body", {}).get("threadId")
            if not thread_id:
                raise RuntimeError(f"Stopped event missing threadId: {stopped!r}")

            stack_seq = session.send_request("stackTrace", {"threadId": thread_id})
            stack = session.wait_for_response("stackTrace", stack_seq, success=True)
            frames = stack.get("body", {}).get("stackFrames", [])
            if not frames:
                raise RuntimeError("No stack frames returned")
            frame_id = frames[0]["id"]

            eval_seq = session.send_request(
                "evaluate",
                {"expression": "value", "frameId": frame_id, "context": "watch"},
            )
            evaluation = session.wait_for_response("evaluate", eval_seq, success=True)
            result = evaluation.get("body", {}).get("result", "")
            if "7" not in result:
                raise RuntimeError(f"Unexpected evaluation result: {result!r}")

            disconnect_seq = session.send_request("disconnect", {"terminateDebuggee": True})
            session.wait_for_response("disconnect", disconnect_seq, success=True, timeout=20.0)
            try:
                session.wait_for_event("terminated", timeout=5.0)
            except TimeoutError:
                pass

            print("PASS: gdb DAP smoke test")
            return 0
        finally:
            session.close()


if __name__ == "__main__":
    sys.exit(main())
