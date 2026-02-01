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
| `F3` | Find next |
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

When VIM mode is enabled, the editor uses modal editing with different shortcuts:

> **Note:** In VIM Normal mode, `Ctrl+D` means "Page down" instead of "Duplicate line".
> Use `:set novim` to disable VIM mode, `:set vim` to re-enable it, or disable VIM mode in preferences.

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
| `Ctrl+U` | Half page up |
| `Ctrl+D` | Half page down |
| `Ctrl+B` | Full page up |
| `Ctrl+F` | Full page down |
| `Ctrl+E` | Scroll down one line |
| `Ctrl+Y` | Scroll up one line |
| `%` | Jump to matching bracket |
| `{` | Previous paragraph |
| `}` | Next paragraph |
| `(` | Previous sentence |
| `)` | Next sentence |
| `f{char}` | Find character forward |
| `F{char}` | Find character backward |
| `t{char}` | Move to before character forward |
| `T{char}` | Move to before character backward |
| `;` | Repeat last f/F/t/T |
| `,` | Repeat last f/F/t/T (opposite direction) |
| `*` | Search word under cursor forward |
| `#` | Search word under cursor backward |
| `n` | Next search result |
| `N` | Previous search result |
| `:` | Enter command mode |
| `/` | Search forward |
| `?` | Search backward |

### Command Mode

| Command | Action |
|---------|--------|
| `:w` | Save file |
| `:q` | Close tab |
| `:wq` / `:x` | Save and close |
| `:q!` | Quit app |
| `:e {file}` | Open file |
| `:{line}` | Go to line |
| `/pattern` | Search forward |
| `?pattern` | Search backward |
| `:s/old/new/` | Replace in line |
| `:%s/old/new/g` | Replace in file |

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
| `Ctrl+V` | Enter Visual Block mode |
| `R` | Enter Replace mode |
| `:` | Enter Command mode |
| `/` | Start forward search |
| `?` | Start backward search |
| `Esc` | Return to Normal mode |

### Operators (combine with motions)

| Key | Action |
|-----|--------|
| `d` | Delete |
| `c` | Change |
| `y` | Yank (copy) |
| `>` | Indent |
| `<` | Unindent |
| `dd` | Delete line |
| `yy` | Yank line |
| `cc` | Change line |
| `>>` | Indent line |
| `<<` | Unindent line |
| `dw` | Delete word |
| `cw` | Change word |
| `D` | Delete to end of line |
| `C` | Change to end of line |

### Text Objects (use with operators: d, c, y)

| Key | Action |
|-----|--------|
| `iw` | Inner word |
| `aw` | Around word (includes space) |
| `i(` or `i)` | Inner parentheses |
| `a(` or `a)` | Around parentheses |
| `i[` or `i]` | Inner brackets |
| `a[` or `a]` | Around brackets |
| `i{` or `i}` | Inner braces |
| `a{` or `a}` | Around braces |
| `i<` or `i>` | Inner angle brackets |
| `a<` or `a>` | Around angle brackets |
| `i"` | Inner double quotes |
| `a"` | Around double quotes |
| `i'` | Inner single quotes |
| `a'` | Around single quotes |
| `` i` `` | Inner backticks |
| `` a` `` | Around backticks |

### Marks

| Key | Action |
|-----|--------|
| `m{a-z}` | Set mark at cursor position |
| `'{a-z}` | Jump to mark |

### Other Commands

| Key | Action |
|-----|--------|
| `x` | Delete character |
| `r{char}` | Replace character |
| `s` | Delete character and enter insert mode |
| `S` | Delete line and enter insert mode |
| `~` | Toggle case of character |
| `u` | Undo |
| `Ctrl+R` | Redo |
| `p` | Paste after |
| `P` | Paste before |
| `J` | Join lines |
| `.` | Repeat last change (partial implementation) |
| `zz` | Center cursor on screen |
| `zt` | Scroll cursor to top |
| `zb` | Scroll cursor to bottom |

### Command Mode

| Command | Action |
|---------|--------|
| `:w` | Save |
| `:q` | Quit |
| `:wq` or `:x` | Save and quit |
| `:q!` | Force quit |
| `:{number}` | Go to line number |
| `/pattern` | Search forward for pattern |
| `?pattern` | Search backward for pattern |
| `:s/old/new` | Substitute on current line |
| `:%s/old/new/g` | Substitute in entire file |
| `:e filename` | Edit file |

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
