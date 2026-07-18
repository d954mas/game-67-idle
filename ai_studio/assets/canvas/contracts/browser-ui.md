# Canvas browser UI contract

`../site/inspector.js` and `../site/workspace.js` are stable browser facades.
Implementation modules may be decomposed underneath them, but their exports,
DOM/CSS, keyboard behavior, pointer behavior, and journal calls remain stable.

The page owns rendering and input only. `site/actions.js` is the sole UI-to-API
mutation seam. A gesture produces one operation and one journal entry. View
state such as selection, zoom, open panels, previews, and tool mode is not
journaled. Errors and long-operation progress use the existing toast layer.

Canvas coordinates are Y-up. Files are rendered through confined `/files`
routes; UI code never edits project JSON or source files directly.

An image with `assetStatus` shows the same lowercase text badge in the bitmap
workspace and the DOM layers row. Color reinforces but never replaces the label:
amber is `quarantine`, blue is `checked`, and green is `accepted`. The badge is
view-only chrome and never enters rendered/exported asset pixels. At low zoom it
compacts to a one-letter text badge; below 16 screen pixels it yields to the full
DOM layers badge instead of overlapping neighboring art.
