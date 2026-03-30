# Language Server Setup Guide

Lightpad provides built-in support for language servers across **Rust, Go, Python, and C++**. Language servers power IDE features including diagnostics, code completion, hover information, go-to-definition, find references, rename, formatting, and code actions.

## Supported Languages

| Language | Server | Command | Default Arguments |
|----------|--------|---------|-------------------|
| C/C++ | clangd | `clangd` | `--background-index` |
| Python | pylsp | `pylsp` | _(none)_ |
| Rust | rust-analyzer | `rust-analyzer` | _(none)_ |
| Go | gopls | `gopls` | `serve` |

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

### Python — pylsp

Python Language Server (pylsp) provides completion, diagnostics, hover, and formatting.

**Install via pip:**
```bash
pip install python-lsp-server
```

**Optional plugins:**
```bash
pip install pylsp-mypy   # Type checking
pip install python-lsp-ruff  # Fast linting
```

Lightpad detects Python projects via `pyproject.toml`, `requirements.txt`, or `setup.py`.

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

Lightpad detects Go projects via `go.mod`.

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
      "command": "pylsp",
      "arguments": [],
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
| `go.mod` | Go |
| `pyproject.toml` | Python |
| `requirements.txt` | Python |
| `setup.py` | Python |
| `CMakeLists.txt` | C/C++ |
| `compile_commands.json` | C/C++ |

The detected root directory is communicated to the language server as the workspace root URI, enabling multi-file features like cross-file references and project-wide diagnostics.

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
   which pylsp        # Python
   which rust-analyzer  # Rust
   which gopls        # Go
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
   pylsp
   gopls serve
   ```

2. **Update the server** to the latest version.

3. **Check project configuration** — Ensure `compile_commands.json` (for C++) or `Cargo.toml` (for Rust) is valid.
