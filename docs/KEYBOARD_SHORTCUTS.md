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
| `Ctrl+]` | Indent line |
| `Ctrl+[` | Unindent line |

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
| `Ctrl+Shift+K` | Toggle Merge Conflicts panel |
| `Ctrl+Shift+M` | Toggle Problems panel |
| `Ctrl+Shift+T` | Toggle Test panel |
| `F3` | Next diff change when diff viewer is focused |
| `Shift+F3` | Previous diff change when diff viewer is focused |

## Merge Conflict Resolver

These apply while a conflicted file is open in the resolver.

| Shortcut | Action |
|----------|--------|
| `Alt+Down` | Go to the next disagreement that still needs an answer |
| `Alt+Up` | Go to the previous disagreement |
| `Ctrl+Z` | Undo the last decision |
| `Ctrl+Shift+Z` / `Ctrl+Y` | Redo the decision you just undid |

## VIM Mode

When VIM mode is enabled, the editor uses modal editing that follows Vim's behaviour (motions, operators, counts, registers, text objects, dot-repeat, macros and Ex commands). The implementation is checked against Vim 9.1 in the unit tests.

> **Note:** While VIM mode is active, Vim's own `Ctrl` keys take priority over application shortcuts in the editor. In Normal/Visual mode that is `Ctrl+A B D E F G I L O Q R U V W X Y ] [`; in Insert mode `Ctrl+A D E H O R T U V W Y [`. Other shortcuts (for example `Ctrl+S`) keep working.
> Use `:set novim` to disable VIM mode, `:set vim` to re-enable it, or disable VIM mode in preferences.

### Motions

Every motion accepts a count and can be combined with an operator.

| Key | Action |
|-----|--------|
| `h` `j` `k` `l`, arrows, `<BS>`, `<Space>` | Character / line movement |
| `w` `b` `e` `ge` | Word forward, back, end, back to end |
| `W` `B` `E` `gE` | WORD (whitespace-delimited) variants |
| `0` `^` `$` `g_` `\|` | Line start, first non-blank, line end, last non-blank, column N |
| `gg` `G` `{N}G` `{N}%` | File start/end, line N, N percent |
| `+` `-` `_` `<CR>` | First non-blank of next / previous / current line |
| `f{c}` `F{c}` `t{c}` `T{c}` `;` `,` | Find character in line and repeat |
| `%` | Matching bracket |
| `[(` `[{` `])` `]}` | Unmatched parenthesis / brace |
| `{` `}` `(` `)` | Paragraph / sentence |
| `[[` `]]` `[]` `][` | Section (brace in column 0) |
| `H` `M` `L` | Top / middle / bottom of window |
| `/pat` `?pat` `n` `N` | Search (Vim regex, offsets such as `/pat/e+1`) |
| `*` `#` `g*` `g#` | Search word under cursor |
| `'{a-z}` `` `{a-z} `` `''` ` `` ` | Marks, previous context |
| `` `[ `` `` `] `` `` `< `` `` `> `` `` `. `` `` `^ `` | Special marks |
| `Ctrl+O` / `Ctrl+I` | Jump list back / forward |
| `g;` `g,` | Change list |

### Operators

| Key | Action |
|-----|--------|
| `d` `c` `y` | Delete, change, yank |
| `>` `<` `=` | Shift right / left, re-indent |
| `g~` `gu` `gU` `g?` | Toggle case, lowercase, uppercase, rot13 |
| `gq` `gw` | Format lines (`gw` keeps the cursor) |

Double the operator for whole lines (`dd`, `cc`, `yy`, `>>`, `guu`, `gUU`, `g~~`, `gqq`). Motions can be forced with `v`, `V` or `Ctrl+V` (for example `dvj`). Operators also work with searches (`d/foo<CR>`) and with `gn` (`cgn` then `.` to change the next match).

### Text Objects

| Key | Action |
|-----|--------|
| `iw` `aw` `iW` `aW` | Word / WORD |
| `is` `as` `ip` `ap` | Sentence / paragraph |
| `i(` `a(` `ib` `ab` | Parentheses |
| `i{` `a{` `iB` `aB` | Braces (multi-line blocks become linewise) |
| `i[` `a[` `i<` `a<` | Brackets |
| `i"` `a"` `i'` `a'` `` i` `` `` a` `` | Quotes (searches forward on the line) |
| `it` `at` | XML/HTML tag |
| `gn` `gN` | Next / previous search match |

### Editing Commands

| Key | Action |
|-----|--------|
| `i` `a` `I` `A` `gI` `gi` `o` `O` | Insert (counts repeat the inserted text) |
| `x` `X` `D` `C` `s` `S` `Y` | Short forms of `dl` `dh` `d$` `c$` `cl` `cc` `yy` |
| `r{c}` `R` | Replace character(s) / Replace mode |
| `J` `gJ` | Join lines with / without spaces |
| `~` | Toggle case |
| `p` `P` `gp` `gP` `]p` `[p` | Put (linewise, charwise and blockwise) |
| `u` `U` `Ctrl+R` | Undo / redo (an insert session is one undo step) |
| `.` | Repeat last change (with a new count if given) |
| `Ctrl+A` `Ctrl+X` | Increment / decrement decimal, hex (`0x`) and binary (`0b`) numbers |
| `q{r}` `q` `@{r}` `@@` `@:` | Record / play macros, repeat last Ex command |
| `m{a-zA-Z}` | Set mark |
| `&` `g&` | Repeat last `:s` on the line / whole file |
| `zz` `zt` `zb` `z<CR>` `z.` `z-` | Scroll cursor line |
| `Ctrl+E` `Ctrl+Y` `Ctrl+D` `Ctrl+U` `Ctrl+F` `Ctrl+B` | Scroll |
| `zo` `zc` `za` `zR` `zM` | Folds |
| `gd` `Ctrl+]` `gf` | Go to definition, open file under cursor |
| `gt` `gT` | Next / previous tab |
| `Ctrl+W s` `Ctrl+W v` `Ctrl+W w` `Ctrl+W q` `Ctrl+W o` | Split, focus, close, unsplit |
| `ZZ` `ZQ` | Save and close / close without saving |
| `ga` `Ctrl+G` | Character info / file info |

### Insert and Replace Mode

| Key | Action |
|-----|--------|
| `Esc` `Ctrl+[` `Ctrl+C` | Back to Normal mode |
| `Ctrl+W` `Ctrl+U` `Ctrl+H` | Delete word / line / character before the cursor |
| `Ctrl+T` `Ctrl+D` | Indent / unindent the line |
| `Ctrl+R {r}` | Insert register contents |
| `Ctrl+O {cmd}` | Run one Normal-mode command |
| `Ctrl+E` `Ctrl+Y` | Copy the character below / above |
| `Ctrl+A` | Insert the last inserted text |
| `Ctrl+V {c}` | Insert a character literally |
| `Insert` | Toggle Insert / Replace |
| `BS` in Replace mode | Restore the replaced characters |

### Visual Mode

`v`, `V` and `Ctrl+V` start characterwise, linewise and block selections; pressing another one switches type. All motions and text objects extend the selection, and a mouse selection is treated as a visual selection.

| Key | Action |
|-----|--------|
| `o` `O` | Other end / other corner |
| `d` `x` `X` `D` `y` `Y` | Delete / yank (uppercase: whole lines, or to end of line in block mode) |
| `c` `s` `C` `S` `R` | Change |
| `r{c}` `J` `gJ` `>` `<` `=` `~` `u` `U` `g?` `gq` | Replace, join, shift, indent, case, format |
| `p` `P` | Replace the selection with a register (`P` keeps the register) |
| `I` `A` `$A` | Block insert / append on every line |
| `Ctrl+A` `Ctrl+X` `g Ctrl+A` | Increment numbers (progressively with `g`) |
| `gv` | Reselect the previous area |
| `:` | Ex command on the selected lines (`:'<,'>`) |

### Registers

| Register | Contents |
|----------|----------|
| `"a`–`"z`, `"A`–`"Z` | Named registers (uppercase appends) |
| `""` | Unnamed register |
| `"0` | Last yank |
| `"1`–`"9` | Last deletions of a line or more (shifted on each delete) |
| `"-` | Last small (within a line) delete |
| `"+` `"*` | System clipboard (`:set clipboard=unnamedplus` to use it by default) |
| `"_` | Black hole |
| `".` `":` `"/` | Last inserted text, last Ex command, last search |

### Ex Commands

Ranges are supported everywhere they make sense: `%`, `.`, `$`, `N`, `'a`, `'<,'>`, `/pat/`, `?pat?`, `+N`/`-N` and `;`. Commands can be chained with `|`, and the command line supports `Up`/`Down` history (prefix filtered), `Ctrl+R {r}`, `Ctrl+R Ctrl+W`, `Ctrl+U`, `Ctrl+W`, `Left`/`Right` and incremental search.

| Command | Action |
|---------|--------|
| `:w` `:wa` `:up` `:sav` | Save, save all, save if modified, save as |
| `:q` `:q!` `:wq` `:x` `:qa` `:wqa` | Close tab / quit |
| `:e {file}` `:tabe {file}` `:enew` | Open file, new file |
| `:sp` `:vs` `:close` `:only` `:bn` `:bp` `:tabn` `:tabp` | Splits and tabs |
| `:{N}` | Go to line |
| `:s/pat/rep/[gine]` `:&` `:&&` `:~` | Substitute (`\1`, `&`, `\r`, `\u` `\U` `\l` `\L` `\e`, `~` in the replacement) |
| `:g/pat/cmd` `:v/pat/cmd` | Run a command on matching / non-matching lines |
| `:normal {keys}` | Run Normal-mode keys (on each line of a range) |
| `:d` `:y` `:pu` `:m` `:t` `:co` `:j` `:>` `:<` | Line editing |
| `:sort [n] [u] [i] [x] [/pat/]` `:sort!` | Sort lines (whole file by default) |
| `:retab` | Convert tabs using `tabstop` / `expandtab` |
| `:noh` | Clear search highlight |
| `:undo` `:redo` `:marks` `:reg` `:ma {a-z}` | Undo, redo, marks, registers |
| `:set {option}` | `ignorecase` `smartcase` `hlsearch` `incsearch` `wrapscan` `expandtab` `autoindent` `joinspaces` `tabstop` `shiftwidth` `textwidth` `clipboard` `nrformats` (supports `no`, `inv`, `!`, `?`, `+=`, `-=`) |

Search patterns use Vim regular expression syntax: `\<` `\>`, `\(` `\)`, `\|`, `\+`, `\=`, `\{n,m}` / `\{-}`, character classes (`\s \d \w \a \u \l \x \h`), `\zs` / `\ze`, `\c` / `\C`, and the `\v` / `\m` / `\M` / `\V` magic levels.

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
