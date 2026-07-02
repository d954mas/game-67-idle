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
- `addText({ projectId, x?, y?, content?, style?, groupId? })` — add a Figma-style
  **text element** (`type: "text"`) in the flat `elements[]` beside images, so z-order,
  grouping/nesting, undo, and marquee are inherited for free. `style` is merged over the
  defaults and **validated loudly** against the fonts manifest (an unknown
  family/weight, a bad align/color, or a non-finite size throws before any write); an
  optional `groupId` drops the text straight into a group. Journaled (mirrors `addImage`,
  incl. the front-order hook). The stored `w`/`h` is a **nominal box** — see **Text
  elements** below. Both clients: the page's **T tool** and the CLI `add-text`.
- `patchElement` (move/resize/rename/`visible`; for a **text** element also `content`
  and `style`) / `removeElement` (element only; file stays) — journaled. A `content`
  string and a **partial** `style` are shallow-merged over the element's current style
  and re-validated against the fonts manifest (nested `stroke`/`shadow` merge, so a
  width-only edit keeps the color); `content`/`style` on a non-text element is a loud
  error. `patchElements` (batched) accepts the same text fields.
- `patchElements({ projectId, patches })` / `removeElements({ projectId, elementIds })`
  — the **batched** multi-element ops behind marquee/multi-select move and multi-delete.
  Each applies the whole gesture in ONE `commitMutation`, so it is **one journal entry**
  and a single undo restores everything (not N steps). Same per-field rules as
  `patchElement`; a bad/missing/unknown id throws **before any write** (atomic — no
  partial batch), and an empty batch is a no-op. Both clients call these: the page's
  drag/delete commits and the CLI `elements-set`/`elements-remove`.
- `moveNodes({ projectId, moves:[{nodeId,x,y}...] })` — the **mixed** marquee/multi-select
  move: loose elements AND group frames in ONE entry, each group cascading its full subtree
  (overlap-safe — a node inside a moved group shifts once, with the topmost moved ancestor).
- `reorderNodes({ projectId, nodeIds, direction|index })` — multi-node z-order: the selected
  same-scope siblings move as ONE block (Figma `front|back|forward|backward`, relative order
  kept); a cross-scope selection applies per scope but stays ONE entry.
- `ungroupGroup({ projectId, groupId })` — dissolve one group level in ONE entry: direct
  children (elements + subgroups) land in the parent scope at the group's former z-slot in
  internal order; a single undo restores the group exactly.
  All three throw **loudly** on bad input (unknown id / empty set / bad direction), never
  half-apply, and back both clients (HTTP + CLI `nodes-move`/`nodes-reorder`/`group-ungroup`).
- `pasteNodes({ projectId, spec, dx?, dy?, scopeId? })` / `duplicateNodes({ projectId,
  nodeIds, dx?, dy?, scopeId? })` / `deleteNodes({ projectId, nodeIds })` — Figma-like
  **copy / paste / duplicate / batched-delete** for canvas objects (elements AND groups,
  mixed OK), each ONE journal entry. **Copy** itself is not an op — it is a page-held
  serialized `spec` of the selection's deep subtree (group defs + member/element defs with
  geometry, style, meta, regions, text content; image elements reference their immutable
  content-addressed file, so a paste stays valid even after the source was deleted), built
  by the pure shared `tree.buildNodesSpec` (page copy buffer AND the CLI). `pasteNodes`
  **instantiates** a spec into the current scope (`scopeId` null/absent = root): fresh ids
  for everything, internal nesting + relative back→front order preserved, shifted by
  `dx`/`dy`; it **validates the spec loudly before any write** (unknown file ref / malformed
  node / empty spec throws atomically) and mints ids server-side. `duplicateNodes` is the
  convenience that builds the spec from live ids and pastes at +offset (default `+16,+16`)
  into the originals' common scope. `deleteNodes` removes loose elements AND whole group
  subtrees together (de-duplicated), one undo deep-restoring every node at its exact z-slot.
  Both clients: the page's **Ctrl+C/V/D** + the multi/mixed **Delete** key, and the CLI
  `nodes-paste`/`nodes-duplicate`/`nodes-delete`. The copy buffer is **view-state** (never
  journaled); the journaled gesture is the paste/duplicate/delete.
- `setRegions({ projectId, elementId, regions })` — replace an element's regions
  array (the ADJUST/SELECT step before slicing). Validates each region has an id
  and an in-source-bounds integer `rect`, while **preserving any extra fields**
  the detector/slicer attach (`content_bbox`, `area_px`, `merged_from`, and any
  future shape field). Journaled, so undo/redo restore the previous regions.
- `createGroup` / `patchGroup` / `fitGroup` / `assignToGroup` / `deleteGroup` — group
  (screen) mutations, journaled; `renderGroup` — composited screen PNG export, not
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
- `setExportSettings({ projectId, elementId, rows })` — replace an element's
  Figma-style export rows `[{scale, suffix, format, quality?, resample}]` (the
  Export section persisted on the layer). Validates + normalizes each row (scale
  token syntax, `format` png/jpg/webp, `resample` lanczos/nearest, filename-safe
  `suffix`; `quality` 1-100 only for the lossy formats, default 90). Journaled like
  `setRegions`, so undo/redo restore the previous rows. Both the page's Export
  section and the CLI `export-set` drive this one op.
- `exportElements({ projectId, elementIds, rows? })` — export each element ×
  each of its rows into `<project>/export/<utc-stamp>/` plus a `manifest.json`.
  Each row scales (`resolveExportScale`) + encodes (png/jpg/webp) via **one** Python
  spawn for the whole batch (`tools/export_images.py`, spec-file pattern). A 1x-png
  export of a png source is a **byte-identical file COPY** done in Node (no
  re-encode, no spawn), so the lead's original pixels are preserved exactly. When no
  `rows` are set on an element it exports the implicit default `1x png`. `rows`, when
  passed, overrides every element's settings for that one run (agent one-shots / the
  CLI `--scale` flags). Not journaled (no project mutation), recorded in `tool_runs`.
- `exportProject({ projectId })` — no-selection project export: composite **every
  visible TOP-LEVEL group** (`parentId` null/absent) at its own default 1x png into ONE
  `<project>/export/<utc-stamp>/` folder plus a combined manifest, reusing the
  `renderGroup` compositor. A nested group is a **component inside its root screen**
  (composited by the recursive painter), never a separate screen. Not journaled; records
  an `export_project` `tool_runs` entry. Errors clearly when there are no visible screens.
- `resolveExportScale(token, srcW, srcH)` / `parseScaleSpec(token)` — the scale-token
  parser (exported for reuse/tests): a multiplier (`0.5x`/`1x`/`2x`/`3x`/`4x` or a
  bare `2`) or a fixed target dimension (`512w` = 512px wide, `512h` = 512px tall;
  the other axis keeps aspect). Throws on anything else — an unknown scale is a clear
  validation error, never a silent fallback.
- `undoOp` / `redoOp` / `readHistory` — see Journal below.
- `opsStats({ projectId })` — read-only per-op timing rollup (count/median/p95
  `duration_ms`) from the journal plus the `errors.jsonl` count. See Observability.

## Groups = screens

A group is a Figma-frame-like named region — a game **screen mockup** that owns
elements, can be hidden/shown, moved as a whole, and exported as one composited
PNG. Groups are additive to `ai_studio.canvas.project.v1` (no migration): a
project gains `groups: [{id, name, x, y, w, h, visible}]` and elements gain
optional `groupId` (absent/`null` = ungrouped) and optional `visible` (default
`true`). Every group/visibility mutation is journaled exactly like an element op —
the metadata-only snapshot now also carries `groups`, so undo/redo restore groups,
elements, and tool runs together.

The model is a **flat Defold-style graph** (no stored tree): the source of truth
is the two arrays, and nesting/z-order are expressed by additive optional fields —
`group.parentId` (its parent group; absent/`null` = a top-level screen), a numeric
`order` z-key (on elements AND groups, among same-scope siblings), and
`group.background` (an optional solid fill; see below). Paint order is **computed**,
never persisted (see **Scene tree & paint order**). `order` is written by `reorderNode`
(group + element z-order, below); `parentId` is written by `createGroup({parentId})` and
`reparentGroup` (**nesting**, below) — groups nest arbitrarily deep, and every
scope-crossing walk (render, move cascade, delete, visibility) is cycle-safe.

- `createGroup({projectId, name, x?, y?, w?, h?, fromElements?, parentId?})` — explicit
  bounds, **or** `fromElements: [elementIds]` = the bounding box of those elements +
  24px padding, assigning them to the new group. Optional `parentId` **nests** the new
  group inside an existing group (validated; `null`/absent = a top-level screen); for
  `fromElements`, a missing `parentId` defaults to the members' **common** `groupId`
  (nest a widget group inside the screen it was built from), root when they differ. One
  journal entry.
- `patchGroup({projectId, groupId, name?, x?, y?, w?, h?, visible?, background?, clip?})` —
  when `x`/`y` change, the group's **full descendant closure** translates by the same
  delta — nested subgroup frames AND every element in the subtree — atomically (one
  journal entry; undo restores the frame and the whole closure). Resize (`w`/`h`) never
  moves members. `background` sets the optional solid fill (see below): `null` clears it,
  `{type:"color", color:"#rrggbb"}` sets it — validated (invalid = a loud error, no
  silent fallback). `None` on an already-unfilled group is a no-op. `clip` is the optional
  Figma-frame clip flag (see **Group clip** below): a real `true` clips the group's members
  to its bounds, `false` clears it (stored as an **absent** field, so `clip:false` on an
  already-unclipped group is a no-op); any non-boolean is a loud error.
- `fitGroup({projectId, groupId, padding?})` — Figma **"Resize to fit"**: set the group's
  frame to the union bounding box of its **full descendant closure** (every descendant
  element AND every nested subgroup frame — both carry `x/y/w/h`; reuses the same
  `elementsBBox` math `createGroup`/`sliceRegions` use) expanded by `padding` on all sides
  (default **24**, the shared slice/group-create pad). **Children never move** — only the
  group's own `x/y/w/h` change, so with `clip:true` the new frame re-evaluates the clip
  (the point of the button); `background`/`clip`/`parentId`/`order`/`name`/`visible` are
  preserved. An **empty group** (no descendant content) is a **loud error** ("nothing to
  fit"), as is a non-finite or negative `padding` (no silent fallback). One journal entry;
  undo restores the old frame.
- `reparentGroup({projectId, groupId, parentId|null, index?})` — move a group under a new
  parent (`null` = top level) at an optional **merged-sibling** `index` (default = front
  of the destination scope). **Cycle guard**: a parent that is the group itself or any
  group in its subtree is a **loud error** (`tree.wouldCycle`), never a silent no-op.
  Order handling follows the "scopes never go half-explicit" invariant: an explicit
  `index` normalizes the destination scope to contiguous `order` (like `reorderNode`); no
  index gives a front `order` iff the destination is already explicit, else drops the
  group's stale order. One journal entry.
- `assignToGroup({projectId, elementIds, groupId|null})` — set or clear the group
  of the given elements. Journaled.
- `deleteGroup({projectId, groupId})` — remove the group **and its entire subtree**
  (nested subgroups AND every element in the closure — a group is a container; deleting
  it deletes the content). One journal entry — undo restores the whole subtree; image
  files stay in `files/` (non-destructive). Returns `removedGroups` + `removedElements`.
  Dissolving a group without deleting content is **Ungroup** (below).
- `renderGroup({projectId, groupId, scale?, background?})` — composite the group's
  **visible subtree** (`isNodeHidden` prunes hidden nodes), in **computed z-order** per
  scope (the scope's `order`, else the v1 array-order fallback). The painter is
  **recursive**: a nested subgroup composites its own background band then its children
  inside the parent band. A `clip:false` subgroup paints into the parent layer (overflow
  preserved); a `clip:true` subgroup composites its band + subtree onto its **own box-sized
  layer** (cropping overflow) then pastes that cropped layer into the parent, so nested
  clips intersect naturally (see **Group clip**). The top group is always clipped to its
  own bounds, into one PNG at `scale` (default 1). Background precedence: an explicit
  `background` arg (`#rrggbb`) **overrides** the group's own `background`; else the group's
  stored fill; else transparent.
- **Ungroup** (op `ungroupGroup`; page action `ungroup`, CLI `group-ungroup`) — dissolve
  **one level** in **ONE** journal entry: the group's direct child elements AND direct
  child subgroups move up to the group's **own parent** (not unconditionally to root, so
  nesting depth is preserved), landing **at the group's former z-slot** in their internal
  relative order, and the now-empty group is removed — all atomically, so a single undo
  restores the group exactly. Grandchildren stay under the surviving subgroups.

HTTP: `POST /api/canvas/projects/<id>/groups/<gid>/fit {padding?}` (resize to content);
`POST /api/canvas/projects/<id>/groups/<gid>/reparent {parentId|null, index?}`;
`POST .../groups {..., parentId?}` nests on create. The **Fit to content** button in the
inspector Position & Size section (disabled for a trivially-empty group) and the group
context-menu **Fit to content** item both call `fitGroup`. Page (Figma nesting): a canvas drag
moves a selected group's **whole subtree**; the layers panel drags a group onto another
group's header **middle** to nest (its own subtree is an inert target — a cycle can't be
dropped), a header **edge**/element row to reorder or reparent across scopes (the
insertion line's indent encodes the target scope); the group context menu's **Move to
group ▸** submenu lists nested targets indented.

### Node z-order (`reorderNode`)

`reorderNode({projectId, nodeId, index})` moves a node — an ELEMENT **or** a GROUP —
to a target `index` among its **merged same-scope siblings** (the elements *and* groups
sharing its parent scope, in the computed back → front order `orderedChildren` yields;
`0` = back / painted first, `N-1` = front / painted last). The move assigns explicit
contiguous `order` values `0..N-1` to **every** sibling of that scope reflecting the new
arrangement — **lazy per-scope normalization**: the first reorder on a scope makes it
explicit, and it never goes half-explicit afterwards. Only that scope is touched; every
other scope is left exactly as it was. One journal entry; undo restores the whole scope's
previous `order` fields (free via the snapshot). An **unknown node id or an out-of-range
index is a loud error** (no silent clamp).

To keep a reordered scope from silently reverting to array order when content is added,
the ops that put a node into a scope give it a **front `order`** when that scope is
already explicit (and drop a stale `order` when the destination is still implicit):
`addImage`, `assignToGroup`, `createGroup` (the new group at root; its members enter a
fresh scope), and `sliceRegions` (the slices group at root). All are no-ops on a
never-reordered scope.

`reorderElement({projectId, elementId, index})` stays as a thin **delegate** to
`reorderNode` (element ids are node ids) for back-compat — its index is now over the
merged siblings too, and it keeps the historical **forgiving** contract that an
out-of-range index snaps to the nearest edge (only the delegate clamps; `reorderNode`
itself is strict).

- HTTP: `POST /api/canvas/projects/<id>/nodes/<nodeId>/reorder {index}` (element or
  group); the element-only `POST .../elements/<eid>/reorder {index}` still routes through
  the delegate.
- Page: **Order ▸** (Bring to front / forward / Send backward / to back) on element and
  group rows in the layers panel and on canvas targets (label right-click), and
  `Ctrl+[` / `Ctrl+]` (`+Alt` = to back / to front) on the single selected node (a group,
  or one element; a multi-element selection is left to the layers drag / Order menu so a
  shortcut is always one journal entry). Dragging a layer row reorders it among its
  siblings (an insertion line between same-scope rows); a group row reorders at its own
  level only (drag-to-nest is a later increment).

### Scene tree & paint order

Paint/composite/layer order is **computed per scope** by the shared pure module
`tree.mjs` (imported by `ops.mjs` in node AND served statically to the site, so
ordering itself obeys tool parity — one implementation, two clients). A "scope" is a
group id (or root): its children are the elements with that `groupId` plus the groups
with that `parentId`, merged and sorted **back → front**. When EVERY sibling carries a
finite numeric `order`, that is the key; otherwise the **v1 fallback** keys an element
on its `elements[]` index and a group on the MIN `elements[]`-index across its subtree
members (the group is anchored at its backmost member; an empty group sorts to the
front). So a legacy project (no `order`/`parentId`) paints exactly as before, except a
group's members now form one contiguous band anchored at the backmost member. The
canvas paints in two passes — artwork (recursive scope walk: element draws, a visible
group fills its background then recurses) then a chrome overlay (frame borders + labels
+ selection). Visibility cascades: a node is hidden when it or any ancestor group is
hidden. All tree walks are cycle-safe (a corrupt `parentId` ring is capped; a dangling
parent resolves to root).

### Group background

`group.background` is an additive optional field: `null`/absent (transparent) or
`{type:"color", color:"#rrggbb"}` (solid). It fills **behind** the group's children on
the canvas AND is composited as the bottom layer by `render_group.py` (canvas + export
parity), while the hairline frame outline + label ALWAYS draw so an empty screen stays
visible. Set it with `patchGroup({background})` (HTTP `PATCH .../groups/<gid>`,
inspector Background section, group context-menu **Background ▸**) or the CLI
`group-set --background '#rrggbb'|none`.

### Group clip

`group.clip` is an additive optional boolean (absent/`false` = no clip, the default;
`true` = clip). It is the Figma frame-clip behavior — members that stick out past the group
bounds are cropped at the frame — and it governs both the on-canvas display and the
**subgroup** render:

- **Canvas**: entering a `clip:true` group pushes a rectangular clip region (its screen
  box) before painting its background + children and pops it after; nested clips intersect
  through the canvas clip stack. Frames/labels/selection are a separate chrome pass outside
  every clip, so an empty or overflowing screen is never hidden.
- **Render / export** (`render_group.py`): a `clip:true` subgroup composites its subtree
  onto its own box-sized layer (cropping overflow) then pastes that cropped layer into the
  parent — nested clips intersect because each cropped layer pastes into the next. The
  **top** group is always cropped at its own bounds regardless of the flag, so **export
  bounds are unchanged**: the flag only governs subgroup overflow *inside* a screen (and the
  on-canvas view). A clipping group's background fills only its box (already true).
- **Hit-test**: a clipped-out pixel is not hit-testable — a canvas point outside a
  `clip:true` group's box skips that group's whole subtree (so "outside ANY clipping
  ancestor" is excluded). The layers panel is unaffected — a fully clipped-out element stays
  selectable there.
- **Marquee**: an element is tested by its **visible** box (its box intersected with every
  clipping ancestor), so a member cropped away by a frame isn't rubber-band-selected where
  it no longer shows.
- **Ghost hint** (anti "lost my sprite"): when a **selected** element (or a selected
  group's member) has geometry outside a clipping ancestor, the chrome pass redraws the
  clipped-away part of the image at low alpha (`0.25`) via an even-odd clip that subtracts
  the visible box — showing *what* is hidden and *where*, without touching the artwork.

Toggle it with `patchGroup({clip})` (HTTP `PATCH .../groups/<gid>`, inspector Position &
Size **Clip content** checkbox, group context-menu **Clip content** toggle) or the CLI
`group-set --clip true|false`.

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
offset relative to the top group origin, and alpha-composited so overlap and
transparency stay correct; anything outside the top group box is clipped. The spec
carries a **recursive** z-ordered `children` tree (built by `compositeGroup` /
`buildRenderNodes`): a nested subgroup contributes its background band + its own
children painted inside the parent band. `render_group.py`'s recursive `paint_children`
paints a `clip:false` subgroup directly onto the parent layer (overflow preserved) and a
`clip:true` subgroup onto its own box-sized layer that it then pastes into the parent
(overflow cropped; nested clips intersect — see **Group clip**). A hidden node prunes its
whole subtree.

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

## Text elements

A **text element** is a Figma-style text node in the flat model — `type: "text"` in
`elements[]` beside images, so z-order, grouping/nesting, undo, marquee, and the
inspector all treat it like any element. It carries a `content` string (explicit `\n`
newlines) and a `style` block:

```json
{ "fontFamily": "Inter", "fontWeight": 400, "fontStyle": "normal", "fontSize": 24,
  "lineHeight": 1.2, "align": "left", "color": "#111111",
  "stroke": { "width": 0, "color": "#000000" }, "shadow": null, "autoResize": "width" }
```

`shadow`, when set, is a HARD offset `{ dx, dy, blur, color }` (blur is stored but always
0 in v1). The defaults are `content` "Text", Inter 400, size 24, lineHeight 1.2
(unitless), align left, color `#111111`, stroke width 0, shadow off.

**v1 scope** (kills the parity traps): **auto-width only** (explicit newlines, NO
auto-wrap), solid fill, OUTLINE + HARD offset shadow, align L/C/R. Letter-spacing, shadow
blur, italic, vertical-align, fixed-box + wrap, rich-text spans, gradient fill, and
standalone per-element text-PNG export are **v1.1+**.

### Fonts (the parity contract)

The one contract that makes canvas preview == PNG export is **the same font files** on
both sides. Static TTF instances (NOT variable fonts — PIL's variation selection is
build-sensitive) live under `site/fonts/<Family>/<File>.ttf`, each family with its own
`OFL.txt`, and a `site/fonts/fonts.json` manifest maps `family` + `weight` + `style` → a
relative file (+ its Google-Fonts origin URL). The page builds `FontFace`s from
`fonts.json` and gates the first text paint on `document.fonts.ready`; `ops.mjs` reads the
same manifest from disk and resolves each entry to an **absolute** path passed to
`render_group.py`, which loads that exact file with PIL. `fonts.mjs` is the shared, pure
module (imported by `ops.mjs` in node AND the site over `/ai_studio/`) holding the style
defaults, the loud validation/merge, the font resolution, and line splitting — one
implementation, two clients. Studio Shell serves `.ttf` as `font/ttf`.

Shipped families (all OFL-1.1, all with **Cyrillic**): **Inter** 400/600/700 (UI sans),
**Rubik** 500/700 (rounded chunky display — replaces Fredoka, which lacks Cyrillic on
Google Fonts), **Bitter** 400/700 (slab serif), **JetBrains Mono** 400/700 (monospace).
All are static instances produced from the Google Fonts variable sources with
`fontTools.varLib.instancer` (all axes pinned; `pythonPath` venv + `pip install
fonttools`). An unknown family/weight vs the manifest is a **loud error** in both clients
and at render time.

### Parity stance

PIL is the **single source of rendered truth**; the canvas page is a faithful **same-font
approximation** (~1-2px glyph drift acceptable; line breaks identical by construction).
Both renderers **re-measure** from `content` + `style` every paint, so the stored `w`/`h`
is never load-bearing — it is **bookkeeping** for selection/marquee only. The page updates
it in memory as it paints and writes it to disk **only** on the `patchElement` that commits
a content/style change (no extra journal entries); the CLI/server `addText` stores a
**nominal** box (no font metrics offline) that the next page-open re-measures precisely.
Two traps the shared contract closes:

- **Stroke.** `strokeText` on canvas centers the stroke on the glyph outline; PIL's
  `stroke_width` grows **outward**. So the page draws `strokeText` UNDER `fillText` with
  `lineWidth = 2 × style.stroke.width` and `lineJoin: "round"`; PIL uses `stroke_width =
  round(style.stroke.width × scale)`. The two then match.
- **Baseline.** canvas `textBaseline = "top"` / PIL `anchor = "la"`; each line's origin
  `y = top + i × (fontSize × lineHeight)`. The hard shadow is the same glyphs in the
  shadow color drawn FIRST (fill only, no outline), then stroke-under-fill on top.

**Export.** Text bakes into a screen PNG: it renders + exports through `renderGroup` /
`exportProject` (the recursive painter emits a `kind:"text"` node with the absolute font
path, split lines, and style; `render_group.py`'s `paint_text` re-measures for
auto-width alignment). **Standalone** per-element text export via `exportElements` is a
loud v1.1 skip ("put it in a group and export the screen"). Text in a `clip:true`
subgroup crops naturally (it paints onto the same cropped sub-layer as images).

### Page

The **T** tool in the tool rail places a text element at the click point (then switches
back to Select and opens the inline editor). Double-click a text element to edit it in
place (a textarea overlay over the box; commit on blur or `Ctrl/Cmd+Enter`, `Esc`
cancels — one `patchElement` per commit, content + the re-measured box in the same
entry). The inspector shows a **Text** section for a selected text element (font family +
weight from `fonts.json`, size, line height, align, fill, outline width + color, and a
drop-shadow toggle with dx/dy + color). The layers row shows a **"T"** glyph placeholder
(text has no image file). Group membership is **never** changed by a canvas drag — a text
element parked outside its group's frame stays a member; joining/leaving a group is
explicit (layers drag, Ctrl+G, Ungroup, CLI `group-assign`).

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
responses **add** a `duration_ms` field AND, whenever the op returned a project, a
folded `history: { seq, canUndo, canRedo }` (existing fields untouched, so the running
page keeps working). That history fold is what lets the page drive its re-render from
the op response alone — no reload GET, no separate `/history` GET (see **Page**).
`/history` still returns `canUndo`/`canRedo` (computed from metadata only, no snapshot
loads) plus a `duration_ms` on each entry; the `historyFlags(root, { projectId })` op
is the lightweight summary (same flags, no entries list) the adapter folds in.

Per-project observability files (all under the project folder, alongside
`project.json`): `journal.jsonl` (thin op log), `snapshots/<seq>.json` (fat
before/after snapshots), `journal.archive.jsonl` (compacted-away lines),
`journal.jsonl.bak` (pre-migration original), `tool_runs.jsonl` (spilled provenance),
and `errors.jsonl` (failure trail). Read the timing rollup with
`ops-stats <id>` (CLI) or `GET /api/canvas/projects/<id>/ops-stats`: per-op
`count` / `median_ms` / `p95_ms` from the journal plus the `errors` count (and a
short recent tail).

## Export contract

Export always produces files in the path-confined automation default
`<project>/export/<utc-stamp>/` (each URL segment confined by `resolveProjectPath`).
Both `exportElements` and `exportProject` write one `manifest.json`
(`ai_studio.canvas.export.v1`) alongside the images. Delivering those bytes to a
folder the lead chose is a separate delivery-to-disk layer (page dir-picker / CLI
`--to`) on top of this one shared op output — the op itself never writes to an
arbitrary path.

**Element export** (`exportElements`): one file per element × export row.

```json
{ "schema": "ai_studio.canvas.export.v1", "project": "<id>", "at": "<iso>",
  "items": [ { "elementId": "...", "name": "...", "file": "wing@2x.png",
              "src": "files/<hash>.png", "scale": "2x", "format": "png",
              "resample": "lanczos", "w": 512, "h": 512, "quality": 90, "meta": {} } ] }
```

- **Scale** `0.5x/1x/2x/3x/4x` (or custom `2x` / `512w` / `512h`); the clean-art
  supersampling (generate 2x → export 1x with Lanczos) lives here.
- **Format** `png` (lossless), `jpg` (flattened onto white, no alpha), `webp`;
  `quality` (1-100) applies to jpg/webp only. **Resample** `lanczos` (smooth,
  default) or `nearest` (pixel art).
- **Suffix** is appended to the filename (`wing@2x.png`), filename-safe and
  collision-suffixed.
- **Fast path**: a 1x-png export of a png source is a byte-identical Node copy — no
  Python, no re-encode. Everything else is one batched `tools/export_images.py`
  spawn. That tool uses the T0218 `_bridge` **config-only** Python
  (`studio.config pythonPath`); a missing venv/Pillow is a loud error naming
  `node ai_studio/assets/tools/image/_bridge/setup_python.mjs` (no silent fallback).
  `render_group.py`/`crop_regions.py` keep the module's legacy interpreter discovery
  until the T0218 canvas seam flips them over.

**Project export** (`exportProject`, no selection): every visible screen composited
at 1x png into one folder, with a combined manifest (`kind: "project"`, `items` are
`{groupId, name, file, w, h, members}`).

### Destinations

- **Page**: on Export the picker (`showDirectoryPicker`) opens **every time**,
  starting at the last-used folder for the project (`startIn` = a handle remembered
  per project in IndexedDB), and the new choice is stored after each export. Cancel
  cancels the export (visible status, no fallback). The browser-download fallback
  runs ONLY when the File System Access API is unavailable — files are never silently
  dropped. This lives in `site/export_dest.mjs` (delivery-to-disk only).
- **CLI**: `--to <dir>` copies the produced files (+ manifest) to that exact path;
  without `--to` the confined `<project>/export/<stamp>/` default stays (agents rely
  on it).

## CLI

```powershell
node ai_studio/assets/canvas/cli.mjs list
node ai_studio/assets/canvas/cli.mjs create [--title "My canvas"]   # omit --title for a random default
node ai_studio/assets/canvas/cli.mjs show <id>
node ai_studio/assets/canvas/cli.mjs rename <id> --title "New title"
node ai_studio/assets/canvas/cli.mjs delete <id>          # moves to .trash
node ai_studio/assets/canvas/cli.mjs add-image <id> --file path.png
node ai_studio/assets/canvas/cli.mjs add-images <id> --files a.png,b.png   # batched multi-image add; one undo step
node ai_studio/assets/canvas/cli.mjs add-text <id> [--x 40 --y 40] [--content "Заголовок"] [--style-json style.json] [--group <gid>]
node ai_studio/assets/canvas/cli.mjs element-set <id> --element <eid> [--content "New text"] [--style-json style.json]   # text edits (validated vs fonts.json)
node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
node ai_studio/assets/canvas/cli.mjs element-set <id> --element <eid> [--name "X"] [--visible true|false]
node ai_studio/assets/canvas/cli.mjs element-remove <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs elements-set <id> --json patches.json    # batched patch [{elementId,x?,y?,w?,h?,name?,visible?}]; one undo step
node ai_studio/assets/canvas/cli.mjs elements-remove <id> --elements e1,e2    # batched delete; one undo step
node ai_studio/assets/canvas/cli.mjs element-reorder <id> --element <eid> --index <n>   # z-order among merged siblings; 0 = back (clamps)
node ai_studio/assets/canvas/cli.mjs node-reorder <id> --node <id> --index <n>          # reorder an element OR group among merged siblings; 0 = back (strict)
node ai_studio/assets/canvas/cli.mjs nodes-move <id> --json moves.json                  # batched mixed element+group move [{nodeId,x,y}]; group subtrees cascade; one undo step
node ai_studio/assets/canvas/cli.mjs nodes-reorder <id> --nodes n1,n2 --direction front|back|forward|backward   # (or --index n) multi-node z-order block, relative order kept; one undo step
node ai_studio/assets/canvas/cli.mjs nodes-paste <id> --spec spec.json [--dx 16 --dy 16] [--group <gid>|none]   # instantiate a copied node spec (new ids); one undo step
node ai_studio/assets/canvas/cli.mjs nodes-duplicate <id> --nodes id1,id2 [--dx 16 --dy 16] [--group <gid>|none]   # duplicate live nodes in place +offset; one undo step
node ai_studio/assets/canvas/cli.mjs nodes-delete <id> --nodes id1,id2   # batched mixed element+group subtree delete; one undo step
node ai_studio/assets/canvas/cli.mjs regions-set <id> --element <eid> --json path.json   # a regions array or {regions:[...]}
node ai_studio/assets/canvas/cli.mjs regions-show <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs slice <id> --element <eid> [--regions r1,r2]
node ai_studio/assets/canvas/cli.mjs export-set <id> --element <eid> --json rows.json      # persist export rows (journaled)
node ai_studio/assets/canvas/cli.mjs export-set <id> --element <eid> --scale 2x [--suffix @2x] [--format png|jpg|webp] [--quality 1-100] [--resample lanczos|nearest]
node ai_studio/assets/canvas/cli.mjs export <id> --elements e1,e2   # or --all, or --project (all visible screens)
node ai_studio/assets/canvas/cli.mjs export <id> --all [--scale 2x --format jpg --quality 80 --suffix @2x --resample lanczos] [--to <dir>]
node ai_studio/assets/canvas/cli.mjs group-create <id> --name X [--elements e1,e2 | --x --y --w --h] [--parent <gid>|none]
node ai_studio/assets/canvas/cli.mjs group-reparent <id> --group g --parent <gid>|none [--index n]   # nest a group; none = top level
node ai_studio/assets/canvas/cli.mjs group-move <id> --group g --x --y
node ai_studio/assets/canvas/cli.mjs group-set <id> --group g [--name] [--visible true|false] [--w --h] [--background '#rrggbb'|none] [--clip true|false]
node ai_studio/assets/canvas/cli.mjs groups-set <id> --groups g1,g2 [--visible true|false] [--clip true|false]   # batched shared toggles (multi-group inspector); one undo step
node ai_studio/assets/canvas/cli.mjs group-fit <id> --group g [--padding n]   # resize the frame to fit its content (padding default 24)
node ai_studio/assets/canvas/cli.mjs group-assign <id> --elements e1,e2 --group g|none
node ai_studio/assets/canvas/cli.mjs group-ungroup <id> --group g   # dissolve one level; children keep the group's z-slot; one undo step
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
  through. Mutating actions drive the page from the op **response** via `applyMutation`
  (adopt the returned `{project}`, set `state.history` from the folded `{history}`
  flags, reconcile region-edit, render) — **zero follow-up GETs**. `reloadProject`
  (full GET + `/history` GET) is kept only for genuine resync (initial open, or a
  response that carried no project).
- `actions.js` — the one place UI intents become a single HTTP API call (add/drop/
  paste image, patch/rename/hide/delete element, detect/slice/export, group create/
  patch/render/ungroup/delete, undo/redo, rename project). No module talks to the
  API directly. Every gesture is exactly one journal entry: multi-select **delete** is one
  `elements-remove` call; a pure-element marquee **move** is one `elements-set` call; a
  **mixed** marquee move (loose elements + group frames) is one `nodes-move` (`moveNodes`)
  call with the group subtrees cascaded server-side; a multi-selection **z-order** (Ctrl+[/],
  Order menu) is one `nodes-reorder` (`reorderNodes`) block move; **Ungroup** is one
  `groups/<id>/ungroup` (`ungroupGroup`) call; **copy/paste/duplicate** (Ctrl+C/V/D) are the
  page copy buffer (`buildNodesSpec`, view-state) plus one `nodes-paste`/`nodes-duplicate`
  call, and a mixed/multi-group **Delete** is one `nodes-delete` (`deleteNodes`) call.
- `home.js` — the **home** view: a full-page grid of project cards (cover thumbnail,
  title, image count, updated date) plus a `+ New project` card that creates a
  project instantly (random default title, Figma-style — no name prompt) and opens
  it straight into the workspace. Card hover reveals only Delete, gated by a lean
  two-step in-place confirm (no browser `confirm()`); renaming lives solely in the
  workspace top bar.
- `workspace.js` — the **workspace** view: the DPR-crisp pan/zoom canvas, the left
  tool rail (Select/Hand), zoom controls + indicator, top bar sync, and all pointer
  interaction (marquee select, drag-move + drop-to-reparent, region select/edit,
  pan). `imageSmoothingEnabled` is off at ≥2× zoom. Drag renders are **rAF-coalesced**
  (many mousemoves per frame → one repaint), and `resizeCanvas` only reallocates the
  backing store when the stage size or DPR actually changed (assigning
  `canvas.width/height` clears the backing store, so it must not run every frame).
- `regions.js` — region workbench geometry: source-pixel rects → world/screen boxes,
  region body + resize-handle hit-testing, and the bright numbered overlay drawing.
  Pure helpers; edits persist through the shared `setRegions` op.
- `layers_panel.js` — the collapsible, group-aware layers list (ungrouped elements at
  top level; groups as collapsible sections with an eye toggle and inline-rename
  name; member rows indented; 24px thumbnail, region-count badge, eye toggle;
  region-bearing elements expand into indented region rows; element rows drag onto a
  group header / top level to reparent; **group rows drag to nest** — onto a header's
  middle to nest into it, an edge/between rows to reorder or reparent across scopes, with
  the drag's own subtree an inert (cycle-safe) target; selection syncs both ways with the
  canvas). A
  structure-signature guard skips the DOM rebuild on selection-only changes, and
  thumbnail `<img>` nodes are **reused** across rebuilds (keyed by element id, guarded
  on `src`), so an unrelated op never re-downloads or re-decodes a thumbnail. The
  `files/` route serves those images with `Cache-Control: public, max-age=31536000,
  immutable` + an ETag (the sha256 filename), since they are content-addressed and
  never rewritten.
- `inspector.js` — the right panel for the selection: element (name, X/Y/W/H,
  source size, provenance, meta, and a calm **Regions** section: a count badge,
  compact per-region rows — number + name/size + delete, coords in the tooltip —
  that select/enter region-edit on the canvas and inline-rename on double-click,
  plus **+ Add region**, **Slice selected region(s)**, and one muted matte-pipeline
  placeholder line), group/screen (name, X/Y/W/H + a **Fit to content** button that
  resizes the frame to its content, **Visible** + **Clip content**
  checkboxes, member count, Background, **Render screen** with scale + background),
  multi-select (count + "each exports
  its own settings" + Export), or "Nothing selected" (with a project-export button
  when there are visible screens). A single element also gets an **Export** section
  at the BOTTOM (Figma-style): a collapsible list of rows (scale + suffix + format,
  a quality slider only for jpg/webp, a resample toggle), **+ Add export setting**, a
  destination hint line, and an Export button labeled by the target. Row edits commit
  through `setExportSettings` (one journal entry per change).
- `export_dest.mjs` — export destination delivery (delivery-to-disk only): the File
  System Access dir picker opens every time starting at the last-used folder (handle
  remembered per project in IndexedDB), cancel aborts the export, and a browser-
  download fallback runs only when the API is unavailable.
- `toasts.js` — the feedback layer (there is no status bar; see **Feedback layer**
  below). A fixed bottom-right toast stack with kinds success/info/error/pinned-result,
  plus `runLongOp` (limiter + progress toast + control-disable) for the python-backed
  ops. `app.js`'s `setStatus`/`setStatusLinks` are thin shims into it.
- `long_op_queue.mjs` — the pure, DOM-free FIFO limiter behind `runLongOp` (max 2
  concurrent long ops; extra requests queue; a failed op frees its slot; a still-queued
  op can be cancelled). Node-unit-tested; a page concern only (the CLI/ops path is
  unlimited).
- `context_menu.js` — the right-click menu (per element / region / group / empty
  canvas), including **Edit regions** + a **Move to screen ▸** submenu on elements,
  **Slice this region** / **Delete region** on a region, and (from the layers panel
  too) the same element/group menus; every item calls an action; closes on
  click-away or Escape. (Export left the context menu for the inspector Export
  section in T0206.)
- `dnd.js` — OS drag & drop (drop images at the drop point with a drop highlight) and the
  **single owner** of the window `paste` event (Ctrl/Cmd+V). Deterministic rule: if the
  paste carries an OS image FILE the existing image path wins (dropped at viewport center);
  only otherwise does the internal node copy buffer paste (`pasteClipboard`). `canvas.js`
  keydown deliberately leaves Ctrl+V alone, so a node paste never double-fires.
- `canvas.js` — the controller: boots the modules, owns view routing (deep link
  `?project=<id>`, last-opened restore via `localStorage`) and the global keyboard.

A debug hook `?select=<elementId>` pre-selects one element on open (handy for
screenshots); its sibling `?regions=<elementId>` opens straight into region-edit
isolation (mode B) with the first region selected; both may stay. Downloads: after
**export** / **Render screen** a **pinned-result toast** shows clickable links served
by the confined `GET /api/canvas/projects/<id>/export/<stamp>/<file>` route.

### Feedback layer

There is no permanent status bar. All feedback is a Figma-like **toast stack** fixed at
the bottom-right of the canvas working area (inset past the inspector so it never covers
the Export button, and clear of the top-center breadcrumb chips). Toasts never block
input (the container is `pointer-events:none`; only the cards are interactive) and never
touch keyboard focus. Kinds:

- **success / info** — a transient confirmation; auto-hides after ~3s (hovering a card
  pauses its timer). Routine metadata confirmations (move/group/rename/assign) are info
  toasts; a long op's completion is a success toast.
- **error** — persists until dismissed (×); shows the op name + message. Every failure
  that used to write a red status is now an error toast — errors are **never silently
  swallowed**.
- **pinned-result** — an export/render outcome ("Exported N files to …" + download
  links); persists until dismissed and multiple results stack.

The stack caps at ~5 visible; when a new toast would exceed that, the oldest **transient**
(info/success) toast is dropped first, so errors and results are never auto-evicted.
`setStatus(msg, isError)` / `setStatusLinks(msg, links)` in `app.js` are thin shims that
route to the right kind, so every existing call site kept working.

**Undo/redo** produce **no** toast: they are high-frequency and the change is already
visible (the canvas updates and the Undo/Redo buttons enable/disable), so a toast per
Ctrl+Z would be noise. (The region-edit undo-clamp still shows its explanatory info
toast, and undo/redo failures still surface as an error toast.)

**Long ops + the limiter.** The python-backed ops — **detect / slice / render / export**
— run through `runLongOp`, which:

- shows a **busy spinner** as a progress toast ("Detecting regions…"); on completion the
  same toast **resolves in place** into the success / pinned-result / error toast;
- **disables the triggering control** (the inspector Detect / Slice / Export / Render
  button) while queued + in flight, while the canvas/page stays fully interactive;
- runs at most **N=2 concurrently** via the pure `long_op_queue.mjs` FIFO limiter. Extra
  requests **queue visibly** ("Queued: Slice… (#2)") and run in order as slots free. A
  **still-queued** op can be cancelled from its toast's × (it never fires); a **running**
  op has no cancel (there is no server-side cancellation). If an op fails, its slot frees
  and the next starts. The queue is **in-memory**: a page reload clears it. The limiter is
  a page concern only — the agent CLI / direct ops path is unlimited (an agent manages its
  own concurrency). Mutating metadata ops (move/patch/undo) are near-instant and get no
  spinner.

Visible groups draw as Figma-like frames with a name label above the top-left corner.
Selection is **Figma-nested**: a single click selects the **top-most container group** of
whatever artwork is under the cursor **within the current scope** (an element directly in
the scope selects itself); double-click **drills one level** into the group under the
cursor (a breadcrumb chip — "Screen ▸ Button — Esc to exit" — shows the entered scope);
`Ctrl`/`Cmd`+click **deep-selects** the leaf element directly; clicking a group's label
selects that group; clicking empty canvas clears the selection **and** exits to root.
`Esc` steps out one scope level (then clears selection). Hit-testing follows the computed
z-order (top-most first). Dragging a selected group moves its **whole subtree**; a hovered
group at the current scope shows a subtle outline (what a click will select). A marquee
selects nodes **at the current scope** (top-level groups + loose elements at root, a
group's own children once entered), by each element's **visible (clipped) box**. Elements
that are `visible:false` or inside a hidden group are neither drawn nor hit-testable. A
`clip:true` group crops its members to its bounds on canvas: clipped-out pixels are not
hit-testable (the layers panel still lists them), and a selected element whose geometry
overflows a clipping frame shows its cropped-away part **ghosted** at low alpha so it never
looks lost. Toggle a group's clip with the inspector **Clip content** checkbox, the group
context-menu **Clip content** item, or the CLI `group-set --clip`.

### Shortcuts

| Key | Action |
| --- | --- |
| `V` / `H` | Select tool / Hand (pan) tool |
| Space (hold) / middle-mouse | Pan (panning is Hand tool / Space-hold / middle-mouse only) |
| Drag on empty canvas | Marquee-select elements (Shift adds to the selection) |
| Click / Double-click / Ctrl+Click | Select top-most group at scope / drill one level in (region-edit on a leaf) / deep-select the leaf element |
| Shift+Click | Add to selection (layers: contiguous range) |
| `0` / `1` / `2` | Fit / 100% / 200% zoom (wheel also zooms) |
| `Ctrl/Cmd`+`Z` / `Ctrl/Cmd`+`Shift`+`Z` or `Ctrl`+`Y` | Undo / Redo |
| `Ctrl/Cmd`+`G` | Group 2+ selected elements into a screen |
| `Ctrl/Cmd`+`C` / `Ctrl/Cmd`+`V` / `Ctrl/Cmd`+`D` | Copy the selection (elements/groups/mixed) to the page buffer / paste it into the current scope (repeat paste offsets again) / duplicate in place (+offset) — one journal entry per paste/duplicate |
| `Ctrl/Cmd`+`]` / `Ctrl/Cmd`+`[` (`+Alt` = to front / to back) | Z-order the single selected node (a group, or one element) among its merged siblings |
| `Delete` / `Backspace` | Region-edit mode: remove selected regions; else remove the selection — elements only, a single group + its subtree, or a mixed/multi-group selection (batched `deleteNodes`), always one journal entry |
| `Escape` | Close menu, then exit region-edit isolation, then step UP one entered-group scope, then clear selection |
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
