# Keyboard Shortcuts Reference

This document lists all keyboard shortcuts available in Lightpad.

## File Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New file |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save file |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+W` | Close current tab |
| `Ctrl+Q` | Quit application |

## Edit Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+Z` | Undo |
| `Ctrl+Y` / `Ctrl+Shift+Z` | Redo |
| `Ctrl+X` | Cut |
| `Ctrl+C` | Copy |
| `Ctrl+V` | Paste |
| `Ctrl+A` | Select all |
| `Ctrl+D` | Duplicate line |
| `Tab` | Indent |
| `Shift+Tab` | Unindent |

## Search and Replace

| Shortcut | Action |
|----------|--------|
| `Ctrl+F` | Find |
| `Ctrl+H` | Find and Replace |
| `F3` / `Ctrl+G` | Find next |
| `Shift+F3` | Find previous |
| `Ctrl+Shift+F` | Find in files |

## Navigation

| Shortcut | Action |
|----------|--------|
| `Ctrl+G` | Go to line |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |
| `Ctrl+1-9` | Switch to tab 1-9 |
| `Home` | Go to start of line |
| `End` | Go to end of line |
| `Ctrl+Home` | Go to start of file |
| `Ctrl+End` | Go to end of file |
| `Ctrl+Left` | Previous word |
| `Ctrl+Right` | Next word |

## Code Editing

| Shortcut | Action |
|----------|--------|
| `Ctrl+Space` | Trigger autocomplete |
| `Ctrl+/` | Toggle line comment |
| `Ctrl+Shift+/` | Toggle block comment |
| `Ctrl+]` | Indent line |
| `Ctrl+[` | Unindent line |
| `Ctrl+Shift+K` | Delete line |

## View

| Shortcut | Action |
|----------|--------|
| `Ctrl++` / `Ctrl+=` | Zoom in |
| `Ctrl+-` | Zoom out |
| `Ctrl+0` | Reset zoom |
| `F11` | Toggle fullscreen |
| `Ctrl+B` | Toggle sidebar |
| `Ctrl+\`` | Toggle terminal |

## VIM Mode

When VIM mode is enabled, additional shortcuts are available:

### Normal Mode

| Key | Action |
|-----|--------|
| `h` | Move left |
| `j` | Move down |
| `k` | Move up |
| `l` | Move right |
| `w` | Next word |
| `b` | Previous word |
| `e` | End of word |
| `0` | Start of line |
| `$` | End of line |
| `^` | First non-space character |
| `gg` | Go to start of file |
| `G` | Go to end of file |
| `Ctrl+U` | Page up |
| `Ctrl+D` | Page down |

### Mode Switching

| Key | Action |
|-----|--------|
| `i` | Enter Insert mode |
| `I` | Insert at line start |
| `a` | Append after cursor |
| `A` | Append at line end |
| `o` | Open line below |
| `O` | Open line above |
| `v` | Enter Visual mode |
| `V` | Enter Visual Line mode |
| `:` | Enter Command mode |
| `Esc` | Return to Normal mode |

### Operators (combine with motions)

| Key | Action |
|-----|--------|
| `d` | Delete |
| `c` | Change |
| `y` | Yank (copy) |
| `dd` | Delete line |
| `yy` | Yank line |
| `cc` | Change line |
| `dw` | Delete word |
| `cw` | Change word |
| `D` | Delete to end of line |
| `C` | Change to end of line |

### Other Commands

| Key | Action |
|-----|--------|
| `x` | Delete character |
| `r` | Replace character |
| `u` | Undo |
| `Ctrl+R` | Redo |
| `p` | Paste after |
| `P` | Paste before |
| `J` | Join lines |

### Command Mode

| Command | Action |
|---------|--------|
| `:w` | Save |
| `:q` | Quit |
| `:wq` or `:x` | Save and quit |
| `:q!` | Force quit |

## LSP Features

When connected to a language server:

| Shortcut | Action |
|----------|--------|
| `F12` | Go to definition |
| `Shift+F12` | Find references |
| `Ctrl+Shift+O` | Go to symbol |
| `F2` | Rename symbol |

## Customization

Keyboard shortcuts can be customized in:
- **Preferences** → **Keyboard Shortcuts**

Or by editing the shortcuts configuration file:
- Linux: `~/.config/lightpad/shortcuts.json`
- Windows: `%APPDATA%/Local/Lightpad/shortcuts.json`
- macOS: `~/Library/Application Support/Lightpad/shortcuts.json`
