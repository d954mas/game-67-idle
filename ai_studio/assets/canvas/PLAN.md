# Canvas 2D conveyor — plan (2026-07-02)

Working plan for the canvas module. Owned by the lead; updated as increments
land and research arrives. Work items live in the taskboard (epic `E010`,
tasks referenced below); this document is the narrative: state, facts, order,
and design decisions.

## Laws (non-negotiable)

- **Tool parity**: every capability is ONE op in `ops.mjs` with two equal
  clients — the agent (CLI / direct import) and the thin page. No UI-only or
  CLI-only features; parity gaps are bugs.
- **Thin page**: the site holds rendering/input only; logic lives in ops.
- **Non-destructive art**: files in `files/` are immutable and
  content-addressed; every transformation produces a NEW file + a journaled
  metadata change. The lead's art must never be silently altered — undo always
  restores pixels exactly.
- **Storage stays plain** (decided 2026-07-02): `project.json` + append-only
  `journal.jsonl` + PNG files. Bench proved the text path costs single-digit
  milliseconds; SQLite would break agent legibility and risks corruption under
  YandexDisk sync. Fix algorithms, not the format.

## State

- Increments 1–5 shipped (`7196124f`, `ac0d2af3`, `34045a98`, `89adbc82`,
  `0c036d00`): projects on YandexDisk root, slice→elements with provenance,
  journal undo/redo, groups=screens with composite render, full UI redesign
  (home cards, workspace panels, dnd), instant create, `/canvas` route,
  delete confirm, layers dblclick rename, CLI parity commands.
- Increment 6 shipped (2026-07-02): region workbench — journaled `setRegions`
  (+ optional region `name`, inherited by sliced crops), two-mode interaction
  (object mode: regions passive; double-click = isolation region-edit mode
  with handles/draw/delete, Esc exits), per-region slice **cropping the
  stored (edited) geometry verbatim** via our own `tools/crop_regions.py`
  (one Python spawn, ~130 ms vs ~830 ms re-detect before), layers region
  tree, marquee select, drag-into-screen reparenting with drop-to-root,
  one-journal-entry drags, cursor fix. Regions stay extensible for future
  polygonal shapes (`T0209`).
- Benchmark harness landed (`9544f6c7`): `tests/bench.mjs`, baseline in
  `tmp/canvas_bench_2026-07-02.json`.
- Landed 2026-07-02 (afternoon): region-workbench fix pass (`07923dd3`),
  layout-independent shortcuts by event.code (`8fca3d90` — Cyrillic layout
  killed ALL hotkeys), undo/journal audit fixes (`cf538516`: region-mode
  state reconcile + layers-tree freeze after rename; verdict: NOTHING
  bypasses the journal; marquee-move/multi-delete = N entries per gesture →
  batched ops queued in T0200), thin journal + sidecar snapshots +
  observability (`4db81a07`, T0201+T0205 CLOSED: append 108→2.9 ms,
  readHistory 109→0.9 ms @1000 ops; history cap 200; duration_ms +
  errors.jsonl + ops-stats). Server on 8780 restarted with the new backend.

## Performance: measured facts (bench + research, 2026-07-02)

Full research: `tmp/canvas_perf_research_2026-07-02.md` (copy; distilled here).

| Fact | Number | Verdict |
| --- | --- | --- |
| Metadata ops (move/delete/undo) | 1–15 ms even at 100 elements | not the problem |
| YandexDisk sync latency | 0.50 ms vs 0.46 ms local | **hypothesis killed** |
| Python cold spawn (numpy+PIL import) | 165–278 ms per spawn | ~50–60% of every detect/slice |
| sliceRegions | ~828 ms (re-detect + slice = 2–3 spawns) | fixed in increment 6 (single-spawn crop) |
| Journal growth | append 1→108 ms, readHistory 0.7→109 ms over 1000 ops | **O(n²), main "gets slower over time" cause** |
| Frontend churn | +2 GETs and full thumbnail rebuild per op | multiplies everything |

## Increment queue (order approved by lead; adjust as needed)

1. **Multi-gesture op integrity** (`T0223`, P1, debt) — batched `moveNodes`
   (mixed groups+elements = 1 entry), `reorderNodes` (multi-select Ctrl+[/]),
   true `ungroupGroup` op (exact z-slot restore). Law: every gesture = one
   journal entry, no exceptions.
2. **UX polish leftovers** (`T0224`, P2, debt) — multi-group inspector,
   filled-body click-select, export suffix clear-in-place, shift-range in
   region lists, clip ghost off-by-default + Alt peek, layers deep-nesting
   pack, export button label count, lead's live-check notes.
3. **History panel** (`T0204`, P2) — Photoshop-style hideable list over the
   journal + `jumpHistory` op (CLI parity).
4. **Perf: warm Python worker** (`T0202`, P1) — JSON-RPC stdio worker; now
   builds on T0218's `_bridge` + pinned venv `pythonPath`.
5. **Agent skill** (`T0226`, P2) — thin `nt-canvas-operations` skill so any
   agent resolves `canvas://` refs and acts via CLI (pointers to README,
   never duplicates).

T0219 groups v2 COMPLETE (2026-07-02 night, → review): flat Defold-style
model (additive `parentId`/`order`/`clip`/`background`, NO stored tree;
paint order computed in shared `tree.mjs`; v1 projects open with zero
converter) + Figma selection (click = top-most group at scope, dbl-click
drills, Ctrl+click deep-selects, Esc steps up; lead's calls). Landed in four
increments: `64965a8d` foundation+background, `5e7a037e` group z-order
(reorderNode over merged siblings), `f9e9ec0f` nesting+selection
(reparentGroup w/ cycle guard, subtree cascades, recursive render,
top-level-only export), `7a4a2a49` clip (nested intersection, ghost hint).
Tests 110→159. Full design record + known compromises in the T0219 task.

T0221 fit group LANDED `8d165cc2` (2026-07-02 night): `fitGroup` op — frame =
union of descendant closure + padding 24, empty group = loud error; inspector
button + context menu + CLI parity.

T0222 text elements INCREMENT 1 LANDED `66f98288` (2026-07-02 night, → review):
`type:"text"` in flat `elements[]` (z/grouping/undo/marquee inherited free);
shared pure `fonts.mjs` (ops + page normalize styles identically); bundled
static OFL fonts w/ Cyrillic — Inter/Rubik/Bitter/JetBrains Mono (Rubik
replaced Latin-only Fredoka), per-family OFL.txt + fonts.json origin manifest
= the parity contract (page @font-face and PIL load the SAME files);
`addText`/`patchElement` content+style, HTTP + CLI `add-text`/`element-set
--content/--style-json`; page T tool + dblclick inline editor + inspector Text
section; `render_group.py paint_text` (anchor la, stroke outward, hard
shadow-first) vs canvas stroke-under-fill 2×. v1 = auto-width only, \n
newlines, NO wrap. v1.1 deferred: standalone text→PNG export, letter-spacing,
shadow blur, italic, v-align, fixed box+wrap. Same commit: geometric
drop-reparenting REMOVED (lead law: membership changes explicit-only; canvas
drag never reparents) + parked-member marquee fix. Tests 172→181.

T0200 perf frontend-churn LANDED `e37d8a5a` (2026-07-02 night): op responses
carry `history{seq,canUndo,canRedo}` and drive the page (zero follow-up GETs,
was 2), `/files` immutable + ETag with `<img>` node reuse, rAF-coalesced drag
renders + realloc guard, batched `patchElements`/`removeElements` (HTTP
`elements-set`/`elements-remove`, CLI parity) — one journal entry per gesture:
8-element move/delete 8→1 entries, one Ctrl+Z. Frontend-churn row of the
perf table is closed; remaining perf item is the Python spawn (`T0202`).

T0206 export panel LANDED `6620d2a6` (2026-07-02 night): journaled per-element
export rows, one-python-spawn batches, scale syntax + Lanczos/nearest,
png/jpg/webp + quality, project-level export of visible groups, CLI
export-set / export --to, destination = Figma behavior (picker every export,
opens at last-used folder, per-project IndexedDB handle). Remaining checks in
the task: live Chrome picker click-through; supersampling before/after on a
real generated sheet.

DONE 2026-07-02: T0201 + T0205 (journal O(n²) + observability, `4db81a07`);
undo/journal audit fix pass (`cf538516`); layout-independent shortcuts
(`8fca3d90`); region fix pass (`07923dd3`); T0217 UI organization + undo
clamp + screen→group + front-at-top layers; T0209 polygonal regions
(`ead8f099`); T0220 legacy sweep (`494ae3dd`: editor deleted, gallery
module, seam closed, standard studio sidebar on canvas `59eb9df0`); evening
UX pass (`4c5b6815` shift-range/indent/labels, `68be02f2` Copy ID canvas://
refs, `59eb9df0` slice→group + delete-group-deletes-content, named regions
at creation).

## Image tools track (T0218 — gates T0210 alpha + T0207 cleanup)

The lead handed over his matte refactor 2026-07-02. Full plan:
`tmp/t0218_image_tools_recon_2026-07-02.md` + decision log in the T0218 task.
Target: `ai_studio/assets/tools/image/<tool>/` per-tool folders (_bridge,
sources, bg_fix, regions, slice, alpha_matte, alpha_dualplate, route), each
with own api.mjs + python + tests + README + architecture-map node. Laws:
NO silent fallbacks (missing dep = loud error; scipy hard import; kill the
quiet simple_key_matte_cutout fallback in slice); pinned studio venv +
`pythonPath` in studio.config replaces ALL interpreter discovery; public
HTTP URLs `/api/asset-tools/raster2d/*` stay (frozen viewer keeps working);
canvas is THE editor, viewer frozen. 6 increments; canvas seam (ops.mjs
imports, 2 functions) LAST and only after T0217 lands. Then T0210 (per-
element alpha op, regions optional, wings = acceptance asset) and T0207
(Quantize/Denoise interactive tools; bg-solidify is internal-only).

## Export design (Figma research, 2026-07-02)

How Figma does it (help-center verified): collapsible **Export** section at
the bottom of the right sidebar; "+" adds export-setting rows; each row =
scale + suffix + format; multiple rows per selection export together (that's
how @1x+@2x+SVG ship in one click); settings persist on the layer; scale
syntax `2x` / `512w` / `512h`; formats PNG/JPG/SVG/PDF (SVG/PDF locked to 1x);
JPG quality is a preset, resampling bicubic ("Detailed") or nearest ("Basic");
a Preview disclosure; bulk export modal on Ctrl+Shift+E.

Our raster adaptation (T0206): same section/rows/persistence/scale syntax;
formats PNG/JPG/WebP with an explicit quality slider for the lossy two (the
lead wants visible "сжатие"); resample = Lanczos (smooth) / nearest (pixel
art); per-element settings stored via a journaled op (agent parity — CLI can
set them too); Export button asks WHERE (File System Access dir picker,
remembered per project, zip/download fallback, CLI `--to`); no selection →
project-level export of screens; `<project>/export/<stamp>/` stays the
automation default. The clean-art supersampling (generate 2x → export 1x with
Lanczos) lands with this panel.

## Clean art track (lead's 4-step ladder against AI noise)

Noise cause: diffusion VAE decoding + detail bias — partially inherent,
largely fixable. Guarantee for all steps: originals untouched, every result a
new file + journaled swap, before/after reports, pixel tests.

1. **Export scale picker** (`T0206`, P2) — Figma-like 0.5x/1x/2x/custom with
   resample filter (Lanczos smooth / nearest pixel-art). Generate big → export
   small = supersampling that averages the noise away. Generation defaults to
   2x of target size.
2. **Post-gen cleanup op** (`T0207`, P2) — background-solidify (also a keyer
   pre-pass; snap near-bg pixels to the exact bg color), palette quantization,
   optional edge-preserving denoise. **Depends on the lead's unified matte
   landing** (shares its color-distance math).
3. **Style locks** (`T0208`, idea) — "flat colors, clean fills, no grain"
   prompt profiles per style.
4. **Model routing** (`T0208`, idea) — flat UI/icons → Recraft-style backends;
   painterly → gpt-image + steps 1–2.

## Future phases

- **Polygonal regions** (`T0209`) — region model kept extensible now
  (increment 6 preserves unknown region fields); editor later, mine the legacy
  system's polygon implementation first.
- **Per-region alpha with method choice** (`T0210`) — after the matte lands;
  inspector seams already built in increment 6.
- **Generation on canvas** (`T0211`) — masked-regen primitive
  `(image, mask, prompt) → N candidates` into non-destructive per-region
  slots + variants rail; composite results locally (donor patch under mask).
- **Promote bridge** — canvas export → game/library with license/origin.
- **Groups clip content** (`T0213`) — Figma-like visual clipping of members to
  frame bounds (render already clips at export).
- **Provenance meta surfacing** (`T0214`) — `--meta` in CLI add-image and
  inspector meta view/edit. The store already accepts `meta` (used 2026-07-02
  for the generated wings: prompt, tool, origin, license recorded).

## Bench fixture (`T0212`)

Standing real canvas project `benchmark-fixture-c7f9dc` (on the projects
root): curated real assets with full generation metadata + scripted
big-project scenarios, so perf work and agent-behavior checks run against
representative content, not synthetic fixtures. First asset: generated angel
wings (codex imagegen, full prompt in `element.meta`). Grow it alongside
`tests/bench.mjs` (synthetic micro-bench) — the two answer different
questions: micro costs vs. real-project feel.

## Dependencies / open

- The matte work was HANDED TO US 2026-07-02 (was: lead's uncommitted WIP) —
  now the T0218 image-tools track above; it gates `T0207` and per-element
  alpha (`T0210`), and feeds the Python worker (`T0202`).
- Single-spawn slice landed with increment 6 (`tools/crop_regions.py`);
  `T0202`'s worker now targets detect/render only.
- Canvas slice crops WITHOUT alpha (verified on the wings test 2026-07-02):
  the alpha step is not bridged to canvas yet — it arrives with T0218's
  `image/alpha_*` tools as a first-class per-element op (regions optional),
  wings = acceptance asset.
