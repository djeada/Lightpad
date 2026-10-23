# Git objects, packfiles, and DAGs

Open **View → Git → Objects & Packfiles…**, or choose **Objects & Packfiles**
in the Git Workbench, Commit Graph, or Source Control's **More → Objects and packfiles…** menu.
The command palette also lists the menu command.

These commands open a reusable **Git Explorer** workspace for the current
repository. **Objects**, **History**, and **Split view** switch between browsing
storage, commit history, and both together. It stays modeless: both panes and
the editor remain usable. History selection updates the object inspector in
Split view. Opening history from an object or inspecting a history commit reuses
the same workspace. Each workspace stays attached to its original repository.

Drag pane dividers and table headers to choose widths. Columns retain manual
widths instead of expanding to fit their contents. Compact panes wrap controls
and use scrollbars for wider content. Switching views preserves the split and
does not maximize the window automatically.

## Find a hash anywhere in local storage

Paste a full object ID or at least four hexadecimal characters into the search
field. Search accepts uppercase and lowercase prefixes, including odd lengths.
Select **All storage** to search every pack index and loose object directory.
**Search All Storage** restores that scope after browsing an individual pack.

Results include commits, trees, blobs, annotated tags, unreachable objects, and
separate copies of the same object in different packs. A hash is not restricted
to the current branch or to the history already loaded in the commit graph.
Linked worktrees use their shared object store. Bare repositories and registered
alternate stores are supported, as are SHA-1 and SHA-256 object IDs.

Results show 250 storage locations per page. Use **Previous** and **Next 250** to
move through a large result set. Column sorting applies to the displayed page.
An empty search browses objects in the selected storage scope.

## Inspect a packfile

The storage list shows each pack's object count, byte size, and index state.
Hover over a pack for its full path, pack/index versions, and index size.
Kept, promisor, cruft, and alternate packs are identified in the state column.
Right-click a pack to copy its path.

Click a pack to clear the hash filter and reveal its index results. This also
clears the previous object inspector; select a result to inspect an object in
the chosen pack. The pack list describes repository storage, not the number of
copies of the previously inspected object. Search a hash in **All storage** to
see each of its stored copies.

Drag the divider between the pack list and the exploration trail to resize
their heights. The pack name column uses available table width, and every
column can also be resized by dragging its header divider. Names are shortened
only when they do not fit the current column; widening it reveals the full name.

Object rows show the full hash in a tooltip,
resolved object type, uncompressed size, pack offset, and storage encoding.
Hover over the encoding to see CRC32 and a delta base hash or offset, when
available. Both original v1 indexes and v2 indexes with 64-bit offsets are read.

Browsing reads pack headers and indexes; **Indexed** does not mean the pack's
contents have been fully verified. **Verify Selected Pack** runs Git's integrity
check and displays the result in **Verification**. Missing or malformed
indexes are reported without hiding readable copies elsewhere in storage.

## Explore contents and dependencies

### Decode physical storage

Select a pack in **Repository storage** to open **On disk** with its decoded
12-byte header and object index. Click an object row to read that exact pack
entry, decompress its zlib stream and show **Decoded content**. **Stored bytes**
shows a hexadecimal preview with absolute file offsets. **Entry details** shows
the header, sizes, base hash and reconstruction instructions for delta entries.
The decoder checks the reconstructed object's hash; it reports corruption in
the selected copy even when another valid copy exists elsewhere.

Select **Loose objects** to list individual `objects/xx/…` files, including
alternate object stores. Click a row to decode that file and its
`<type> <size>\0` header. **Copy Path** copies the selected physical file path.
**Content**, **Relationships** and **Object DAG** remain available for logical
inspection of the same object.

Decoding runs in the background and Stop cancels it. Text previews are capped at
256 KiB and stored byte previews at 4096 bytes. Physical inflation and delta
reconstruction have a 32 MiB limit; larger entries can have a partial preview
without a completed hash check, or use **Content** for Git's bounded preview.

Select a result to see its contents and full ID. **Copy Hash** copies that ID.
Text previews are limited to 256 KiB. Binary objects show a hexadecimal preview
of the first 4096 bytes. Previews display literal object contents, including when
Git replacement refs exist.

**Relationships** lists commit parents and trees, tree entries, and annotated
tag targets. Activate a row with a double-click or Enter to inspect its target.
Unusual filenames, including tabs and newlines, are retained. Large relationship
lists are bounded and explicitly marked when truncated.

**Object DAG** draws the inspected object's direct dependencies. Arrows point
from the inspected card on the left to the objects it references on the right.
Cards distinguish object types, show paths or numbered merge parents, and expose
full hashes in tooltips. Click a card once to follow it; its literal contents
appear in the preview below the graph. Cards highlight on hover, support Enter
and Space, and offer Open object / Copy hash in their context menu. Drag to pan,
and use **Back**, **Forward**, clickable breadcrumbs, or the **Exploration trail**
to revisit objects. Separate paths are retained
when multiple tree entries reference the same object.

Opening the DAG expands the detail pane while keeping pack storage visible.
**Show Results** restores the result table; **Expand DAG** hides it again.
The initial view preserves readable text, with scrolling for large trees.
Use **+**, **−**, or the mouse wheel to zoom, **100%** for full-size cards,
and **Fit** for an overview of every displayed dependency. Large trees show
up to 24 links in the drawing; use Relationships for more entries.
**Preview** toggles the content pane. Drag or Shift+wheel to scroll a large graph.

For a commit, **Commit DAG** shows the repository's branch and merge graph at
that commit. Commits outside the initial page, including unreachable commits,
open with their ancestry as the history scope. Changing the scope controls
returns to the normal repository view.

## Commit graph improvements

- **All refs** includes detached HEAD, stash, tags, and other references.
- Graph and list use the same history scope, order, loaded pages, and filters.
- **Load More History** works from either tab. Scrolling near the bottom also
  loads more history. Counts explicitly refer to loaded commits.
- Search matches full hashes, author email, dates, branch names, and tags as
  well as subjects. Filters survive refreshes.
- First-parent mode draws only first-parent edges. Edges crossing the viewport
  remain visible while scrolling, and the working-tree node connects to HEAD.
- A commit's context menu can inspect its object or set a real comparison anchor.
- History scrolls inside the graph without displacing commit details. Ref badges
  share a bounded area with the subject; hover a row to see every full ref name.
- Parent hashes in the inspector navigate history; its commit hash opens the
  object inspector. Click a changed file to open its diff. Diff windows stay
  modeless and reuse an existing window for the same commit. Merge patches and
  their file lists compare against the first parent, including conflict-free
  merges whose combined diff would be empty. Branch, tag,
  cherry-pick, revert and reset commands live under **Actions…**.

## Keyboard and background work

| Action | Shortcut |
| --- | --- |
| Focus hash search / graph search | Ctrl+F |
| Refresh object storage | Platform Refresh shortcut (typically F5) |
| Previous inspected object | Alt+Left |
| Next inspected object | Alt+Right |

Scans, hash searches, previews, and verification run in the background.
**Stop** cancels those operations; closing the explorer also cancels its work.
Inspection uses local data and disables lazy fetching, replacement objects,
optional Git locks, and credential prompts. Objects absent from a partial clone
or submodule store can appear as unavailable; opening a gitlink does not fetch
its target.

The explorer does not repack, prune, repair indexes, check out files, or otherwise
change the repository. Refresh after another process fetches, repacks, or prunes.

Implementation references: Git's [pack/index format](https://git-scm.com/docs/gitformat-pack),
[cat-file](https://git-scm.com/docs/git-cat-file), and
[verify-pack](https://git-scm.com/docs/git-verify-pack) documentation.

## Shared Git interface

Git dialogs use shared headings, spacing, control styles, empty states and
wrapping action rows. The Diff toolbar wraps by tool group; Undo keeps long
explanations in a scrollable body with its actions visible. The Workbench
inspector scrolls independently, and its initial history loads on opening.

In Source Control, **Fetch**, **Stashes…** and **Sync…** remain visible.
**More** contains pull, push, merge, integration, comparison, worktrees,
repository tools and object inspection. **Tools** in the panel header opens
simulation, command history and recovery. At narrow dock widths, the repository
state map collapses to a summary with the full state available in its tooltip.

Fetch asks only for a remote. Push's **Force with lease** option checks that the
remote still matches the last fetched state. Selection-dependent remote/tag
commands stay disabled until their inputs are available. Pressing Enter in a
browsing or revision field does not implicitly start a repository change.

`GitUiConsistencyTests` exercises every Git dialog, Source Control and both
conflict surfaces in dark/light themes at compact sizes. It checks action and
label clipping, scroll viewport widths, safe keyboard defaults, selection states,
repeat theme application and wrapped action interaction. For a screenshot
gallery, set `LIGHTPAD_GIT_UI_CAPTURE_DIR` when running `test_gituiconsistency`.
