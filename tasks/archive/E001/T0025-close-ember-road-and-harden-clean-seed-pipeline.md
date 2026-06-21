---
id: T0025
title: Close Ember Road and harden clean-seed pipeline
status: done
epic: E001
priority: P0
tags: []
created: 2026-06-21
updated: 2026-06-21
---

## What

Close the stopped Ember Road prototype and harden the reusable clean-seed
pipeline so a closed game cannot remain active through runtime, taskboard,
project docs, or validation hooks.

### Scope

- Preserve Ember Road through a snapshot tag and archived task evidence.
- Remove Ember Road runtime, assets, project docs, and game-specific tools from
  the working tree.
- Keep only reusable pipeline/seed lessons and validators.
- Restore the clean seed native runtime to build against the current native
  DevAPI ABI.
- Harden reset/doc-reference/pipeline validation so closed prototype artifacts
  do not keep driving live work.

### Out Of Scope

- No Ember Road visual, UX, gameplay, asset, or content continuation.
- No new game concept.
- No unrelated pipeline redesign beyond closeout/reset hardening.

## Done when

- [x] Ember Road is snapshotted and no longer the active game concept.
- [x] Ember Road tasks are dropped/archive-kept with explicit closeout logs.
- [x] Ember Road project docs, runtime assets, game-specific tools, and generic
  leaked game files are removed from the working tree.
- [x] The clean seed native runtime builds.
- [x] Reusable validation no longer hardcodes Ember Road gates.
- [x] Historical task archives do not fail live doc-reference checks after a
  prototype reset removes project artifacts.
- [x] `reset_to_seed` removes known generic game leakage.
- [x] Full pipeline validation passes with a prepared Python runner.

## Open questions

- None.

## Log

- 2026-06-21: Lead closed Ember Road. Tagged snapshot
  `ember-road-snapshot-2026-06-21`, dropped E003/T0014-T0024, moved tasks to
  `tasks/archive/E003/`, removed active game status, and reset runtime/assets/
  docs/tools to clean seed.
- 2026-06-21: Hardened reusable validation: removed Ember Road-specific
  validator hooks from `tools/pipeline_validate.mjs`, taught
  `tools/doc_reference_check.mjs` to ignore historical `tasks/archive/`, and
  updated DevAPI smoke guidance to use project-specific `tools/<game-id>/`
  scripts.
- 2026-06-21: Fixed clean seed native build against the current engine-native
  DevAPI ABI by using `game_devapi_ui.*`, descriptor-based command
  registration, and engine `nt_devapi`/`nt_devapi_net`.
- 2026-06-21: Installed pinned full-gate Python dependencies into bundled
  Python and validated imports: Python 3.12.13, numpy 2.4.6, scipy.ndimage.
- 2026-06-21: Evidence passed:
  `cmake --build --preset native-debug --target game_seed`,
  `node tools/taskboard/cli.mjs validate`, `node tools/ai.mjs validate --review`,
  and `AI_PIPELINE_PYTHON=<bundled-python> node tools/ai.mjs validate --full`.
