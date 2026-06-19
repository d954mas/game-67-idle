# Throughput And Handoff

Load this reference when generating many assets, re-running changed jobs, or
connecting generated art to the deterministic asset pipeline.

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
assets through intake, crop, and assemble. This is usually much cheaper than one
generation per asset.

Reserve per-asset dual-plate generation for a few genuinely soft hero effects.
Use route warnings from the asset pipeline to decide where the extra generation
cost is justified.

## Handoff To The Asset Pipeline

All generation paths produce raw source images only. The deterministic pipeline
still owns cleanup and runtime integration:

- `generated-game-ui-assets`: prompt packet, intake, crop, runtime PNGs,
  composition, derivation audits.
- `game-visual-art-direction`: art direction checks and screen-level critique.
- `node tools/ai.mjs gate`: product/visual gate for the assembled screen.

Clean crops prove the pipe, not the screen. Judge the assembled game screen
against the fake shot or art bible. Keep raw generations in `tmp/` until
accepted.

## Maintenance

- Canonical skill files live in `.codex/skills/`.
- Run `node tools/skills_sync.mjs` after edits to regenerate the
  `.claude/skills` pointer.
- Update the references instead of re-deriving commands when a working recipe
  changes.
