# Lightpad Architecture

This document describes the architecture and design decisions of Lightpad, a Qt-based code editor.

## Overview

Lightpad follows a modular architecture with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────┐
│                     User Interface                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │  MainWindow │  │   Dialogs   │  │      Panels         │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                      Core Layer                             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────┐  │
│  │ Document │ │ TextArea │ │ TabWidget│ │ SyntaxHighlight│ │
│  └──────────┘ └──────────┘ └──────────┘ └───────────────┘  │
├─────────────────────────────────────────────────────────────┤
│                   Services Layer                            │
│  ┌────────────┐ ┌──────────────┐ ┌───────────────────────┐ │
│  │FileManager │ │SettingsManager│ │    PluginManager    │  │
│  └────────────┘ └──────────────┘ └───────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                Infrastructure Layer                         │
│  ┌────────┐ ┌───────────┐ ┌───────────┐ ┌───────────────┐  │
│  │ Logger │ │AsyncWorker│ │ LspClient │ │   VimMode     │  │
│  └────────┘ └───────────┘ └───────────┘ └───────────────┘  │
│  ┌────────┐ ┌─────────────────────┐                        │
│  │  I18n  │ │AccessibilityManager │                        │
│  └────────┘ └─────────────────────┘                        │
└─────────────────────────────────────────────────────────────┘
```

## Directory Structure

```
Lightpad/
├── App/
│   ├── accessibility/  # Accessibility features
│   │   └── accessibilitymanager.h/cpp
│   ├── core/           # Core editing components
│   │   ├── async/      # Async operations (AsyncWorker, AsyncTask)
│   │   ├── io/         # File I/O (FileManager)
│   │   ├── logging/    # Logging infrastructure
│   │   ├── document.h/cpp      # Document model
│   │   ├── textarea.h/cpp      # Main editor widget
│   │   ├── lightpadpage.h/cpp  # Tab page container
│   │   └── lightpadtabwidget.h/cpp  # Tab widget
│   ├── editor/         # Editor features
│   │   └── vimmode.h/cpp   # VIM modal editing
│   ├── filetree/       # File tree/explorer
│   ├── i18n/           # Internationalization
│   │   └── i18n.h/cpp      # Translation management
│   ├── lsp/            # Language Server Protocol
│   │   └── lspclient.h/cpp
│   ├── plugins/        # Plugin system
│   │   ├── iplugin.h       # Base plugin interface
│   │   ├── isyntaxplugin.h # Syntax plugin interface
│   │   └── pluginmanager.h/cpp
│   ├── settings/       # Settings management
│   │   ├── settingsmanager.h/cpp
│   │   ├── preferenceseditor.h/cpp
│   │   ├── textareasettings.h/cpp
│   │   └── theme.h/cpp
│   ├── syntax/         # Syntax highlighting
│   │   └── lightpadsyntaxhighlighter.h/cpp
│   ├── ui/             # User interface
│   │   ├── dialogs/    # Dialog windows
│   │   ├── panels/     # UI panels
│   │   └── mainwindow.h/cpp
│   └── resources/      # Icons, themes, templates
├── tests/              # Unit tests
│   └── unit/
├── docs/               # Documentation
└── CMakeLists.txt
```

## Core Components

### Document (`core/document.h`)

The `Document` class represents a text document in the editor:

- **Responsibilities:**
  - Manage document content and metadata
  - Track modification state
  - Handle file loading and saving
  - Emit signals for state changes

- **Key features:**
  - State machine: New → Saved → Modified → Saved
  - Language detection from file extension
  - Integration with FileManager for I/O

### TextArea (`core/textarea.h`)

The `TextArea` class is the main editing widget based on `QPlainTextEdit`:

- **Responsibilities:**
  - Text editing and display
  - Line number rendering
  - Bracket matching
  - Integration with syntax highlighter

### FileManager (`core/io/filemanager.h`)

Singleton service for file operations:

- **Responsibilities:**
  - Read and write files with error handling
  - Emit signals for file events
  - Provide file path utilities

### SettingsManager (`settings/settingsmanager.h`)

Singleton for application settings:

- **Responsibilities:**
  - Store and retrieve settings
  - Use OS-specific config directories
  - Support settings migration

### Logger (`core/logging/logger.h`)

Centralized logging with configurable levels:

- **Log levels:** Debug, Info, Warning, Error
- **Features:** File output, console output, thread-safe

## Plugin System

### Architecture

```
┌──────────────────────────────────┐
│        PluginManager             │
│  ┌────────────┐ ┌────────────┐  │
│  │ IPlugin    │ │ISyntaxPlugin│  │
│  │ (base)     │ │ (syntax)   │  │
│  └────────────┘ └────────────┘  │
└──────────────────────────────────┘
        │                │
        ▼                ▼
   ┌─────────┐    ┌─────────────┐
   │ Plugin1 │    │ RustPlugin  │
   └─────────┘    └─────────────┘
```

### Creating a Plugin

1. Implement `IPlugin` or `ISyntaxPlugin` interface
2. Export plugin metadata via Qt plugin system
3. Place compiled plugin in plugin directory

```cpp
class MyPlugin : public QObject, public IPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IPlugin_iid)
    Q_INTERFACES(IPlugin)
    
public:
    PluginMetadata metadata() const override;
    bool initialize() override;
    void shutdown() override;
    bool isLoaded() const override;
};
```

## LSP Integration

The `LspClient` provides Language Server Protocol support:

- **Protocol:** JSON-RPC over stdio
- **Features:**
  - Document synchronization
  - Completion requests
  - Hover information
  - Go to definition
  - Find references
  - Diagnostics

## Async Operations

### AsyncWorker

Base class for background operations:

```cpp
class MyWorker : public AsyncWorker {
protected:
    void doWork() override {
        while (!isCancelled()) {
            // Do work
            reportProgress(50, "Halfway done");
        }
    }
};
```

### AsyncThreadPool

Manages background worker execution:

```cpp
AsyncThreadPool::instance().submitTask([](AsyncTask* task) {
    // Long-running operation
    task->reportProgress(100, "Done");
});
```

## VIM Mode

Modal editing support with VimMode class:

- **Modes:** Normal, Insert, Visual, Visual Line, Command
- **Motions:** h, j, k, l, w, b, e, 0, $, ^, gg, G
- **Operators:** d (delete), c (change), y (yank)
- **Commands:** :w, :q, :wq, :q!

## Settings Storage

Settings are stored in OS-specific locations:

| Platform | Location |
|----------|----------|
| Linux    | `~/.config/lightpad/` or `$XDG_CONFIG_HOME` |
| Windows  | `%APPDATA%/Local/Lightpad/` |
| macOS    | `~/Library/Application Support/Lightpad/` |

## Testing

Unit tests use Qt Test framework:

```bash
cd build
cmake -DBUILD_TESTS=ON ..
cmake --build .
ctest --output-on-failure
```

## Build System

CMake-based build system:

- **Minimum CMake:** 3.10
- **Qt version:** 5.x
- **C++ standard:** C++14

## Future Considerations

- Qt 6 migration
- C++17/C++20 features
- Improved plugin isolation
- Web-based LSP for browser support
