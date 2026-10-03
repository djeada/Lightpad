# Debug Adapter Protocol (DAP) in Lightpad

## 1) What is DAP?

The **Debug Adapter Protocol (DAP)** is a JSON-based protocol that standardizes communication between:

- A **debug client** (IDE/editor UI)
- A **debug adapter** (language/runtime-specific debugger bridge)

This lets one IDE talk to many debuggers through one common protocol, instead of implementing a custom integration for each runtime.

Reference:
- https://microsoft.github.io/debug-adapter-protocol/overview
- https://microsoft.github.io/debug-adapter-protocol/specification

## 2) What DAP contains

At a high level, DAP defines:

- **Message framing** over stdio/socket using headers like `Content-Length`
- **Requests** (`initialize`, `launch`, `attach`, `setBreakpoints`, `threads`, `stackTrace`, etc.)
- **Responses** (success/failure + optional data body)
- **Events** (`initialized`, `stopped`, `continued`, `output`, `terminated`, etc.)
- **Capabilities negotiation** (adapter advertises supported features in `initialize` response)
- **Debug lifecycle**:
  - Initialize
  - Configure breakpoints/options
  - `configurationDone`
  - Run / stop / inspect / continue
  - Terminate / disconnect

## 3) Lightpad implementation overview

Main implementation areas:

- Protocol client: `App/dap/dapclient.h`, `App/dap/dapclient.cpp`
- Session lifecycle: `App/dap/debugsession.h`, `App/dap/debugsession.cpp`
- Breakpoints: `App/dap/breakpointmanager.h`, `App/dap/breakpointmanager.cpp`
- Watches/evaluate: `App/dap/watchmanager.h`, `App/dap/watchmanager.cpp`
- Debug UI: `App/ui/panels/debugpanel.h`, `App/ui/panels/debugpanel.cpp`

## 4) Coverage summary (current state)

Estimated from static code review:

- **DAP overview happy path coverage:** ~70%
- **Robust protocol/interoperability coverage:** ~50-60%

These are implementation-read estimates (not full runtime conformance tests).

## 5) What is implemented well

### 5.1 Lifecycle and core requests

Implemented:

- `initialize`
- `launch` / `attach`
- `disconnect` / `terminate`
- `configurationDone` (capability-gated)
- `restart` (when adapter supports it)

### 5.2 Breakpoint and execution control

Implemented:

- Source breakpoints (`setBreakpoints`)
- Function breakpoints (`setFunctionBreakpoints`)
- Data breakpoints (`setDataBreakpoints`) with unsupported fallback detection
- Exception breakpoints (`setExceptionBreakpoints`)
- Continue/pause/step (`continue`, `pause`, `next`, `stepIn`, `stepOut`)

### 5.3 Inspection and debug console

Implemented:

- `threads`
- `stackTrace`
- `scopes`
- `variables`
- `evaluate`
- `setVariable`
- Output event handling in debug console

### 5.4 Event handling

Implemented:

- `initialized`
- `stopped`
- `continued`
- `thread`
- `output`
- `breakpoint`
- `exited`
- `terminated`

## 6) Main gaps / risks (status as of this update)

### 6.1 Protocol framing bug — FIXED

Parsing is byte-based on `QByteArray` with buffer/message caps (`App/dap/dapclient.cpp`,
`handleAdapterData`). Covered by `testDapClientFramingHandlesUtf8AndChunkedDelivery`
(UTF-8 payloads, chunked delivery).

### 6.2 `runInTerminal` — IMPLEMENTED

Reverse requests spawn the command in an integrated terminal tab
(`App/ui/mainwindow.cpp`) and reply with the real pid. `kind: "external"` launches the command in a detached
system terminal emulator (x-terminal-emulator, gnome-terminal, konsole, xfce4-terminal,
kitty, alacritty, xterm; Linux/BSD) and falls back to the integrated terminal when none
is available or on other platforms.

### 6.3 Adapter-specific launch/attach arguments — FORWARDED

`DebugSession` sends adapter-built launch/attach arguments which merge all of
`DebugConfiguration::adapterConfig` over adapter defaults.

### 6.4 Detach vs stop conflict — FIXED

A single `disconnect{terminateDebuggee}` is sent; no second forced terminate.
`stop()` now also grants a short drain window so the disconnect is not truncated.

### 6.5 Restart fallback path — COMPLETE

Non-restart-capable adapters are relaunched by capturing and restarting the adapter
program (stdio) or reconnecting (socket).

### 6.6 Capability gating — IMPLEMENTED

- `configurationDone`: skipped only when the capability is explicitly `false`
  (lenient default for older adapters).
- `terminate`: falls back to `disconnect(terminateDebuggee=true)` unless
  `supportsTerminateRequest` is advertised.
- `setVariable`: gated on `supportsSetVariable` (lenient default).
- The client no longer advertises unimplemented client capabilities:
  `supportsMemoryReferences` and `supportsProgressReporting` were removed from
  `initialize`.

## 7) Newly supported protocol areas

- `exceptionInfo` request/response parsing, auto-requested on exception stops by
  `DebugSession`, surfaced in the debug console (traceback, inner exceptions,
  type name). Gated on `supportsExceptionInfoRequest`.
- Socket transport: `DapClient::startSocket(host, port)` attaches to adapters
  already listening over TCP (e.g. debugpy/codelldb server mode). Connection loss
  maps to session termination.
- `${command:pickProcess}` placeholder resolution via the process picker dialog
  (`App/ui/dialogs/processpickerdialog.*`), wired into
  `MainWindow::prepareDebugConfigurationForStart`.
- A scripted fake DAP adapter (`tests/unit/fakedapadapter_main.cpp`) provides full
  protocol round-trip tests: initialize → launch → threads/stack/scopes/variables/
  evaluate/setVariable/exceptionInfo → disconnect, plus capability-gating assertions
  via a request trace file.

## 8) Added in the debugging improvement pass

- Client requests: `completions`, `dataBreakpointInfo`, `readMemory`, `disassemble`
  (capability-gated; unsupported adapters fail fast via `*Failed` signals) and the
  client now advertises `supportsMemoryReferences`.
- Robustness: inspection requests (threads, stackTrace, scopes, variables, evaluate,
  setVariable, exceptionInfo, completions, memory, disassembly, dataBreakpointInfo) time
  out after `DapClient::requestTimeoutMs()` (default 30 s), report an error, emit
  `requestTimedOut`, and late replies are discarded. Adapter crashes terminate the session.
- UI: Tab completion in the debug console (via `completions`), "Break on Value Change..."
  in the Variables context menu (via `dataBreakpointInfo`, shown in the Breakpoints list with
  enable/remove; cleared when the session ends because data ids are session specific),
  "View Memory / Disassembly" viewer for variables with a `memoryReference`, and inline
  variable values in the editor at the stopped location.
- Tests: fake adapter gained `completions`, `dataBreakpointInfo`, `setDataBreakpoints`,
  `readMemory`, `disassemble`, `--hang-on`, `--crash-on`, `--stop-reason`,
  `--run-in-terminal`; covered in `tests/unit/test_dap.cpp`.

## 9) Still missing

- `setInstructionBreakpoints`, `gotoTargets`/`goto`, `stepBack`/`reverseContinue`,
  modules request, progress/cancel UI, memory write, Java adapter preset.

## 10) Recommended next steps

1. Language-server based `inlineValues` (current inline values are a name-matching heuristic).
2. Disassembly stepping (`granularity: instruction`) and instruction breakpoints.
3. Per-session picker UI for compound launch configurations.
