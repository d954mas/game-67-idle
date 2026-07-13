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
