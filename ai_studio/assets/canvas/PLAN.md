# Canvas 2D conveyor — plan (2026-07-02)

Working plan for the canvas module. Owned by the lead; updated as increments
land and research arrives. Work items live in the taskboard (epic `E010`,
tasks referenced below); this document is the narrative: state, facts, order,
and design decisions.

Current operational contracts are routed from [README.md](README.md); load only
the owning contract for the task at hand.

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

Queue is EMPTY as of 2026-07-03 morning — next candidates for the lead to
order: `T0207` (post-gen cleanup: Quantize/Denoise interactive tools — the
image-tools track's next step), Render-group delivery unified onto the
save-dialog (T0229 leftover, flagged), Ctrl+G auto-expand of the new group
in layers (T0224 nuance), mixed element+group range-select in layers,
`T0211` generation on canvas.

T0210 alpha cutout inc1 LANDED `80f2827f` (2026-07-03 night, → review):
`alphaCutout` op — element pixels through route/key_matte REUSED verbatim
(new tools/alpha_cutout.py, warm worker), swap to NEW content-addressed
alpha PNG, one entry, byte-exact undo; auto router (soft zones → dual_plate
= LOUD error, single element has no plate pair) or forced matte;
regions-scoped composition in python (rect + polygon masks); inspector
Alpha control replaces the Coming-soon hint; HTTP /alpha + CLI alpha;
meta.alpha + tool_runs provenance. Wings acceptance: alpha'd DUPLICATE
beside the original on benchmark-fixture-c7f9dc, artifacts in
tmp/t0210_wings_before_after/ (corners 0, subject 255) — visual verdict =
lead. Tests 232→240.

T0202 warm python worker LANDED `9ffe0b27` (2026-07-03 night, → review):
generic script-runner in image/_bridge (worker.py + worker.mjs, line-JSON
stdio, runpy per request, imports cached); runPython signature unchanged →
zero call-site churn; ops.mjs candidate-discovery DELETED (pinned venv only);
all canvas spawns warm (detect/crop/export/render): render 99→7.5 ms, detect
429→19 ms; 5-min idle kill, crash = loud + respawn, no orphans.

T0228 history actor attribution LANDED `10525404` (2026-07-03 night, →
review): journal entries record actor at the transport seam (CLI = agent
inside its isMain guard; page/imports = user); listHistory rows carry actor +
🤖 prefix in the shared label (page/CLI parity by construction); legacy
entries unmarked. Tests 226.

T0229 export destination redesign LANDED `425cfb27` (2026-07-03 night, →
review): save-file dialog with editable name (single output), server-built
STORE zip over GET export-zip/<stamp> (multi; zip.mjs, node built-ins only),
suffix removed (loud reject on new writes, legacy ignored), automatic naming
slug + @scale + _NN (deterministic), dir-picker/IndexedDB path deleted; CLI
--to unchanged, --zip added. Tests 232. Live browser check (real Downloads
write + Windows unzip) = lead's morning item.

T0204 history panel LANDED `df8cfe09` (2026-07-03 night, → review):
`jumpHistory` = history NAVIGATION over the existing sidecar snapshots
(N-step jump ≡ N undos/redos in one call; nav marker like undo/redo, no
compaction, loud off-spine validation); `listHistory` mirrors the exact
undo/redo walks (stale branches never listed); pure exported
`historyEntryLabel` = identical text on page and CLI. Page: floating
Photoshop palette (History button + ` toggle, hidden by default,
localStorage), row click = one jump action, signature-guarded refresh.
HTTP history-list/history-jump + CLI parity. Tests 215→223.

T0224 UX polish LANDED `d95fcf3b` (2026-07-03, → review): patchGroups
batched op + tri-state multi-group inspector; filled-body click-select;
export suffix clear-in-place (textInput allowEmpty, rename guards intact);
shared rangeSelectIds (layers + regions); clip ghost OFF by default +
Alt-hold peek; addImages batched op (multi-drop = 1 entry — one-entry law
now exception-free); layers deep-nesting pack (collapse-by-default +
selection-path auto-reveal, indent guides, draggable panel width); export
label = top-level visible count. Tests 195→204. Mixed-selection click
support added post-landing (`61e39efd` + layers toggles `5bf8bf60`,
`b272730e`).

T0226 agent skill LANDED `838f3d95` (2026-07-03, → review): thin
`nt-canvas-operations` skill (.codex/skills + synced Claude surface) —
canvas:// ref grammar, CLI routing, laws, README as single source. Live
smoke passed: a fresh agent given ONLY a canvas:// ref resolved it and
ran a journaled mutation unaided.

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

T0223 op integrity LANDED `8869777d` (2026-07-02 night, → review): the
one-gesture-one-entry law is now exception-free on the page except multi-file
drop (→ T0224 addImages). `moveNodes` (mixed element+group move, overlap-safe
via topmost-moved-ancestor delta), `reorderNodes` (multi-select block reorder,
Figma semantics, per-scope but one entry), `ungroupGroup` (children land at the
group's former z-slot, internal order kept, one undo deep-restores). Pure
block math in `tree.mjs`; HTTP + CLI `nodes-move`/`nodes-reorder`/
`group-ungroup`; full gesture audit table in the task log. Tests 181→195.

T0227 node clipboard + delete completeness LANDED `d5c346b9` (2026-07-03,
→ review): Figma-like Ctrl+C/V/D for nodes — pure `buildNodesSpec` subtree
snapshot (works after source deletion; immutable files), `pasteNodes`
(atomic validation, server-side id mint, +16 per repeat paste, one entry),
`duplicateNodes`, batched `deleteNodes` (mixed elements+group subtrees, one
entry, exact z-slot undo); Ctrl+V owned solely by the dnd.js paste event
(OS image wins over node buffer); HTTP + CLI nodes-paste/duplicate/delete.
Delete key covers every selection shape. Tests 204→215. Live-verify fixes
same day: Delete-on-group from layers (selectGroupOnly wiring `1b44152f`),
fractional-DPR bottom-edge artifact (`c3500214`), no accidental
region-edit/region-on-tap (`80381b33`), multi-group drag keeps selection
(`41b8c641`).

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

Our raster adaptation (T0206, destination REDESIGNED by T0229 2026-07-03):
same section/rows/persistence/scale syntax; formats PNG/JPG/WebP with an
explicit quality slider for the lossy two; resample = Lanczos (smooth) /
nearest (pixel art); per-element settings stored via a journaled op (agent
parity — CLI can set them too); no selection → project-level export of
screens; `<project>/export/<stamp>/` stays the automation default. The
clean-art supersampling (generate 2x → export 1x with Lanczos) lands with
this panel. DESTINATION (T0229, replaces the dir-picker after Chrome blocked
system folders): single output = save-file dialog with editable name;
multiple outputs = one STORE zip via the same dialog; suffix REMOVED — names
are automatic (slug + @scale marker only when needed); CLI `--to` unchanged,
`--zip` added.

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
