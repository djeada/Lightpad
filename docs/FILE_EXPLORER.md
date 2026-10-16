# File explorer

Open a project folder to use the explorer. Drag the divider beside the editor
to resize it; the width, search settings, selection and expanded folders are
remembered for each project.

## Find files or search their contents

The explorer has two explicit search modes:

| Mode | Searches | Examples |
|------|----------|----------|
| Files | File and folder names and project-relative paths | `mainwindow`, `src widget`, `*.cpp`, `**/test_*.py` |
| Contents | Saved UTF-8 text, with line and column previews | `TODO`, `class Widget`, `fetch\(` with regex enabled |

Files mode ranks exact names first, then prefixes, substrings, paths and fuzzy
subsequence matches. Both modes search the full project, including folders that
have never been expanded in the tree. Search runs in the background; typing a
new query cancels the previous request. Only visible explorers perform searches.

Use **Aa** for case-sensitive search. Contents mode also offers whole-word and
regular-expression matching. Invalid expressions appear as a message below the
query. Click **Stop** to cancel a search, or use **Refresh** in the explorer menu
to run it again.

**Filters** opens include and exclude fields. Enter comma-separated patterns:

- `*.cpp, *.h` includes matching filenames anywhere in the project.
- `src/**` includes everything under `src`.
- `src/**/*.cpp` includes C++ files directly in `src` and in its descendants.
- `generated/**, *.min.js` excludes generated files and minified JavaScript.

Paths are relative to the project root, including when searching one folder.
The default exclusions skip Git metadata, dependencies, build output and common
caches. They are editable patterns, independent of `.gitignore`. Hidden files
are included by default and can be toggled in Filters.

Right-click a folder and choose **Find in Folder…** to scope content search.
Click the folder scope above the results to return to the whole project.

Press Down from the query to focus results, and Enter to open a result at its
line and column. Double-clicking a folder result reveals it in the tree.
Right-click results for the same file operations available in the tree.
Escape clears the query and restores the tree without changing its expansion
state; a second Escape clears the folder scope.

Content search reads saved files. Binary files, unreadable files, symbolic links
and files larger than 2 MiB are skipped. Directory links are never traversed.
Results are capped at 1,000 with an explicit message to narrow the search.

**Quick Open** (`Ctrl+P`) uses the same filename search and project exclusions,
runs in the background, and lists up to 50 files. It searches the open project
even when the active file is outside it.

## File operations

Use the header buttons for **New File** and **New Folder**. Names such as
`src/widgets/panel.cpp` create intermediate folders. The overflow menu contains
refresh, reveal active file, collapse and expand actions.

The context menu groups opening, searching, creation, clipboard operations,
rename, duplication, destination-based move/copy, deletion, run/debug and path
utilities. Shift-click or Ctrl-click to select multiple entries. Selecting both
a folder and its descendants operates on that folder once.

Copies and moves never replace an existing destination. A numbered suffix is
added instead, including when the existing entry is a dangling symbolic link.
Partial cut-and-paste failures retain failed sources on the clipboard for retry.
Failed recursive copies remove the incomplete destination.

Renames and moves keep open buffers, recent files, navigation history and folder
search scope associated with the new paths. This is filesystem renaming; language
imports are not rewritten automatically.

Delete moves items to system trash. Shift+Delete requests permanent deletion.
Both ask before deleting, and unsaved open documents get the normal save/discard/
cancel prompt before any file is removed.

## Drag and drop

- Drag within Lightpad to move files or folders. Hold Ctrl to copy (Option on macOS).
- Drop files or folders from a system file manager to copy them into the project.
  Hold Shift to request a move.
- Drop on a folder to use that folder, on a file to use its containing folder,
  or on empty tree space to use the project root.
- The drop indicator names the destination and whether the action is Copy or Move.
  Hover over folders to expand them; the tree scrolls near its edges.

Folders cannot be dropped inside themselves or their descendants. Every item in
a multi-file drop is validated before the operation starts.
