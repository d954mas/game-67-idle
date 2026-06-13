# Tools Layout

`tools/` intentionally contains several layers. Do not treat every file here as
equally portable.

The machine-readable map is `tools/tool_layers.json`.

## Layers

### `portable_ai_pipeline`

Generic workflow tools for AI-assisted development. These move to a clean new
game project:

- `tools/ai.mjs` - fast facade for `start`, `focus`, `context`, `checkpoint`,
  `run`, `validate`, `status`, and `reflect`
- `tools/ai.test.mjs`
- `tools/ai_profile/`
- `tools/taskboard/`
- `tools/game_context/`
- `tools/bootstrap/export_base.mjs`
- `tools/pipeline_validate.mjs`
- `tools/skills_eval.mjs`
- `tools/skills_sync.mjs`
- `tools/assets/new_art_job.mjs`

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
5. Run `node tools/pipeline_validate.mjs` after cleanup/export.
