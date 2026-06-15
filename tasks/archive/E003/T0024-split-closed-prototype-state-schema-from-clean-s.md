---
id: T0024
title: Split closed prototype state schema from clean seed state
status: done
epic: E003
priority: P1
tags: []
created: 2026-06-15
updated: 2026-06-15
---

## What

Complete the clean-template split after `T0022`: the default runtime no longer
compiles closed prototype actions/assets, but the shared generated
`game_state.c/.h` still contains Splash Rods and Rune Marches fields. Split or
parameterize state codegen so the clean seed state is minimal, while closed
prototype state remains available through the explicit closed-prototypes path.

## Done when

- [x] Default clean seed generated state has no fishing/Rune fields, enums, or
      JSON path handlers.
- [x] Closed prototype build still has the legacy fishing/Rune state fields it
      needs.
- [x] State codegen outputs are not clobbered by switching between default and
      closed-prototype builds.
- [x] Fixtures/migrations/tests cover both clean seed and closed prototype
      state paths, or document why closed state is inspection-only.
- [x] `node tools/pipeline_validate.mjs` or a narrower state-codegen validation
      proves the split.

## Open questions

- Resolved: keep one `GameState` ABI per build, selected by schema. Default
  builds generate clean state into the build tree; closed-prototype builds
  generate legacy Rune/Fishing state into their own build tree.

## Log

- 2026-06-15: Created during `T0022`. Runtime/actions/assets are split, but
  `state/game_state.schema.json` is still shared and contains project-specific
  closed prototype fields.
- 2026-06-15: Done. Added clean `state/game_state.schema.json`, moved legacy
  Rune/Fishing fields to `state/closed_prototypes_game_state.schema.json`,
  parameterized `tools/state_codegen/generate_state.py`, switched CMake state
  codegen to build-local output, and added variant tests.
- 2026-06-15: Verified with `py -3.12 -m unittest
  tools.state_codegen.generate_state_test`, default and closed native configure
  plus build, clean/closed generated-output token audits, `node
  tools/pipeline_validate.mjs` through `node tools/ai.mjs run`, and `node
  tools/ai.mjs status --require-current-scope-usable`.
