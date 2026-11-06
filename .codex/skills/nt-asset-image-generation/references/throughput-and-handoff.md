# Throughput And Handoff

Use for asset families, batches or repeated generation. Single-image work can
follow the entrypoint directly.

## Choose A Batch Shape

Prefer source sheets for opaque families that can be separated reliably;
per-asset generation remains useful for distinct compositions or soft effects.
Use bounded concurrency supported by the provider and the task's spending scope.
Preview large runs and reuse valid unchanged outputs rather than regenerating.

For repeated axes such as grade/material/shape, `scripts/expand_jobs.py` can
expand a declarative pack into jobs. Inspect the planned jobs before paid work.
See `references/build_spec_pack_expander_2026-07-07.md` for the pack format.

Generation adapters must satisfy `references/generation-paths.md`.
`gen_batch.py` defaults to a legacy adapter; do not execute that default route.
Use `--generator <approved-adapter.py>` only with a compatible, verified adapter,
or use the available official native/tool batch capability. `--dry-run` only
prints planned commands and does not establish that an adapter is approved.

For completed source sheets, `scripts/slice_pack.py --jobs <jobs.json>` prepares
axis-named outputs through bg_fix, region detection and slicing. Preserve the
hard region-count gate: merged or missing cells can silently mislabel assets.
Use distinct jobs/output paths when configs share a directory.

## Disk Or Canvas

- A disk pack is useful for a temporary batch; accepted output still receives
  the required Canvas provenance and asset-workflow handoff.
- A Canvas recipe pack keeps editable style/configuration, review and resumable
  generation together when the family will be revisited. Read
  `ai_studio/assets/canvas/contracts/recipe-pack.md` for its schema and controls,
  and `contracts/generation-and-export.md` in that module for style-lock,
  provenance and generation-state requirements.

Before Canvas generation, verify its configured adapter satisfies
`references/generation-paths.md`; current legacy defaults do not. With a
permitted adapter, run `recipe-pack-preview` before `recipe-pack-generate` and
inspect the printed prompts/sheets: generation has no dry-run and single-image
preview uses a different prompt path. Preview is a spending boundary, not proof
that the adapter or generated output is valid.

Opaque recipe/pack placeholders have their own background restrictions. Those
do not prevent importing a genuine RGBA image produced by a supported tool.
Choose alpha preparation by the actual source/background contract; soft glow
may need different treatment from hard silhouettes.

## Acceptance And Handoff

Generation produces source images. Keep raw rejects in `tmp/`; preserve accepted
sources, prompts, references, source decisions and integrity in Canvas and the
owning game's durable metadata. Keep production style locks and Canvas review
states truthful; a successful generation/export does not authorize promotion.

`nt-asset-workflow` owns cleanup, license routing, storage and game integration.
Verify crops and composable parts after preparation. For game-use claims, inspect
the assembled screen against the accepted art direction and record applicable
quality evidence. Report incomplete integration separately from source-art success.
