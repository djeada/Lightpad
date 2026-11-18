# Run and Debug

Run (`Ctrl+F5`) and Debug (`F5`) start one shared *launch target*: the file in
the editor, a saved configuration, or a compound of saved configurations. This
page describes how each is resolved, the files Lightpad stores, and how to add
templates of your own. The [User Manual](USER_MANUAL.md#running-and-debugging)
covers the everyday workflow.

## The current file

With **Current File** selected, Lightpad picks how the file starts:

| File | Run | Debug |
| --- | --- | --- |
| C/C++ source owned by a CMake target | builds the target, runs the binary | builds, then GDB/LLDB |
| C/C++ source outside CMake | `gcc`/`g++` (or the chosen template), runs it | rebuilds with `-g -O0`, then GDB/LLDB |
| `test_*.py`, `*_test.py` | pytest in the Tests panel | debugpy running pytest |
| FastAPI/Flask backend files | the project's web server | debugpy with the server |
| Other Python | the project's Python environment | debugpy |
| Rust in a Cargo project | `cargo run` | (choose a configuration) |
| Go in a module | `go run .` / `go test` | Delve |
| JavaScript/TypeScript with `npm start` | `npm start` | Node.js debugger on the file |
| Anything else | the language's first run template | the template's debug recipe, if any |

The file's own settings (Run > Edit Configurations > Current File) override the
template, arguments, working directory, environment, compiler flags, extra
sources, before/after commands and Python environment. Debug applies the same
settings, so a program receives identical arguments and environment either
way. The dialog's preview shows both commands before anything runs.

## Saved configurations

`.lightpad/debug/launch.json` holds the workspace's configurations in VS Code's
`launch.json` format. Lightpad writes the file only when you save a
configuration; it is safe to commit and to edit by hand (the file is reloaded
before every launch).

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "API",
      "type": "debugpy",
      "request": "launch",
      "module": "uvicorn",
      "args": ["main:app", "--reload"],
      "cwd": "${workspaceFolder}",
      "env": { "APP_ENV": "dev" },
      "preLaunchTask": "alembic upgrade head"
    },
    {
      "name": "Web",
      "type": "command",
      "request": "launch",
      "program": "npm",
      "args": ["run", "dev"],
      "cwd": "${workspaceFolder}/web"
    }
  ],
  "compounds": [
    { "name": "Full stack", "configurations": ["API", "Web"], "stopAll": true }
  ]
}
```

| Property | Meaning |
| --- | --- |
| `type` | The debugger: `debugpy`, `cppdbg` (GDB or LLDB), `node`, `go`, `lldb` (CodeLLDB), `lldb-dap`, `coreclr` (netcoredbg), `dart`, `flutter`, `ocamlearlybird`, one of your own (see [Debuggers](#debuggers)), or `command` for programs that only run. |
| `adapterId` | Optional; forces one adapter when a type has several (`cppdbg-gdb`, `cppdbg-lldb`). |
| `request` | `launch` starts the program; `attach` connects to a running one (`processId`, `host`/`port`, `miDebuggerServerAddress`). `"processId": "${command:pickProcess}"` asks at start. |
| `program`, `args`, `cwd`, `env` | What starts, with which arguments, where, and with which extra environment. |
| `module` | Python: run a module (`python -m`) instead of a script. |
| `runtimeExecutable`, `runtimeArgs` | Node.js: the runtime (`node`, `npm`, `npx`) and its arguments. |
| `mode` | Go: `debug` (default), `test`, or `exec` for a prebuilt binary. |
| `preLaunchTask` | Shell command that must succeed before the launch (a build, a migration). Shown as *Before launch*. |
| `postDebugTask` | Shell command run after the program or debug session ends. Shown as *After stop*. |
| `pythonMode`, `pythonVenvPath`, `pythonInterpreter` | The Python environment, as in the Python environment panel. |
| `cmakeBinaryDir` | C/C++: build directory used when the program is a CMake target. |
| anything else | Passed to the debugger unchanged (`justMyCode`, `django`, `pathMappings`, `setupCommands`, ...). |

**Run** starts a configuration without a debugger, in its own terminal tab
that is reused when it runs again. The command comes from the configuration:
the Python interpreter with the script or `-m module`, the Node runtime with
the script, `go run`/`go test` for Go, and the program itself otherwise.
Attach configurations can only be debugged; `command` configurations can only
run.

Older versions of Lightpad wrote five example configurations and a
"Server + Client" compound into every project. They are hidden as long as they
are unchanged.

## Debuggers

| `type` | Debugger | Languages |
| --- | --- | --- |
| `debugpy` | debugpy | Python |
| `cppdbg` | GDB (`gdb --interpreter=dap`) or LLDB | C, C++, Fortran, Rust |
| `node` | js-debug | JavaScript, TypeScript |
| `go` | Delve | Go |
| `lldb` | CodeLLDB | Rust, C, C++ |
| `lldb-dap` | LLVM's lldb-dap (lldb-vscode) | C, C++, Rust, Swift, Zig, Objective-C |
| `coreclr` | netcoredbg | C#, F# |
| `dart` / `flutter` | `dart debug_adapter` / `flutter debug_adapter` | Dart, Flutter |
| `ocamlearlybird` | ocamlearlybird | OCaml (bytecode) |

The last four rows, and any debugger you add, are plain data in
`App/dap/debug_adapters.json`: any program that speaks the Debug Adapter
Protocol over stdio works. Add one (or replace a built-in one by `id`) in
`~/.config/Lightpad/Lightpad/debug_adapters.json`:

```json
{
  "adapters": [
    {
      "id": "my-dap",
      "name": "MyLang",
      "type": "mylang",
      "languages": ["mylang"],
      "extensions": [".my"],
      "command": ["my-dap", "/opt/mylang/bin/my-dap"],
      "arguments": ["--stdio"],
      "probe": ["--version"],
      "launch": { "program": "${file}", "cwd": "${cwd}" },
      "run": { "program": "mylang", "args": ["${program}", "${args}"] },
      "install": "Install it with: mylang tools install dap",
      "docs": "https://example.com/my-dap"
    }
  ]
}
```

| Field | Meaning |
| --- | --- |
| `command` | Executables to look for, in order; the first found on `PATH` is started. *Debugger options* in a configuration can point to another one. |
| `arguments`, `transport` | Arguments for the adapter. `"transport": "server"` for adapters that print `listening at host:port` instead of using stdio. |
| `probe` | Arguments for an availability check (for example `--version`); without it, being on `PATH` is enough. |
| `launch`, `attach` | Defaults for a new configuration and for *Debug* on the current file. `${file}`, `${fileDirname}`, `${fileBasenameNoExtension}` and `${cwd}` are filled in; other variables stay for launch time. |
| `processIdKey` | Name of the process id in attach requests (`pid` for lldb-dap). |
| `run` | How *Run* starts the configuration without the debugger; `${program}`, `${cwd}` and `${args}` are replaced. Without it the program itself runs. |
| `exceptionFilters`, `supports` | Exception breakpoint filters to offer, and `functionBreakpoints`/`conditionalBreakpoints`/`hitConditionalBreakpoints`/`logPoints`/`restart` (`true` unless set). |
| `install`, `docs` | Shown when the adapter is missing. |

The new types appear in the configuration dialog's *Debugger* list and in
**Add Configuration** templates (LLDB, .NET launch and attach, Dart, Flutter,
OCaml).

## While debugging

* The editor shows the stopped frame's values after the lines that use them,
  from the start of that function down to the stop line.
* The Debug Console logs why and where execution stopped
  (`Paused on breakpoint in deposit · app.py:12`); steps are only shown in the
  location pill. Debugger start-up chatter (GDB's licence banner, "pending"
  breakpoint notices, debuginfod prompts) is left out; program output and
  errors are not filtered.
* Breakpoints list markers like the gutter: dots for line breakpoints (with
  bars when conditional), diamonds for logpoints, squares for function
  breakpoints, hexagons for data breakpoints. A ring means the debugger has
  not bound the breakpoint yet; the tooltip says why.
* The process and thread selectors appear only when there is more than one;
  **Program I/O** opens the program's terminal.

## Variables

Every text field accepts these variables:

| Variable | Value |
| --- | --- |
| `${file}` | Path of the current file |
| `${fileDir}`, `${fileDirname}` | Folder of the current file |
| `${fileBasename}` | File name with extension |
| `${fileBasenameNoExt}`, `${fileBasenameNoExtension}` | File name without extension |
| `${fileExt}` / `${fileExtname}` | Extension without / with the dot |
| `${relativeFile}`, `${relativeFileDirname}` | Current file / its folder relative to the workspace |
| `${workspaceFolder}`, `${workspaceFolderBasename}` | Project folder and its name |
| `${projectRoot}` | Run templates: nearest folder with the template's `projectMarkers` |
| `${python}`, `${venv}`, `${venvBin}` | Interpreter and virtual environment of the selected Python environment |
| `${env:NAME}` | Environment variable `NAME` |
| `${userHome}`, `${pathSeparator}` | Home folder, `/` or `\` |
| `${command:pickProcess}` | Attach: pick a process when the session starts |

A configuration that refers to the current file cannot start without an open
file; Lightpad says so instead of passing an empty path.

## Templates

### Configuration templates

**Add** in the configurations dialog (and *Add Configuration* in the launch
menu) offers the templates in `App/run_templates/launch_templates.json`. To add
your own, create `launch_templates.json` in Lightpad's configuration folder
(`~/.config/Lightpad/Lightpad/` on Linux). An entry with an existing `id`
replaces the built-in one.

```json
{
  "categories": ["Team"],
  "templates": [
    {
      "id": "team.api",
      "category": "Team",
      "name": "Team API",
      "description": "Starts the API with the shared dev settings.",
      "languages": ["py"],
      "markers": ["manage.py"],
      "configuration": {
        "type": "debugpy",
        "request": "launch",
        "program": "${workspaceFolder}/manage.py",
        "args": ["runserver", "0.0.0.0:8000"],
        "django": true
      }
    }
  ]
}
```

`languages` and `markers` (files at the project root) decide which templates
are suggested first.

### File run templates

`App/run_templates/run_templates.json` (and `run_templates.json` in the
configuration folder) defines how Run starts a file of each language: a
`command`, `args`, `workingDirectory`, `env`, the `extensions` it applies to,
and `projectMarkers` for `${projectRoot}`. `defaultFor` makes a template the
default for an extension that shares a language with others (`c_gcc` for
`.c`).

An optional `debug` recipe tells Debug how to start the same file:

```json
"debug": {
  "type": "cppdbg",
  "build": ["g++", "-std=c++17", "-g", "-O0", "-o",
            "${fileDir}/${fileBasenameNoExt}", "${file}"],
  "program": "${fileDir}/${fileBasenameNoExt}"
}
```

`build` runs first (with the file's extra sources and compiler flags
appended); the rest is a launch configuration. Templates without a recipe
debug the file with its language's debugger when one exists.
