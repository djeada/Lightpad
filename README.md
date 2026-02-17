# Lightpad

Lightpad is a Qt 6-based code editor with a modern UI, built-in language tooling, and an extensible plugin system. It targets fast editing, project-level workflows, and developer-friendly features like LSP, Git, and debugging.

![Lightpad](https://github.com/user-attachments/assets/5beddbc9-4d6e-4345-85c9-358e0e15db41)

## Highlights
- Multi-language syntax highlighting with a plugin-based registry
- Autocompletion with keyword, snippet, and LSP providers
- LSP features: completion, hover, go-to definition, references, rename, and diagnostics
- Debug Adapter Protocol (DAP) client with breakpoints, sessions, watches, and debug console
- Git integration: status, diff gutter, staging, commits, branches, remotes, and stash
- Command palette, file quick open, go to symbol/line
- Multi-cursor editing and split editors (horizontal/vertical)
- Problems panel, breadcrumbs, minimap, and integrated terminal
- Run and format templates with per-project assignments
- Image viewer and optional PDF viewer (Qt6Pdf)

## Supported languages (built-in syntax plugins)
Cpp, Python, JavaScript, TypeScript, Java, Rust, Go, HTML, CSS, JSON, YAML, Markdown, Shell, Make, CMake, Bazel, Meson, Ninja

## Build requirements
- C++17 compiler
- CMake 3.16+
- Qt 6 (Core, Widgets, Gui)
- Optional: Qt6Pdf + Qt6PdfWidgets for the PDF viewer
  - Disable with `-DENABLE_PDF_SUPPORT=OFF`

## Build on Linux (Ubuntu/Debian)
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake qt6-base-dev qt6-pdf-dev libqt6pdf6 libqt6pdfwidgets6

git clone https://github.com/djeada/Lightpad.git
cd Lightpad
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
```

Run it:
```bash
./build/App/Lightpad
```

Run tests (optional):
```bash
ctest --test-dir build --output-on-failure
```

## Build with Makefile shortcuts
```bash
make install
make build
make run
```

## Build on macOS
```bash
brew install qt cmake
git clone https://github.com/djeada/Lightpad.git
cd Lightpad
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build
./build/App/Lightpad.app/Contents/MacOS/Lightpad
```

## Build on Windows
1. Install Qt 6 and CMake.
2. Configure and build with CMake (CLI or GUI).
3. The binary is produced in the build output directory.

## Diagnostics
For plugin and startup diagnostics, run:
```bash
LIGHTPAD_LOG_LEVEL=debug ./build/App/Lightpad
```

## Project configuration
Lightpad stores project-specific settings in a `.lightpad/` directory at the project root:
- `run_config.json` for run template assignments
- `format_config.json` for formatter assignments
- `highlight_config.json` for language overrides
- `debug/launch.json`, `debug/breakpoints.json`, `debug/watches.json`

User settings are stored in:
- Linux: `~/.config/lightpad/settings.json` (fallback: `~/.config/lightpad/`)
- Windows: `%LOCALAPPDATA%/Lightpad/settings.json`
- macOS: `~/Library/Application Support/Lightpad/settings.json`

Plugins are discovered from:
- App dir: `<app>/plugins`
- User: `AppDataLocation/plugins`
- Linux system: `/usr/lib/lightpad/plugins`, `/usr/local/lib/lightpad/plugins`

## Documentation
- User manual: docs/USER_MANUAL.md
- Keyboard shortcuts: docs/KEYBOARD_SHORTCUTS.md
- Plugin development: docs/PLUGIN_DEVELOPMENT.md
- Syntax plugins: docs/SYNTAX_PLUGINS.md

## Contributing
See CONTRIBUTING.md for the workflow and guidelines.

## License
GPL-3.0. See LICENSE for details.

## Third-Party Licenses
This application uses the Qt framework, which is available under LGPLv3.
Qt is a registered trademark of The Qt Company Ltd. and its subsidiaries.
Licensing details: https://www.qt.io/licensing/
Qt source code: https://www.qt.io/download-open-source
