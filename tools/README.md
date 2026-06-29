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

Copied by `tools/bootstrap/export_base.mjs`:

- validators: `ai_studio/core_harness/validation/doc_reference_check.mjs`,
  module-owned tests;
- generated agent surfaces: `ai_studio/core_harness/agent_surfaces/`;
- workflow state/profiling: `ai_studio/taskboard/`, `ai_studio/core_harness/profiling/`;
- game startup/runtime scaffolding: `tools/game_context/`;
- quality rules: `ai_studio/quality/`;
- asset browser/review surface: `ai_studio/assets/asset_viewer/`;
- reusable asset helpers: `tools/assets/`;
- export helpers: `tools/bootstrap/`.

Quality starts from `ai_studio/quality/README.md`: pick the changed-work group,
start with the group's `001` rule, then apply only the relevant higher-numbered
checks.

## Runtime Infrastructure

Reusable only with a compatible native/runtime stack:

- `tools/state_codegen/`
- `tools/devapi/`
- `tools/perf/`

Copy/adapt after engine/runtime policy is known.

## Asset Routing

Do not load this as an asset manual. Use:

- `ai_studio/assets/asset_viewer/` for browsing the shared library, reviewing
  game-local assets, pulling reusable assets, and promoting selected assets;
- `.codex/skills/game-asset-pipeline/` for source/provenance/cutout/pack work;
- `.codex/skills/generated-game-ui-assets/` for UI source sheets, slice9,
  atlases, derivation, composition proof, and responsive UI checks.

Short rule: preserve provenance, separate generated/runtime assets, validate
manifests before final-art claims, and keep timing/debug data out of normal blockers.

## Cleanup And Validation

Generated caches (`__pycache__/`, `*.pyc`) are ignored scratch.

Validation is owned by modules:

```powershell
node ai_studio/core_harness/validation/doc_reference_check.mjs
node ai_studio/architecture_map/validate_map.mjs
node ai_studio/taskboard/cli.mjs validate
node --test tools/bootstrap/export_base.test.mjs
```
