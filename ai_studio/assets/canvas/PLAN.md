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

0. **Undo/journal audit fix pass** (in flight 2026-07-02) — Ctrl+Z inside
   region-edit mode must undo visibly (mode is pure UI state, Figma isolation
   convention: undo works on the document, mode reconciles after reload, exits
   only if the edited element vanished); full sweep of all site actions for
   journal-bypassing mutations.
1. **UI organization** (`T0217`, P1) — context-menu diet (no Rename/Hide/
   Export, regions collapse to one entry), Figma-like titled collapsible
   inspector sections with persisted state, z-order (layers sibling drag +
   Bring forward/Send backward/front/back + Ctrl+[/] by event.code) on a new
   journaled reorder op.
2. **Export panel** (`T0206`, P1) — Figma-style Export section at the bottom
   of the inspector (see "Export design" below); absorbs export-destination
   from T0203; export leaves the context menu.
3. **Perf: frontend churn** (`T0200`, P1) — use op responses instead of
   reload+history GETs, immutable cache headers + `<img>` reuse, rAF drag,
   batched multi-ops.
4. **Perf: journal O(n²) + history limits** (`T0201`, P1) — sidecar snapshots,
   O(1) seq, `tool_runs` cap, history depth cap (~200, `studio.config` knob) +
   compaction on open. Industry norm: Photoshop caps steps, Figma checkpoints;
   unlimited verbatim history is not kept anywhere. (Agent in flight.)
5. **Feedback layer** (`T0203`, P1) — toasts replace the bottom status line,
   busy spinner, input never blocked, max-N concurrent long ops with a visible
   queue.
6. **Perf: warm Python worker** (`T0202`, P1) — JSON-RPC stdio worker behind
   the raster2d bridge; coordinate with the lead's in-flight matte work.
7. **History panel** (`T0204`, P2) — Photoshop-style hideable list over the
   journal + `jumpHistory` op (CLI parity).
8. **Observability** (`T0205`, P2) — `duration_ms` on journal entries,
   per-project `errors.jsonl`, timings in API responses, `ops-stats` CLI.
   Today the module logs successful ops only (journal), with no durations and
   no error trail — this closes that gap. (Agent in flight, with T0201.)

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

- Lead's unified Python matte (uncommitted `raster2d`/`cutout` work) gates
  `T0207` and per-region alpha (`T0210`), and should be coordinated with the
  Python worker (`T0202`).
- Single-spawn slice landed with increment 6 (`tools/crop_regions.py`);
  `T0202`'s worker now targets detect/render only.
- Canvas slice crops WITHOUT alpha (verified on the wings test 2026-07-02):
  the alpha step is not bridged to canvas yet — it arrives with the matte as
  a first-class per-element op (regions optional), wings = acceptance asset.
