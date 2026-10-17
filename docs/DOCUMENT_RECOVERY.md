# Unsaved document recovery

Lightpad keeps recovery copies of modified text editors, including untitled
documents and editors in other split groups. Recovery works independently of
**Auto Save**: it stores a separate draft and does not save over the source file.

## Recover after an interruption

When Lightpad starts, it offers drafts left by windows that are no longer
running. You can also open **File → Recover Unsaved Documents…** at any time.

1. Select a row to preview its text, original path, recovery time, and encoding.
2. Filter by document name or original path if there are many drafts.
3. Check the drafts you want and choose **Restore**. Hidden rows do not take part
   in the action while a filter is active.
4. Each restored draft opens in a new unsaved tab. Use Save or Save As to choose
   where it belongs. The current original file is left unchanged during recovery.

Recovered drafts keep their text, cursor position, UTF-8 BOM or Latin-1 encoding
choice, and line-ending setting. If the original file changed or was deleted,
the draft is still available as an independent unsaved document. Save As retains
the recovered format, with the existing UTF-8 fallback if Latin-1 cannot represent
new characters.

**Later** closes the recovery screen and keeps the copies. **Discard selected**
requires confirmation and permanently removes those selected recovery copies.
It does not delete source files. Copies owned by other running windows are never
offered for restoration or discard.

A restored draft is durably transferred into the current window's recovery
session before its previous copy is removed. It can be recovered again if the
application is interrupted a second time before you save it.

## Checkpoints and storage

Modified editors are checkpointed approximately once per second, with filesystem
writes on a background worker. A checkpoint contains the whole current text;
text typed after the last completed checkpoint may be lost in an interruption.
Busy storage can delay checkpoints. Recovery is a best effort safeguard rather
than a substitute for saving or version control.

Copies live in the application's local data directory under `recovery/`, in
separate UUID-named session folders. They use private directory/file permissions
where the platform supports them, session locks, checksums, and atomic writes.
No recovery copies are written into your project. These copies contain your
unsaved text; they are ordinary local files, not encrypted storage.

The maximum draft size is 64 MiB of UTF-8 text. If an update exceeds that limit,
the last good copy is retained and the status bar reports the problem. A failed
write also retains the previous copy. Invalid or unsupported copies are kept,
with their paths available through **Details** in the recovery screen. Copies
are never silently truncated. The preview displays at most 262,144 characters;
Restore keeps the full draft. The review screen loads at most 1,000 drafts or
256 MiB of decoded text per batch; after processing a batch, Refresh finds the
remaining copies.

Saving a document, undoing back to its saved version, or explicitly closing and
discarding it clears its current recovery copy. An accepted normal application
close clears that window's session after every Save / Don't Save / Cancel
choice has completed. Cancelling the close keeps its drafts. Choosing Don't
Save deliberately discards that document's unsaved text; it is not a way to keep
work for the next launch.

## Save and close behavior

- A tab's unsaved indicator follows the document's actual modified state. Undo
  back to the saved version clears it; redo makes it dirty again.
- Ctrl+W and the tab close button use the same Save / Don't Save / Cancel prompt.
- Save-time whitespace and tab cleanup stays in undo history. If writing fails,
  the document stays modified and those changes can be undone.
- Editor saves require an atomic replacement. If a temporary file cannot be
  created beside the destination, saving fails instead of falling back to an
  in-place overwrite.
- Save As refuses a destination already open in another editor, preventing one
  open document from overwriting another editor's contents.
- Saving is deferred while a replacement batch is active or an editor is
  read-only.
