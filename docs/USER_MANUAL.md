# Lightpad User Manual

## Introduction

Lightpad is a modern, lightweight code editor designed for developers. It provides essential features for code editing while maintaining fast performance and a clean interface.

## Getting Started

### Installation

#### Linux (Ubuntu/Debian)
```bash
# Install dependencies
sudo apt-get install build-essential cmake qtbase5-dev qttools5-dev-tools

# Build from source
git clone https://github.com/djeada/Lightpad.git
cd Lightpad
mkdir build && cd build
cmake ..
cmake --build .

# Run
./App/Lightpad
```

#### Windows
1. Install Qt 6.x development tools
2. Install CMake 3.10 or later
3. Build using CMake GUI or command line

#### macOS
```bash
brew install qt cmake
git clone https://github.com/djeada/Lightpad.git
cd Lightpad
mkdir build && cd build
cmake ..
cmake --build .
./App/Lightpad.app/Contents/MacOS/Lightpad
```

### First Launch

When you first launch Lightpad:
1. A new empty file opens automatically
2. Use `Ctrl+O` to open existing files
3. Use `Ctrl+N` to create new files

## Basic Usage

### Opening Files

- **Menu:** File → Open (or `Ctrl+O`)
- **Drag and drop:** Drag files from file manager onto Lightpad
- **Command line:** `lightpad filename.txt`

### Editing Text

Lightpad works like any standard text editor:
- Click to position cursor
- Type to insert text
- Select text by clicking and dragging
- Use clipboard shortcuts (`Ctrl+C`, `Ctrl+X`, `Ctrl+V`)

### Saving Files

- **Save:** `Ctrl+S` or File → Save
- **Save As:** `Ctrl+Shift+S` or File → Save As
- Unsaved files show an indicator in the tab

### Working with Tabs

- Each file opens in a new tab
- Click tabs to switch between files
- Middle-click or `Ctrl+W` to close a tab
- Drag tabs to reorder them

## Features

### Syntax Highlighting

Lightpad automatically detects the programming language based on file extension:

| Extension | Language |
|-----------|----------|
| `.cpp`, `.cc`, `.h`, `.hpp` | C++ |
| `.py` | Python |
| `.js`, `.jsx` | JavaScript |
| `.ts`, `.tsx` | TypeScript |
| `.java` | Java |
| `.rs` | Rust |
| `.go` | Go |

### Source Control: Commit History

The Source Control panel shows recent commits for the current repository. Right-click a commit in the history list to:
- **Checkout Commit** to view files at that revision (detached HEAD)
- **Create Branch** to start a new branch from that commit

### Line Numbers

Line numbers are displayed by default. Toggle them in:
- **Preferences** → **Editor** → **Show line numbers**

### Auto-Indentation

Lightpad automatically indents new lines based on the previous line. Configure:
- **Preferences** → **Editor** → **Auto indent**
- **Preferences** → **Editor** → **Tab width**

### Bracket Matching

Matching brackets are highlighted when the cursor is on a bracket. Toggle:
- **Preferences** → **Editor** → **Highlight matching brackets**

### Current Line Highlighting

The current line is subtly highlighted. Toggle:
- **Preferences** → **Editor** → **Highlight current line**

### Autocompletion

Trigger autocompletion with `Ctrl+Space`. See [AUTOCOMPLETION.md](../App/AUTOCOMPLETION.md) for details.

## Find and Replace

### Basic Find

1. Press `Ctrl+F` or go to Edit → Find
2. Type your search term
3. Press `Enter` or click Find Next
4. Use `F3` for next match, `Shift+F3` for previous

### Find and Replace

1. Press `Ctrl+H` or go to Edit → Replace
2. Enter search term and replacement
3. Click Replace or Replace All

### Find Options

- **Case Sensitive:** Match exact case
- **Whole Word:** Match complete words only
- **Regular Expression:** Use regex patterns

## Customization

### Color Themes

Change the editor theme:
1. Go to **Preferences** → **Appearance**
2. Select a theme from the dropdown
3. Changes apply immediately

### Font Settings

Customize the editor font:
1. Go to **Preferences** → **Editor**
2. Choose font family and size
3. Enable/disable font ligatures

### Editor Settings

Configure editor behavior:
- **Tab width:** Number of spaces per tab (default: 4)
- **Insert spaces:** Use spaces instead of tabs
- **Word wrap:** Wrap long lines
- **Show whitespace:** Display space/tab characters

## VIM Mode

Lightpad supports VIM-style modal editing:

1. Enable VIM mode in **Preferences** → **Editor** → **VIM Mode**
2. The editor starts in Normal mode
3. See [KEYBOARD_SHORTCUTS.md](KEYBOARD_SHORTCUTS.md) for VIM commands

## Language Server Protocol (LSP)

For enhanced code intelligence:

1. Install a language server for your language
2. Configure the language server path in Preferences
3. Features include:
   - Code completion
   - Hover information
   - Go to definition
   - Find references
   - Inline diagnostics

### Go to Definition

Lightpad supports Go to Definition for C/C++, Python, Rust, Go, TypeScript/JavaScript, and Java via dedicated language servers. Each server is auto-detected and started on demand.

See [Go to Definition](GO_TO_DEFINITION.md) for installation instructions for each language server.

## Plugins

Lightpad supports plugins for extensibility:

### Installing Plugins

Place plugin files in:
- Linux: `~/.local/share/lightpad/plugins/`
- Windows: `%APPDATA%/Local/Lightpad/plugins/`
- macOS: `~/Library/Application Support/Lightpad/plugins/`

### Available Plugin Types

- **Syntax Plugins:** Add highlighting for new languages
- **Theme Plugins:** Add custom color themes
- **Tool Plugins:** Add new features and tools

## Troubleshooting

### Lightpad Won't Start

1. Check Qt libraries are installed
2. Run from terminal to see error messages
3. Try deleting config: `~/.config/lightpad/`

### Slow Performance

1. Check file size (very large files may be slow)
2. Disable syntax highlighting for large files
3. Check for running language servers

### Syntax Highlighting Wrong

1. Check file extension matches language
2. Manually set language: View → Set Language
3. Report issue if language not supported

## Accessibility

### High Contrast Themes

Use high contrast themes for better visibility:
- **Preferences** → **Appearance** → **High Contrast**

### Font Scaling

Increase font size for readability:
- `Ctrl++` to zoom in
- `Ctrl+-` to zoom out
- `Ctrl+0` to reset

### Keyboard Navigation

All features are accessible via keyboard shortcuts. See [KEYBOARD_SHORTCUTS.md](KEYBOARD_SHORTCUTS.md).

## Getting Help

- **Documentation:** https://github.com/djeada/Lightpad/docs
- **Issues:** https://github.com/djeada/Lightpad/issues
- **Contributing:** See [CONTRIBUTING.md](../CONTRIBUTING.md)
