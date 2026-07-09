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

## Canvas stores

The default Canvas store is the public Studio projects root above. A private
game mount may opt into Canvas by listing `canvas` in `enabledStores` (or by
already having `.ai_studio/canvas/projects` under the game root); that store is
selected explicitly with `--game <id>` or `--store game:<id>`.

Agent CLI reads are public-only by default. `list --include-private` and
`GET /api/canvas/projects?include-private=true` aggregate private game stores and
decorate rows with `storeId`, `visibility`, and `qualifiedId`; selected
`create`/`show`/mutating CLI commands run inside the selected store's projects
root via `--game <id>` / `--store game:<id>`. The HTTP API accepts
`x-ai-studio-store: game:<id>` for selected store routing; `?game=<id>` and
`?store=game:<id>` remain manual/legacy fallbacks, not the browser UI contract.
Private CLI exports may write inside the owning game store or outside the parent
Studio checkout, but `--to`/`--zip` destinations inside the public parent repo
are rejected to avoid copying private art into public git.

The browser page opens `/canvas` as a local store browser: the home screen lists
all mounted Canvas stores, remembers the selected home filter in `localStorage`,
and sends store scope through request headers for normal JSON APIs. Project URLs
keep the legacy `/canvas?project=<id>` shape and do not write `store=game:<id>`
into the address bar; last-project restore keeps the private store id in
`localStorage`. Public Copy ID refs keep their human-readable tail, while private
Copy ID refs use `canvas://game/<gameId>/...` without project/object names in the
tail.

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

Private game-store refs are qualified without the human-readable tail:

```
canvas://game/<gameId>/<projectId>
canvas://game/<gameId>/<projectId>/group/<groupId>
canvas://game/<gameId>/<projectId>/element/<elementId>
canvas://game/<gameId>/<projectId>/element/<eId>/region/<rId>
```

Resolve them through the same store selector (`--game <gameId>` or
`x-ai-studio-store: game:<gameId>`); bare project ids never search private stores.

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
  error. Also accepts `rotation` (finite degrees, normalized to `[0,360)`) and `flipH`/
  `flipV` (image-only booleans) — see **Rotation & flip** below (T0232 increment 3a).
  Also accepts `opacity` (finite `[0,1]`, stored only when `!= 1`, T0260) and `filters`
  (image-only, whole-object replace, `null`/`{}` clears — see **Image filters** below,
  T0273). `patchElements` (batched) accepts the same text, transform, opacity, and
  filters fields.
- `patchElements({ projectId, patches })` / `removeElements({ projectId, elementIds })`
  — the **batched** multi-element ops behind marquee/multi-select move and multi-delete.
  Each applies the whole gesture in ONE `commitMutation`, so it is **one journal entry**
  and a single undo restores everything (not N steps). Same per-field rules as
  `patchElement`; a bad/missing/unknown id throws **before any write** (atomic — no
  partial batch), and an empty batch is a no-op. Both clients call these: the page's
  drag/delete commits + the multi-image Filters "Приглушить N images" preset, and the
  CLI `elements-set`/`elements-remove`.
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
- `alignNodes({ projectId, nodeIds, align, reference? })` / `distributeNodes({ projectId,
  nodeIds, axis })` — **(T0232 increment 1)** align/distribute for screen assembly, each ONE
  journaled entry. Target math is **pure** (`tree.alignMoves`/`tree.distributeMoves`) and
  applied through the **same overlap-safe cascade `moveNodes` uses** (a moved group carries
  its whole subtree; a node inside a moved group shifts once, with the topmost moved
  ancestor). `align` is `left|hcenter|right|top|vcenter|bottom`; the reference frame is
  **Figma-auto** by default (`reference: "auto"`): 2+ selected nodes align to the
  **union bounding box** of the selection, exactly **1** node with a parent group aligns to
  **that group's frame** (the "center this widget inside the screen" case); 1 node with no
  parent is a loud error. `distributeNodes` needs **3+** nodes: sorted by position along
  `axis` (`h`/`v`), gaps equalized, the two extreme boxes stay put. Already-aligned/-spaced
  nodes write **no journal entry** (the existing before===after no-op guard). Both clients:
  the inspector's **Align** row (multi-selection, multi-group selection, or a single node
  inside a group) and the CLI `nodes-align`/`nodes-distribute`.
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
  For chroma-key source sheets, hand-edited `rect`s must keep key-colored
  background padding around the visible component (detector default: 8 px; use
  8-16 px for UI sheets when space allows). `content_bbox` may be tight, but
  the slicing `rect` must not hug the art edge: Canvas crops the stored rect
  verbatim, and alpha/matte needs background samples around anti-aliased borders.
- `setSlice9({ projectId, elementId, insets })` — **(T0233)** set or clear an image
  element's 9-slice insets (`insets` an object `{left,top,right,bottom,scale?}` ->
  validated + stored on `element.slice9`; `insets: null` clears it). Same
  dedicated-op shape as `setRegions`/`setExportSettings` (not a `patchElement`
  field): image-only, loud set-time validation, free undo/redo (slice9 rides the
  elements snapshot). Journaled. See **9-slice elements** below.
- `createGroup` / `patchGroup` / `fitGroup` / `scaleGroup` / `assignToGroup` /
  `deleteGroup` — group (screen) mutations, journaled; `renderGroup` — composited
  screen PNG export, not journaled. See **Groups = screens** below.
- `createRecipeCard({projectId, name?, x?, y?, w?, h?, parentId?})` / `patchRecipe({
  projectId, groupId, patch})` / `generateFromRecipe({projectId, groupId, generators?})` —
  **(T0239 increments 1-2)** the generation recipe card: a group carrying an additive
  `recipe` blob, plus generation end-to-end (codex/gemini/both engines, mints new RAW
  element(s) beside the card). See **Recipe card** below.
- `detectRegions` — reads the element image, runs it through the image tools
  upload + detect pipeline, stores `element.regions` (and backfills
  `source_w`/`source_h`), records a `tool_runs` entry — journaled. Requires Python
  (numpy + Pillow), as the rest of the image tools pipeline.
- `sliceRegions({ projectId, elementId, regionIds?, perRegionMeta?, targetParentId? })` —
  crops the element's **stored** regions into new immutable content-addressed image
  elements, cropping each region's rect **verbatim** from the element's own pixels (so
  moved, resized, and hand-drawn regions all crop exactly where they sit). Requires
  `element.regions` (errors clearly otherwise). Default: all regions. Each crop is
  a new image element named `<parent-name>#<region-id>`, placed in a grid to the
  right of the parent (16px gap in source pixels), with `meta.parent =
  { elementId, regionId, sheetSrc }`. All crops land in a fresh
  `"<sheet name> slices"` group so a big slice never dumps N loose elements onto
  the scene. One journal entry per slice (undo removes the group and every
  created crop) plus a `slice_regions` `tool_runs` entry. Requires Python
  (Pillow) via our own `tools/crop_regions.py`. T0332 B3: two additional, optional,
  backward-compatible opts (both absent = today's exact behavior) — `perRegionMeta` is an
  array of plain objects, ONE per selected region in order, shallow-merged onto each
  crop's `meta` **alongside** `meta.parent` (never replacing it); `targetParentId` nests
  the fresh slices-group under an existing group (or, for a single-crop slice with no
  wrapper group, the lone crop's own `groupId` directly). Used by `packSlice` below.
- `packSlice({ projectId, groupId, runGroupId? })` — slice EVERY sheet of a pack run
  (a recipe card's `group.recipe.pack` result) into cuts: for each sheet element
  (`meta.pack` carrying the full `cells` manifest) in the resolved run group,
  `detectRegions` then a **hard gate** `region_count === cells.length` — a mismatch
  REJECTs that one sheet (siblings still slice); on a match, `sliceRegions` mints the
  cuts with a **minimal** per-cut `meta.pack = {cardId, sheet_element_id, cell, axes}`
  (the full manifest/prompt stay on the sheet — the provenance anchor) and reparents the
  fresh slices-group into the run group. `runGroupId` (`--run`) selects an explicit run
  group (must carry `pack_run` for this card); omitted resolves
  `recipe.last_run.run_group_id`. Never throws mid-sheet: returns a per-sheet contract
  `[{sheet_element_id, verdict: "OK"|"REJECT"|"MISSING", region_count, cells_len,
  cut_ids}]` — `MISSING` covers a sheet whose detection could not even run (unreadable
  image, transformed element, tool crash), `REJECT` is the count-gate mismatch (got/
  expected = `region_count`/`cells_len`). One `detectRegions` + (on a match) one `slice`
  journal entry per sheet, same as calling them by hand. CLI: `recipe-pack-slice <id>
  --group g [--run <groupId>]`.
- `alphaCutout({ projectId, elementId, method?, regions? })` — run the element's
  **current** pixels through the image-tools matte pipeline and **mint the cutout as a NEW
  element beside the source** in ONE journaled entry. **(T0336)** The original element and
  its pixels are **never touched** — "не ломать арт + легко сравнивать разные методы бок о
  бок", so the lead can A/B `key_matte` vs `corridorkey` vs `vitmatte` vs `birefnet` on the
  same art side by side; undo removes the copy and the original is byte-identical because it
  was never written. `method` is `"auto"` (the
  soft-score router picks `key_matte`, and **refuses** a wide soft zone that would need a
  dual-plate pair — a loud error, no silent single-plate fallback) or `"matte"` (force
  `key_matte`, the prod keyer). `regions`, when given, is a list of the element's **stored
  region ids**: the alpha is applied **only inside** those region masks (rect, or the
  polygon when present) and the rest of the element is left untouched; omitted = the whole
  element. `alpha_dualplate` (a white+black plate PAIR) is out of v1 scope on a single
  element and is a loud error. The new element is placed to the **right** of the source
  (16px gap), sized to the source's exact display box (a pixel-perfect side-by-side twin —
  the keyer output always equals the source's pixel dims), and named `"<source> · <method>"`.
  Records `element.meta.alpha` (method, params, `parentSrc`, `parentElementId`, routing
  metrics) like slice provenance, plus an `alpha_cutout` `tool_runs` row (its `elementId`
  stays the source). Requires Python (numpy + scipy +
  Pillow) via our own `tools/alpha_cutout.py`, which reuses the image-tools
  `route`/`route_cutout` + `alpha_matte`/`key_matte` modules **unmodified**. `elementIds`
  (2+ images), given INSTEAD of `elementId`, **batches** a multi-selection into ONE
  journaled entry (T0230): each element keys its own current pixels sequentially (same
  spec/pipeline as the single-element path), and only once EVERY element succeeds does the
  whole batch mint N new copies beside their sources in one commit — one undo removes **all**
  the copies and leaves every source byte-exact. If ANY element refuses, the whole batch
  throws with that element's message and nothing is mutated (atomic — no copy, no journal
  entry). `regions` is not accepted with a batch — a loud error; regions stay
  single-element. Return: `result.element` (single) / `result.elements` (batch) are the NEW
  copy element(s). Both clients: the page's inspector **Alpha cutout** control (single
  element or region scope) / multi-selection **Alpha** section ("Apply to N images") and
  the CLI `alpha --element`/`alpha --elements`.
- `alphaDualPlate({ projectId, elementIds: [a, b] })` — **(T0237)** closes the loop
  `alphaCutout` refuses: TWO image elements — the **same art** rendered on a white plate
  and a black plate, in **either order** (`tools/alpha_dualplate.py` auto-detects which is
  which by overall brightness) — key into **ONE NEW** content-addressed cut element in ONE
  journaled entry. Unlike `alphaCutout`, this **never touches** the two source elements
  (non-destructive; the lead deletes the plates himself once happy with the result); one
  undo removes the new element. The new element is named `"<first plate's name> alpha"`
  and placed at the **first plate's `x`/`y`**; `element.meta.alpha` records `method:
  "dual_plate"`, both parent `src`s, and the pair gate's own verdict/metrics, plus an
  `alpha_dualplate` `tool_runs` row. Requires **exactly 2** elementIds (a loud error
  otherwise) and both must be image elements. Refusals are loud and specific: the pair
  gate's own message travels through as a clean error (misaligned/redrawn plates,
  same-color plates — a `SystemExit`, never a Python traceback). See **Dual-plate alpha
  tool** below. Both clients: the multi-selection inspector's **Dual-plate cutout**
  button (shown only for an exact 2-image selection) and the CLI `alpha-dual --elements`.
- `alphaDualPlateGenerate({ projectId, elementId, prompt?, generator? })` — **(T0238, works
  from ANY art since T0248)** the AUTOMATIC dual-plate flow: an action on ONE existing image
  element ("сделай дуал-плейт альфу этому арту") instead of a 2-element selection.
  `check_flat_background.py` REPORTS whether the element's border is a flat light
  background (no refusal, T0248 — the lead correctly called the old loud refusal wrong on
  real art); flat -> the element's CURRENT pixels ARE the dual-plate LIGHT plate (the
  original one-codex-call path, unchanged); anything else -> the WHITE plate is generated
  FIRST, as a codex edit of the element's OWN pixels (`gen_dual_plate.sh`'s white-plate
  step, generated exactly once, no retry), stored content-addressed, and used as the light
  plate for the rest of the flow — exactly the chain the reference script runs. Either way
  the DARK plate is generated as a codex EDIT of the light plate (the exact subject-lock
  chain `gen_dual_plate.sh`'s black-plate step uses); the pair then runs through the SAME
  `alphaDualPlate` tool (role detection + T0243 align + the pair gate + extraction,
  unmodified — one engine for both the manual and automatic paths), with ONE automatic
  retry on a gate refusal (only for the dark plate — the white plate is never retried).
  `prompt` is optional extra subject text appended to BOTH the white-plate and black-plate
  subject-lock prompts; `generator` is the injectable `{inputPngPath, prompt} ->
  Buffer|path` GENERIC plate seam (defaults to `tools/dual_plate_generate.mjs`'s
  codex-backed `generatePlate` — tests inject a fake one so codex never runs in the suite);
  the SAME seam is called for both the white-plate and black-plate steps, ops.mjs decides
  which prompt/input goes on each call. On success, mints ONE new cut element named
  `"<source name> alpha"`, placed to the RIGHT of the source element's bbox (16px gap,
  mirroring `alphaDualPlate`'s own pair placement); its `element.meta.alpha` carries
  `method: "dual_plate"`, `plates: [{src, role:"light", generated}, {src, role:"dark",
  generated}]` (fixed roles; the additive `generated` flag, T0248, is `false` for the light
  plate only on the flat-light path — the dark plate always costs a codex call), the
  `prompt`, the pair gate's verdict, and the T0243 `align` delta. The source element is
  NEVER mutated (non-destructive, like the manual pair op); ONE journal entry covers the
  whole gesture (generation itself runs outside the journal), one undo removes just the new
  element. A refusal that survives the retry is loud and names the (possibly generated)
  light plate plus every preserved dark-plate attempt (`files/`, content-addressed) plus the
  manual fallback (place both plates on the canvas, run `alphaDualPlate`). See **Dual-plate
  alpha tool** below. Both clients: the inspector's per-plate **Add to canvas** buttons live
  alongside the Alpha section's plate thumbnails once a result exists (no generate button
  yet — the lead triggers this via agent/CLI) and the CLI `alpha-dual-generate --element`.
- `addImageFromFile({ projectId, src, name?, x?, y? })` — mint a normal journaled image
  element from an EXISTING project file `src` — no browser re-upload, no duplicate bytes
  (content-addressing already dedupes identical bytes to the same `files/` entry, so this
  is a plain disk read through the SAME `addImage` op every other add goes through: same
  journaling/front-order shape). Backs the inspector's per-plate **Add to canvas** button
  (T0238): promote a `alphaDualPlateGenerate` plate (light or dark) straight onto the
  canvas as its own element. Loud when `src` is missing, unsafe, or not found on disk. Both
  clients: the per-plate **Add to canvas** button and the CLI `add-image-from-file --src`.
- `setExportSettings({ projectId, elementId, rows })` — replace an element's
  Figma-style export rows `[{scale, format, quality?, resample, base?}]` (the Export
  section persisted on the layer). Validates + normalizes each row (scale token syntax,
  `format` png/jpg/webp, `resample` lanczos/nearest; `quality` 1-100 only for the lossy
  formats, default 90; `base` source/canvas, see below). **Suffix is removed (T0229)**: a
  NEW write carrying a `suffix` field is rejected **loudly** (a stale client), while the
  export readers **ignore** a legacy stored `suffix` (additive schema; journal untouched)
  and derive file names automatically. Journaled like `setRegions`, so undo/redo restore
  the previous rows. Both the page's Export section and the CLI `export-set` drive this
  one op.
  - **`base` (T0235, additive)**: `"canvas"` (the DEFAULT — omitted/absent; lead's
    same-day flip: "2x" means twice what you SEE) or `"source"`. Picks which dims a
    row's scale token resolves against at export time: `canvas` = the element's
    **current** on-canvas `w`/`h`, resolved fresh on every export (tracks a later
    resize — never frozen into the token); `source` = the element's original
    `source_w`/`source_h` (full-resolution export of a downscaled sprite). A stored
    row only ever carries `base:"source"`; `"canvas"`/absent is normalized away so the
    JSON stays minimal. Both the site's card-level **Source | Canvas** segmented control
    and the CLI `--base source|canvas` flag drive this field; an unknown value is a loud
    validation error, never a silent fallback.
- `exportElements({ projectId, elementIds, rows? })` — export each element ×
  each of its rows into `<project>/export/<utc-stamp>/` plus a `manifest.json`.
  Each row scales (`resolveExportScale`) against its `base` dims (source pixels, or the
  element's live on-canvas `w`/`h` for a `"canvas"` row — a loud error if that on-canvas
  size is missing/zero) + encodes (png/jpg/webp) via **one** Python spawn for the whole
  batch (`tools/export_images.py`, spec-file pattern). A 1x-png export of a png source is
  a **byte-identical file COPY** done in Node (no re-encode, no spawn) when the resolved
  pixels match the source file's actual dims, so the lead's original pixels are preserved
  exactly. **File naming is automatic (T0229)**: base = the element/screen name (slugged);
  a single row is the clean `name.<ext>` regardless of its export `base`; several rows get
  a Figma **scale marker** (`name@2x.png`, `name@0.5x.png`, `name@512w.png`; a canvas `1x`
  row stays clean/unmarked). A **source-base** row in a multi-row set always gets a marker
  tagged `-source` — even at `1x` (`name@1x-source.png`) — so it never collides with the
  unmarked canvas `1x` baseline name. Any remaining collision (same scale+format+base
  twice, or two elements sharing a name) gets a deterministic numeric `_NN`. When no
  `rows` are set on an element it exports the implicit default `1x png` (`base:"canvas"`).
  `rows`, when passed, overrides every element's settings for that one run (agent
  one-shots / the CLI `--scale`/`--base` flags). Not journaled (no project mutation),
  recorded in `tool_runs`.
- `zipExport({ projectId, stamp })` — bundle a finished export run's image files into ONE
  **STORE-mode** `.zip` (no compression — PNG/JPG/WebP are already compressed) via the
  minimal `zip.mjs` writer (node built-ins only). Reads the run's `manifest.json` to learn
  the produced file names; loud on a bad stamp / corrupt manifest / missing file. Backs the
  page's multi-output save dialog (served over `GET export-zip/<stamp>`) and the CLI
  `--zip` flag. No project mutation.
- `exportProject({ projectId })` — no-selection project export: composite every
  **`screen:true`-flagged, visible, TOP-LEVEL group** (`parentId` null/absent) at its own
  default 1x png into ONE `<project>/export/<utc-stamp>/` folder plus a combined manifest,
  reusing the `renderGroup` compositor. A nested group is a **component inside its root
  screen** (composited by the recursive painter), never a separate screen. T0332 B1
  (export opt-in inversion, 2026-07-07): a group is a screen ONLY when explicitly ticked —
  `group.screen` is absent by default (`patchGroup`/CLI `group-set --screen true|false`),
  so a freshly created top-level group does NOT export until flagged. There is no special
  recipe/style/pack_run skip any more; those groups simply never carry `screen` by
  construction. An existing (pre-flip) project migrates via
  `tools/migrate_screen_flags.mjs` (one-shot, dry-run by default; preserves that project's
  exact export set). Not journaled; records an `export_project` `tool_runs` entry. Errors
  clearly when there are no screen-flagged visible groups.
- `resolveExportScale(token, srcW, srcH)` / `parseScaleSpec(token)` — the scale-token
  parser (exported for reuse/tests): a multiplier (`0.5x`/`1x`/`2x`/`3x`/`4x` or a
  bare `2`) or a fixed target dimension (`512w` = 512px wide, `512h` = 512px tall;
  the other axis keeps aspect). Throws on anything else — an unknown scale is a clear
  validation error, never a silent fallback.
- `undoOp` / `redoOp` / `readHistory` / `listHistory` / `jumpHistory` — see Journal below.
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
- `patchGroup({projectId, groupId, name?, x?, y?, w?, h?, visible?, background?, clip?, screen?})` —
  when `x`/`y` change, the group's **full descendant closure** translates by the same
  delta — nested subgroup frames AND every element in the subtree — atomically (one
  journal entry; undo restores the frame and the whole closure). Resize (`w`/`h`) never
  moves members. `background` sets the optional solid fill (see below): `null` clears it,
  `{type:"color", color:"#rrggbb"}` sets it — validated (invalid = a loud error, no
  silent fallback). `None` on an already-unfilled group is a no-op. `clip` is the optional
  Figma-frame clip flag (see **Group clip** below): a real `true` clips the group's members
  to its bounds, `false` clears it (stored as an **absent** field, so `clip:false` on an
  already-unclipped group is a no-op); any non-boolean is a loud error. `screen` (T0332
  B1) is the **export opt-in flag**: `true` makes this top-level group an exportable
  screen (`exportProject`/the page's Export-project count key off `screen === true`
  alone); `false` clears it to an **absent** field, same convention as `clip` — a group is
  never a screen until explicitly ticked (CLI `group-set --screen true|false`).
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
- `scaleGroup({projectId, groupId, x, y, w, h})` — **(T0271)** scale the group's **full
  subtree** to a new frame: unlike `fitGroup`/`patchGroup` resize, **children move and
  resize with it**. The pure math (`tree.scaleGroupMoves`) maps the group's OWN frame
  **and** its full descendant closure (nested subgroup frames AND every element in the
  subtree) from the group's **current** frame to the given `{x,y,w,h}` by the same
  per-axis factors a multi-select block-scale uses — a group-subtree scale **is** a block
  scale, just anchored on the group's own frame. A **text** descendant never has its box
  stretched: only its `fontSize` scales (by the height ratio, same formula as
  `scaledFontSize`); every other node's box is remapped as-is, with a rotated element's
  `rotation` left untouched (mirrors the existing multi-select block-scale path — T0232
  never invented a subtree-rotation convention, and this doesn't either). All four
  `x/y/w/h` are required (this **is** the final frame, not a partial patch); a non-finite
  `x`/`y` or a non-positive `w`/`h` is a **loud error**, as is an unknown `groupId`. A
  same-frame call is a valid no-op (no journal entry). **One** journal entry covers the
  group AND every descendant; one undo restores them all exactly.
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
`POST /api/canvas/projects/<id>/groups/<gid>/scale {x,y,w,h}` (T0271: scale the full
subtree — server computes every descendant patch via `scaleGroup`, so the page never sends
them itself); `POST /api/canvas/projects/<id>/groups/<gid>/reparent {parentId|null,
index?}`; `POST .../groups {..., parentId?}` nests on create. The **Fit to content** button
in the inspector Position & Size section (disabled for a trivially-empty group) and the
group context-menu **Fit to content** item both call `fitGroup`. Page (Figma nesting): a
canvas drag moves a selected group's **whole subtree**; the layers panel drags a group onto
another group's header **middle** to nest (its own subtree is an inert target — a cycle
can't be dropped), a header **edge**/element row to reorder or reparent across scopes (the
insertion line's indent encodes the target scope); the group context menu's **Move to
group ▸** submenu lists nested targets indented.

**Scale handles (T0271).** Dragging a selected group's scale handle **scales its content by
default**: the group's own frame AND its full descendant closure move/resize with it (a
text descendant's font size scales instead of its box), committed via `scaleGroup`.
Holding **Ctrl/Cmd** during the drag switches to the **original T0232 frame-only** behavior
— only the group's own `w`/`h` change, children stay exactly where they were, committed via
the plain `PATCH .../groups/<gid> {w,h}` — read live per event like `Shift`(proportional)/
`Alt`(from-center), so toggling the modifier mid-drag flips modes on the very next frame.

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

### Recipe card

**(T0239 increments 1-2 — the card object + `recipe` meta + inspector surface, AND
generation end-to-end.)** A recipe card is a **group carrying an additive
`group.recipe` object**, not a new element type — the group primitive already gives it
a container, a framed render with a label, membership (drag an image in = a ref, via the
existing `assignToGroup`), move-as-a-whole, z-order, marquee, and copy/paste for free.
Design: `tmp/design_T0239_recipe_card_2026-07-03.md` (read the full doc INCLUDING
revisions R1-R3 — R1 replaces the original meta+clipboard style plan with an on-canvas
STYLE CARD component, R2 adds the codex|gemini engine choice, R3 adds "both" compare
mode).

`group.recipe` shape (a fresh card via `createRecipeCard` gets this default):

```jsonc
{
  "v": 1,
  "prompt": "",                 // the lead's base prompt; "" is the draft state
  "expanded": null,              // last Expand-prompt output — increment 3
  "use_expanded": true,          // increment 2: generate sends `expanded` when present+enabled, else `prompt`
  "engine": "codex",             // "codex" | "gemini" | "both" (R2/R3) — which generator(s) a run uses
  "params": {                    // bg_key/n_candidates/size/quality are PATCHABLE (T0332 v2); model stays immutable.
                                 // Engine usage: codex consumes size/quality/model; gemini/agy consumes ONLY size
                                 // (its own model, no quality knob — ENGINE_PARAMS_USED in tools/recipe_generate.mjs);
                                 // bg_key/n_candidates are canvas-level (cutout advisory / pack overgen count).
    "size": "1024x1024", "quality": "high", "model": "gpt-image-2",
    "bg_key": "#ff00ff", "n_candidates": 1
  },
  "style_ref": null,             // nullable by-id pointer to a STYLE CARD (group.style, minted via createStyleCard)
  "pack": null,                  // T0332 v2: null = single-image Generate (unchanged); set = pack mode, see Pack mode below
  "last_run": null                // set by generateFromRecipe; null = "draft", non-null = "done"/"partial"
}
```

`last_run` (increment 2), when set: `{at, result_element_id, verdict}` where `verdict` is
`"ok"` (every attempted engine landed) or `"partial"` (engine `"both"`, at least one engine
failed/skipped but the other landed — see **Generation** below).

Refs are **not** in `recipe` — they are the card's ordinary members (image elements with
`groupId === cardId`), discovered at generate time (increment 2). A card is a **workshop
object, not a screen**: `createRecipeCard` never sets `group.screen`, so it stays
unflagged by construction (T0332 B1) — `exportProject`/the page's Export-project count
key off `screen === true` alone, no recipe-aware special-casing needed.

- `createRecipeCard({projectId, name?, x?, y?, w?, h?, parentId?})` — mint a card: a
  group with a fresh default `recipe` blob. Bounds are optional (unlike `createGroup`, a
  card is never `fromElements`) — omitted `w`/`h` fall back to a 360x280 default frame
  (purely cosmetic; the frame never feeds generation — decision 4). Optional `parentId`
  nests it like any group. One journal entry.
- `patchRecipe({projectId, groupId, patch})` — a **partial**, validated update of the
  recipe blob. Increment 1 accepts `prompt` (a string; empty is a valid draft),
  `engine` (`"codex"|"gemini"|"both"`), and `style_ref` (`null` or a string id — the
  reserved R1 pointer; resolving it across canvases is increment 3, not this op).
  Anything else, or a group with **no** `recipe` at all (a plain group is not a card),
  is a **loud error** — no silent fallback. One journal entry; undo restores the prior
  recipe blob byte-exact.
- Copy/paste: `tree.buildNodesSpec` deep-clones the whole group record for its spec (only
  `id`/`parentId`/`order` are stripped), so `recipe` — and a future `style` blob —
  already survive `nodes-paste`/`nodes-duplicate`/`Ctrl+C/V/D` with **no schema-aware
  carve-out needed**. A pasted/duplicated card keeps its recipe verbatim under a fresh
  group id; `style_ref` is copied as-is (no remap — it stays `null` until increment 3
  gives it something to point at across canvases).
- Rendering: a card draws as a group frame that reads as a distinct "special container"
  at a glance (lead, live: "сейчас выглядит как группа") — a **dashed amber** frame
  stroke (`#d7a14a`, mirrors canvas.css's `--amber`) regardless of selection, a small
  **"Recipe"** tag chip in the same accent beside the name label, and a truncated
  (~40 char) prompt-preview line inside the frame. All pure chrome (`workspace.js`
  `drawGroupFrame`) — nothing interactive is added; the existing name-label pill stays
  the only click-selectable hit-area. Inspector: selecting a group with `recipe` shows an
  additive **Recipe** section (above **Render group**, same "presence of the field"
  pattern as **Group background**) with a Prompt textarea, an Engine select, and a
  **Generate** button (Style controls stay disabled — increment 3).
- HTTP: `POST /api/canvas/projects/<id>/recipe-cards {name?, x?,y?,w?,h?, parentId?}`;
  `PATCH /api/canvas/projects/<id>/recipe-cards/<gid>` — body **is** the recipe patch
  itself (`{prompt?, engine?, style_ref?}`), not wrapped; `POST
  /api/canvas/projects/<id>/recipe-cards/<gid>/generate {}` (increment 2 — see
  **Generation** below). CLI: `recipe-create <id> [--name X] [--x --y --w --h] [--parent
  <gid>|none]`; `recipe-set <id> --group g [--prompt "..."] [--engine codex|gemini|both]
  [--style <id>|none]`; `recipe-generate <id> --group g`. Page: the canvas context menu's
  **New recipe card** item (empty-canvas right-click; mints the card at the click point);
  the Recipe inspector's **Generate** button.

The STYLE CARD component and the Expand-prompt / Extract-style helpers are later
increments (3-4) of T0239 — see the design doc.

#### Generation (`generateFromRecipe`, T0239 increment 2)

`generateFromRecipe({projectId, groupId, generators?})` — the Recipe inspector's
**Generate** button / `recipe-generate` CLI verb / `POST .../recipe-cards/<gid>/generate`.
Validates loudly (in order): the group exists and carries `recipe`; the resolved prompt
(`use_expanded && expanded ? expanded : prompt`, trimmed) is non-empty; the card's member
IMAGE elements (`groupId === cardId`, visible only — decision 3's refs) number at most 5
(`generate_image.py`'s `--input-image` cap). `recipe.style_ref` is **not** resolved this
increment (reserved for increment 3's style cards) — the frame's `w`/`h` is never read
either way (decision 4).

Engine (`recipe.engine`, R2/R3):

- `"codex"` (default) — one generation via `tools/recipe_generate.mjs`'s
  `generateImageCodex`, which shells `generate_image.py` (the SAME script
  `dual_plate_generate.mjs` uses) with `--prompt`, one `--input-image` per ref,
  `--size`/`--quality`/`--model` from `recipe.params`.
- `"gemini"` — one generation via `generateImageGemini`, which shells the **agy**
  (Antigravity) CLI headless (skill Path B), verified by **output file existence, never
  stdout**. agy ref support is **unverified** (R2): a card with **any** ref on
  `engine="gemini"` **refuses loudly before any generation** — text-only gemini
  generation is fine.
- `"both"` (R3 compare mode) — runs BOTH engines and mints TWO result elements named
  `"<card name> codex"` / `"<card name> agy"`, each with its own frozen `meta.recipe`
  snapshot, in **ONE journal entry**. **Partial success is real success** here: one
  engine failing still lands the other (`result.failed` names what didn't, e.g.
  `[{engine, error}]`) and `recipe.last_run.verdict` becomes `"partial"`. A card with
  refs on `engine="both"` **skips** the gemini attempt (never calls it) and reports the
  skip in `failed` instead of refusing the whole run. Only when **every** attempted
  engine fails (including a plain single-engine run) does the op throw loudly, and
  nothing is written in that case — no journal entry, no card mutation.

Placement (R1) — the result(s) land in the card's **PARENT** scope
(`groupId = card.parentId ?? null`, **never** `groupId = cardId`, so a result can never
become a ref feeding a future run of the same card), positioned to the **right** of the
card frame (16px gap); a second result (`engine="both"`) stacks **below** the first
(16px gap).

Each minted element carries a frozen `element.meta.recipe` snapshot (no `element.meta.alpha`
— raw art, decision 5):

```jsonc
element.meta.recipe = {
  "cardId": "grp_…",
  "engine": "codex",                     // per-element engine, even under recipe.engine "both"
  "at": "2026-07-03T12:00:00.000Z",
  "prompt_snapshot": "a red fox riding a dragon",
  "refs_snapshot": ["files/<hash>.png", …],   // <=5, project-relative srcs
  "params_snapshot": { "size": "1024x1024", "quality": "high", "model": "gpt-image-2",
                        "bg_key": "#ff00ff" }
                       // ONLY what this element's engine actually consumed (+ bg_key, the
                       // canvas-level cutout advisory) — a gemini element records just
                       // {size, bg_key}, never a gpt-image model/quality it had no knob for
}
```

A `tool_runs` row (`op: "generate_from_recipe"`) records `cardId`, the sent prompt,
`refs`, the requested `engine`, the engine-filtered params (same law as `params_snapshot`:
a codex run records `size`/`quality`/`model` + `bg_key`, a gemini run just `size` +
`bg_key`; a `"both"` run records the codex superset in this ONE aggregate row — each
element's own `meta.recipe` is the per-engine exact record), and a `result_summary` of
`{results: [{engine, elementId, bytes}], failed: [{engine, error}]}`. The card's own `recipe.last_run` is set
in the SAME `commitMutation` as the mint(s) — one journal entry, one undo removes every
minted element AND reverts `last_run` together. Generation itself runs **outside** the
journal (a codex/agy spawn, minutes) — only the final mint commits, mirroring
`alphaDualPlateGenerate` (T0238).

The generator seam (`tools/recipe_generate.mjs`) is injectable per engine
(`generators: {codex?, gemini?}`, mirrors `alphaDualPlateGenerate`'s `generator` arg) —
tests inject fakes, so codex/agy **never** spawn in the suite. Pure argv/instruction
builders (`buildGenerateCommand`, `buildAgyInstruction`, `buildAgyCommand`) are exported
and tested directly, no spawn.

#### Pack mode (`recipe.pack`, T0332 v2)

An optional `pack` field on the SAME recipe blob turns a plain single-image card into a
multi-sheet **axis pack** — not a new card type. `pack: null` (default) keeps today's
single-image Generate unchanged; a non-null `pack` switches Generate to a sheet-generation
run instead. Everything else a pack needs already lives on the recipe: `prompt` is the
subject template, sent to the expander **verbatim** (pack mode never reads
`expanded`/`use_expanded` — a stale hand-edited expansion must never silently leak into an
axes-driven sheet); `style_ref` supplies `style_prefix` **and** the style card's ref image
through the SAME resolve the single-image branch uses (a style card's ref image reaches
every sheet by construction, no separate pack-only path); member images are refs for every
sheet. `engine` is `"codex"`, `"gemini"`, or `"both"` (lead decision 2026-07-07: cost is
the lead's call) — `"both"` fans **every job out to both engines**, 2× the paid calls,
minting two sheets per job named `"<job> codex"` / `"<job> agy"` (the single-image compare
convention). Each minted sheet records the engine that actually generated it in
`meta.pack.engine`, and **sheet identity everywhere is the `(sheet_axes, engine)` pair**:
resume (`--run`) skips a sheet only when ITS engine's version already landed, and a forced
`--sheet` regen replaces only its own engine's prior sheet. A sheet minted before engines
were recorded counts as codex (factually true — codex was the only pack engine then).
Deliberate consequence: flipping the card's engine and resuming `--run` generates the NEW
engine's versions beside the old ones — the cheap "get the agy versions side by side"
gesture, printed per sheet, never silent double-billing.

**agy (gemini) packs — draw-no-lines rule** (smoke-checked 2026-07-07, 3 sheets): agy
holds the grid layout and the flat key background well, but with the expander's default
constraints it reliably **draws thin separator lines between cells** ("no grid lines" in
the template was not enough — 2/2 sheets REJECTed at slice with `region_count` = cells+1,
the line cross detected as an extra region). Cure is prompt-side, in the CARD's own
prompt: state that objects float on one continuous flat background and forbid dividing
lines/separators/borders explicitly (e.g. "…objects float on the continuous flat
background — ABSOLUTELY NO dividing lines, separators, borders or frames between cells,
the background color runs unbroken across the whole image") — with that line the smoke
sheet passed the slice gate 4/4 on the first try. codex has not needed this.

```jsonc
"pack": {
  "v": 1,
  "axes": { "grade": ["common", "rare", "epic"] },  // axisName -> non-empty string[]; key order preserved
  "vary": "grade",                                   // must be a key of axes — the per-cell axis
  "grid": [3, 3],                                     // [rows, cols], each 1..3
  "max_jobs": 12                                      // loud cap — expand_jobs.py's own law
}
```

`patchRecipe({patch:{pack}})` **replaces `recipe.pack` wholesale** — unlike every other
recipe field (including `params`, see below), this is not a merge: `pack: null` turns pack
mode off, a non-null value must carry all four fields. A caller that wants to change ONE
field must read the CURRENT pack first and send the full object back; `recipe-set`'s pack
flags (below) do this read-modify-write for you — a raw `patchRecipe` call does not.

`params` (`bg_key`/`n_candidates`/`size`/`quality`) is **patchable**, not advisory or
write-once-at-creation: `patchRecipe({patch:{params}})` merges a partial object one level
deep onto the stored `recipe.params` — the opposite convention from `pack` above, a
genuine partial patch. `model` stays immutable (a loud error, even set to its current
value; `supersample` was removed 2026-07-07 as dead — nothing ever consumed it, legacy
blobs carrying it are harmless and patching it stays a loud unknown-key error). Pack mode derives two of its own config fields from `params`, not
from `pack` itself: `bg_key` must be **exactly** `#ff00ff` (magenta) or `#00ff00` (green) —
any other hex is a loud error at preview/generate time, not patch-time (`params.bg_key`
itself stays generic hex, since it also serves the single-image cutout path); `n_candidates`
becomes the expander's `candidates` (overgen per sheet).

Three CLI verbs (mirror `ops.mjs`'s `packPreview` / `generateFromRecipe`'s pack branch /
`packSlice`):

- `recipe-pack-preview <id> --group g` — **ephemeral**: not journaled, writes nothing to
  the blob. Assembles the config and runs the REAL `expand_jobs.py` expander, printing the
  sheet count, a `style_ref_image` flag, and every sheet's prompt. **Always run this before
  generate** — it is the only honest preview of a cell's prompt (single-image Generate
  builds its prompt a different way).
- `recipe-pack-generate <id> --group g [--run <runGroupId>] [--sheet <slug>]` — the pack
  branch of `generateFromRecipe`: real engine spawns (codex or agy, per the card's own
  `recipe.engine`), one PER SHEET, sequential (N ×
  30-60s+ — pass `timeout=max`, not a short default). Each finished sheet mints under its
  own short commit as soon as it lands, so a crash on sheet 3 never loses sheets 1-2.
  `--run <runGroupId>` resumes into an existing pack run group: sheets whose `sheet_axes`
  are already represented there are **skipped** (gen_batch's skip-if-exists parity).
  `--sheet <slug>` force-regenerates exactly ONE sheet (by the expander's own job name)
  into that same group even if already present — "one bad candidate out of 21 doesn't
  repay for the whole pack".
- `recipe-pack-slice <id> --group g [--run <runGroupId>]` — slice EVERY sheet of a run:
  per sheet, `detectRegions` then a **hard gate** `region_count === cells.length` (a
  mismatch REJECTs only that one sheet; siblings still slice), then `sliceRegions`
  reparented into the run group. Never throws mid-run — returns a per-sheet contract
  `[{sheet_element_id, verdict: "OK"|"REJECT"|"MISSING", region_count, cells_len,
  cut_ids}]` (`REJECT` always names got/expected via `region_count`/`cells_len`; `MISSING`
  = detection itself couldn't run — unreadable/rotated/deleted sheet). Omitted `--run`
  resolves `recipe.last_run.run_group_id`.

**Run groups.** The first successful sheet of a fresh run mints a plain group named
`"<style card name | 'no-style'>/<vary> <ts>"` beside the card; later sheets land inside
it. The group carries a `pack_run = {v:1, cardId, at}` marker — **provenance only**
(resolves `--run`, gates the Slice verb) — it has no export role. `renderGroup` still
works on it like any group; to have it show up in `exportProject`/the page's Export
project you must explicitly tick `group.screen` (`group-set --screen true`, see **Groups
= screens** above) — a pack run group is never auto-flagged as a screen.

**Sheet = provenance anchor — do not delete sheets before cuts are promoted.** A sheet
element carries the FULL manifest, `meta.pack = {cardId, engine, job, at, sheet_axes, cells,
prompt_snapshot, refs_snapshot, params_snapshot, style_snapshot?}` (`params_snapshot` is
engine-filtered, same law as `meta.recipe`'s: a gemini sheet records `{size, bg_key,
n_candidates}`, never a gpt-image model/quality; `job` is the expander's bare job name —
the stable `--sheet` key even when the display name carries a `"both"` run's codex/agy
suffix, and the UI Regenerate button uses it, so renaming a sheet no longer breaks regen
for sheets minted after 2026-07-07); a cut's own `meta.pack`
after `recipe-pack-slice` is deliberately **minimal**, `{cardId, sheet_element_id, cell,
axes}` — just its own cell plus a pointer back to the sheet. The prompt/refs/params/style
for every cut on a sheet live ONLY on that sheet (duplicating them onto each cut would
balloon a 21-cut pack into 100+KB of repeated JSON) — deleting a sheet orphans its cuts'
provenance. Do **not rename** a sheet element before regen/slice either: a forced `--sheet`
regen identifies its target by matching the expander's own job name against the sheet
verbatim, so a renamed sheet silently stops being reachable by `--sheet`. **Regenerate
REPLACES**, it does not duplicate: a forced `--sheet` regen removes the OLD sheet AND its
own in-run slice subgroup (if `recipe-pack-slice` already cut it) from the run group, in the
SAME commit as minting the fresh sheet — any cut already promoted/copied OUTSIDE the run
group is untouched.

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

### Alpha cutout tool

`alphaCutout` brings the image-tools matte pipeline onto the canvas as a first-class
per-element op ("готовый арт сразу в альфу" — alpha is not a region workflow; regions are
optional refinement). Like slice, it uses our OWN Python tool `tools/alpha_cutout.py`
(spawned once through the shared warm worker), but that tool **reuses the image-tools
alpha modules verbatim** — `route/route_cutout` (the soft-score router) for the border key
+ the `key_matte` vs `dual_plate` decision, and `alpha_matte/key_matte` (the prod keyer)
for the keying — so there is **no matte logic duplicated** in node or in a second Python
implementation, and a missing module is a loud import error.

- **Method.** `"auto"` routes the source: opaque/flat-key art keys with `key_matte`; a WIDE
  soft/semi-transparent zone (glow, glass, soft shadow) routes to `dual_plate`, which needs
  a white+black plate PAIR — a single element has one image, so that is a **loud error**
  (no silent single-plate fallback). `"matte"` forces `key_matte` regardless of the routing
  recommendation. `"corridorkey"` (T0261) is the third, EXPLICIT method — see below.
  `dual_plate`/generation is out of v1 scope; a future "generate a
  dual-plate pair" op is where a pair source could come from.
- **`"corridorkey"` — neural GREEN/MAGENTA-screen matte (T0261, magenta shim + regions
  T0262).** The third path, for chroma-key **soft glow / translucent / soft-edge** art — the
  one case `key_matte` block-quantizes and the single-image `dual_plate` route can't cut. It
  reuses the video Track-B invocation **`runCorridorKey()` verbatim**
  (`ai_studio/assets/tools/video/matte/matte.mjs`, imported cross-module) as the ONE source of
  truth for prep → `corridorkey_cli` → EXR→RGBA; ops.mjs adds the canvas seam (key gate, the
  magenta hue180 shim, 1-frame staging, region composite, new-element mint, provenance). It is:
  **explicit-only** (the auto router NEVER yields it), **green-native + magenta-via-shim** (the
  element's border key is estimated via `route_cutout`; a key that is neither is a **loud
  refusal** naming the key and pointing at `key_matte`/`alphaDualPlate` — no silent fringe),
  **region-aware** (CorridorKey itself is always whole-frame; a region-scoped request runs it
  once on the whole element and composites the result into the requested regions — see below),
  and **slow** (~13-16s cold GPU model load per call — it is never a silent default; the page
  shows the long-op busy toast). Provenance (`element.meta.alpha`) records
  `{method:"corridorkey", tool:"corridorkey", key_color, screen_color:"green", shim:"hue180"?,
  commit, license, timings:{wall_seconds, per_frame_seconds}}` (`shim` only present when the
  magenta path ran). Licence: **CC-BY-NC-SA-4.0** (asset-processing carve-out). The CK inference
  is an **injectable seam** on the op (`corridorKey` — the `generator` precedent) so tests fake
  the ~15s GPU run; a live GPU smoke lives at `tests/live/ck_smoke.mjs` (out of the suite, both
  the native-green and magenta-shim paths). Choose it deliberately; `key_matte` stays the
  default for crisp opaque sprites AND the recommended path for FLAT magenta art (it beats CK
  there outright — exact color, ~200ms).
  - **Magenta via hue180 shim (T0262).** No magenta CorridorKey checkpoint exists (only
    green/blue), so the canvas fools the shipped GREEN checkpoint instead: rotate the input's
    hue +180° (magenta 300° → green 120°, value-preserving HSV rotation — S/V untouched, so
    dark subjects survive), stage that as the frame, run the native green path unchanged, then
    rotate the reconstructed FG's RGB back −180° (its own inverse mod 360); the alpha channel is
    copied through byte-exact, never touched by the color shim. Measured a **strict upgrade**
    over the blue-on-magenta path on the research's hard-edge fixtures (subject dE76 2.7 → 2.0,
    rim contamination → 0%; `tmp/research_corridorkey_magenta_2026-07-05.md`, runner precedent
    `static_eval/trick_run.py` in the since-deleted video-gen experiment folder) — **UNTESTED on soft/glow
    magenta** so far (only hard-edge fixtures were measured); `key_matte` remains the
    recommended default for flat magenta. The HSV math is ported into
    `tools/ck_pixel_ops.py` (`hue_shift_image` — pure numpy, this repo's venv has no cv2).
  - **Region-scoped CorridorKey (T0262).** CorridorKey has no per-region neural pass, so a
    region-scoped request runs the whole-frame CK cutout ONCE (with the magenta shim first, if
    applicable) and then pastes that result into the requested regions of a copy of the
    ORIGINAL source — everywhere else keeps the original opaque pixels. The composite step
    (`tools/ck_pixel_ops.py`'s `compose_regions`) reuses `alpha_cutout.py`'s
    `region_mask`/`clamp_rect` **verbatim** (imported, not duplicated), so it is the exact
    mask/paste contract `key_matte`'s own region path already uses.
- **`"vitmatte"` — neural ViTMatte thin-detail matte (T0335, alpha bench 2026-07-07).** The
  fourth path, EXPLICIT-only, for **thin/fine detail** (spider-web, mesh, fur, hair strands) on a
  flat **GREEN/MAGENTA** key — ~2× more accurate than CorridorKey on strand-level structure — and
  the **second-priority glow** keyer (CorridorKey is first on glow: it despills natively; ViTMatte
  wins raw alpha but leaves more residual tint, the lead's glow-wings ruling). Like corridorkey it
  has a **key gate** (border key via `route_cutout`; a key that is neither green nor magenta is a
  **loud refusal** pointing at `key_matte`/`birefnet`/`alphaDualPlate`) and **no auto-router entry**.
  Two hard rules from the tool contract: it runs in its **OWN GPU-torch venv**
  (`ai_studio/assets/tools/image/vitmatte_matte/.venv`, cu128 torch must not enter the shared repo
  venv — a missing venv is a loud error naming `node …/vitmatte_matte/setup_python.mjs`, never a
  fallback), and it is **whole-element only in v1** (a region-scoped request is a loud refusal — use
  corridorkey for region-scoped neural keying). ~1-3s GPU. The trimap is auto-built from chroma
  distance to the key (`T1=70`/`T2=150`, tuned once on magenta, frozen); despill is on by default.
  Provenance records `{method:"vitmatte", tool:"vitmatte", model, key_color, despill, license,
  timings:{seconds}}`. **License: ALLOW-WITH-CONDITIONS** — code MIT, weights **local-only** (the
  Composition-1k / Adobe-DIM noncommercial caveat; final commercial call is the lead's, T0335 gate).
  Injectable seam `vitmatte`; see `ai_studio/assets/tools/image/vitmatte_matte/README.md`.
- **`"birefnet"` — neural BiRefNet-general cutout on any background (T0335, alpha bench 2026-07-07).**
  The fifth path, EXPLICIT-only, for a **ready image on an arbitrary/unknown background** with **no
  chroma key at all** — its niche is exactly where `route_cutout` finds no flat key, so it has **NO
  key gate** (the key detector is never called) and **no auto-router entry**. Runs in the **shared
  repo venv** (studio.config `pythonPath`) through the same warm-worker bridge every other canvas
  python tool uses, **CPU onnxruntime ~10-30s** (plus a one-time ~930 MB model download). **Whole-
  element only in v1** (a region-scoped request is a loud refusal). Weak on flat monochrome line-art
  (a documented routing nuance — its SOD training distribution is photographic, not vector art — not
  a bug). Provenance records `{method:"birefnet", tool:"birefnet", model, license, timings:{seconds}}`
  (no `key_color` — there is no key). **License: MIT** (rembg + BiRefNet-general code AND weights;
  `briaai/RMBG-2.0` is FORBIDDEN — non-commercial weights — enforced in the tool). Injectable seam
  `birefnet`; see `ai_studio/assets/tools/image/birefnet_cutout/README.md`.
- **Regions optional.** Without regions the whole element is keyed. With region ids, the
  op keys **only inside** each region's mask (rect, or the polygon when present) and pastes
  each keyed crop back over the **untouched original opaque pixels** — the region-mask
  composition happens IN Python, in one worker call, never split across node.
- **Always a NEW element beside the source (T0336).** The cutout is written as a new
  content-addressed file and minted as a **new element** placed to the right of the source
  (16px gap, mirroring the dual-plate tools), named `"<source> · <method>"`, sized to the
  source's exact display box (the keyer output always equals the source's pixel dims, so it
  is a pixel-perfect side-by-side twin). The original element and its pixels are **never
  touched** — "не ломать арт + легко сравнивать разные методы бок о бок" (A/B the four keyers
  on the same art side by side). `element.meta.alpha` on the copy records the run plus
  `parentSrc` + `parentElementId`. One undo removes the copy; the source is byte-identical
  because it was never written.
- **Batch (T0230).** A multi-selection of 2+ image elements keys as ONE operation: each
  element runs through its own worker spawn sequentially (whole-element only — regions stay
  single-element, so a batch call never accepts `regions`), and only after EVERY element
  keys successfully does the op commit ONE journal entry minting a new copy beside each
  source + writing every copy's `meta.alpha`. A refusal on any element (dual-plate guard,
  non-image, tool error) rejects the whole batch with that element's message and mutates
  nothing — no copy. One undo removes ALL the copies and leaves every source byte-exact.

### Dual-plate alpha tool

`alphaDualPlate` (T0237) closes the loop `alphaCutout` refuses ("dual_plate... is out of
v1 scope on a single element"): TWO selected image elements — the SAME art rendered once
on a flat **white** plate and once on a flat **black** plate — key into ONE brand-new cut
element. Like `alphaCutout`, it uses our OWN Python tool `tools/alpha_dualplate.py`
(spawned once through the shared warm worker), but that tool **reuses the image-tools
dual-plate modules verbatim** — `dual_plate_pair_gate.evaluate` (the pair-consistency
gate) and `dual_plate_alpha.extract_dual_plate_alpha`/`build_report` (the Smith & Blinn
1996 projection extractor) — so there is **no matte logic duplicated** in node or in a
second Python implementation.

- **Plate roles, auto-detected.** The two elements arrive as an unordered pair — nothing
  on the canvas tags "this one is the white plate". The tool picks by comparing each
  plate's **overall mean brightness** (the background fills most of the frame, so a
  white-bg plate reads far brighter than a black-bg plate); a tie (both plates the same
  brightness) is a loud, specific refusal. This is a thin ordering step, not a second
  consistency implementation — the reused gate/extractor never change.
- **The pair gate decides refusal.** `dual_plate_pair_gate.evaluate`'s own verdict gates
  extraction: `pass`/`align` (plates agree) proceeds; `regenerate` (the subject was
  redrawn or shifted between plates — ghosted alpha) is a **loud refusal**, surfaced as the
  gate's own message (`SystemExit`, no Python traceback) — nothing is written. A
  post-extraction sanity check (`build_report`, also reused) catches a degenerate result
  (no visible alpha pixels) even when the pair gate passed.
- **Non-destructive, always a NEW element.** Like `alphaCutout` (T0336), this never
  touches the two plate elements — they stay on the canvas exactly as they were; the lead
  deletes them himself once happy with the result. The new element is minted the same way
  every other add is (`storeAddImage` — same id/type/src/x/y/w/h/meta shape as
  `addImage`/`addImages`, no hand-rolled element), named `"<first plate's name> alpha"`,
  placed at the **first plate's `x`/`y`**, sized to the extraction output (equals the plate
  size). `element.meta.alpha` records `method: "dual_plate"`, both parent `src`s, and the
  pair gate's verdict/metrics, plus an `alpha_dualplate` `tool_runs` row. ONE journal
  entry; one undo removes the new element byte-exact — the plates are never part of the
  undo (they were never mutated).
- Both clients: the multi-selection inspector's **Alpha** section grows a **Dual-plate
  cutout** button under the batch "Apply to N images" row, shown only when the selection is
  **exactly 2 images**; the CLI `alpha-dual --elements a,b`.

### Automatic dual-plate generation

`alphaDualPlateGenerate` (T0238, lead: "Генерировать пару, проверять, и делать"; **works
from ANY art since T0248**) is an action on **ONE** existing image element instead of a
2-element selection — it generates the missing plate(s) rather than asking the lead to
build them by hand:

- **T0248 fix: this now works from ANY art, not only art already on a flat light
  background.** T0238 wrongly collapsed the reference script's white-plate step, treating
  the element's CURRENT pixels as the light plate outright and loudly REFUSING anything
  else — the lead correctly called that wrong on real art.
  `.codex/skills/nt-asset-image-generation/scripts/gen_dual_plate.sh` never assumes a flat
  background either: it GENERATES the white plate from arbitrary source art first, then
  generates the black plate as an edit of THAT white plate. This op now runs the same
  chain: `tools/check_flat_background.py` (reusing the `pair_align` border-median idea —
  the outer few px on every edge) is **REPORT-only** — it always writes `{flat_light,
  median_luma, spread, ...}` and exits 0, no refusal. The op reads `flat_light` and
  ROUTES:
  - **`flat_light: true`** — the element's CURRENT pixels ARE the light plate (no separate
    generation step; the original T0238 one-codex-call path, unchanged). This holds
    regardless of how the art got there — including a generation placeholder's raw output,
    which per the T0239 reframe carries no alpha of its own.
  - **`flat_light: false`** — the WHITE plate is generated FIRST, as a codex edit of the
    ELEMENT's own (arbitrary) pixels, using `gen_dual_plate.sh`'s white-plate prompt
    (`buildWhitePlatePrompt` — copied verbatim: the same background-swap-to-white +
    subject-lock clauses as the black prompt, just recoloured to white). Generated exactly
    **once** (never retried — only the dark plate gets the automatic retry) and stored
    content-addressed immediately, so it survives even if everything downstream fails. It
    then becomes the light plate for the rest of the flow.
- **The dark plate is generated as a codex EDIT of the light plate** (the element's own
  pixels, or the freshly generated white plate above), via the SAME subject-lock chain
  `gen_dual_plate.sh`'s black-plate step uses ("a BACKGROUND-COLOR SWAP only, not a redraw"
  + the pixel-identity lock clause) — built and invoked from ONE place,
  `tools/dual_plate_generate.mjs`, so the prompt text and the codex invocation never drift
  into a second copy. `prompt` (optional) is extra subject text APPENDED after the locked
  clauses, on BOTH the white-plate and black-plate prompts. The generator is a GENERIC
  injectable seam (`generator?` on the op; `{inputPngPath, prompt} -> Buffer|path`) reused
  for BOTH steps — ops.mjs decides which prompt/input to send on each call; tests inject a
  fake one, so codex never runs in the suite; the default (`generatePlate`) spawns
  `nt-asset-image-generation`'s `generate_image.py` exactly as the shell chain does, for
  either step.
- **The pair runs through the SAME `alphaDualPlate` tool** (`tools/alpha_dualplate.py` —
  role detection, T0243 translation-align, the pair gate, extraction — completely
  unmodified: ONE engine backs both the manual pair op and this automatic flow). On a gate
  `regenerate` verdict, the op retries **exactly once** — the DARK plate only (a fresh dark
  plate, same prompt; the white plate, if generated, is never re-generated). If it still
  fails, the error is loud and names EVERY preserved dark-plate attempt plus the light
  plate (each stored content-addressed in `files/` regardless of outcome, so nothing
  generated is ever lost — this includes a GENERATED white plate on the non-flat path) and
  the manual fallback path (place both plates on the canvas, run the `alphaDualPlate` pair
  op yourself).
- **Non-destructive, always a NEW element** — the source element is never mutated (mirrors
  the manual pair op's own stance, extended to a generated-not-selected second plate; a
  generated white plate is a stored FILE, never a canvas element). The new element is named
  `"<source name> alpha"`, placed to the **RIGHT of the source element's bbox** (16px gap —
  mirrors `alphaDualPlate`'s own plate-pair placement). `element.meta.alpha` carries
  `method: "dual_plate"`, `plates: [{src, role:"light", generated}, {src, role:"dark",
  generated}]` (FIXED roles — we already know which is which, unlike the manual op's
  unordered 2-element input; the additive `generated` flag, **T0248**, records which plates
  cost a codex call — `false` for the light plate only on the flat-light path, always
  `true` for the dark plate), the `prompt`, the pair gate's verdict, and the T0243 `align`
  delta, plus an `alpha_dualplate_generate` `tool_runs` row. The whole gesture (generation
  + gating + retry) runs OUTSIDE the journal; only the final mint commits, so ONE journal
  entry covers everything and one undo removes just the new element.
- Both clients: the inspector's Alpha section shows the two plate thumbnails (served over
  the existing project-files route) with role labels and a per-plate **Add to canvas**
  button once `element.meta.alpha.plates` exists (`addImageFromFile` — mints a normal
  journaled element from that plate's STORED file, no re-upload); the CLI
  `alpha-dual-generate --element <eid> [--prompt "..."]`. There is no UI **generate**
  button yet (the lead triggers generation via agent/CLI today) — the API route
  (`POST /alpha-dual-generate`) exists for parity and a future button.

## Text elements

A **text element** is a Figma-style text node in the flat model — `type: "text"` in
`elements[]` beside images, so z-order, grouping/nesting, undo, marquee, and the
inspector all treat it like any element. It carries a `content` string (explicit `\n`
newlines) and a `style` block:

```json
{ "fontFamily": "Inter", "fontWeight": 400, "fontStyle": "normal", "fontSize": 24,
  "lineHeight": 1.2, "align": "left", "color": "#ffffff",
  "stroke": { "width": 2, "color": "#000000" }, "shadow": null, "autoResize": "width" }
```

`shadow`, when set, is a HARD offset `{ dx, dy, blur, color }` (blur is stored but always
0 in v1). The defaults are `content` "Text", Inter 400, size 24, lineHeight 1.2
(unitless), align left, color `#ffffff`, stroke width 2 `#000000` (white text on a black
outline reads on typical art), shadow off.

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

## Note elements

**(T0268.)** A **note** is a Miro/FigJam-style sticky card — `type: "note"` in the flat
`elements[]` beside images and text, so z-order, grouping/nesting, undo, marquee,
copy/paste, and the inspector treat it like any element. Notes exist so the lead can write
lots of plain text onto a big board and stay oriented. It carries a `content` string
(explicit `\n` respected), a `style` block (the font **subset** — no stroke/shadow/italic),
and an optional `background` fill:

```json
{ "type": "note", "x": 40, "y": 50, "w": 220, "h": 180,
  "content": "Todo\nbuy milk",
  "style": { "fontFamily": "Inter", "fontWeight": 400, "fontSize": 18,
             "lineHeight": 1.35, "align": "left", "color": "#1a1a1a" },
  "background": { "type": "color", "color": "#fff9b1" } }
```

Defaults: `content` empty, a **220×180** box, Inter 400 / size 18 / lineHeight 1.35 / align
left / color `#1a1a1a`, and the yellow sticky preset as `background`. `background` is the
same additive `{type:"color", color:"#rrggbb"}` shape as `group.background` (a sticky preset
or arbitrary `#rrggbb`; `null` = no fill = absent field), validated **loudly** on write and a
loud error on a non-note element (`background` is note-only). Unknown font family/weight,
bad align/color, or a non-finite size are the same **loud** manifest errors text uses.

**Differences from text (deliberate):**

- **Fully-fixed box.** Both `w` **and** `h` are user-set and resizable (canvas handles +
  the inspector's editable Position & Size). Text is auto-width; a note never auto-sizes.
- **Word-wrap + clip.** The text is greedy word-wrapped (`ctx.measureText`) to the padded
  inner width and **clipped** at the box; overflow shows a bottom **fade** indicator (more
  text is there — double-click to read/edit it all). Wrap is a **browser-display concern
  only** (`site/fonts.js` `wrapNoteLines`, cached per element on content + inner width +
  font): because the box is user-fixed **and notes never reach a PNG**, there is **no PIL
  wrap and no nominal-box math** anywhere.
- **Never render content.** A note is a **work annotation**. `renderGroup` /
  `exportProject` **prune** every `type:"note"` node before the render spec is written
  (`buildRenderNodes`), so `render_group.py` never sees it — a group with a note renders
  **pixel-identical** to the same group without it (proven in `tests/note.test.mjs`).
  `exportElements` on a note **refuses loudly** (same spirit as standalone text). Group
  labels/chrome and recipe cards already never reach a PNG either.

**Fonts** reuse the same shared parity contract as text (`fonts.mjs` — `DEFAULT_NOTE_STYLE`,
`mergeNoteStyle`, the sticky presets, `NOTE_PADDING`); the note simply uses the font subset.

**Ops / parity.** `addNote` mints one (front-order hook + one journaled entry, mirrors
`addText`); `patchElement` carries `content`/`style`/`background` (content/style valid on a
text **or** a note; background note-only). HTTP: `POST /projects/<id>/note`; patches ride the
existing element `PATCH` route. CLI: `add-note` (`--w/--h/--content/--style-json/--background`)
and `element-set --background '#rrggbb'|none`. Undo restores a note byte-exact (content,
style, background, box).

**Page.** The **N** tool in the rail places a note at the click point (then switches back to
Select and opens the inline editor); the empty-canvas context menu's **New note** does the
same at the click point. Double-click a note to edit it in the **same** textarea overlay as
text, but `wrap=soft` over the note's fixed box (no live auto-width) — commit on blur or
`Ctrl/Cmd+Enter`, `Esc` cancels, **one** `patchElement` per commit (never per keystroke).
Resize handles change the box (`w`/`h`, free on both axes) and the text re-wraps. The
inspector shows editable **Position & Size**, a **Background** section (preset swatch row +
custom color + None), and a **Text** section (font/weight/size/line/align/fill). The layers
row shows a **"▤"** glyph placeholder plus the same content preview text rows use.

## Rotation & flip

**(T0232 increment 3a — data model, render, export parity; no interactive gizmo yet.)**
`element.rotation` (finite degrees; absent/`0` = unrotated) and `element.flipH`/
`element.flipV` (booleans; stored **absent** when `false`, like `group.clip`) are
**additive whitelisted fields** on `patchElement`/`patchElements` — no new op. `rotation`
is validated + normalized to `[0,360)` (a non-finite value is a loud error); `flipH`/
`flipV` must be real booleans and are **image-only** (a loud error on a `type:"text"`
element — mirroring pixels makes no sense for a text box). `rotation` itself **is**
allowed on a text element ("rotates the box"), but **glyph pixel rotation is not yet
applied** by either renderer in this increment — deferred to avoid touching the live
auto-width measurement path; only **image** elements get real pixel rotation/flip.

**The parity contract (load-bearing — both renderers MUST agree byte-for-byte on this):**
the canvas is Y-down image space. `rotation` = **degrees clockwise on screen**, about the
element's own box **center** `(x+w/2, y+h/2)`. Composition order is **resize → flip →
rotate → paste centered** (flip is innermost, applied to the pixels first).

- **Canvas** (`site/workspace.js` `paintElement`): translates to the element's screen-
  space center, applies `ctx.rotate(+theta)` (CW-positive on a Y-down canvas) then
  `ctx.scale(flipH?-1:1, flipV?-1:1)`, translates back, then draws the image at its
  ORIGINAL (unrotated) screen box — algebraically identical to drawing in a box centered
  at the origin, since pan/zoom never rotate (screen space and world space differ only by
  a similarity transform).
- **PIL** (`tools/render_group.py` `paint_element`): resizes to the display box, flips
  (`Image.transpose`), then `image.rotate(-rotation, resample=BICUBIC, expand=True)` — PIL's
  own angle is **CCW-positive**, so the sign is negated to match the canvas's CW-positive
  convention. `expand=True` keeps the rotated image's own center at its pre-rotation
  center, so pasting it centered on the element's box center reproduces "rotate about the
  box center" exactly.
- **PIL is the single source of rendered truth**; the canvas is a faithful same-transform
  approximation (~1px edge-AA drift from differing resample kernels is the declared
  acceptable gap — same stance as text). Geometry (center, angle, size) is exact by
  construction on both sides.
- `ops.buildRenderNodes` forwards `rotation`/`flipH`/`flipV` verbatim on an image paint
  node (and `rotation` alone, unread by `paint_text`, on a text paint node); a `clip:true`
  group crops the already-composited rotated pixels with no extra work (rotation/flip
  happen before the paste, same as any other element).
- `ops.elementsBBox` (createGroup `fromElements` padding, `fitGroup`) uses the ROTATED
  footprint (`tree.nodeAABB`/`tree.rotatedCorners`) so a rotated child is never clipped by
  a freshly-sized or fitted group frame.

**Region ops refuse loudly on a transformed element (R7).** `detectRegions`,
`sliceRegions`, and `alphaCutout` (single AND batch — the WHOLE batch refuses before any
element is spawned, atomic) all read the element's **untransformed source pixels**, so
they throw `element <id> is rotated/flipped — reset rotation/flip to edit regions or
slice (the source is untransformed)` while `rotation !== 0 || flipH || flipV`
(`tree.isNodeTransformed`). The page mirrors the same refusal: the inspector grays out
Detect/Slice/Alpha with the reason, the canvas double-click region-edit entry is blocked
(a status toast explains why), and the element context menu's "Edit regions" item is
disabled with the same reason as its title. `setRegions` (hand-editing already-detected
regions) is **not** guarded — only the four source-pixel entry points are.

**Hit-test / selection stay AABB in this increment.** Every consumer other than
`elementsBBox` (marquee, `hitElement`/`pointInElement`, the selection outline stroke) is
**unchanged** — still the plain unrotated `x/y/w/h` box. Rotation-aware hit-testing, the
rotated selection quad, and the interactive rotate handle are **increment 3b** (the one
gizmo mode); this increment intentionally ships the parity-hazardous render/export math
FIRST, proven by a headless test, before any interactive code depends on it.

**Both clients (strict tool parity):** the page's inspector **Rotation** number input +
**Flip H**/**Flip V** buttons (Position & Size section) and the element context menu's
"Flip horizontal"/"Flip vertical" all commit through `patchElementBox` → the SAME
`patchElement` fields the agent sets via `element-set --rotation <deg> --flip-h
true|false --flip-v true|false` (or `elements-set`'s batched JSON patches) — whatever the
page can set, the CLI/API can set identically.

## Element opacity

**(T0260.)** `element.opacity` — a finite number in `[0,1]`, absent = `1` (fully opaque),
stored only when `!= 1`. An **additive whitelisted field** on `patchElement`/
`patchElements` (same "absent means the default" convention as `rotation`/`flipH`).
Applies to **image** elements in both renderers today (a translucent **text** element
renders opaque in both — out of scope). The canvas multiplies it into `ctx.globalAlpha`
before the rotate/flip/slice9 draw (`site/workspace.js` `paintElement`);
`render_group.py`'s `paint_element` scales the pasted layer's alpha channel by the same
factor right before compositing. Both clients: the inspector's **Filters** section
**Opacity** slider (see **Image filters** below) and `element-set --opacity <0..1>` /
`elements-set`'s batched JSON patches.

## Image filters

**(T0273 — non-destructive brightness/saturation/contrast/tint; lead use case: mute/dim a
background image sitting behind a UI mockup.)** `element.filters` — an **additive**
object, absent = no filters, **image-only** (a loud error patching `filters` onto a
`text`/`note` element — mirrors the `flipH`/`flipV` image-only guard). Up to four optional
keys, each stored only when it differs from its default (the object itself is omitted
entirely once every key is back at default — the same "absent means the default"
convention `rotation`/`flipH`/`background` already use):

- `brightness` — finite number `[0,2]`, default `1`.
- `saturation` — finite number `[0,2]`, default `1`.
- `contrast` — finite number `[0,2]`, default `1`.
- `tint` — `{ color: "#rrggbb", strength: [0,1] }`, stored only when `strength > 0`
  (both fields are still validated loudly even at `strength: 0`).

A `filters` patch is a **whole-object REPLACE**, not a merge (like `style`): patching
`{contrast:1.3}` resets `brightness`/`saturation`/`tint` on that element back to their
defaults too. `null` or `{}` (or an object that normalizes to all-defaults) clears
`filters` to an absent field. All validation is loud and happens **before any write**:
a non-finite/out-of-range number or a bad `#rrggbb` hex throws atomically.

**The parity contract (load-bearing — both renderers implement the SAME per-pixel math):**
non-premultiplied sRGB channels in `[0,1]`; **alpha is never touched** by
brightness/saturation/contrast (a fully transparent pixel stays fully transparent, a
semi-transparent one keeps its exact alpha). Canonical order:

1. **brightness**: `C' = clamp(C * b)`
2. **saturate** (the SVG `feColorMatrix type="saturate"` matrix, luma
   `0.2126/0.7152/0.0722` — **not** PIL's default `0.299/0.587/0.114` grayscale luma; at
   `saturation: 0` this is the one trap the whole design guards against):
   ```
   R' = (0.2126+0.7874s)R + (0.7152-0.7152s)G + (0.0722-0.0722s)B
   G' = (0.2126-0.2126s)R + (0.7152+0.2848s)G + (0.0722-0.0722s)B
   B' = (0.2126-0.2126s)R + (0.7152-0.7152s)G + (0.0722+0.9278s)B
   ```
3. **contrast**: `C' = clamp((C - 0.5) * c + 0.5)` (pivots at mid-gray)
4. **tint** (a scrim, RGB only): `C' = C*(1-strength) + tintC*strength`; alpha unchanged.

Element `opacity` (see **Element opacity** above) multiplies alpha **at composite time**,
already implemented on both sides — filters never touch it, never duplicate it. Filters
are pure per-pixel color ops on the element's **own** pixels: geometry (x/y/w/h/rotation/
flip), hit-test, marquee, and regions are **unaffected**. Source-space ops
(`detectRegions`/`sliceRegions`/`alphaCutout`) read the element's **untransformed source**
pixels and stay valid with filters on — unlike rotation/flip (R7), filters need **no**
refusal guard.

- **Canvas** (`site/workspace.js` `paintElement`): brightness/saturation/contrast wrap the
  existing `drawImage`/slice9-patch loop in `ctx.filter = "brightness(b) saturate(s)
  contrast(c)"` (CSS filters apply left→right, matching the canonical order above; empty
  string — `ctx.filter` never touched — when all three are at default, and always reset to
  `"none"` after). `tint` cannot be expressed by `ctx.filter` (the mix must stay inside the
  element's own pixels), so an element carrying `tint.strength > 0` instead draws through a
  small **cached offscreen canvas**: the filtered image is drawn once, then a
  `globalCompositeOperation:"source-atop"` + `globalAlpha:strength` fill lays the tint
  scrim on top; the result (a plain bitmap at the source's natural size) flows through the
  UNCHANGED rotate/flip/opacity/slice9 pipeline. The offscreen is cached per element id
  (keyed by `src` + the filters signature — mirrors `layers_panel.js`'s thumbnail cache
  precedent) and pruned each render alongside it; an element with no tint pays zero
  offscreen cost, an element with no filters at all takes the exact pre-existing fast path.
- **PIL** (`tools/filters_math.py` `apply_filters` — T0274: extracted out of
  `render_group.py` so it is the ONE shared implementation both `render_group.py`
  (`paint_element`, called right after the resize/slice9 step, **before** flip/rotate —
  per-pixel color ops commute with geometry, so this ordering is a free choice, picked so
  filters read the untransformed post-resize pixels) and `tools/bake_filters.py` (see
  **Filters bake ("Apply")** below) import): the same four steps in `numpy`, alpha channel
  copied through byte-for-byte untouched. **PIL is the single source of rendered truth**;
  the canvas approximates via the browser's own spec'd CSS filters, which compute the same
  functions.
- `ops.buildRenderNodes` forwards `filters` verbatim on an image paint node (`undefined`
  when absent, so `JSON.stringify` drops the key — zero shape change for an unfiltered
  element), same as `rotation`/`flipH`/`opacity`/`slice9`.

**Inspector (page):** a collapsible **Filters** section on an image element — **Opacity**,
**Brightness**, **Saturation**, **Contrast** sliders (0-200%, 0-100% for opacity) with a
per-control reset ("×", always rendered, disabled at the default — mirrors the Rotation
row's "↺"), plus a **Tint** color input + **Tint strength** slider. Each slider drags LIVE
(mutates the in-memory element + repaints, the same view-state idiom the T0207 cleanup
preview uses) and commits **ONE** `patchElement` on release; releasing back at the value
the drag started from commits nothing. **«Приглушить»** (lead's one-click dim preset) sets
`{opacity:0.7, filters:{brightness:0.7, saturation:0.6}}` in one patch/one undo; **Reset
all** clears both back to defaults, also one patch/one undo. Multi-select: when every
selected element is an image, the same «Приглушить» preset is offered batched into ONE
`patchElements` call (mirrors the Alpha section's "Apply to N images"); per-slider live
editing is **not** batched (single-element only, v1).

**Export stance:** `renderGroup`/`exportProject` (both flow through `render_group.py`)
apply filters — that is the whole point for a muted background. Single-element
`exportElements` does **not**: it only resizes/re-encodes the element's raw source
pixels (`tools/export_images.py` never reads `rotation`/`flipH`/`flipV`/`opacity`/
`filters`) — the same stance rotation/flip/opacity already have there, kept identical for
filters (an "export the source asset" action, not an "export what's on screen" action).

**Both clients (strict tool parity):** the inspector's Filters section commits through
`patchElementBox`/`patchElementsBatch` → the SAME `patchElement`/`patchElements` fields the
agent sets via `element-set --filters-json '<json|null>'` (or `elements-set`'s batched
JSON patches) — whatever the page can set, the CLI/API can set identically.

### Filters bake ("Apply")

**(T0274 — Photoshop-rasterize semantics; lead: "принял -> получил новый арт -> ползунки
снова в 0".)** `element.filters`/`element.opacity` are non-destructive so the lead can
iterate, but **Apply** commits a look: it burns the element's CURRENT filters + opacity
into a **new** content-addressed source file (own Python tool, own temp dir — mirrors
`alphaCutout`/`cleanupApply` exactly) as **one** journaled mutation, then **clears** both
fields so the sliders reset to their defaults. `source_w`/`source_h`/`w`/`h`/`x`/`y`/
`rotation`/`flip`/regions are all **untouched** (geometry never changes — the output PNG
is written at the source's full resolution, same as `alphaCutout`), and the previous file
stays in `files/`, so undo restores the previous src **and** the previous filters/opacity
byte-exact.

**Math (single source of truth).** `tools/bake_filters.py` imports the SAME
`tools/filters_math.py` `apply_filters()` the live render path uses (see **Image
filters** above — one implementation, never copy-pasted), then multiplies the alpha
channel by `opacity` with the **identical** formula `render_group.py`'s `paint_element`
applies at composite time (`image.putalpha(image.getchannel("A").point(lambda a: round(a
* factor)))`, `factor` clamped to `[0,1]`) — so the baked pixels are bit-identical to
what the on-canvas preview showed. Opacity-to-alpha lives **only** in `bake_filters.py`;
`render_group.py` still applies opacity at composite time for every *other*
(non-baked) element.

**Op / API / CLI.** `bakeFilters({ projectId, elementId | elementIds })` — image
elements only (loud on text/note), and loud when the element has **nothing to bake**
(`filters` absent/default AND `opacity` absent-or-1: *"...has nothing to apply — filters
are at defaults"*). `elementIds` (2+ images), given INSTEAD of `elementId`, batches a
multi-selection into ONE journal entry — every element is validated (image, has
something to bake) **before** any Python spawn, so a batch is fully atomic: one refusing
element rejects the whole batch, nothing mutated. `POST
/api/canvas/projects/<id>/elements/<eid>/filters-bake {}` (single) and `POST
/api/canvas/projects/<id>/filters-bake {elementIds}` (batch) — mirrors how `/alpha`
handles single vs. batch. CLI: `filters-bake <id> --element <eid>` /
`filters-bake <id> --elements e1,e2`.

**Provenance.** `element.meta.filters_bake = { prev_src, baked: { filters, opacity },
at }` (`baked` records exactly what was burned in) + a `tool_runs` row (`op:
"filters_bake"`), the same convention `alphaCutout`/`cleanupApply` use.

**Page.** The Filters section's **Apply** button (disabled when there is nothing to
bake) runs through the long-op queue with a progress toast, like Alpha/Cleanup; on
success the section re-renders with every slider back at its default. Multi-select:
when every selected element is an image, an **"Apply filters on N images"** button sits
beside the batched **«Приглушить»** preset — enabled only when *every* selected image
has something to bake (front-loading the batch op's own atomic gate into a useful
disabled state).

## 9-slice elements

**(T0233 — data model, shared math, both renderers, op/API/CLI, inspector UI.)** Lead
ask (verbatim): «Сделать что-то слайс9 картинкой (чтобы проверить а работает ли
слайс9)» — give an image element 9-slice insets and prove corners stay a fixed size,
edges stretch on one axis, and the center stretches both, when the element's box is
resized (the UI-panel/button use case).

**Model.** `element.slice9 = { left, top, right, bottom, scale? }` — an **additive**
top-level field beside `regions`/`export` (not `meta`), insets in **source pixels**,
non-negative integers. Absent = today's plain single-`drawImage`/resize behavior
everywhere (zero migration). `scale` (lead: «важно чтобы я мог скейлить края, иногда
мне нужно больше или меньше») is an optional multiplier `> 0`, capped at 16, that
fattens/thins the DESTINATION corner/edge band **only** — the SOURCE crop never
changes size — and is stored only when `!= 1` (mirrors the `rotation:0`/`flipH:false`
absent-is-default convention already used elsewhere in this schema). Image elements
only — a text element is a loud error (mirrors the flip image-only guard).

**Loud invariant (set-time; mirrors the engine's `nt_sprite_renderer.c:697` check
`sl+sr < source_w && st+sb < source_h`).** Each inset must be a non-negative integer;
`left+right` must be `< source_w`; `top+bottom` must be `< source_h`; `scale` (when
given) must be finite in `(0, 16]`. Any violation throws **before any write**, naming
the exact numbers (e.g. `slice9 left+right (120) must be < source width 100`).

**Op / API / CLI (strict tool parity).** The dedicated `setSlice9({ projectId,
elementId, insets })` op — `insets` an object validates + stores; `insets: null`
clears — the same shape as `setRegions`/`setExportSettings`, **not** a `patchElement`
field, so the source-dim validation and the null-to-clear stay localized to one op.
`PUT /api/canvas/projects/<id>/elements/<eid>/slice9 {insets}`. CLI: `slice9-set <id>
--element <eid> [--left n --top n --right n --bottom n] [--scale n] | --clear`
(omitted flags **merge** over the element's current slice9, so `--left 30` alone
bumps just that inset; `--clear` sends `null`). Undo/redo are free — slice9 rides the
whole `elements` array snapshot `commitMutation` already takes on every op.

**Shared math (parity keystone): one algorithm, two hand-mirrored twins, pinned by a
golden test.** `ai_studio/assets/canvas/slice9.mjs` (`validateSlice9`/
`slice9Patches`) and its exact Python twin `ai_studio/assets/canvas/tools/slice9.py`
(`validate`/`slice9_patches`) both compute the same `<=9` patches
`{sx,sy,sw,sh,dx,dy,dw,dh}` from the identical proportional-clamp formula (CSS
border-image style: each inset is scaled first, then clamped so corners never
overlap when the box is smaller than their sum on an axis; a squished-to-zero band
is dropped, never emitted negative). `slice9.mjs` is imported by `ops.mjs` (the loud
set-time gate) **and** served to the page for `site/workspace.js`'s canvas paint —
one module, two consumers, byte-identical math; `tools/slice9.py` is imported by
`tools/render_group.py`'s PIL export. `tests/slice9.test.mjs`'s parity golden spawns
the studio venv Python **once** against a fixture set of `(insets incl. scale, src,
dst)` cases and asserts `slice9.mjs`'s patches deep-equal `tools/slice9.py`'s patches
for every case — the two twins are pinned together directly, on top of the
render-pixel tests that already pin them indirectly (an asymmetric 9-color fixture
rendered through the real `render_group.py`, sampled per zone).

**Render — composes with rotation/flip for free (T0232).** Both renderers replace the
single "resize the whole image to the box" step with the 9-patch loop, **inside** the
same rotate/flip transform each already carries, so a rotated/flipped slice-9 panel
needs no new code: `site/workspace.js` `paintElement` draws the patches at the
element's own (unrotated) screen box, inside the existing
translate→rotate→scale(flip)→translate-back `ctx` wrapper; `tools/render_group.py`
`paint_element` assembles the sliced `box_w x box_h` image and assigns it **before**
the existing flip-transpose → `rotate(-rotation, expand=True)` → paste-centered
chain. Corners are crisp by construction (an integer source crop pastes byte-exact
onto an integer dst at export scale 1); **PIL stays the single source of rendered
truth**, the page a same-pixels approximation (~1px edge-drift tolerance, the same
stance text/rotation already take). Region ops (`detectRegions`/`sliceRegions`/
`alphaCutout`) read the element's **untransformed source pixels** and never touch
slice9 — no new guard, no interaction with the T0232 rotation/flip refusal either
(slice-9 is not added to `isNodeTransformed`).

**Inspector (page).** A collapsible **Slice-9** section — image elements only,
placed after **Cleanup** and before **Extracted prompts** — shows the source size,
then Left/Top/Right/Bottom + Scale. Absent slice9: draft fields (prefilled with a
sensible default — a quarter of the smaller source dimension, floored, capped at
24px, so the default itself is always a valid call) plus an **Enable** button that
commits all four insets + scale as **one** `setSlice9`. Present: every field is
live-bound — a commit per edit, one `setSlice9` per field (a settings tweak, not a
drag gesture, so a per-field journal entry is fine, the same stance the Position &
Size X/Y/W/H grid already takes) — plus a **Clear** button (`insets: null`). A
rejected commit (insets too big for the source, an out-of-range scale) surfaces as
an error toast, same as every other inspector action; there is no client-side
re-validation beyond basic number parsing.

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

### History panel + jump (T0204)

`listHistory({ projectId })` (`GET /history-list`, CLI `history-list`) is the Photoshop
history-panel view: the current **linear spine** — a synthetic `Base` (seq `0`), then the
applied **undo chain** (head back to base), then the **redo tail** (future states, forward
from the head). Each row is `{ seq, op, label, summary, current, undone }`, so BOTH clients
render identical text with no journal parsing — the human `label`/`summary` come from the
pure, exported `historyEntryLabel(op, args_summary)`. A stale (invalidated) branch is never
on the spine. `current` marks the applied head; `undone` marks the dimmed-but-clickable redo
tail.

`jumpHistory({ projectId, seq })` (`POST /history-jump`, CLI `history-jump --seq N`) moves
the applied head to any spine seq — `0` = base, an undo-chain seq = jump **back**, a redo-tail
seq = jump **forward**. It restores that seq's **existing** sidecar snapshot (`state` for a real
entry; the oldest retained entry's `undo_patch` for base) and repoints `history_seq`, so the
result is **exactly** what N undos or N redos would produce — one HTTP call, ZERO recomputation.
It appends only a `{ op: "jump", target_seq, from_seq }` **nav marker** (like the undo/redo
markers — no snapshot, not a mutation), so no parent pointer changes and no compaction runs:
undo/redo/jump from the new head behave identically to N manual steps and a jump is itself
reversible (`redo`/jump-forward after a back jump, and vice-versa). Loud on a non-integer/
negative seq or a seq not on the current spine (unknown or stale-branch); a jump to the current
head is a no-op (no marker).

### Concurrency guard — `expectHead` (T0234)

Incident 2026-07-03: an agent read a project's journal at head `823`, the lead kept
working live to head `876`, and the agent's `history-jump` forked the spine at `817`
and orphaned the lead's newest entries (recovered manually from sidecar snapshots).
`undoOp`, `redoOp`, and `jumpHistory` accept an optional `expectHead` (a number; the
CLI's `--expect-head` may arrive as a string and is coerced with `Number()` and
validated as a finite integer — a bad value is a loud error). When given and it does
not match the project's **actual current** `history_seq`, the call refuses LOUDLY
**before any write** — the error names the current head, the caller's stale value, and
the remedy (re-read `history-list` and retry). Nothing is written on refusal (journal
and `project.json` untouched). When it matches, or when `expectHead` is omitted
entirely, behavior is byte-identical to before T0234 — the page (which never sends it)
is unaffected.

The **CLI is the agent transport**, and every command it runs is agent-attributed
(`setOpsActor("agent")` in its `isMain` guard), so `undo`, `redo`, and `history-jump`
REQUIRE `--expect-head <n>` — omitting it is a loud `fail()` explaining the live-project
race and pointing at `history-list`. `history-list`'s output carries the current head
under both `history_seq` (existing field, unchanged) and the new additive `head` field,
and the CLI additionally prints a `head: N` line before the JSON so it can't be missed.
The agent workflow is: `history-list` → note `head` → `undo|redo|history-jump ... --expect-head <head>`.
The HTTP API passes `expectHead` through from the JSON body (`{expectHead?}` on
`/undo`, `/redo`, `/history-jump`) for a non-CLI agent; the page does not send it (v1 —
two-tab page safety is a deferred follow-up).

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
(`ai_studio.canvas.export.v1`) alongside the images. Delivering those bytes to where the
lead chose is a separate delivery-to-disk layer (page save-file dialog / CLI `--to`) on
top of this one shared op output — the op itself never writes to an arbitrary path.

**Element export** (`exportElements`): one file per element × export row — a resize +
re-encode of the element's **raw source** pixels only; `rotation`/`flipH`/`flipV`/
`opacity`/`filters` are display transforms and are **not** applied here (see **Image
filters** → "Export stance"). `renderGroup`/`exportProject` (composited screens) DO apply
all of them.

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
- **File name** is automatic (T0229 — no suffix column): base = the element/screen
  name (slugged); a single row = clean `name.<ext>`; several rows on one element get a
  Figma **scale marker** (`wing@2x.png`; a `1x` row stays clean); any remaining collision
  gets a deterministic numeric `_NN`.
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

- **Page (T0229, Figma-style)**: delivery is a **save-FILE dialog** with an editable
  name, not a directory picker (Chrome refused `showDirectoryPicker` for
  Downloads/системные папки). **One** output → the image is saved under its own suggested
  `name.<ext>`; **several** outputs → ONE **STORE-mode `.zip`** built server-side (fetched
  over `GET export-zip/<stamp>`) saved as `<project/selection>.zip`. Dialog **abort** = a
  quiet cancel (info toast "Отмена в диалоге = отмена экспорта"); any **other** picker/write
  failure is loud (error toast) with **no** silent fallback; a plain browser download runs
  ONLY when `showSaveFilePicker` is absent. `site/export_dest.mjs` (`saveBlobToFile`,
  delivery-to-disk only) owns the dialog; `actions.js` fetches the bytes.
- **CLI**: `--to <dir>` copies the produced files (+ manifest) to that exact path;
  `--zip <path>` writes ONE STORE-mode `.zip` of the run's images (the same archive the
  page builds). Without either, the confined `<project>/export/<stamp>/` default stays
  (agents rely on it) — unchanged.

## CLI

```powershell
node ai_studio/assets/canvas/cli.mjs list
node ai_studio/assets/canvas/cli.mjs create [--title "My canvas"]   # omit --title for a random default
node ai_studio/assets/canvas/cli.mjs show <id>
node ai_studio/assets/canvas/cli.mjs rename <id> --title "New title"
node ai_studio/assets/canvas/cli.mjs delete <id>          # moves to .trash
node ai_studio/assets/canvas/cli.mjs add-image <id> --file path.png
node ai_studio/assets/canvas/cli.mjs add-images <id> --files a.png,b.png   # batched multi-image add; one undo step
node ai_studio/assets/canvas/cli.mjs add-image-from-file <id> --src files/<hash>.png [--name X] [--x 40 --y 40]   # mint an element from an EXISTING project file; no re-upload, no duplicate bytes
node ai_studio/assets/canvas/cli.mjs add-text <id> [--x 40 --y 40] [--content "Заголовок"] [--style-json style.json] [--group <gid>]
node ai_studio/assets/canvas/cli.mjs element-set <id> --element <eid> [--content "New text"] [--style-json style.json]   # text edits (validated vs fonts.json)
node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
node ai_studio/assets/canvas/cli.mjs element-set <id> --element <eid> [--name "X"] [--visible true|false]
node ai_studio/assets/canvas/cli.mjs element-set <id> --element <eid> [--rotation <deg>] [--flip-h true|false] [--flip-v true|false]   # T0232 3a: rotation = degrees CW about the box center, normalized [0,360); flip is image-only
node ai_studio/assets/canvas/cli.mjs element-set <id> --element <eid> --opacity 0.7 --filters-json '{"brightness":0.7,"saturation":0.6}'   # T0273: image-only, whole-object replace; --filters-json null clears
node ai_studio/assets/canvas/cli.mjs element-remove <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs elements-set <id> --json patches.json    # batched patch [{elementId,x?,y?,w?,h?,name?,visible?,rotation?,flipH?,flipV?,opacity?,filters?}]; one undo step
node ai_studio/assets/canvas/cli.mjs elements-remove <id> --elements e1,e2    # batched delete; one undo step
node ai_studio/assets/canvas/cli.mjs element-reorder <id> --element <eid> --index <n>   # z-order among merged siblings; 0 = back (clamps)
node ai_studio/assets/canvas/cli.mjs node-reorder <id> --node <id> --index <n>          # reorder an element OR group among merged siblings; 0 = back (strict)
node ai_studio/assets/canvas/cli.mjs nodes-move <id> --json moves.json                  # batched mixed element+group move [{nodeId,x,y}]; group subtrees cascade; one undo step
node ai_studio/assets/canvas/cli.mjs nodes-reorder <id> --nodes n1,n2 --direction front|back|forward|backward   # (or --index n) multi-node z-order block, relative order kept; one undo step
node ai_studio/assets/canvas/cli.mjs nodes-align <id> --nodes n1,n2 --align left|hcenter|right|top|vcenter|bottom [--reference auto|selection|parent]   # 2+ nodes -> selection bbox; 1 node in a group -> the group frame; one undo step
node ai_studio/assets/canvas/cli.mjs nodes-distribute <id> --nodes n1,n2,n3 --axis h|v   # equal-gap distribute (3+ nodes; endpoints fixed); one undo step
node ai_studio/assets/canvas/cli.mjs nodes-paste <id> --spec spec.json [--dx 16 --dy 16] [--group <gid>|none]   # instantiate a copied node spec (new ids); one undo step
node ai_studio/assets/canvas/cli.mjs nodes-duplicate <id> --nodes id1,id2 [--dx 16 --dy 16] [--group <gid>|none]   # duplicate live nodes in place +offset; one undo step
node ai_studio/assets/canvas/cli.mjs nodes-delete <id> --nodes id1,id2   # batched mixed element+group subtree delete; one undo step
node ai_studio/assets/canvas/cli.mjs regions-set <id> --element <eid> --json path.json   # a regions array or {regions:[...]}
node ai_studio/assets/canvas/cli.mjs regions-show <id> --element <eid>
node ai_studio/assets/canvas/cli.mjs slice <id> --element <eid> [--regions r1,r2]
node ai_studio/assets/canvas/cli.mjs alpha <id> --element <eid> [--method auto|matte|corridorkey] [--regions r1,r2]   # alpha-cutout the element (auto routes; matte forces key_matte; corridorkey = neural GREEN/MAGENTA(shim) matte for soft glow, region composite, ~15s GPU); one undo
node ai_studio/assets/canvas/cli.mjs alpha <id> --elements e1,e2 [--method auto|matte]   # batch: 2+ images keyed into ONE journal entry/undo; no --regions with a batch
node ai_studio/assets/canvas/cli.mjs alpha-dual <id> --elements a,b   # white-plate + black-plate pair (either order) -> ONE new cut element; plates untouched; one undo
node ai_studio/assets/canvas/cli.mjs alpha-dual-generate <id> --element <eid> [--prompt "extra subject text"]   # AUTOMATIC: element's current pixels = light plate; generates the dark plate, gates (one auto-retry), cuts -> ONE new element beside the source; source untouched; one undo
node ai_studio/assets/canvas/cli.mjs export-set <id> --element <eid> --json rows.json      # persist export rows (journaled)
node ai_studio/assets/canvas/cli.mjs export-set <id> --element <eid> --scale 2x [--format png|jpg|webp] [--quality 1-100] [--resample lanczos|nearest] [--base source|canvas]
node ai_studio/assets/canvas/cli.mjs export <id> --elements e1,e2   # or --all, or --project (all visible screens)
node ai_studio/assets/canvas/cli.mjs export <id> --all [--scale 2x --format jpg --quality 80 --resample lanczos --base canvas] [--to <dir>] [--zip out.zip]   # --base canvas = export at the element's ON-CANVAS size, not source pixels; --zip = one STORE-mode archive of the run
node ai_studio/assets/canvas/cli.mjs group-create <id> --name X [--elements e1,e2 | --x --y --w --h] [--parent <gid>|none]
node ai_studio/assets/canvas/cli.mjs group-reparent <id> --group g --parent <gid>|none [--index n]   # nest a group; none = top level
node ai_studio/assets/canvas/cli.mjs group-move <id> --group g --x --y
node ai_studio/assets/canvas/cli.mjs group-set <id> --group g [--name] [--visible true|false] [--w --h] [--background '#rrggbb'|none] [--clip true|false]
node ai_studio/assets/canvas/cli.mjs groups-set <id> --groups g1,g2 [--visible true|false] [--clip true|false]   # batched shared toggles (multi-group inspector); one undo step
node ai_studio/assets/canvas/cli.mjs group-fit <id> --group g [--padding n]   # resize the frame to fit its content (padding default 24)
node ai_studio/assets/canvas/cli.mjs group-scale <id> --group g --x n --y n --w n --h n   # T0271: scale the group's FULL subtree (frame + every descendant + text fontSize) to a new frame; server computes descendant patches; one undo step. Distinct from group-set's frame-only --w/--h (children pinned).
node ai_studio/assets/canvas/cli.mjs group-assign <id> --elements e1,e2 --group g|none
node ai_studio/assets/canvas/cli.mjs group-ungroup <id> --group g   # dissolve one level; children keep the group's z-slot; one undo step
node ai_studio/assets/canvas/cli.mjs group-delete <id> --group g
node ai_studio/assets/canvas/cli.mjs recipe-create <id> [--name X] [--x --y --w --h] [--parent <gid>|none]   # T0239 increment 1: mint a recipe card (a group with an additive `recipe` blob); omitted w/h default to 360x280; no generation yet
node ai_studio/assets/canvas/cli.mjs recipe-set <id> --group g [--prompt "a red fox"] [--engine codex|gemini|both] [--style <id>|none]   # partial recipe blob update; --style is a reserved by-id pointer (style cards land later)
node ai_studio/assets/canvas/cli.mjs recipe-generate <id> --group g   # T0239 increment 2: generate — mints 1 (codex/gemini) or 2 (both, compare mode) new RAW elements beside the card, in its PARENT scope; one undo step; real codex/agy spawn (minutes)
node ai_studio/assets/canvas/cli.mjs render-screen <id> --group g [--scale 2] [--background '#rrggbb']
node ai_studio/assets/canvas/cli.mjs undo <id> --expect-head <n>  # --expect-head required (T0234); read it from history-list first
node ai_studio/assets/canvas/cli.mjs redo <id> --expect-head <n>
node ai_studio/assets/canvas/cli.mjs history <id>
node ai_studio/assets/canvas/cli.mjs history-list <id>            # labeled linear spine the panel shows (Base + undo chain + dimmed redo tail); prints "head: N" first
node ai_studio/assets/canvas/cli.mjs history-jump <id> --seq <n> --expect-head <n>  # jump the head to a spine seq (0 = base); like N undos/redos, undoable
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
  call, and a mixed/multi-group **Delete** is one `nodes-delete` (`deleteNodes`) call; the
  inspector's **Align** row is one `nodes-align`/`nodes-distribute` (`alignNodes`/
  `distributeNodes`, T0232 increment 1) call per click.
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
  source size, a **Reset to source size** button, a **Rotation** number input + **Flip
  H**/**Flip V** toggle buttons (T0232 increment 3a — see **Rotation & flip**; Detect/
  Slice/Alpha gray out with a reason while the element is rotated/flipped), a
  **Filters** section (T0273 — Opacity/Brightness/Saturation/Contrast sliders + Tint
  color/strength, a per-slider reset, and «Приглушить»/Reset all presets; see **Image
  filters**), provenance,
  meta, and a calm **Regions** section: a count badge,
  compact per-region rows — number + name/size + delete, coords in the tooltip —
  that select/enter region-edit on the canvas and inline-rename on double-click,
  plus **Detect**, **Slice** (selected regions, else all), an **Alpha cutout** control (a
  method dropdown Key matte / **CorridorKey (green glow)** (T0261/T0262 — neural green matte for
  soft glow, green native/magenta via hue180 shim, region composite, ~15s GPU) / **ViTMatte (thin
  detail)** (T0335 — neural thin-detail / 2nd-choice-glow on a green/magenta key, own GPU venv,
  ~1-3s, whole-element only) / **BiRefNet (any bg)** (T0335 — SOD cutout for an arbitrary/unknown
  background, no key, CPU ~25s, whole-element only) / Dual-plate + a run button scoped to the selected
  regions when any are selected, else the whole element — a long-op via the queue), which additionally shows
  two compact plate-thumbnail rows + role labels + a per-plate **Add to canvas** button
  (T0238) once `element.meta.alpha.plates` exists (an `alphaDualPlateGenerate` result)),
  group/screen (name, X/Y/W/H + a **Fit to content** button that
  resizes the frame to its content, **Visible** + **Clip content**
  checkboxes, member count, Background, **Render screen** with scale + background),
  multi-select (count + "each exports
  its own settings" + Export; when EVERY selected element is an image, an **Alpha**
  section (T0230) — the same method dropdown + one button labeled "Apply to N images"
  that keys the whole selection as ONE journaled op), or "Nothing selected" (with a
  project-export button when there are visible screens). A single element also gets an **Export** section
  at the BOTTOM (Figma-style): a collapsible list of rows (scale + format, a quality
  slider only for jpg/webp, a resample toggle — **no suffix column** as of T0229, file
  names are automatic), **+ Add export setting**, and an Export button labeled by the
  target. Row edits commit through `setExportSettings` (one journal entry per change).
- `export_dest.mjs` — export destination delivery (delivery-to-disk only): `saveBlobToFile`
  hands a Blob to a **save-FILE dialog** (`showSaveFilePicker`) with an editable suggested
  name (T0229, replacing the T0206 dir picker Chrome refused for Downloads). Dialog abort =
  a quiet cancel; any other failure is loud (no silent fallback); a plain browser download
  runs only when `showSaveFilePicker` is absent.
- `history_panel.js` — the Photoshop-style **History** palette: a hideable floating
  list of journal steps (Base + the applied undo chain + the dimmed, still-clickable
  redo tail), toggled from the top-bar **History** button or the **`** key, hidden by
  default with its open state persisted in localStorage (view-state only — never
  journaled). A thin view over `listHistory` (labeled rows — no journal parsing on the
  page); a row click jumps the project to that step via the one `jumpHistory` op (`GET
  /history-list` + `POST /history-jump`, the same ops the CLI's `history-list`/
  `history-jump` drive). It renders through the refresh bus so it stays live: the current
  step re-highlights instantly from the folded op-response head, then the list re-fetches
  (structure-signature guarded). Quiet like undo/redo — the highlight is the feedback, no
  toast. The undo/redo keyboard shortcuts are untouched.
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
**export** / **Render screen** a **pinned-result toast** names what was saved (the file
name from the save dialog, not a directory handle) and keeps clickable per-file links
served by the confined `GET /api/canvas/projects/<id>/export/<stamp>/<file>` route.

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
| Drag a group's scale handle (`Ctrl/Cmd` held = frame/zone only) | **(T0271)** Default: scale the group's full subtree — children move/resize proportionally, text font sizes scale (`scaleGroup`). `Ctrl`/`Cmd` held: resize only the group's own frame, children unchanged (the original T0232 behavior, `patchGroup {w,h}`). `Shift`/`Alt` (proportional-lock / from-center) work the same in both modes |
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
