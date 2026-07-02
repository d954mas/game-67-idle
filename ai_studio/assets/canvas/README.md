# Canvas

Multi-image canvas projects (a Figma/Recraft-like workspace) whose capabilities
are all callable two equal ways: by an agent (CLI or direct import) and by the
thin browser page. The tools are the product; the page is only a local interface.

## Owner and boundary

This module owns canvas project persistence, the shared operation layer, its HTTP
adapter, the agent CLI, and the thin page. It does not own the 2D image pipeline:
`detect_regions` bridges to the existing `../tools/image/` ops unmodified so the
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
  rendering/input — every action is one HTTP API call. Studio Shell also mounts a
  short `/canvas` route (query string preserved, so `/canvas?project=<id>` deep
  links work) that 302-redirects to `/ai_studio/assets/canvas/site/canvas.html`;
  see `ai_studio/studio_shell/server.mjs`.
- `tests/` — `node:test` suites for the store, ops, API, and studio config.

## Projects root

The on-disk projects root is resolved from studio config
(`ai_studio/studio.config.json`, `canvasProjectsRoot`) via
`../../core_harness/tool_lib/studio_config.mjs`. It is created lazily on first
project create, never at load time. The `CANVAS_PROJECTS_ROOT` env var overrides
config so tests and one-off runs never touch the configured location.

The same config carries `canvasHistoryDepth` (default 200) — the retained undo-depth
cap read via `canvasHistoryDepth(root)`; `CANVAS_HISTORY_DEPTH` overrides it for
tests. See **History depth cap + compaction** below.

## Object references (`canvas://`)

The page's right-click "Copy ID" copies a paste-into-chat reference so the lead
can point an agent at an exact object:

```
canvas://<projectId>                          — the project
canvas://<projectId>/group/<groupId>          — a group
canvas://<projectId>/element/<elementId>      — an element
canvas://<projectId>/element/<eId>/region/<rId> — a region on an element
```

A human-readable tail follows after ` — ` (project/element/region names); it is
display sugar only. When you receive such a reference, take the bare ids out of
the URI part and drive the normal CLI/ops (`show <projectId>`, element/region
ids as-is). Multi-selection copies one reference per line.

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
- `setRegions({ projectId, elementId, regions })` — replace an element's regions
  array (the ADJUST/SELECT step before slicing). Validates each region has an id
  and an in-source-bounds integer `rect`, while **preserving any extra fields**
  the detector/slicer attach (`content_bbox`, `area_px`, `merged_from`, and any
  future shape field). Journaled, so undo/redo restore the previous regions.
- `createGroup` / `patchGroup` / `assignToGroup` / `deleteGroup` — group (screen)
  mutations, journaled; `renderGroup` — composited screen PNG export, not
  journaled. See **Groups = screens** below.
- `detectRegions` — reads the element image, runs it through the image tools
  upload + detect pipeline, stores `element.regions` (and backfills
  `source_w`/`source_h`), records a `tool_runs` entry — journaled. Requires Python
  (numpy + Pillow), as the rest of the image tools pipeline.
- `sliceRegions({ projectId, elementId, regionIds? })` — crops the element's
  **stored** regions into new immutable content-addressed image elements, cropping
  each region's rect **verbatim** from the element's own pixels (so moved, resized,
  and hand-drawn regions all crop exactly where they sit). Requires
  `element.regions` (errors clearly otherwise). Default: all regions. Each crop is
  a new image element named `<parent-name>#<region-id>`, placed in a grid to the
  right of the parent (16px gap in source pixels), with `meta.parent =
  { elementId, regionId, sheetSrc }`. All crops land in a fresh
  `"<sheet name> slices"` group so a big slice never dumps N loose elements onto
  the scene. One journal entry per slice (undo removes the group and every
  created crop) plus a `slice_regions` `tool_runs` entry. Requires Python
  (Pillow) via our own `tools/crop_regions.py`.
- `exportElements({ projectId, elementIds, format? })` — copies each element's
  current image file into `<project>/export/<utc-stamp>/` under a sanitized,
  collision-suffixed name plus a `manifest.json`. Not journaled (it makes no
  project mutation), but recorded in `tool_runs`.
- `undoOp` / `redoOp` / `readHistory` — see Journal below.
- `opsStats({ projectId })` — read-only per-op timing rollup (count/median/p95
  `duration_ms`) from the journal plus the `errors.jsonl` count. See Observability.

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
- `deleteGroup({projectId, groupId})` — remove the group **and its member
  elements** (a group is a container; deleting it deletes the content). One
  journal entry — undo restores the group and every member; image files stay in
  `files/` (non-destructive). Dissolving a group while keeping the elements is
  `assignToGroup(..., null)` (the page's Ungroup).
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
spawns the script directly with the same robust Python discovery the image tools
bridge uses (`AI_STUDIO_PYTHON`/`PYTHON` env, bundled runtime, `py -3.12`, then
`python`). A direct child-process call is fine here because this tool is ours.
Each element is drawn at its display box (`element.w`/`h`) scaled by `scale`,
offset relative to the group origin, and alpha-composited so overlap and
transparency stay correct; anything outside the group box is clipped.

### Slice crop tool

Cropping PNGs has no dependency-free pure-Node path, so `sliceRegions` uses our
OWN Python tool `tools/crop_regions.py` (PIL), spawned the same way as
`tools/render_group.py`: `ops.sliceRegions` writes a crop spec (absolute source
path + the element's selected regions with their exact rects) and spawns the
script once. Each region is cropped from the element's own pixels by its **stored**
rect — no re-detection — so user-moved, resized, and hand-drawn regions (ids that
never came from any detect run) all crop exactly where they sit. This replaced the
earlier detect-then-export image tools bridge, which re-normalized/keyed the pixels
and re-derived geometry (two Python spawns); `detectRegions` still uses the
image tools bridge, only slice is ours.

Each per-region spec entry is an **object** (`{ id, rect }`), not a bare rect, so a
future polygonal shape (`{ shape: { type: "polygon", points: [...] } }`) slots in
additively without changing the spec contract — and `setRegions` preserves unknown
region fields so that geometry survives a round-trip through the op layer today.

## Journal, undo, and redo

Each project folder has an append-only `journal.jsonl` next to `project.json`.
Every mutating op appends one **thin** metadata line — no fat snapshot inline:
`{ seq, at, op, args_summary, parent, duration_ms, has_snapshot: true }`. The
before/after project snapshot for that op lives OUT of the line in a sidecar file
`snapshots/<seq>.json` = `{ undo_patch, state }`:

- `undo_patch` is the `{ title, elements, groups, tool_runs }` snapshot **before**
  the op (undo restores it); `state` is the snapshot **after** (redo re-applies it).
  Files are immutable, so a metadata-only snapshot always fully restores
  `project.json`. Keeping these in a sidecar makes every journal line **O(1)** in
  project size, so `appendJournal`, `readHistory`, and undo/redo's scan only read
  tiny lines and load exactly the one snapshot they need by `seq`.
- `parent` is the history head that was current when the op ran, linking the log
  into a chain; `seq` is monotonic and unique per physical line. The next `seq` is
  allocated in **O(1)** by tail-reading the last journal line (not re-parsing the
  whole file).
- Undo and redo append audit markers
  `{ seq, at, op: "undo"|"redo", target_seq, duration_ms }` (no snapshot).

`project.json` carries one pointer, `history_seq` (the applied head; `0` = base).
Undo loads the head entry's sidecar `undo_patch` and moves the head to
`entry.parent`. Redo picks the greatest-`seq` mutation whose `parent` equals the
current head, restores its `state`, and advances the head. A new mutation after an
undo attaches to the current head as its newest child, so redo (greatest-`seq`
child) never re-picks the stale branch — the redo tail is invalidated automatically
(standard linear history). Append is a single `O_APPEND` write (atomic for this
single-writer local tool); a torn last line is tolerated on read.

### History depth cap + compaction

History is capped at `canvasHistoryDepth` (studio config, default **200**;
`CANVAS_HISTORY_DEPTH` env overrides for tests; `<= 0` = unlimited). After each
mutation, compaction walks the undo chain from the head; if it exceeds the cap it
keeps every line with `seq >= ` the horizon (the cap-th step from the tip), archives
the older thin lines to append-only `journal.archive.jsonl`, deletes their sidecar
snapshots, and rebases the horizon entry's `parent` to `0` so undo stops cleanly
("nothing to undo") at the horizon. Redo children of kept entries always have a
larger `seq`, so the retained undo/redo tree is fully preserved. Because compaction
physically shrinks `journal.jsonl` back to ~cap lines, the per-op journal scan stays
bounded instead of growing over a session. This matches industry norms (Photoshop
caps steps; unlimited verbatim history is not kept) and is the deliberate trade for
`journal.archive.jsonl`: dropped **metadata** is retained as an audit trail while the
fat snapshots past the horizon are reclaimed.

### tool_runs cap

`detect`/`slice`/`export`/`render` append provenance rows to `project.tool_runs`,
which rides inside every snapshot and every `project.json` read/write. The array is
capped at the last **50** (`CANVAS_TOOL_RUNS_CAP` env overrides); overflow spills to
an append-only `tool_runs.jsonl` sidecar, so provenance is never lost but `P` stays
small.

### Legacy fat-journal migration

Projects created before this layout inlined `undo_patch`/`state` on every line
(O(project) per line, O(n²) per session). On the **first mutating open**, the store
transparently migrates them: it extracts each fat line's snapshot to
`snapshots/<seq>.json`, rewrites `journal.jsonl` thin, and keeps the original as
`journal.jsonl.bak` (non-destructive — the lead's history is never deleted). The
gate is O(1) (a `snapshots/` dir means "already migrated"), and migration is
idempotent. Read-only opens never migrate.

Note: because `exportElements` is intentionally not journaled, its `tool_runs`
entry is not protected by the undo chain — undoing past the point where the export
ran can drop that provenance row. Export creates no element/geometry change, so
this only affects the audit row.

## Observability

Every journaled entry carries `duration_ms` (measured around the whole op, so
detect/slice include their Python spawn). Failed ops that touch a resolvable project
append one row to `<project>/errors.jsonl`
(`{ at, op, args_summary, error, duration_ms }`); this is wired from both clients
(the API adapter's central catch and the CLI's top-level catch), so a project-not-
found failure — which has no folder to write to — is simply not logged. API mutating
responses **add** a `duration_ms` field (existing fields are untouched, so the
running page keeps working), and `/history` still returns `canUndo`/`canRedo`
(computed from metadata only, no snapshot loads) plus a `duration_ms` on each entry.

Per-project observability files (all under the project folder, alongside
`project.json`): `journal.jsonl` (thin op log), `snapshots/<seq>.json` (fat
before/after snapshots), `journal.archive.jsonl` (compacted-away lines),
`journal.jsonl.bak` (pre-migration original), `tool_runs.jsonl` (spilled provenance),
and `errors.jsonl` (failure trail). Read the timing rollup with
`ops-stats <id>` (CLI) or `GET /api/canvas/projects/<id>/ops-stats`: per-op
`count` / `median_ms` / `p95_ms` from the journal plus the `errors` count (and a
short recent tail).

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
node ai_studio/assets/canvas/cli.mjs create [--title "My canvas"]   # omit --title for a random default
node ai_studio/assets/canvas/cli.mjs show <id>
node ai_studio/assets/canvas/cli.mjs rename <id> --title "New title"
node ai_studio/assets/canvas/cli.mjs delete <id>          # moves to .trash
node ai_studio/assets/canvas/cli.mjs add-image <id> --file path.png
node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
node ai_studio/assets/canvas/cli.mjs element-set <id> --element <eid> [--name "X"] [--visible true|false]
node ai_studio/assets/canvas/cli.mjs element-remove <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs regions-set <id> --element <eid> --json path.json   # a regions array or {regions:[...]}
node ai_studio/assets/canvas/cli.mjs regions-show <id> --element <eid>
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
node ai_studio/assets/canvas/cli.mjs ops-stats <id>   # per-op count/median/p95 + errors count
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
  title, image count, updated date) plus a `+ New project` card that creates a
  project instantly (random default title, Figma-style — no name prompt) and opens
  it straight into the workspace. Card hover reveals only Delete, gated by a lean
  two-step in-place confirm (no browser `confirm()`); renaming lives solely in the
  workspace top bar.
- `workspace.js` — the **workspace** view: the DPR-crisp pan/zoom canvas, the left
  tool rail (Select/Hand), zoom controls + indicator, top bar sync, and all pointer
  interaction (marquee select, drag-move + drop-to-reparent, region select/edit,
  pan). `imageSmoothingEnabled` is off at ≥2× zoom.
- `regions.js` — region workbench geometry: source-pixel rects → world/screen boxes,
  region body + resize-handle hit-testing, and the bright numbered overlay drawing.
  Pure helpers; edits persist through the shared `setRegions` op.
- `layers_panel.js` — the collapsible, group-aware layers list (ungrouped elements at
  top level; groups as collapsible sections with an eye toggle and inline-rename
  name; member rows indented; 24px thumbnail, region-count badge, eye toggle;
  region-bearing elements expand into indented region rows; element rows drag onto a
  group header / top level to reparent; selection syncs both ways with the canvas).
- `inspector.js` — the right panel for the selection: element (name, X/Y/W/H,
  source size, provenance, meta, and a calm **Regions** section: a count badge,
  compact per-region rows — number + name/size + delete, coords in the tooltip —
  that select/enter region-edit on the canvas and inline-rename on double-click,
  plus **+ Add region**, **Slice selected region(s)**, and one muted matte-pipeline
  placeholder line), group/screen (name, X/Y/W/H, visible, member count,
  **Render screen** with scale + background), multi-select (count + Export
  selected), or "Nothing selected".
- `context_menu.js` — the right-click menu (per element / region / group / empty
  canvas), including **Edit regions** + a **Move to screen ▸** submenu on elements,
  **Slice this region** / **Delete region** on a region, and (from the layers panel
  too) the same element/group menus; every item calls an action; closes on
  click-away or Escape.
- `dnd.js` — OS drag & drop (drop images at the drop point with a drop highlight)
  and Ctrl/Cmd+V clipboard paste at the viewport center.
- `canvas.js` — the controller: boots the modules, owns view routing (deep link
  `?project=<id>`, last-opened restore via `localStorage`) and the global keyboard.

A debug hook `?select=<elementId>` pre-selects one element on open (handy for
screenshots); its sibling `?regions=<elementId>` opens straight into region-edit
isolation (mode B) with the first region selected; both may stay. Downloads: after
**export** / **Render screen** the
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
| Space (hold) / middle-mouse | Pan (panning is Hand tool / Space-hold / middle-mouse only) |
| Drag on empty canvas | Marquee-select elements (Shift adds to the selection) |
| Click / Shift+Click / Ctrl+Click | Select / add-to-selection on canvas and in layers |
| `0` / `1` / `2` | Fit / 100% / 200% zoom (wheel also zooms) |
| `Ctrl/Cmd`+`Z` / `Ctrl/Cmd`+`Shift`+`Z` or `Ctrl`+`Y` | Undo / Redo |
| `Ctrl/Cmd`+`G` | Group 2+ selected elements into a screen |
| `Delete` / `Backspace` | Region-edit mode: remove selected regions; else remove selected elements |
| `Escape` | Close menu, then exit region-edit isolation, then clear element/group selection |
| Right-click | Context menu (element / region / group / empty); double-click a name to inline-rename |

**Region workbench (isolation mode)** — regions use a Figma-style edit-in-place
pattern with two modes:

- **Object mode (default).** Selecting an element draws its regions as a passive,
  numbered hint. Regions are NOT hit-testable — a click or drag always moves the
  whole image, so region drags can't happen by accident.
- **Region-edit mode (isolation).** Enter by double-clicking an image that has
  regions, via **Edit regions** in its context menu, via **+ Add region** in the
  inspector, or by clicking a region row in the layers tree / inspector. The
  isolated image is locked, other elements dim out, and a breadcrumb shows the mode
  ("Regions: <name> — Esc to exit"). Now regions are the only hit-testable things:
  click a region to select (Shift multi), drag inside it to move, drag its
  corner/edge handles to resize, drag the image's empty area to rubber-band a NEW
  region, `Delete` removes the selected regions. Every gesture commits once via
  `setRegions`. `Esc` (or clicking outside the image) exits back to object mode.

Regions carry an optional first-class `name` (inline-rename a region row in the
layers tree or inspector; shown in place of the size hint, and used to name the
crop element on slice). Dropping an element with its centre inside another screen
frame reparents it; drag element rows in the layers panel onto a group header (or
the panel's top-level area) to reassign, with a ghost + drop highlight.

## Validation

```powershell
node --test ai_studio/assets/canvas/tests/*.test.mjs
node ai_studio/architecture_map/validate_map.mjs
node ai_studio/core_harness/validation/doc_reference_check.mjs
```

## Bench

`tests/bench.mjs` is a plain Node script (not `*.test.mjs`, so `node --test
ai_studio/assets/canvas/tests/*.test.mjs` never runs it) that baselines every
metadata op, `readHistory` at 10/100/1000 journal entries, the Python-bridged
`detectRegions`/`sliceRegions`/`renderGroup` ops (plus raw Python spawn
overhead in isolation), and one HTTP round-trip, all against a throwaway temp
projects root:

```powershell
node ai_studio/assets/canvas/tests/bench.mjs
```

Prints an aligned console table and writes JSON results under `tmp/`
(gitignored).
