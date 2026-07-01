# Features

Reusable feature packs live here as `features/<feature-id>/`.

A feature is a copyable game capability, not just one source file. It can include
code, assets, state schema, migrations, UI, DevAPI hooks, tests, examples, and
notes. The current model is deliberately simple: copy the feature into a template
or game, then customize that local copy for the project.

There is no plugin manager, install command, dependency solver, or automatic
enable/disable system here yet. Keep feature packs small enough that a human or
agent can inspect and copy them safely.

## Suggested Shape

```text
features/<feature-id>/
  README.md        what it does, how to copy it, dependencies, origin
  feature.json     optional metadata when the feature needs it
  src/             code to copy into the game or template
  assets/          source assets or packed asset inputs
  state/           schemas, migrations, or seed state
  tests/           focused validation or smoke tests
  example/         tiny runnable example when useful
```

Only add the folders a feature actually needs. For example, a settings screen can
be a feature with UI code, state keys, assets, and a short integration note.

## Rules

- A feature must be self-contained enough to copy without guessing hidden files.
- `README.md` is required for each feature folder.
- List dependencies explicitly: engine APIs, template systems, other features,
  assets, state keys, build changes, and runtime hooks.
- Do not reach into a specific game's globals. Use the game/template's public
  world, state, and system boundaries.
- After copying a feature into `games/<game-id>/` or `templates/<template-id>/`,
  that project owns its copy and may edit it freely.
- Promote useful local improvements back here only after they are generalized.

## Later

A future architecture pass can add feature manifests, install scripts, dependency
checks, preview examples, or an enable/disable switch. Until then, this folder is
the shared pool of inspected, copyable feature packs.
