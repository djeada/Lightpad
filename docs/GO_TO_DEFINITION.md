# Go to Definition

Lightpad supports Go to Definition for multiple programming languages via Language Server Protocol (LSP) providers. Each language uses a dedicated language server that runs in the background and provides symbol navigation.

## Supported Languages and Servers

| Language | Server | Command | Website |
|----------|--------|---------|---------|
| C / C++ | clangd | `clangd` | https://clangd.llvm.org |
| Python | pylsp | `pylsp` | https://github.com/python-lsp/python-lsp-server |
| Rust | rust-analyzer | `rust-analyzer` | https://rust-analyzer.github.io |
| Go | gopls | `gopls` | https://pkg.go.dev/golang.org/x/tools/gopls |
| TypeScript / JavaScript | typescript-language-server | `typescript-language-server` | https://github.com/typescript-language-server/typescript-language-server |
| Java | jdtls | `jdtls` | https://github.com/eclipse-jdtls/eclipse.jdt.ls |

## Installing Language Servers

Each language server must be installed separately and available in your system `PATH`. Lightpad auto-detects installed servers and starts them on demand when you first use Go to Definition for that language.

### C / C++ — clangd

**Ubuntu / Debian:**
```bash
sudo apt-get install clangd
```

**Fedora:**
```bash
sudo dnf install clang-tools-extra
```

**macOS (Homebrew):**
```bash
brew install llvm
# Add to PATH: export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
```

**Windows:**
Download the LLVM installer from https://releases.llvm.org and ensure `clangd` is on your PATH.

> clangd works best when your project has a `compile_commands.json` file. For CMake projects, add `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` to your build command.

### Python — pylsp

```bash
pip install python-lsp-server
```

Or with extra features:
```bash
pip install "python-lsp-server[all]"
```

Verify installation:
```bash
pylsp --help
```

### Rust — rust-analyzer

**Via rustup (recommended):**
```bash
rustup component add rust-analyzer
```

**Via package manager:**

Ubuntu / Debian:
```bash
sudo snap install rust-analyzer --classic
```

macOS:
```bash
brew install rust-analyzer
```

Verify installation:
```bash
rust-analyzer --version
```

### Go — gopls

```bash
go install golang.org/x/tools/gopls@latest
```

Make sure `$GOPATH/bin` (or `$HOME/go/bin`) is in your PATH:
```bash
export PATH="$PATH:$(go env GOPATH)/bin"
```

Verify installation:
```bash
gopls version
```

### TypeScript / JavaScript — typescript-language-server

```bash
npm install -g typescript-language-server typescript
```

Verify installation:
```bash
typescript-language-server --version
```

### Java — jdtls (Eclipse JDT Language Server)

**macOS (Homebrew):**
```bash
brew install jdtls
```

**Manual install:**
1. Download the latest milestone from https://download.eclipse.org/jdtls/milestones/
2. Extract to a directory (e.g., `~/.local/share/jdtls`)
3. Create a wrapper script named `jdtls` on your PATH that launches the server with appropriate JVM arguments

Example wrapper script (`~/.local/bin/jdtls`):
```bash
#!/bin/bash
exec java \
  -Declipse.application=org.eclipse.jdt.ls.core.id1 \
  -Dosgi.bundles.defaultStartLevel=4 \
  -Declipse.product=org.eclipse.jdt.ls.core.product \
  -noverify \
  -Xms256m \
  -jar ~/.local/share/jdtls/plugins/org.eclipse.equinox.launcher_*.jar \
  -configuration ~/.local/share/jdtls/config_linux \
  -data ~/.local/share/jdtls/workspace \
  "$@"
```

Make it executable:
```bash
chmod +x ~/.local/bin/jdtls
```

## How It Works

1. When you trigger Go to Definition (via `F12`, `Ctrl+B`, `Ctrl+Click`, or the context menu), Lightpad checks if a language server is available for the current file's language.
2. If the server binary is found in your PATH, Lightpad starts it automatically in the background.
3. The language server analyzes your project and responds with definition locations.
4. Lightpad navigates you to the target file and line.

Servers are started lazily — only when you first request a definition for that language — and stay running for the duration of your editing session.

## Triggers

| Trigger | Action |
|---------|--------|
| `F12` | Go to Definition at cursor |
| `Ctrl+B` | Go to Definition at cursor |
| `Ctrl+Click` | Go to Definition for clicked symbol |
| Right-click → **Go to Definition** | Context menu action |
| `Alt+Left` | Navigate back |
| `Alt+Right` | Navigate forward |

## Behavior

- **1 result:** Lightpad jumps directly to the definition.
- **Multiple results:** A quick-pick popup lets you choose which definition to navigate to.
- **No results:** A status bar message says "No definition found".
- **Server not installed:** A message tells you which server to install for the language.

## Troubleshooting

### "Language server 'X' is not available"

The required language server is not installed or not on your PATH. Follow the installation instructions above for your language.

Check that the server binary is accessible:
```bash
which clangd       # C/C++
which pylsp         # Python
which rust-analyzer # Rust
which gopls         # Go
which typescript-language-server  # TypeScript/JS
which jdtls         # Java
```

### Server starts but no definitions are found

- Make sure the language server has finished indexing your project. Large projects may take a moment.
- For C/C++, ensure a `compile_commands.json` file exists in your project root.
- For Python, ensure your virtual environment is activated before starting Lightpad.
- For Go, ensure your project has a `go.mod` file.

### Diagnostics

Run Lightpad with debug logging to see language server activity:
```bash
LIGHTPAD_LOG_LEVEL=debug ./build/App/Lightpad
```

Look for log lines like:
```
[INFO] Started language server 'clangd (C/C++)' (clangd) for languages: cpp, c
[INFO] Language server 'pylsp' (pylsp) not found in PATH
```
