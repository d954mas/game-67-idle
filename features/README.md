# Features

Reusable feature packs live here as `features/<feature-id>/`.

A feature is a copyable game capability, not just one source file. It can include
code, assets, state schema, migrations, UI, DevAPI hooks, tests, examples, and
notes. The current model is deliberately simple: copy the feature into a template
or game, then customize that local copy for the project.

There is no plugin manager, install command, dependency solver, or automatic
enable/disable system here yet. Keep feature packs small enough that a human or
agent can inspect and copy them safely.

## Install And Flags

The shared folder is the upstream feature pack. A template or game uses a
feature by carrying an installed copy of the needed schema/code/assets/build
wiring in its own tree. Generated files should be reproducible from that local
copy, either in the build directory or as explicitly checked-in generated
outputs when a project chooses that policy.

Every feature must include an install manual. Use `INSTALL.md` for the concrete
copy/build/enable/verify/uninstall steps. Keep high-level purpose and ownership
in `README.md`; keep exact integration commands in `INSTALL.md`.

Feature runtime flags use `FEATURE_<FEATURE_ID_UPPER_SNAKE>`, for example
`FEATURE_GAME_STATE`. Dev-only integrations stay behind their own global guard,
for example generated state DevAPI code compiles only under
`FEATURE_GAME_STATE && GAME_DEVAPI_ENABLED`.

## Suggested Shape

```text
features/<feature-id>/
  README.md        what it does, how to copy it, dependencies, origin
  INSTALL.md       exact install, enable/disable, verify, uninstall steps
  feature.json     optional metadata when the feature needs it
  src/             code to copy into the game or template
  assets/          source assets or packed asset inputs
  state/           schemas, migrations, or seed state
  tests/           focused validation or smoke tests
  example/         tiny runnable example when useful
```

Only add the folders a feature actually needs. For example, a settings screen can
be a feature with UI code, state keys, assets, and a short integration note.

## Current Packs

- `game-state/`: schema-first generated GameState, save/load contract,
  migrations, and DevAPI state adapters. This is the first feature pack and the
  reference shape for future reusable features.

## Rules

- A feature must be self-contained enough to copy without guessing hidden files.
- `README.md` is required for each feature folder.
- `INSTALL.md` is required for each feature folder, even when the install is
  only "already installed by this template".
- List dependencies explicitly: engine APIs, template systems, other features,
  assets, state keys, build changes, and runtime hooks.
- Do not reach into a specific game's globals. Use the game/template's public
  world, state, and system boundaries.
- After copying a feature into `games/<game-id>/` or `templates/<template-id>/`,
  that project owns its copy and may edit it freely.
- Promote useful local improvements back here only after they are generalized.

## Later

A future architecture pass can add installer scripts, dependency checks, preview
examples, or richer enable/disable tooling. Until then, install manuals are the
contract that keeps feature packs copyable.
