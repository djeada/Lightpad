# Language Server Setup Guide

Lightpad provides built-in support for language servers across **C/C++, Python, Rust, Go, JavaScript/TypeScript, Java, and Dockerfile**. Language servers power IDE features including diagnostics, code completion, hover information, go-to-definition, find references, rename, formatting, and code actions.

## Supported Languages

| Language | Server | Command | Default Arguments |
|----------|--------|---------|-------------------|
| C/C++ | clangd | `clangd` | `--background-index` |
| Python | Basedpyright (pylsp fallback) | `basedpyright-langserver` | `--stdio` |
| Rust | rust-analyzer | `rust-analyzer` | _(none)_ |
| Go | gopls | `gopls` | `serve` |
| JavaScript / Node.js | typescript-language-server | `typescript-language-server` | `--stdio` |
| TypeScript | typescript-language-server | `typescript-language-server` | `--stdio` |
| Java | Eclipse JDT LS | `jdtls` | _(workspace data directory added automatically)_ |
| Dockerfile | Docker language server | `docker-langserver` | `--stdio` |

## Installation

### C++ — clangd

clangd is part of the LLVM/Clang toolchain.

**Ubuntu/Debian:**
```bash
sudo apt install clangd
```

**macOS:**
```bash
brew install llvm
```

**Tip:** For best results, generate a `compile_commands.json` in your project root using CMake:
```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -S . -B build
```
Lightpad will automatically detect `compile_commands.json` or `CMakeLists.txt` to set the workspace root.

### Python — Basedpyright or pylsp

Basedpyright is the default Python server. It provides hover information and
navigation into library source and type stubs, including compiled APIs such as
NumPy's `array`:

```bash
python -m pip install basedpyright
basedpyright-langserver --stdio
```

Lightpad uses the interpreter selected in **Python Environment**, including a
project virtual environment or a custom interpreter. The language server may be
installed separately from that environment: library lookup uses the selected
interpreter, not the interpreter that happens to run Lightpad or the server.
Saving a different interpreter restarts Python analysis with the new selection.

If Basedpyright is unavailable, Lightpad can use `pylsp` from the project or
system PATH. Existing explicit `pylsp` settings remain supported:

```bash
python -m pip install python-lsp-server
```

Some compiled APIs return documentation but no navigable definition with pylsp.
For these, select `basedpyright-langserver` with arguments `["--stdio"]` in the
language server settings. Library-provided `.pyi` files are valid definition
targets; native implementation source need not be installed.

Server configuration references:
[Basedpyright settings](https://docs.basedpyright.com/latest/configuration/language-server-settings/)
and [pylsp settings](https://github.com/python-lsp/python-lsp-server/blob/develop/CONFIGURATION.md).

Lightpad detects `pyproject.toml`, `requirements.txt`, `setup.py`, `setup.cfg`,
`Pipfile`, and project `.venv`/`venv` directories.

### Rust — rust-analyzer

rust-analyzer is the official Rust language server.

**Install via rustup:**
```bash
rustup component add rust-analyzer
```

**Or install standalone:**
```bash
# Download from https://github.com/rust-lang/rust-analyzer/releases
```

Lightpad detects Rust projects via `Cargo.toml`.

### Go — gopls

gopls is the official Go language server.

**Install via go:**
```bash
go install golang.org/x/tools/gopls@latest
```

Make sure `$GOPATH/bin` (or `$HOME/go/bin`) is in your `PATH`.

Lightpad detects Go projects via `go.mod` or `go.work`.

### JavaScript / TypeScript / Node.js

Install Node.js and npm, then install the language server and TypeScript:

```bash
npm install -g typescript typescript-language-server
```

Lightpad configures `typescript-language-server --stdio` for both `js` and `ts`.
It detects projects using `package.json`, `tsconfig.json`, or `jsconfig.json`.
JavaScript module extensions `.mjs`/`.cjs` and TypeScript module extensions
`.mts`/`.cts` are recognized, as are JSX/TSX React language IDs.

See [Rust, Go, and Node.js workflows](RUST_GO_NODEJS.md) for project runs,
formatting, and tests.

## Configuration

### Settings

Language server settings are stored in the Lightpad settings file. You can configure each language server individually under the `languageServers` key:

```json
{
  "languageServers": {
    "cpp": {
      "command": "clangd",
      "arguments": ["--background-index"],
      "environment": [],
      "enabled": true
    },
    "py": {
      "command": "basedpyright-langserver",
      "arguments": ["--stdio"],
      "environment": [],
      "enabled": true
    },
    "rust": {
      "command": "rust-analyzer",
      "arguments": [],
      "environment": [],
      "enabled": true
    },
    "go": {
      "command": "gopls",
      "arguments": ["serve"],
      "environment": [],
      "enabled": true
    },
    "js": {
      "command": "typescript-language-server",
      "arguments": ["--stdio"],
      "enabled": true
    },
    "ts": {
      "command": "typescript-language-server",
      "arguments": ["--stdio"],
      "enabled": true
    }
  }
}
```

### Configuration Options

| Option | Type | Description |
|--------|------|-------------|
| `command` | string | Path or name of the language server executable |
| `arguments` | string[] | Command-line arguments passed to the server |
| `environment` | string[] | Environment variables (e.g., `["RUST_LOG=info"]`) |
| `enabled` | boolean | Set to `false` to disable a specific language server |
| `settings` | object | Server settings sent through `workspace/didChangeConfiguration` and returned to `workspace/configuration` requests |
| `initializationOptions` | object | Server-specific initialization options |

### Disabling a Language Server

To disable a language server for a specific language, set `enabled` to `false`:

```json
{
  "languageServers": {
    "go": {
      "enabled": false
    }
  }
}
```

## Workspace Detection

Lightpad automatically detects the project root directory for each opened file by searching upward for project markers:

| Marker File | Language |
|-------------|----------|
| `Cargo.toml` | Rust |
| `go.mod`, `go.work` | Go |
| `package.json`, `jsconfig.json` | JavaScript / Node.js |
| `tsconfig.json` | TypeScript |
| `pyproject.toml` | Python |
| `requirements.txt` | Python |
| `setup.py` | Python |
| `CMakeLists.txt` | C/C++ |
| `compile_commands.json` | C/C++ |

Each language/project uses its own server session; Python sessions also use the
selected interpreter. The project root is set before initialization as the
workspace root, workspace folder, and server working directory. This keeps
external dependency lookup attached to the correct project when multiple
projects are open.

Configured `environment` entries are applied to the server process. Commands,
arguments, and environment values support `${workspaceFolder}`, `${fileDir}`,
`${file}`, and `${python}`. Project-local `node_modules/.bin` servers are detected
for JavaScript and TypeScript. JDT LS receives a separate workspace data directory
for each Java project.

Java projects also recognize `pom.xml`, `build.gradle`, and `build.gradle.kts`.
Servers for additional languages can be configured under `languageServers`;
their definitions use the same navigation path as the built-in servers.

## Server Capabilities

When a language server starts, Lightpad negotiates capabilities with it. The following LSP features are supported when the server advertises them:

- **Diagnostics** — Real-time error and warning reporting
- **Completion** — Context-aware code completion
- **Hover** — Documentation and type information on hover
- **Go to Definition** — Navigate to where a symbol is defined
- **Go to Declaration** — Navigate to where a symbol is declared
- **Go to Type Definition** — Navigate to the type of a symbol
- **Find References** — Find all usages of a symbol
- **Rename Symbol** — Rename a symbol across the project
- **Document Symbols** — Outline of symbols in the current file
- **Workspace Symbols** — Search for symbols across the workspace
- **Signature Help** — Parameter hints while typing function calls
- **Formatting** — Format the current document
- **Code Actions** — Quick fixes and refactoring suggestions
- **Semantic Tokens** — Enhanced syntax highlighting from the server

If a server does not support a particular capability, Lightpad will gracefully skip that feature rather than showing errors.

## Health Status

Lightpad tracks the health of each language server:

| Status | Description |
|--------|-------------|
| **Unknown** | Server has not been started yet |
| **Starting** | Server process is launching |
| **Running** | Server is initialized and responding |
| **Error** | Server encountered an error or crashed |
| **Stopped** | Server is disabled or was stopped |

## Troubleshooting

### Server not starting

1. **Check the executable is in your PATH:**
   ```bash
   which clangd      # C++
   which basedpyright-langserver  # Python (or pylsp)
   which rust-analyzer  # Rust
   which gopls        # Go
   which typescript-language-server  # JavaScript/TypeScript
   ```

2. **Check the settings** — Make sure `languageServers.<lang>.enabled` is `true` and `command` points to the correct executable.

3. **Check the Lightpad log** — Look for error messages like:
   - `"Failed to start '<command>'. Is it installed?"`
   - `"Language server for '<lang>' is disabled in settings."`

### No diagnostics or completion

1. **Wait for initialization** — Language servers need time to index your project. Large projects may take longer.

2. **Check workspace root detection** — Ensure your project has the appropriate marker file (`Cargo.toml`, `go.mod`, etc.) in the project root.

3. **Check server capabilities** — Some features may not be available with all servers. The Lightpad log will show which capabilities the server advertised.

### Server crashes or errors

1. **Try running the server manually** to check for configuration issues:
   ```bash
   clangd --background-index
   rust-analyzer
   basedpyright-langserver --stdio
   gopls serve
   ```

2. **Update the server** to the latest version.

3. **Check project configuration** — Ensure `compile_commands.json` (for C++) or `Cargo.toml` (for Rust) is valid.

## External library navigation

Hover requests use the identifier under the pointer, including the member in a
dotted expression such as `np.array`. Ctrl+B, F12, and Ctrl+Click first request a
definition, then try declarations and type definitions when the server provides
those capabilities. This supports libraries that expose headers or type stubs
rather than implementation files.

Server-provided virtual source documents, including Java JAR class sources, open
read-only. Their original URI and project server are retained for further hover
and navigation. Native library files and headers also retain the originating
project context, including its interpreter and compiler settings. Disabling
diagnostic display does not disable these language features, and
definition/hover requests flush pending document edits.
