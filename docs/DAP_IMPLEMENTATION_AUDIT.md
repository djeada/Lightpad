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

## 6) Main gaps / risks

### 6.1 Protocol framing bug (high impact)

`Content-Length` is byte-based in DAP, but parsing is currently done on `QString` character indices.  
This can break parsing for non-ASCII payloads.

Files:
- `App/dap/dapclient.cpp` (buffering/parsing logic around `onReadyReadStandardOutput`)

### 6.2 `runInTerminal` advertised but not truly implemented

Client reports support in `initialize`, but reverse `runInTerminal` requests are only acknowledged and not actually launched.

Files:
- `App/dap/dapclient.cpp` (`doInitialize`, `handleReverseRequest`)

### 6.3 Adapter-specific launch/attach arguments are not forwarded

`DebugConfiguration` stores adapter-specific fields (`adapterConfig`), but `DebugSession` only forwards a fixed subset to DAP `launch`/`attach`.

Files:
- `App/dap/debugconfiguration.h`
- `App/dap/debugsession.cpp`

### 6.4 Detach behavior can conflict with stop flow

`stop(false)` calls `disconnect(false)`, then `DapClient::stop()` may also send `disconnect` with `terminateDebuggee=true`, which can violate detach intent.

Files:
- `App/dap/debugsession.cpp`
- `App/dap/dapclient.cpp`

### 6.5 Restart fallback path is incomplete

If adapter does not support `restart`, code issues `disconnect(true)` and comments that reconnect will happen, but reconnect logic is missing.

File:
- `App/dap/dapclient.cpp`

### 6.6 Capability gating is partial

Some requests are gated by capabilities (`configurationDone`, `restart`), others are not consistently gated (`setVariable`, `setFunctionBreakpoints`, `terminate`, etc.).

File:
- `App/dap/dapclient.cpp`

## 7) Missing protocol areas (not currently implemented)

Not seen in current client:

- `setInstructionBreakpoints`
- `gotoTargets` / `goto`
- `stepBack`, `reverseContinue`
- `completions`
- `exceptionInfo`
- `dataBreakpointInfo`
- Memory requests (`readMemory`, `writeMemory`)
- Disassembly requests
- Loaded sources/modules requests
- Progress/cancel handling (`progressStart`/`progressUpdate`/`progressEnd`, `cancel`)

Note: Not all are required for a functional debugger, but they improve compatibility and advanced debugging workflows.

## 8) Recommended priority order

1. Fix byte-correct DAP framing/parsing in `DapClient`.
2. Implement real `runInTerminal` handling (or stop advertising support).
3. Forward adapter-specific launch/attach arguments from `DebugConfiguration::adapterConfig`.
4. Fix detach vs terminate semantics in stop/disconnect flow.
5. Complete manual restart fallback path.
6. Add systematic capability-based request gating.

## 9) Short conclusion

Lightpad has a solid base DAP integration with good support for core debugging workflows (launch/attach, breakpoints, stepping, stack/variables, watches, console).  
The main work left is protocol correctness and interoperability hardening, especially around framing, reverse requests, and adapter-specific configuration flow.
