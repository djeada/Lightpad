# Search and replace

Use **Edit → Find in file** or **Edit → Replace in file** for the current editor,
and the corresponding **in project** commands for the workspace. The panel keeps
searching as you type without opening files or moving the editor cursor.

## Choose what to search

| Scope | Searches |
| --- | --- |
| Current file | The active editor, including unsaved changes and untitled documents. |
| Selection | The text selected when you choose this scope. Navigating matches keeps that captured range. Choose another scope and Selection again to capture a new range. |
| Open files | Text editors in every editor group, including untitled documents. Multiple views of the same file count once. |
| Project | Files under the project root, using open editors' unsaved contents in preference to their saved files. |

The explorer's **Files** mode finds names and paths. This panel searches **text
inside files**. The explorer's Contents mode is useful for quick navigation; use
this panel for advanced matching and replacement.

Turn on **Results** to show matching lines for a local search. Open-files and
project searches show grouped results automatically. Enter on a result opens the
file and selects the exact match. Results are paged in groups of 200; a file's
checkbox selects or clears its matches across every page.

## Matching options

- **Aa**: case-sensitive matching.
- **ab**: whole words, including Unicode letters, digits, combining marks, and
  underscores as word characters.
- **.\***: regular expressions. Anchors `^` and `$` work on each line. Matches may
  span multiple lines; **Options → . matches newline** lets `.` cross line breaks.
- **AB** beside replacement: preserve uppercase, lowercase, and title case.
  Mixed-case identifiers keep the replacement's spelling.
- **Options → Escape sequences**: interpret `\n`, `\r`, `\t`, and `\\` in literal
  search and replacement. Regex search always uses regex escape syntax.
- **Options → Wrap around**: continue from the opposite end of the results, with
  an explicit wrap message. Turn it off to stop at the first or last match.

In Project scope, **Options** also provides comma-separated Include and Exclude
globs. Examples: `*.cpp, *.h`, `src/**`, or `tests/**`. Directory exclusions prune
their entire subtree. The defaults skip version-control internals, dependency
folders, common build outputs, and caches. Clear or adjust them to include those
files. Hidden text files and extensionless text files are searchable.

Invalid regex patterns show their error and position without navigating or
replacing. Large searches run in the background, and **Stop** cancels them.
Changing the query, options, replacement, scope, or an open document invalidates
its preview; stale background work cannot overwrite a newer result.

## Review and apply replacements

**Replace** changes the currently selected search match. If no current match is
selected, it selects the next match first. An empty replacement deletes matched
text. Each application is one editor undo block.

For a batch, choose **Preview changes**, review the highlighted Before and After
columns, then uncheck individual matches or whole files as needed. **Apply N
replacements** applies only checked changes; replacements identical to the
original text do not count as changes.

Regex replacements support `$1`, `\1`, `${1}`, `${name}`, and `$0` for the complete
match. `$$` inserts a literal dollar sign; `\\` inserts a literal backslash.
Captured text is inserted once and never interpreted as replacement syntax.
For example, searching `(?<key>\w+)=(\d+)` and replacing with `${key}: $2`
changes `timeout=30` to `timeout: 30`.

Open editors are updated in memory and remain unsaved. Closed files are saved
using atomic file writes. Before any disk write, the whole disk batch is checked
against its preview; each file is checked again immediately before writing. If a
write fails or you stop the batch, completed writes are rolled back when their
contents still match this operation. External edits are never intentionally
overwritten during rollback; a failure identifies any files that could not be
restored. A batch is protected from window closure while it is being committed.

**Undo replacement** restores the last successful batch in this panel session.
It checks that affected editor text and undo history, and affected disk bytes,
still match that batch. Later edits, closed or renamed editors, or external file
changes block undo rather than discarding newer work. Ordinary Ctrl+Z remains
available within each edited document. Batch undo is kept in memory and does not
survive restarting Lightpad.

## File handling and limits

Closed-file replacement supports valid UTF-8 text, including an optional UTF-8
BOM. Existing BOMs and untouched line-ending bytes are retained, even in files
with mixed line endings. New line breaks use the file's prevailing line ending.
Binary data, other encodings, symlinks, unreadable files, and files larger than
2 MiB are skipped; the status reports skips. Open editor contents are searched
regardless of that disk-file size limit. Read-only targets fail before a disk
batch is written.

Search retains at most 20,000 matches and 64 MiB of source and replacement text.
When a limit is reached, replacement is disabled until the search is narrowed.
This prevents applying an incomplete project preview.

## Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| Ctrl+F | Find in current file; seed from a single-line editor selection. |
| Ctrl+H | Replace in current file. |
| Ctrl+Alt+F | Find in project. |
| Ctrl+Shift+H | Replace in project. |
| Enter / Shift+Enter in Find | Next / previous match. |
| Ctrl+Enter in the panel | Refresh matches, including external file changes. |
| F3 / Shift+F3 | Next / previous match from the panel or editor. |
| Enter in Replace | Replace the selected match, or select the next match. |
| Ctrl+Shift+1 in the panel | Replace the selected match. |
| Ctrl+Alt+Enter in the panel | Preview changes; when previewing, apply selected changes. |
| Alt+C / Alt+W / Alt+R | Toggle case / whole words / regex. |
| Alt+Up / Alt+Down in Find | Recall recent searches. |
| Down in Find | Focus the results list when it is visible. |
| Enter on a result | Open the match and focus its editor. |
| Escape | Close the panel and focus the editor. |

Find and Replace use Qt's platform key bindings. Ctrl+Shift+F keeps its existing
Format Document action. Recent search and replacement menus retain 20 entries
in memory. Vim's `/`, `?`, and `:` commands use their own compact command field;
leaving command mode restores the normal search panel's query and scope.
