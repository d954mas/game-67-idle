# Tools Layout

`tools/` intentionally contains several layers. Do not treat every file here as
equally portable. This file is the source of truth for the layers; the sections
below describe what moves to a new game vs what stays project-specific.

## Tool Contract

AI tooling must help the game work move faster. Defaults should be quiet,
bounded, and advisory:

- default commands print short actionable output;
- slow, broad, destructive, artifact-generating, or deep-retrospective behavior
  must require an explicit flag or explicit user/task need;
- scripts must not turn stale generated diagnostics into blockers for normal
  game work;
- generated scratch outputs go under `tmp/` or another ignored path unless the
  lead explicitly promotes them;
- validation scripts should run the narrow proof first and reserve broad/final
  checks for release, portable-base, or shared-behavior changes;
- a script that finds a problem should report the next useful action, not create
  a new process obligation by default.

When adding or changing a tool, prefer `summary`/passive output as the default
and `--verbose`, `--all`, or an explicit subcommand for exhaustive inspection.

## Layers

### `portable_ai_pipeline`

Generic workflow tools for AI-assisted development. These move to a clean new
game project. The executable source of truth is the `COPY` allowlist in
`tools/bootstrap/export_base.mjs`; `tools/bootstrap/export_base.test.mjs` proves
the exported base includes task guides and generated skill pointers.

Portable categories:

- facade, validation, context, and doc guards: `tools/ai.mjs`,
  `tools/pipeline_validate.mjs`, `tools/context_budget.mjs`,
  `tools/doc_reference_check.mjs`, `tools/skills_eval.mjs`, and
  `tools/skills_sync.mjs`;
- workflow state and profiling: `tools/taskboard/`, `tools/ai_profile/`;
- game startup/runtime proof: `tools/game_context/`, `tools/product_gate/`;
- reusable asset pipeline helpers under `tools/assets/`;
- bootstrap/export helpers under `tools/bootstrap/`.

Use product-gate helpers through `tools/ai.mjs`: `gate` for screenshot/player
readability, `critic` for a reusable visual critique packet, and `close-slice`
before handoff/review. `tools/product_gate/` also contains responsive layout
and slice-hygiene audits. Detailed gate policy lives in
`.codex/skills/game-feature-iteration/` and
`.codex/skills/game-visual-art-direction/`; this README only names the route.

`tools/game_context/new_prototype.mjs` starts a new game concept and
`tools/game_context/iteration_context.mjs` guards broad implementation until the
startup context and live-state matrix exist.

Use asset helpers through the matching skill instead of loading this README as
a reference manual:

- general source/provenance/cutout/pack work:
  `.codex/skills/game-asset-pipeline/`;
- generated runtime UI source families, slice9, atlases, derivation,
  composition proof, and responsive UI gates:
  `.codex/skills/generated-game-ui-assets/`.

The short rule: art tools must preserve source provenance, keep generated and
runtime assets separate, validate manifests before final-art claims, and record
human-readable proof outputs without turning timing/debug artifacts into normal
workflow blockers.

### `reusable_game_infrastructure`

Reusable only when the next game keeps a compatible native/runtime stack:

- `tools/state_codegen/`
- generic `tools/devapi/` clients, capture helpers, and probes

Copy or adapt these deliberately after the engine/runtime policy is known.

### `project_specific_67_world`

Specific to the current 67 World game, its release package, balance, art packs,
or child-test evidence. These should be deleted, archived, or intentionally
adapted when starting a different game:

- `tools/project_67_world/`
- `tools/project_67_world/assets/build_67_world_*`
- `tools/project_67_world/assets/validate_67_world_pack_inputs.py`
- `tools/project_67_world/balance/simulate_67_world.py`
- `tools/project_67_world/devapi_scenarios/*.py`
- `tools/project_67_world/package_native_release.mjs`
- `tools/project_67_world/release_candidate_audit.py`

### Generated Cache

`__pycache__/` and `*.pyc` files are ignored scratch artifacts. They are safe to
delete and should never be copied into a new project.

## New Game Cleanup Rule

For a clean new game:

1. Keep `portable_ai_pipeline`.
2. Decide whether `reusable_game_infrastructure` matches the selected runtime.
3. Remove or archive `project_specific_67_world`.
4. Delete generated caches.
5. Run `node tools/pipeline_validate.mjs` after normal cleanup, or
   `node tools/pipeline_validate.mjs --full` after export/runtime template
   changes.

`--quick` is the default and is the right mode after narrow pipeline/tooling
edits. `--full` is heavy (it exports the repo into `tmp/` and re-runs every
suite inside the export) and is reserved for portable-base/export/runtime/
release gates. Each `--full` run leaves a `tmp/pipeline-validate-<stamp>/` copy;
every run now prunes those to the newest 3 by default (`--keep-exports <n>` to
change, `--no-prune` to disable).
