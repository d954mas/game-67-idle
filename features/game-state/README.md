# Game State Feature

Reusable schema-first game-state feature pack.

This is the first feature pack under `features/` and should be treated as the
example shape for future reusable features: a small README, optional metadata,
copyable scripts, references, and explicit integration boundaries.

## Purpose

Use this feature when a game needs typed, persistent, DevAPI-visible state that
agents can inspect and update safely.

The feature provides:

- schema-first `GameState` generation;
- stable persisted field ids and `reserved` tombstones;
- generated C storage, JSON serialization, and DevAPI adapters;
- save/load envelope and migration guidance;
- review rules for state changes, fixtures, and runtime proof.

## Contents

```text
features/game-state/
  README.md
  INSTALL.md
  feature.json
  references/
    contract.md
    workflow.md
    review.md
  scripts/
    generate_state.py
    generate_state_test.py
    run_tests.py
    state_modules_test.py
    state_codegen/
  benchmarks/
    benchmark_codegen.py
    baseline.json
    fixtures/multi_fragment.schema.json
```

## Integration Model

There is no general feature installer yet. This feature's correct use is:

1. Keep the reusable generator and references here.
2. Keep an installed copy in each template or game that wants state:
   `state/`, `state/migrations/`, `src/game_storage.*`, and CMake/runtime
   wiring.
3. Generate `game_state.*` from that local schema into the build directory, or
   into a checked-in generated folder if that project explicitly chooses to
   version generated C.
4. Let that template or game own its local schema and migrations.

The default template has this feature installed and enabled by default. New games
created from that template inherit the installed copy.

For exact install, enable/disable, verification, and uninstall steps, read
`features/game-state/INSTALL.md`.

Default template integration uses:

- schema source: `templates/template/state/game_state.schema.json`;
- generated output: `templates/template/build/<config>/generated/game-state/`;
- migrations: `templates/template/state/migrations/`;
- always compiled (the `FEATURE_GAME_STATE` on/off flag was removed
  2026-07-07 — a game without state is impossible);
- DevAPI registrations from the hand-written `src/game_save_devapi.c` registry
  dispatch (`game_save_register_devapi()`) only when `GAME_DEVAPI_ENABLED` is
  also on;
- semantic runtime commands and domain actions in the game or template source.

For a game-specific variant, pass explicit paths:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state.py --schema games/<game-id>/state/game_state.schema.json --out-dir games/<game-id>/src/generated
```

Runtime state cannot be disabled (no build flag). To remove DevAPI commands
from the build, configure `GAME_DEVAPI_ENABLED=OFF`.

## Commands

Generate from the template schema:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state.py --schema templates/template/state/game_state.schema.json
```

Without `--out-dir`, the command writes to `build/generated/game-state` under
the template or game that owns the required `--schema` path.

Run generator tests:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/run_tests.py
```

The aggregate runner executes these focused suites:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state_test.py
node ai_studio/dev_environment/python_run.mjs -m unittest features/game-state/scripts/state_modules_test.py
node ai_studio/dev_environment/python_run.mjs -m unittest features/game-state/benchmarks/benchmark_codegen_test.py
```

Run the advisory local benchmark:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/benchmarks/benchmark_codegen.py
```

## Boundaries

- The schema is source of truth. Do not hand-edit generated `game_state.*`
  files.
- Runtime feature code always compiles. DevAPI registration is gated by
  `GAME_DEVAPI_ENABLED`; release builds must not compile or register those
  commands.
- Generated state stores and serializes data. Gameplay rules belong in domain
  actions owned by the game or template.
- Migrations transform old JSON before parsing into current runtime structs.
  They must not call domain actions.
- Raw `game.state.*` writes are for debug/editor overrides, fixtures, and
  targeted tests. Bots and gameplay checks should prefer semantic actions.
- Runtime proof collection belongs to `ai_studio/runtime_automation/`.
- Quality acceptance belongs to `ai_studio/quality/`.
- DevAPI command registration belongs to the installed feature copy, and release
  builds must keep DevAPI disabled.

## Feature-Pack Example Rules

Use this folder as the minimum bar for future feature packs:

- explain what the feature does and what it does not own;
- list dependencies and copy points;
- keep reusable scripts close to the feature;
- keep references specific to the feature;
- expose an agent-facing skill only as a thin router when discoverability helps.
