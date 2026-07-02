# Canvas

Multi-image canvas projects (a Figma/Recraft-like workspace) whose capabilities
are all callable two equal ways: by an agent (CLI or direct import) and by the
thin browser page. The tools are the product; the page is only a local interface.

## Owner and boundary

This module owns canvas project persistence, the shared operation layer, its HTTP
adapter, the agent CLI, and the thin page. It does not own the 2D image pipeline:
`detect_regions` bridges to the existing `../tools/raster2d/` ops unmodified so the
browser and an agent get identical pixels/regions.

## Layout

- `store.mjs` — project persistence. One project is a folder under the configured
  canvas projects root: `project.json` (schema `ai_studio.canvas.project.v1`) plus
  an immutable, content-addressed `files/`. All writes are atomic; all ids and
  file names are path-confined.
- `ops.mjs` — the one operation layer both clients call. Thin wrappers over the
  store plus the bridged `detectRegions`.
- `api.mjs` — HTTP adapter (`createCanvasApi`) mounted by Studio Shell on
  `/api/canvas/`.
- `cli.mjs` — agent client over the same ops.
- `site/` — the thin page, served via the existing `/ai_studio/` static route. One
  HTML document (`canvas.html`) that swaps a **home** view and a **workspace** view;
  the behavior is split into small ES modules (see **Page** below). It reuses the
  Asset Tools viewport module for pan/zoom/fit and holds no logic beyond
  rendering/input — every action is one HTTP API call.
- `tests/` — `node:test` suites for the store, ops, API, and studio config.

## Projects root

The on-disk projects root is resolved from studio config
(`ai_studio/studio.config.json`, `canvasProjectsRoot`) via
`../../core_harness/tool_lib/studio_config.mjs`. It is created lazily on first
project create, never at load time. The `CANVAS_PROJECTS_ROOT` env var overrides
config so tests and one-off runs never touch the configured location.

## Operations

Every capability is one op in `ops.mjs`:

- `listProjects` / `createProject` / `getProject` / `updateProject`
- `patchProject({ projectId, title })` — rename a project. Journaled: the title
  lives in the metadata snapshot, so undo/redo restore it with everything else.
- `deleteProject({ projectId })` — move the whole project folder to
  `<projectsRoot>/.trash/<id>-<stamp>/` instead of deleting it (safety: recoverable,
  never `rm`'d). A project-level action, so it is **not** journaled (the per-project
  journal moves with the folder). `listProjects` skips the dot-prefixed `.trash`.
- `addImage` (parses real PNG/JPEG/GIF dimensions, persists `source_w`/`source_h`,
  writes an immutable file) — journaled
- `patchElement` (move/resize/rename/`visible`) / `removeElement` (element only;
  file stays) — journaled
- `createGroup` / `patchGroup` / `assignToGroup` / `deleteGroup` — group (screen)
  mutations, journaled; `renderGroup` — composited screen PNG export, not
  journaled. See **Groups = screens** below.
- `detectRegions` — reads the element image, runs it through the raster2d
  upload + detect pipeline, stores `element.regions` (and backfills
  `source_w`/`source_h`), records a `tool_runs` entry — journaled. Requires Python
  (numpy + Pillow), as the rest of the raster2d pipeline.
- `sliceRegions({ projectId, elementId, regionIds? })` — crops the element's
  detected regions into new immutable content-addressed image elements. Requires
  `element.regions` (errors clearly otherwise). Default: all regions. Each crop is
  a new image element named `<parent-name>#<region-id>`, placed in a grid to the
  right of the parent (16px gap in source pixels), with `meta.parent =
  { elementId, regionId, sheetSrc }`. One journal entry per slice (undo removes
  every created crop) plus a `slice_regions` `tool_runs` entry. Requires Python.
- `exportElements({ projectId, elementIds, format? })` — copies each element's
  current image file into `<project>/export/<utc-stamp>/` under a sanitized,
  collision-suffixed name plus a `manifest.json`. Not journaled (it makes no
  project mutation), but recorded in `tool_runs`.
- `undoOp` / `redoOp` / `readHistory` — see Journal below.

## Groups = screens

A group is a Figma-frame-like named region — a game **screen mockup** that owns
elements, can be hidden/shown, moved as a whole, and exported as one composited
PNG. Groups are additive to `ai_studio.canvas.project.v1` (no migration): a
project gains `groups: [{id, name, x, y, w, h, visible}]` (one level, no nesting)
and elements gain optional `groupId` (absent/`null` = ungrouped) and optional
`visible` (default `true`). Every group/visibility mutation is journaled exactly
like an element op — the metadata-only snapshot now also carries `groups`, so
undo/redo restore groups, elements, and tool runs together.

- `createGroup({projectId, name, x?, y?, w?, h?, fromElements?})` — explicit
  bounds, **or** `fromElements: [elementIds]` = the bounding box of those
  elements + 24px padding, assigning them to the new group. One journal entry.
- `patchGroup({projectId, groupId, name?, x?, y?, w?, h?, visible?})` — when
  `x`/`y` change, **all member elements translate by the same delta** atomically
  (one journal entry; undo restores the frame and every member). Resize (`w`/`h`)
  never moves members.
- `assignToGroup({projectId, elementIds, groupId|null})` — set or clear the group
  of the given elements. Journaled.
- `deleteGroup({projectId, groupId})` — remove the group; members keep their
  positions with `groupId` cleared. Journaled.
- `renderGroup({projectId, groupId, scale?, background?})` — composite all
  **visible** member elements (`element.visible !== false`), in element array
  order (z-order), clipped to the group bounds, into one PNG at `scale`
  (default 1) over a transparent background or an optional solid `#rrggbb`.

### Render contract

`renderGroup` writes `<project>/export/<utc-stamp>/screen_<sanitized-name>.png`
plus `manifest.json` (schema `ai_studio.canvas.export.v1`, `kind: "screen"`), the
handed-over `render_spec.json`, and a small `render_report.json`. It records a
`render_group` `tool_runs` entry and, like `exportElements`, makes no project
geometry change so it is **not** journaled. Compositing is done by our own Python
tool `tools/render_group.py` (PIL): `ops.renderGroup` writes a render spec
(absolute file paths, group bounds, scale, background, output/report paths) and
spawns the script directly with the same robust Python discovery the raster2d
bridge uses (`AI_STUDIO_PYTHON`/`PYTHON` env, bundled runtime, `py -3.12`, then
`python`). A direct child-process call is fine here because this tool is ours.
Each element is drawn at its display box (`element.w`/`h`) scaled by `scale`,
offset relative to the group origin, and alpha-composited so overlap and
transparency stay correct; anything outside the group box is clipped.

### Slice bridge choice

Cropping PNGs has no dependency-free pure-Node path, so `sliceRegions` reuses the
raster2d Python slicer unmodified, exactly as the Asset Tools surface does: it
stages the element bytes as a raster2d session (`uploadRaster2dSource`), normalizes
+ detects to get a clean keyed image (`detectRaster2dRegions`), then slices the
SELECTED region rects in one `exportRaster2dRegions` call and re-imports each crop
PNG as a canvas element. No raster2d code is modified.

## Journal, undo, and redo

Each project folder has an append-only `journal.jsonl` next to `project.json`.
Every mutating op appends one line
`{ seq, at, op, args_summary, undo_patch, state, parent }`:

- `undo_patch` is the `{ elements, tool_runs }` snapshot **before** the op (undo
  restores it); `state` is the snapshot **after** (redo re-applies it). Files are
  immutable, so a metadata-only snapshot always fully restores `project.json`.
- `parent` is the history head that was current when the op ran, linking the log
  into a chain; `seq` is monotonic and unique per physical line.
- Undo and redo append audit markers `{ seq, at, op: "undo"|"redo", target_seq }`.

`project.json` carries one pointer, `history_seq` (the applied head; `0` = base).
Undo restores the head entry's `undo_patch` and moves the head to `entry.parent`.
Redo picks the greatest-`seq` mutation whose `parent` equals the current head,
restores its `state`, and advances the head. A new mutation after an undo attaches
to the current head as its newest child, so redo (greatest-`seq` child) never
re-picks the stale branch — the redo tail is invalidated automatically (standard
linear history). Append is a single `O_APPEND` write (atomic for this
single-writer local tool); a torn last line is tolerated on read.

Note: because `exportElements` is intentionally not journaled, its `tool_runs`
entry is not protected by the undo chain — undoing past the point where the export
ran can drop that provenance row. Export creates no element/geometry change, so
this only affects the audit row.

## Export contract

`exportElements` writes `<project>/export/<utc-stamp>/` (path-confined under the
project folder) containing each element's current image file copied under its
element name (sanitized, collision-suffixed) plus `manifest.json`:

```json
{ "schema": "ai_studio.canvas.export.v1", "project": "<id>", "at": "<iso>",
  "items": [ { "elementId": "...", "name": "...", "file": "...", "src": "...", "meta": {} } ] }
```

## CLI

```powershell
node ai_studio/assets/canvas/cli.mjs list
node ai_studio/assets/canvas/cli.mjs create --title "My canvas"
node ai_studio/assets/canvas/cli.mjs show <id>
node ai_studio/assets/canvas/cli.mjs rename <id> --title "New title"
node ai_studio/assets/canvas/cli.mjs delete <id>          # moves to .trash
node ai_studio/assets/canvas/cli.mjs add-image <id> --file path.png
node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
node ai_studio/assets/canvas/cli.mjs slice <id> --element <eid> [--regions r1,r2]
node ai_studio/assets/canvas/cli.mjs export <id> --elements e1,e2   # or --all
node ai_studio/assets/canvas/cli.mjs group-create <id> --name X [--elements e1,e2 | --x --y --w --h]
node ai_studio/assets/canvas/cli.mjs group-move <id> --group g --x --y
node ai_studio/assets/canvas/cli.mjs group-set <id> --group g [--name] [--visible true|false] [--w --h]
node ai_studio/assets/canvas/cli.mjs group-assign <id> --elements e1,e2 --group g|none
node ai_studio/assets/canvas/cli.mjs group-delete <id> --group g
node ai_studio/assets/canvas/cli.mjs render-screen <id> --group g [--scale 2] [--background '#rrggbb']
node ai_studio/assets/canvas/cli.mjs undo <id>
node ai_studio/assets/canvas/cli.mjs redo <id>
node ai_studio/assets/canvas/cli.mjs history <id>
```

`show <id>` includes `groups` in its project output.

## Page

The browser page is a thin, Figma/Recraft-like local interface. `canvas.html` is
one document that swaps two views; the JS is split into focused ES modules under
`site/`:

- `app.js` — shared page state, the `fetch` API helper, read-only view helpers over
  `project.json`, the image cache, and a small refresh bus every module renders
  through.
- `actions.js` — the one place UI intents become a single HTTP API call (add/drop/
  paste image, patch/rename/hide/delete element, detect/slice/export, group create/
  patch/render/ungroup/delete, undo/redo, rename project). No module talks to the
  API directly.
- `home.js` — the **home** view: a full-page grid of project cards (cover thumbnail,
  title, image count, updated date) plus a `+ New project` card with an inline title
  input (no `prompt()`); card hover reveals inline rename and delete.
- `workspace.js` — the **workspace** view: the DPR-crisp pan/zoom canvas, the left
  tool rail (Select/Hand), zoom controls + indicator, top bar sync, and all pointer
  interaction (select, drag-move, pan). `imageSmoothingEnabled` is off at ≥2× zoom.
- `layers_panel.js` — the collapsible, group-aware layers list (ungrouped elements at
  top level; groups as collapsible sections with an eye toggle and inline-rename
  name; member rows indented; 24px thumbnail, region-count badge, eye toggle;
  selection syncs both ways with the canvas).
- `inspector.js` — the right panel for the selection: element (name, X/Y/W/H,
  source size, provenance, regions, meta), group/screen (name, X/Y/W/H, visible,
  member count, **Render screen** with scale + background), multi-select (count +
  Export selected), or "Nothing selected".
- `context_menu.js` — the right-click menu (per element / group / empty canvas);
  every item calls an action; closes on click-away or Escape.
- `dnd.js` — OS drag & drop (drop images at the drop point with a drop highlight)
  and Ctrl/Cmd+V clipboard paste at the viewport center.
- `canvas.js` — the controller: boots the modules, owns view routing (deep link
  `?project=<id>`, last-opened restore via `localStorage`) and the global keyboard.

A debug hook `?select=<elementId>` pre-selects one element on open (handy for
screenshots); it may stay. Downloads: after **export** / **Render screen** the
status area shows clickable links served by the confined
`GET /api/canvas/projects/<id>/export/<stamp>/<file>` route.

Visible groups draw as Figma-like frames with a name label above the top-left
corner; clicking the label selects the group and dragging it moves the whole
screen. Elements that are `visible:false` or inside a hidden group are neither
drawn nor hit-testable.

### Shortcuts

| Key | Action |
| --- | --- |
| `V` / `H` | Select tool / Hand (pan) tool |
| Space (hold) | Temporary pan; middle-mouse always pans |
| Click / Shift+Click / Ctrl+Click | Select / add-to-selection on canvas and in layers |
| `0` / `1` / `2` | Fit / 100% / 200% zoom (wheel also zooms) |
| `Ctrl/Cmd`+`Z` / `Ctrl/Cmd`+`Shift`+`Z` or `Ctrl`+`Y` | Undo / Redo |
| `Ctrl/Cmd`+`G` | Group 2+ selected elements into a screen |
| `Delete` / `Backspace` | Remove selected elements (image files kept on disk) |
| `Escape` | Close menu / clear selection (never leaves the project) |
| Right-click | Context menu; double-click a name to inline-rename |

## Validation

```powershell
node --test ai_studio/assets/canvas/tests/*.test.mjs
node ai_studio/architecture_map/validate_map.mjs
node ai_studio/core_harness/validation/doc_reference_check.mjs
```
