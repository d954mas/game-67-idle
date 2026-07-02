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
- `site/` — the thin page (`canvas.html` / `canvas.js` / `canvas.css`), served via
  the existing `/ai_studio/` static route. It reuses the Asset Tools viewport
  module for pan/zoom/fit and holds no logic beyond rendering/input.
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

The page mirrors these: click selects, Shift+click toggles, Escape clears, Delete
removes selected, Ctrl/Cmd+Z undo, Ctrl/Cmd+Shift+Z (or Ctrl+Y) redo, plus Slice
and Export selected buttons. Visible groups draw as Figma-like frames with a name
label above the top-left corner; clicking the label selects the group and
dragging it moves the whole screen, a selected group offers **Render screen** and
**Hide/Show**, and **Group selection** turns 2+ selected elements into a screen.
Elements that are `visible:false` or inside a hidden group are neither drawn nor
hit-testable.

## Validation

```powershell
node --test ai_studio/assets/canvas/tests/*.test.mjs
node ai_studio/architecture_map/validate_map.mjs
node ai_studio/core_harness/validation/doc_reference_check.mjs
```
