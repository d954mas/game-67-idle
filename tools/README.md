# Tools Layout

`tools/` is layered. This is a route map, not a manual.

## Contract

AI tooling must make game work faster:

- default output is short and actionable;
- broad/slow/destructive/artifact-heavy modes need explicit flag or task need;
- stale diagnostics do not block normal work;
- scratch output goes to `tmp/` unless promoted;
- narrow proof runs before broad/final checks;
- failures report the next useful action.

Use passive defaults; reserve `--verbose`, `--all`, `--full`, or explicit
subcommands for exhaustive work.

## Portable AI Pipeline

Copied to clean games by `tools/bootstrap/export_base.mjs`:

- agent facade and validators: `tools/ai.mjs`, `tools/pipeline_validate.mjs`,
  `tools/context_budget.mjs`, `tools/doc_reference_check.mjs`,
  `tools/skills_eval.mjs`, `tools/skills_sync.mjs`;
- workflow state/profiling: `tools/taskboard/`, `tools/ai_profile/`;
- game startup/runtime proof: `tools/game_context/`, `tools/product_gate/`;
- reusable asset helpers: `tools/assets/`;
- export helpers: `tools/bootstrap/`.

Product gates route through `node tools/ai.mjs gate`, `critic`, and `close-slice`.
Policy lives in `docs/ai-pipeline/quality-validation.md` and game/visual skills.

## Runtime Infrastructure

Reusable only with a compatible native/runtime stack:

- `tools/state_codegen/`
- `tools/devapi/`
- `tools/perf/`

Copy/adapt after engine/runtime policy is known.

## Asset Routing

Do not load this README as an asset manual. Use:

- `.codex/skills/game-asset-pipeline/` for source/provenance/cutout/pack work;
- `.codex/skills/generated-game-ui-assets/` for UI source sheets, slice9,
  atlases, derivation, composition proof, and responsive UI gates.

Short rule: preserve provenance, separate generated/runtime assets, validate
manifests before final-art claims, and keep timing/debug data out of normal blockers.

## Cleanup And Validation

Generated caches (`__pycache__/`, `*.pyc`) are ignored scratch.

Validation:

```powershell
node tools/ai.mjs validate
node tools/ai.mjs validate --review
node tools/ai.mjs validate --full
```

Underlying: `tools/pipeline_validate.mjs`. `--review` adds strict context
pressure; `--full` exports to `tmp/pipeline-validate-<stamp>/` and keeps newest 3.
