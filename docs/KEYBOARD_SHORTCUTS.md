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
| `Alt+Left` | Navigate back |
| `Alt+Right` | Navigate forward |

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
| `Ctrl+Shift+G` | Toggle Source Control |
| `F3` | Next diff change when diff viewer is focused |
| `Shift+F3` | Previous diff change when diff viewer is focused |

## VIM Mode

When VIM mode is enabled, the editor uses modal editing with different shortcuts:

> **Note:** In VIM Normal mode, `Ctrl+D` means "Page down" instead of "Duplicate line".
> Use `:set novim` to disable VIM mode, `:set vim` to re-enable it, or disable VIM mode in preferences.

### Normal Mode — Movement

| Key | Action |
|-----|--------|
| `h` / `←` | Move left |
| `j` / `↓` | Move down |
| `k` / `↑` | Move up |
| `l` / `→` | Move right |
| `w` | Next word start |
| `b` | Previous word start |
| `e` | End of word |
| `W` | Next WORD start (whitespace-delimited) |
| `B` | Previous WORD start (whitespace-delimited) |
| `E` | End of WORD (whitespace-delimited) |
| `0` | Start of line (column 0) |
| `$` | End of line |
| `^` | First non-space character |
| `gg` | Go to start of file |
| `G` | Go to end of file (or line N with count) |
| `H` | Move to top of visible screen |
| `M` | Move to middle of visible screen |
| `L` | Move to bottom of visible screen |
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
| `Esc` / `Ctrl+[` | Return to Normal mode |

### Operators (combine with motions)

| Key | Action |
|-----|--------|
| `d` | Delete |
| `c` | Change |
| `y` | Yank (copy) |
| `>` | Indent |
| `<` | Unindent |
| `g~` | Toggle case (with motion) |
| `gu` | Lowercase (with motion) |
| `gU` | Uppercase (with motion) |
| `dd` | Delete line |
| `yy` | Yank line |
| `cc` | Change line |
| `>>` | Indent line |
| `<<` | Unindent line |
| `dw` | Delete word |
| `cw` | Change word |
| `D` | Delete to end of line |
| `C` | Change to end of line |

### Text Objects (use with operators: d, c, y, g~, gu, gU)

| Key | Action |
|-----|--------|
| `iw` | Inner word |
| `aw` | Around word (includes space) |
| `iW` | Inner WORD (whitespace-delimited) |
| `aW` | Around WORD (includes surrounding space) |
| `ip` | Inner paragraph |
| `ap` | Around paragraph (includes trailing blank lines) |
| `is` | Inner sentence |
| `as` | Around sentence (includes trailing space) |
| `it` | Inner HTML/XML tag |
| `at` | Around HTML/XML tag (includes the tags) |
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

### Registers

Named registers allow you to store and recall multiple clipboard values.

| Key | Action |
|-----|--------|
| `"{reg}` | Use register `{reg}` for the next yank/delete/paste |
| `"a`–`"z` | Named registers (lowercase writes, uppercase appends) |
| `"0` | Yank register (last yanked text) |
| `"1`–`"9` | Delete history (most recent to oldest) |
| `"+` | System clipboard register |
| `"_` | Black hole register (discard) |
| `".` | Last inserted text (read-only) |

Example: `"ayy` yanks the current line into register `a`, `"ap` pastes from register `a`.

### Macros

| Key | Action |
|-----|--------|
| `q{a-z}` | Start recording macro into register |
| `q` | Stop recording macro (when recording) |
| `@{a-z}` | Play back macro from register |
| `@@` | Replay last played macro |

The status bar shows **●REC @{reg}** while recording.

### Marks

| Key | Action |
|-----|--------|
| `m{a-z}` | Set mark at cursor position |
| `'{a-z}` | Jump to mark (line) |

### g-Prefix Commands

| Key | Action |
|-----|--------|
| `gi` | Go to last insert position and enter Insert mode |
| `gv` | Reselect last visual selection |
| `g~{motion}` | Toggle case over motion |
| `gu{motion}` | Lowercase over motion |
| `gU{motion}` | Uppercase over motion |

### Other Normal Mode Commands

| Key | Action |
|-----|--------|
| `x` | Delete character |
| `r{char}` | Replace character |
| `s` | Delete character and enter insert mode |
| `S` | Delete line and enter insert mode |
| `~` | Toggle case of character |
| `u` | Undo |
| `Ctrl+R` | Redo |
| `p` | Paste after cursor |
| `P` | Paste before cursor |
| `J` | Join current line with next (supports count) |
| `.` | Repeat last change |
| `Ctrl+A` | Increment number under cursor |
| `Ctrl+X` | Decrement number under cursor |
| `zz` | Center cursor on screen |
| `zt` | Scroll cursor to top |
| `zb` | Scroll cursor to bottom |

### Visual Mode

All normal motions work to extend the selection. Additional commands:

| Key | Action |
|-----|--------|
| `d` / `x` | Delete selection |
| `c` / `s` | Change selection (delete and enter Insert) |
| `y` | Yank selection |
| `>` | Indent selection |
| `<` | Unindent selection |
| `~` | Toggle case of selection |
| `u` | Lowercase selection |
| `U` | Uppercase selection |
| `J` | Join selected lines |
| `o` | Move cursor to other end of selection |

### Command Mode (Ex Commands)

| Command | Action |
|---------|--------|
| `:w` | Save file |
| `:q` | Close tab |
| `:wq` / `:x` | Save and close |
| `:q!` | Force quit application |
| `:e {file}` | Open file |
| `:{number}` | Go to line number |
| `/pattern` | Search forward for pattern |
| `?pattern` | Search backward for pattern |
| `:s/old/new/` | Substitute on current line |
| `:%s/old/new/g` | Substitute in entire file |
| `:noh` / `:nohlsearch` | Clear search highlighting |
| `:bn` | Next buffer (tab) |
| `:bp` | Previous buffer (tab) |
| `:sp` | Split horizontally |
| `:vsp` | Split vertically |
| `:sort` | Sort selected lines (or all lines) |
| `:registers` | Show register contents |
| `:marks` | Show marks |
| `↑` / `↓` | Recall previous `:` commands |

### Status Bar Indicators

The vim mode status is shown as a colored badge in the bottom status bar:

| Color | Mode |
|-------|------|
| 🟢 Green | NORMAL |
| 🔵 Blue | INSERT |
| 🟡 Yellow | VISUAL / VISUAL LINE / VISUAL BLOCK |
| 🔴 Red | REPLACE |
| 🟣 Purple | COMMAND |

The status bar also shows:
- **Pending keys** (e.g., `d` waiting for a motion)
- **Macro recording indicator** (●REC @{reg})

## LSP Features

When connected to a language server (see [Go to Definition](GO_TO_DEFINITION.md) for setup):

| Shortcut | Action |
|----------|--------|
| `F12` | Go to definition |
| `Ctrl+B` | Go to definition |
| `Ctrl+Click` | Go to definition for clicked symbol |
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
