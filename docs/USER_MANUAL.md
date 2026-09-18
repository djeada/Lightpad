# Lightpad User Manual

## Introduction

Lightpad is a modern, lightweight code editor designed for developers. It provides essential features for code editing while maintaining fast performance and a clean interface.

## Getting Started

### Installation

#### Linux (Ubuntu/Debian)
```bash
git clone https://github.com/djeada/Lightpad.git
cd Lightpad
./scripts/install-deps.sh
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

### Building and Debugging CMake Projects

When you start debugging a C/C++ file inside a project that contains a
`CMakeLists.txt`, Lightpad builds the project automatically before launching
the debugger:

1. The project is configured on first use (`cmake -S . -B build`); results are
   cached in `build/`.
2. `cmake --build build -j<nproc>` compiles the targets, with a progress
   dialog showing live output (Cancel aborts the build).
3. If `CMakeLists.txt` declares exactly one executable target, it is selected
   automatically; with several targets a picker dialog appears.
4. The debug session launches against the freshly built binary.

Notes:
- Override the build directory per configuration with `"cmakeBinaryDir"` in
  the launch configuration.
- A `preLaunchTask` string in the debug configuration runs as a shell command
  from the project root before the CMake step (useful for code generators).
- Projects without `CMakeLists.txt` keep the single-file behavior: the current
  file is compiled with `-g -O0` when its binary is missing or stale.

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

### Merging Branches

Press **Merge…** in the Source Control panel to combine two branches. The merge
screen asks four questions in order and answers each one before anything runs:

1. **Which work do you want to bring in?** Pick a branch. When the same branch
   name exists both on this machine and on a remote, Lightpad asks which copy you
   mean and warns you when the two are not at the same commit.
2. **Where it lands.** The branch you are standing on. It is the only one that
   changes; the branch you are merging from is left untouched.
3. **What will happen.** How many commits come in, how many files they touch,
   and which files are likely to need a decision from you. If the merge cannot
   start — uncommitted edits, another half-finished operation — it says so
   instead of failing halfway.
4. **If the same lines disagree.** Decide each one yourself (the default), or
   have one side always win. This step is hidden when the merge cannot conflict.

### Resolving Merge Conflicts

When a merge stops, the **Merge Conflicts** panel opens by itself
(`Ctrl+Shift+K`, or **View → Toggle Merge Conflicts**). It lists every file the
merge is waiting on, how many separate spots inside each file still need an
answer, why each one clashed, and how far along you are. It stays up until the
merge is finished, so the **Finish the merge** button is where you left it once
the last file is settled.

Opening a conflicted file does not show you raw `<<<<<<<` markers. Instead the
file becomes a list of numbered cards, one per disagreement, showing:

- **YOUR VERSION** — what is on the branch you are standing on
- **THEIR VERSION** — what is coming in

Each side names the branch it came from and how many lines it is. For every
disagreement you can keep one side, keep both in either order, keep neither, see
what both branches started from, or type the replacement yourself. Long stretches
of unchanged code are folded away and can be unfolded on demand.

Every choice is undoable (`Ctrl+Z`), a decided card can be reopened with **Change
my mind**, and `Alt+Down` / `Alt+Up` walk between disagreements. **Save and mark
fixed** only becomes available once nothing is left undecided; until then the
file stays unmerged as far as Git is concerned, so partial work is never
mistaken for a finished file.

### Opening Large or Binary Files

Reading a file into the editor costs far more memory than the file itself: the
bytes, then a UTF-16 copy, then the document built on top — roughly seven times
the file size. A few hundred megabytes is therefore enough to exhaust memory and
end the process, and binary files are worse still, because they have no line
breaks and land on a single enormous line.

Lightpad now checks a file before reading it:

- **Large text files** (8 MB or more) offer a read-only preview of the first
  2 MB, opening the whole file anyway, or cancelling.
- **Binary files** (detected by a NUL byte in the opening 8 KB, the same rule
  Git uses) offer a read-only look or cancelling.

A preview holds only part of the file, so it opens read-only and **saving it is
refused** — writing the buffer back would discard everything after the preview.
Autosave skips previews for the same reason.

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
