# Playable Gates

Load this when feature work touches product feel, visual quality, named
references, build/launch/release configuration, or prototype handoff.

## Reference-Driven Work

If a feature is based on a named reference, use `nt-design-knowledge` before
coding. The reference study must be ready enough for the implementation claim:
sources, observed facts, borrow/avoid/copy-risk, current-build mismatch, and
next screenshot/scenario proof.

If evidence is missing, state that the reference study is not ready for
implementation and gather sources or ask for user material.

## Quality Checks

Use `nt-quality-checks` and record selected rule IDs when a task changes:

- player-facing clarity or UI: `QCLR`;
- game design loop or motivation: `QDES`;
- GDD/source package: `QGDD`;
- art/visual finish: `QART`;
- assets/license/runtime readiness: `QASSET`;
- technical behavior or build proof: `QTECH`.

Blocking quality review or lead rejection blocks feature/content expansion
unless the user accepts debt for the current slice.

## Runtime Proof

For player-facing changes, prefer evidence from the active game's primary
runtime. Use `nt-runtime-automation` for DevAPI discovery, native iteration,
screenshots, recordings, pixel/readability checks, and stable-frame proof.

The slice is done when runtime evidence supports the player-facing claim, not
when probes are merely green.

## Build, Launch, And Release

When work is about build/launch/release configuration:

1. Discover local build sources before inventing commands: `CMakePresets.json`,
   `.vscode/tasks.json`, package scripts, engine docs, or examples.
2. Separate configure, build, run, release, serve, and package tasks.
3. Keep asset-pack generation explicit unless the project intentionally requires
   automatic packs.
4. After editing config, parse the file, list available presets/tasks if useful,
   run the smallest affected build, and state output paths.

## Handoff

Before committing or handing off a prototype slice, record:

- build evidence;
- scenario/probe evidence;
- screenshot or recording evidence when player-facing work changed;
- selected quality rule IDs and task-log outcomes when quality rules applied;
- known debt or accepted gaps.

Do not promise push until push/upstream state is checked.
