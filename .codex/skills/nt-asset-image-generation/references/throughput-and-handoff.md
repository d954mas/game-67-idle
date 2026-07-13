# Throughput And Handoff

Load this reference when generating many assets, re-running changed jobs, or
connecting generated art to `nt-asset-workflow` and `ai_studio/assets`.

## Whole-Game Art Runs

Generation is the dominant cost. Avoid one serial call per asset when producing
large sets.

Use:

- Batch generation:
  `scripts/gen_batch.py --jobs <jobs.json> --concurrency 3`.
- Low bounded concurrency, usually 3-4, to avoid rate-limit churn.
- `--dry-run` before a large batch to inspect planned commands.
- Skip-if-exists sidecars: `generate_image.py` writes `<out>.gen.json` with a
  hash of prompt, size, quality, and input-image content; unchanged jobs skip
  unless `--force` is passed.

## Source-Sheet First

For bulk opaque families such as UI icons, generate one source sheet and crop N
assets through the region tools (`ai_studio/assets/tools/image/`: bg_fix ->
regions -> slice). This is usually much cheaper than one generation per asset.

Reserve per-asset dual-plate generation for a few genuinely soft hero effects.
Use route warnings from the asset tools to decide where the extra
generation cost is justified.

## Declarative Packs (config -> expander)

When a family varies along axes (grade/material/shape), do NOT hand-write N
prompts. Write a pack config and expand it (T0330, dual-reviewed spec + pilot:
`references/build_spec_pack_expander_2026-07-07.md`; worked example configs:
`tmp/packs/swords-grade-test/`):

1. `scripts/expand_jobs.py --config <pack.json> [--out <jobs.json>]` — config =
   `style_prefix` (verbatim style card / art_contract block) + `subject_template`
   with `{axis}` slots + `axes` (a grade LADDER of descriptors is the progression
   control) + `sheet.vary`/`grid` (vary <= 9 cells) + `background`
   (magenta/green = prompt-level key color only; transparent = loud error, REST
   only) + optional `anchor` (abs-resolved; editing the anchor file re-busts the
   whole pack) + `candidates` N (overgen per sheet, `__cN`) + loud `max_jobs` cap.
2. `scripts/gen_batch.py --jobs <jobs.json> --concurrency 3` — as above;
   sheet-first = ~7 assets per paid call.
3. `scripts/slice_pack.py --jobs <jobs.json>` — bg_fix -> detect_regions ->
   HARD count gate (region_count == cells; mismatch rejects the whole sheet:
   a merged/empty cell would silently mislabel axes) -> slice with axis-named
   outputs into per-sheet subdirs.

Soft-glow cells (fire/halo/mist) keep magenta spill after key_matte — re-key
those from the raw sheet on the canvas (`alpha --method corridorkey`, needs the
videoGenRoot CorridorKey install) or route dual-plate; hard silhouettes cut
clean. Multiple configs sharing one `out_dir` need explicit distinct `--out`
paths (default `<out_dir>/jobs.json` collides). Expansion is deterministic:
re-running an edited config re-busts only the sheets whose prompts changed.

## Disk Pack vs Canvas Pack

Same expander (`expand_jobs.py`), two hosts — pick by how the pack gets USED, not
by convenience:

- **Disk** (above: `expand_jobs.py` -> `gen_batch.py` -> `slice_pack.py`): a
  one-off tmp conveyor. Cheapest path, no UI, no persisted config — the pack
  config is a file written once and the run ends in a **handoff** (below) to
  `nt-asset-workflow`. Pick this for a throwaway batch nobody revisits.
- **Canvas** (`recipe.pack` on a recipe card — `ai_studio/assets/canvas/README.md`'s
  **Pack mode**): the config LIVES on a recipe card — visible, editable,
  repeatable. Runs are resumable (`--run`) and single-sheet-regenerable
  (`--sheet`); style comes from a composable style card (text + ref image
  composition); the lead picks/reviews sheets in the UI before paying for a
  slice. Pick this when the pack will be revisited, tuned, or needs the lead's
  eyes on it before spending more generation calls.

Both call the SAME `expand_jobs.py` — the table is host-side wiring, not a
second expander:

| Disk config (`pack.json`) | Canvas (`recipe.pack` + `recipe.params`) |
|---|---|
| `background`: `magenta`\|`green`\|`transparent` (transparent = loud error, REST-only — v1 targets codex) | `params.bg_key` hex — only `#ff00ff`/`#00ff00` accepted at preview/generate time; **no transparent path exists** on canvas v1 (no third hex maps to a background) |
| `anchor` (one optional file, becomes every job's `input_image`) | no `anchor` field — the style card's ref image + the recipe card's own member images (<=5 total) are the refs, sent to every sheet |
| `jobs.json`'s `out`/`input_image` (real — `gen_batch.py` writes to `out`, reads `input_image` from the anchor) | `out`/`input_image` are **dead fields** — canvas calls the engine seam directly per the card's `recipe.engine` (codex, agy, or `both` = every sheet on BOTH engines at 2x the calls; sheet identity = axes+engine), never `gen_batch.py` |
| `sheet.grid` — any positive `[rows, cols]` | `pack.grid` — each of `[rows, cols]` capped **1..3** (canvas-only restriction) |
| `gen_batch.py` skip-if-exists keys per output file (so `candidates > 1` re-runs correctly) | `--run` resume dedups by `sheet_axes` only — with `n_candidates > 1` the first landed candidate "satisfies" the sheet and later candidates are skipped on resume (known v1 gap, T0332; harmless at the default `n_candidates: 1`) |

Rule: on a canvas pack, **ALWAYS run `recipe-pack-preview` before
`recipe-pack-generate`** and read the printed sheets/prompts — it is the only
preview that reflects what the expander will actually send (single-image
Generate builds its prompt a different way, and the generate verb itself has
no dry-run).

Rule (agy packs): on `engine: "gemini"` the model holds the grid and the flat
key background, but with the template's default constraints it reliably draws
thin separator lines between cells, and the slice count gate rejects the sheet
(`region_count` = cells+1 — the line cross is an extra region; 2/2 REJECT in
the 2026-07-07 smoke). Put an explicit no-lines clause in the CARD prompt —
"objects float on the continuous flat background — ABSOLUTELY NO dividing
lines, separators, borders or frames between cells, the background color runs
unbroken across the whole image" — with it the smoke sheet sliced 4/4 first
try. codex has not needed this clause.

## Handoff To Asset Workflow

All generation paths produce raw source images only. The deterministic asset
pipeline still owns cleanup, metadata, and project/library acceptance:

- the current game: game-specific prompts, contracts, rejected-candidate notes,
  and durable provenance when the game needs them.
- `nt-asset-workflow`: source-first decision, intake, crop, prepared outputs,
  license routing, asset storage owners, and project/library handoff.
- `ai_studio/quality/README.md`: select existing player-clarity/art rules for the
  assembled screen and record evidence.

Clean crops prove the preparation step, not the screen. Judge the assembled game screen
against the fake shot or art bible. Keep raw generations in `tmp/` until
accepted.

## Maintenance

- Canonical skill files live in `.codex/skills/`.
- Run `node ai_studio/core_harness/agent_surfaces/sync.mjs` after edits to regenerate the
  `.claude/skills` pointer.
- Update the references instead of re-deriving commands when a working recipe
  changes.
