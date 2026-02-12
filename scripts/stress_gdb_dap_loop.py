#!/usr/bin/env python3
"""
Timeout-guarded GDB DAP stress harness for loop stepping.

What it does:
- Builds a tiny C++ loop binary with debug symbols
- Starts `gdb --interpreter=dap`
- Runs a DAP session with source breakpoints + repeated step-over
- Optionally mimics Lightpad-style inspection requests per stop
- Tracks event/request counts and RSS memory
- Enforces strict timeouts and force-kills GDB on hang
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import pathlib
import select
import subprocess
import sys
import tempfile
import textwrap
import time
from dataclasses import dataclass
from typing import Callable, Deque, Dict, List, Optional


def now() -> float:
    return time.monotonic()


def read_rss_kb(pid: int) -> int:
    try:
        with open(f"/proc/{pid}/status", "r", encoding="utf-8") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    parts = line.split()
                    if len(parts) >= 2:
                        return int(parts[1])
    except OSError:
        return -1
    return -1


def build_test_binary(workdir: pathlib.Path) -> tuple[pathlib.Path, pathlib.Path, int]:
    src = workdir / "loop.cpp"
    exe = workdir / "loop.bin"
    code = textwrap.dedent(
        """\
        #include <cstdint>
        #include <iostream>

        volatile std::int64_t g_sink = 0;

        int main() {
          std::int64_t acc = 0;
          for (int i = 0; i < 1000000; ++i) {
            acc += (i % 7); // BREAK_HERE
            g_sink = acc;
          }
          std::cout << acc << "\\n";
          return 0;
        }
        """
    )
    src.write_text(code, encoding="utf-8")

    lines = src.read_text(encoding="utf-8").splitlines()
    bp_line = 0
    for i, line in enumerate(lines, start=1):
        if "BREAK_HERE" in line:
            bp_line = i
            break
    if bp_line <= 0:
        raise RuntimeError("Failed to locate BREAK_HERE line")

    cmd = ["g++", "-g", "-O0", "-fno-omit-frame-pointer", str(src), "-o", str(exe)]
    subprocess.run(cmd, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return src, exe, bp_line


def parse_program_args(values: Optional[List[str]]) -> List[str]:
    if not values:
        return []
    out: List[str] = []
    for v in values:
        if not v:
            continue
        out.append(v)
    return out


class DapProtocolError(RuntimeError):
    pass


@dataclass
class DapMessage:
    raw: dict

    @property
    def type(self) -> str:
        return str(self.raw.get("type", ""))

    @property
    def event(self) -> str:
        return str(self.raw.get("event", ""))

    @property
    def command(self) -> str:
        return str(self.raw.get("command", ""))


class DapIO:
    def __init__(self, proc: subprocess.Popen[bytes], verbose: bool = False):
        self.proc = proc
        self.buffer = bytearray()
        self.seq = 1
        self.by_seq: Dict[int, DapMessage] = {}
        self.events: Deque[DapMessage] = collections.deque()
        self.other: Deque[DapMessage] = collections.deque()
        self.verbose = verbose

    def send_request(self, command: str, arguments: Optional[dict] = None) -> int:
        seq = self.seq
        self.seq += 1
        payload: dict = {"seq": seq, "type": "request", "command": command}
        if arguments:
            payload["arguments"] = arguments
        blob = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        header = f"Content-Length: {len(blob)}\r\n\r\n".encode("ascii")
        self.proc.stdin.write(header + blob)
        self.proc.stdin.flush()
        return seq

    def _pump_once(self, timeout_s: float) -> Optional[DapMessage]:
        if self.proc.poll() is not None:
            raise DapProtocolError(f"GDB exited early with code {self.proc.returncode}")

        if b"\r\n\r\n" not in self.buffer:
            rlist, _, _ = select.select([self.proc.stdout], [], [], timeout_s)
            if not rlist:
                return None
            chunk = os.read(self.proc.stdout.fileno(), 65536)
            if not chunk:
                raise DapProtocolError("DAP stdout closed")
            self.buffer.extend(chunk)

        # Parse as many frames as possible, return first parsed message.
        while True:
            hdr_end = self.buffer.find(b"\r\n\r\n")
            if hdr_end < 0:
                return None
            header = bytes(self.buffer[:hdr_end])
            content_len = -1
            for ln in header.split(b"\n"):
                ln = ln.strip()
                if ln.lower().startswith(b"content-length:"):
                    try:
                        content_len = int(ln.split(b":", 1)[1].strip())
                    except ValueError:
                        content_len = -1
                    break
            if content_len <= 0:
                # Resync by dropping malformed header.
                del self.buffer[: hdr_end + 4]
                continue

            msg_start = hdr_end + 4
            msg_end = msg_start + content_len
            if len(self.buffer) < msg_end:
                rlist, _, _ = select.select([self.proc.stdout], [], [], timeout_s)
                if not rlist:
                    return None
                chunk = os.read(self.proc.stdout.fileno(), 65536)
                if not chunk:
                    raise DapProtocolError("DAP stdout closed while waiting payload")
                self.buffer.extend(chunk)
                continue

            payload = bytes(self.buffer[msg_start:msg_end])
            del self.buffer[:msg_end]
            try:
                obj = json.loads(payload.decode("utf-8", errors="replace"))
            except json.JSONDecodeError:
                continue
            return DapMessage(obj)

    def pump(self, timeout_s: float) -> Optional[DapMessage]:
        msg = self._pump_once(timeout_s)
        if msg is None:
            return None
        if msg.type == "response":
            req_seq = int(msg.raw.get("request_seq", -1))
            if req_seq >= 0:
                self.by_seq[req_seq] = msg
            else:
                self.other.append(msg)
            if self.verbose:
                print(
                    f"[dap] response req_seq={req_seq} cmd={msg.command} "
                    f"success={msg.raw.get('success', False)}"
                )
        elif msg.type == "event":
            self.events.append(msg)
            if self.verbose:
                print(f"[dap] event {msg.event} body={msg.raw.get('body', {})}")
        else:
            self.other.append(msg)
            if self.verbose:
                print(f"[dap] other {msg.raw}")
        return msg

    def wait_response(self, req_seq: int, timeout_s: float) -> DapMessage:
        deadline = now() + timeout_s
        while now() < deadline:
            hit = self.by_seq.pop(req_seq, None)
            if hit is not None:
                return hit
            self.pump(max(0.01, min(0.25, deadline - now())))
        raise TimeoutError(f"Timeout waiting response request_seq={req_seq}")

    def wait_event(
        self,
        event_name: str,
        timeout_s: float,
        predicate: Optional[Callable[[dict], bool]] = None,
    ) -> DapMessage:
        deadline = now() + timeout_s
        while now() < deadline:
            # Scan queued events first.
            for _ in range(len(self.events)):
                msg = self.events.popleft()
                if msg.event == event_name and (predicate is None or predicate(msg.raw)):
                    return msg
                self.events.append(msg)
            self.pump(max(0.01, min(0.25, deadline - now())))
        raise TimeoutError(f"Timeout waiting event={event_name}")


def summarize_stop(evt: dict) -> str:
    body = evt.get("body", {}) or {}
    reason = body.get("reason", "?")
    th = body.get("threadId", 0)
    all_stopped = body.get("allThreadsStopped", False)
    desc = body.get("description", "")
    return f"reason={reason} thread={th} allThreadsStopped={all_stopped} desc={desc}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--steps", type=int, default=300, help="Number of step-over operations")
    ap.add_argument(
        "--mode",
        choices=("lightpad", "minimal"),
        default="lightpad",
        help="lightpad: threads+stack+scopes+variables each stop; minimal: step only",
    )
    ap.add_argument("--timeout", type=float, default=3.0, help="Per wait timeout in seconds")
    ap.add_argument("--session-timeout", type=float, default=120.0, help="Whole session timeout")
    ap.add_argument("--max-rss-kb", type=int, default=700000, help="Kill if gdb RSS exceeds this")
    ap.add_argument("--include-registers", action="store_true", help="Request register scope variables too")
    ap.add_argument("--verbose", action="store_true", help="Print raw event/response flow")
    ap.add_argument(
        "--launch-order",
        choices=("before", "after"),
        default="before",
        help="Send setBreakpoints before launch (before) or after launch (after)",
    )
    ap.add_argument(
        "--idle-after-steps",
        type=float,
        default=0.0,
        help="Seconds to keep session idle after final step and count spontaneous events",
    )
    ap.add_argument(
        "--program",
        type=str,
        default="",
        help="Path to prebuilt debug target (if empty, harness builds sample loop binary)",
    )
    ap.add_argument(
        "--source",
        type=str,
        default="",
        help="Source file path used for setBreakpoints (required with --program)",
    )
    ap.add_argument(
        "--break-line",
        type=int,
        default=0,
        help="Breakpoint line number in --source (required with --program)",
    )
    ap.add_argument(
        "--cwd",
        type=str,
        default="",
        help="Working directory for launch request (defaults to program folder or temp dir)",
    )
    ap.add_argument(
        "--program-arg",
        action="append",
        default=[],
        help="Program argument (repeat flag for multiple arguments)",
    )
    ap.add_argument(
        "--variables-count",
        type=int,
        default=0,
        help="If >0, send variables requests with count limit",
    )
    ap.add_argument(
        "--variables-filter",
        choices=("", "named", "indexed"),
        default="",
        help="Optional DAP variables filter",
    )
    ap.add_argument(
        "--stop-on-entry",
        action="store_true",
        help="Request stop on entry before running to breakpoints",
    )
    ap.add_argument(
        "--ignore-variables-errors",
        action="store_true",
        help="Continue test when variables request fails",
    )
    ap.add_argument(
        "--inspect-first-stop",
        action="store_true",
        help="Run threads/stack/scopes/variables inspection at first stop before stepping",
    )
    ap.add_argument(
        "--max-scope-loads",
        type=int,
        default=0,
        help="If >0, only query variables for the first N non-register scopes per stop",
    )
    ap.add_argument(
        "--prefer-scope",
        type=str,
        default="",
        help="If set, scopes containing this substring are queried first",
    )
    ap.add_argument(
        "--locals-via-evaluate",
        action="store_true",
        help='For "Locals" scope, use evaluate(\'interpreter-exec console "info locals"\')',
    )
    args = ap.parse_args()

    global_deadline = now() + args.session_timeout

    with tempfile.TemporaryDirectory(prefix="gdb-dap-loop-") as td:
        tmp = pathlib.Path(td)
        if args.program:
            exe = pathlib.Path(args.program).expanduser().resolve()
            if not exe.exists():
                raise FileNotFoundError(f"Program not found: {exe}")
            if not args.source:
                raise ValueError("--source is required when --program is provided")
            if args.break_line <= 0:
                raise ValueError("--break-line must be > 0 when --program is provided")
            src = pathlib.Path(args.source).expanduser().resolve()
            if not src.exists():
                raise FileNotFoundError(f"Source not found: {src}")
            bp_line = args.break_line
        else:
            src, exe, bp_line = build_test_binary(tmp)
        launch_cwd = (
            pathlib.Path(args.cwd).expanduser().resolve()
            if args.cwd
            else (exe.parent if args.program else tmp)
        )
        program_args = parse_program_args(args.program_arg)
        print(f"[harness] source={src} line={bp_line} exe={exe}")

        proc = subprocess.Popen(
            ["gdb", "--interpreter=dap", "-q"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        assert proc.stdin is not None
        assert proc.stdout is not None
        io = DapIO(proc, verbose=args.verbose)

        req_count = 0
        evt_count = 0
        stop_count = 0
        dup_stop_count = 0
        prev_stop_fingerprint = None
        peak_rss = -1

        def check_guard(where: str) -> None:
            nonlocal peak_rss
            if now() > global_deadline:
                raise TimeoutError(f"Session timeout exceeded at {where}")
            rss = read_rss_kb(proc.pid)
            if rss > peak_rss:
                peak_rss = rss
            if rss > args.max_rss_kb:
                raise RuntimeError(f"GDB RSS {rss}kB exceeded limit {args.max_rss_kb}kB at {where}")

        try:
            # initialize
            s = io.send_request(
                "initialize",
                {
                    "clientID": "lightpad-harness",
                    "clientName": "LightpadHarness",
                    "adapterID": "gdb",
                    "linesStartAt1": True,
                    "columnsStartAt1": True,
                    "pathFormat": "path",
                    "supportsVariableType": True,
                    "supportsVariablePaging": True,
                    "supportsRunInTerminalRequest": False,
                    "supportsMemoryReferences": True,
                },
            )
            req_count += 1
            init_resp = io.wait_response(s, args.timeout).raw
            if not init_resp.get("success", False):
                raise DapProtocolError(f"initialize failed: {init_resp}")
            supports_cfg_done = bool(
                ((init_resp.get("body") or {}).get("supportsConfigurationDoneRequest", False))
            )

            try:
                io.wait_event("initialized", args.timeout)
                evt_count += 1
            except TimeoutError:
                # Some adapters are lax here; continue if initialize succeeded.
                if args.verbose:
                    print("[harness] initialized event not observed within timeout")

            if args.launch_order == "before":
                s = io.send_request(
                    "setBreakpoints",
                    {
                        "source": {"path": str(src)},
                        "breakpoints": [{"line": bp_line}],
                    },
                )
                req_count += 1
                bp_resp = io.wait_response(s, args.timeout).raw
                if not bp_resp.get("success", False):
                    raise DapProtocolError(f"setBreakpoints failed: {bp_resp}")

            # launch
            s = io.send_request(
                "launch",
                {
                    "program": str(exe),
                    "cwd": str(launch_cwd),
                    "args": program_args,
                    "stopOnEntry": args.stop_on_entry,
                    "stopAtBeginningOfMainSubprogram": args.stop_on_entry,
                },
            )
            req_count += 1
            launch_resp = io.wait_response(s, args.timeout).raw
            if not launch_resp.get("success", False):
                raise DapProtocolError(f"launch failed: {launch_resp}")

            if args.launch_order == "after":
                s = io.send_request(
                    "setBreakpoints",
                    {
                        "source": {"path": str(src)},
                        "breakpoints": [{"line": bp_line}],
                    },
                )
                req_count += 1
                bp_resp = io.wait_response(s, args.timeout).raw
                if not bp_resp.get("success", False):
                    raise DapProtocolError(f"setBreakpoints failed: {bp_resp}")

            if supports_cfg_done:
                s = io.send_request("configurationDone", {})
                req_count += 1
                cfg_resp = io.wait_response(s, args.timeout).raw
                if not cfg_resp.get("success", False):
                    raise DapProtocolError(f"configurationDone failed: {cfg_resp}")
            else:
                if args.verbose:
                    print("[harness] adapter did not advertise configurationDone support")

            # Wait until we hit breakpoint inside loop.
            while True:
                check_guard("waiting first stop")
                ev = io.wait_event("stopped", max(args.timeout, 8.0)).raw
                evt_count += 1
                stop_count += 1
                body = ev.get("body", {}) or {}
                reason = body.get("reason", "")
                if reason == "entry":
                    active_thread = int(body.get("threadId", 0))
                    s = io.send_request("continue", {"threadId": active_thread})
                    req_count += 1
                    cont_resp = io.wait_response(s, args.timeout).raw
                    if not cont_resp.get("success", False):
                        raise DapProtocolError(f"continue after entry failed: {cont_resp}")
                    continue
                if reason in ("breakpoint", "step", "pause"):
                    break

            print(f"[harness] first stop: {summarize_stop(ev)}")

            active_thread = int((ev.get("body") or {}).get("threadId", 0))

            def run_lightpad_inspection(step_index: int) -> None:
                nonlocal req_count, active_thread
                # threads
                s2 = io.send_request("threads", {})
                req_count += 1
                th_resp = io.wait_response(s2, args.timeout).raw
                if not th_resp.get("success", False):
                    raise DapProtocolError(f"threads failed at step {step_index}: {th_resp}")
                threads = ((th_resp.get("body") or {}).get("threads") or [])
                if threads and active_thread <= 0:
                    active_thread = int(threads[0].get("id", 0))

                # stackTrace
                s2 = io.send_request(
                    "stackTrace",
                    {"threadId": active_thread, "startFrame": 0, "levels": 64},
                )
                req_count += 1
                st_resp = io.wait_response(s2, args.timeout).raw
                if not st_resp.get("success", False):
                    raise DapProtocolError(f"stackTrace failed at step {step_index}: {st_resp}")
                frames = ((st_resp.get("body") or {}).get("stackFrames") or [])
                if not frames:
                    raise DapProtocolError(f"No stack frames at step {step_index}")
                frame_id = int(frames[0].get("id", 0))

                # scopes
                s2 = io.send_request("scopes", {"frameId": frame_id})
                req_count += 1
                sc_resp = io.wait_response(s2, args.timeout).raw
                if not sc_resp.get("success", False):
                    raise DapProtocolError(f"scopes failed at step {step_index}: {sc_resp}")
                scopes = ((sc_resp.get("body") or {}).get("scopes") or [])
                if args.verbose:
                    for sc in scopes:
                        print(
                            "[harness] scope "
                            f"name={sc.get('name','')} "
                            f"ref={sc.get('variablesReference',0)} "
                            f"expensive={sc.get('expensive',False)}"
                        )
                if args.prefer_scope:
                    pref = args.prefer_scope.lower()
                    scopes = sorted(
                        scopes,
                        key=lambda sc: 0 if pref in str(sc.get("name", "")).lower() else 1,
                    )
                loaded_scopes = 0
                for scope in scopes:
                    name = str(scope.get("name", "")).lower()
                    if "register" in name and not args.include_registers:
                        continue

                    if args.locals_via_evaluate and "local" in name:
                        s2 = io.send_request(
                            "evaluate",
                            {
                                "expression": 'interpreter-exec console "info locals"',
                                "frameId": frame_id,
                                "context": "repl",
                            },
                        )
                        req_count += 1
                        er_resp = io.wait_response(s2, args.timeout).raw
                        if not er_resp.get("success", False):
                            if args.ignore_variables_errors:
                                if args.verbose:
                                    print(f"[harness] locals evaluate failed and ignored: {er_resp}")
                                continue
                            raise DapProtocolError(
                                f"locals evaluate failed at step {step_index}: {er_resp}"
                            )
                        continue

                    if args.max_scope_loads > 0 and loaded_scopes >= args.max_scope_loads:
                        continue
                    vref = int(scope.get("variablesReference", 0))
                    if vref <= 0:
                        continue
                    v_args = {"variablesReference": vref}
                    if args.variables_filter:
                        v_args["filter"] = args.variables_filter
                    if args.variables_count > 0:
                        v_args["count"] = args.variables_count
                    s2 = io.send_request("variables", v_args)
                    req_count += 1
                    vr_resp = io.wait_response(s2, args.timeout).raw
                    if not vr_resp.get("success", False):
                        if args.ignore_variables_errors:
                            if args.verbose:
                                print(f"[harness] variables failed and ignored: {vr_resp}")
                            continue
                        raise DapProtocolError(f"variables failed at step {step_index}: {vr_resp}")
                    loaded_scopes += 1

            if args.inspect_first_stop and args.mode == "lightpad":
                run_lightpad_inspection(0)

            for i in range(1, args.steps + 1):
                check_guard(f"step {i} pre")

                s = io.send_request("next", {"threadId": active_thread})
                req_count += 1
                resp = io.wait_response(s, args.timeout).raw
                if not resp.get("success", False):
                    raise DapProtocolError(f"next failed at step {i}: {resp}")

                ev = io.wait_event("stopped", args.timeout).raw
                evt_count += 1
                stop_count += 1
                body = ev.get("body", {}) or {}
                active_thread = int(body.get("threadId", active_thread))

                fingerprint = (
                    body.get("reason"),
                    body.get("threadId"),
                    bool(body.get("allThreadsStopped", False)),
                    body.get("description", ""),
                    body.get("text", ""),
                )
                if fingerprint == prev_stop_fingerprint:
                    dup_stop_count += 1
                prev_stop_fingerprint = fingerprint

                if args.mode == "lightpad":
                    run_lightpad_inspection(i)

                check_guard(f"step {i} post")
                if i == 1 or i % 25 == 0:
                    rss = read_rss_kb(proc.pid)
                    print(
                        f"[harness] step={i} gdb_rss_kb={rss} req={req_count} "
                        f"evt={evt_count} dup_stops={dup_stop_count}"
                    )

            spontaneous_events = 0
            if args.idle_after_steps > 0:
                idle_deadline = now() + args.idle_after_steps
                while now() < idle_deadline:
                    check_guard("idle-after-steps")
                    msg = io.pump(0.2)
                    if msg is not None and msg.type == "event":
                        spontaneous_events += 1
                print(
                    f"[harness] idle_after_steps={args.idle_after_steps}s "
                    f"spontaneous_events={spontaneous_events}"
                )

            print("[harness] completed successfully")
            print(
                f"[harness] summary steps={args.steps} mode={args.mode} req={req_count} "
                f"evt={evt_count} stops={stop_count} dup_stops={dup_stop_count} "
                f"spontaneous_events={spontaneous_events} peak_gdb_rss_kb={peak_rss}"
            )
            return 0
        finally:
            # Best-effort disconnect with short timeout.
            try:
                if proc.poll() is None:
                    s = io.send_request("disconnect", {"terminateDebuggee": True})
                    try:
                        io.wait_response(s, 1.0)
                    except Exception:
                        pass
            except Exception:
                pass
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=1.5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=1.0)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print(f"[harness] ERROR: {exc}", file=sys.stderr)
        sys.exit(2)
