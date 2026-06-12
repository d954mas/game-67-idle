---
id: T0002
title: "Harden pipeline gate: state codegen check + cmake configure"
status: done
epic: ""
priority: P1
tags: [pipeline, validation]
created: 2026-06-12
updated: 2026-06-12
---

## What

agents-best-practices audit (2026-06-12) found that
`tools/pipeline_validate.mjs` did not touch the C runtime seed: a broken
state schema or build configuration would pass the "full" gate. Add runtime
seed checks to the gate, conditional on file presence so the workflow-only
exported copy of the script keeps working.

## Done when

- [x] `pipeline_validate.mjs` runs `py -3.12
  tools/state_codegen/generate_state.py` when `state/game_state.schema.json`
  exists
- [x] `pipeline_validate.mjs` runs `cmake --preset native-debug` (configure)
  when `CMakePresets.json` exists
- [x] Both steps are skipped in workflow-only exports (no schema / no
  presets)
- [x] `AI_PIPELINE.md` gate description matches the new behavior
- [x] Full gate passes end to end

## Open questions

## Log

- 2026-06-12: Extended `run()` in `tools/pipeline_validate.mjs` to accept an
  executable override; added conditional "state codegen" and "cmake
  configure" steps before the portable export. Updated the gate description
  in `AI_PIPELINE.md`.
- 2026-06-12: Evidence: `node tools/pipeline_validate.mjs` -> ok; new steps
  visible in output ("state codegen" -> "state generated files are up to
  date", "cmake configure" -> "Build files have been written to
  build/_cmake/native-debug"); exported project re-validated clean (export
  contains no schema/presets, so runtime steps skip there by design).
