# Git objects, packfiles, and DAGs

Open **View → Git → Objects & Packfiles…**, or choose **Objects & Packfiles**
in the Git Workbench, Commit Graph, or Source Control's **More → Objects and packfiles…** menu.
The command palette also lists the menu command.

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

Select a pack to browse its index. Object rows show the full hash in a tooltip,
resolved object type, uncompressed size, pack offset, and storage encoding.
Hover over the encoding to see CRC32 and a delta base hash or offset, when
available. Both original v1 indexes and v2 indexes with 64-bit offsets are read.

Browsing reads pack headers and indexes; **Indexed** does not mean the pack's
contents have been fully verified. **Verify Selected Pack** runs Git's integrity
check and displays the result in **Verification**. Missing or malformed
indexes are reported without hiding readable copies elsewhere in storage.

## Explore contents and dependencies

Select a result to see its contents and full ID. **Copy Hash** copies that ID.
Text previews are limited to 256 KiB. Binary objects show a hexadecimal preview
of the first 4096 bytes. Previews display literal object contents, including when
Git replacement refs exist.

**Relationships** lists commit parents and trees, tree entries, and annotated
tag targets. Activate a row with a double-click or Enter to inspect its target.
Unusual filenames, including tabs and newlines, are retained. Large relationship
lists are bounded and explicitly marked when truncated.

**Object DAG** draws the inspected object's direct dependencies. Arrows point
from an object to the objects it references. Double-click a node to follow it,
drag to pan, and use **Back** / **Forward** to retrace the exploration. Large
trees show up to 24 links in the drawing; use Relationships for more entries.

For a commit, **Commit DAG** opens the repository's branch and merge graph at
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
