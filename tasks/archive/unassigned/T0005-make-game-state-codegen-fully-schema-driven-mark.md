---
id: T0005
title: Make game_state codegen fully schema-driven (markers in template)
status: done
epic: ""
priority: P1
tags: [state, pipeline, codegen]
created: 2026-06-12
updated: 2026-06-12
---

## What

T0004 revealed that `game_state.c.in` was a hand-maintained template with
per-field code in 6 sections, so every new scalar field required ~6 manual C
edits. Replace the per-field sections with `/*@GEN:...@*/` markers and
generate them from the schema in `generate_state.py`:

- `ENUM_TABLES` (name arrays + `game_state_<enum>_name` functions; header
  declarations also generated)
- `DEFAULTS`, `VALIDATE`, `TO_JSON` (with nested-group objects),
  `GET_PATH`, `SET_PATH`, `FROM_JSON` (with nested-group reads)

Generated: all scalar fields (bool/int/float/string/enum, dotted paths,
`*_index` enum fields get a name alias path). Hand-written in the template:
structural patterns only — items map, inventory id list, ref-checked
`string?`. The post-expansion coverage check still verifies every schema
field appears in the final source.

## Done when

- [x] Adding a scalar field to the schema requires zero C edits: regenerate
  -> build -> field works through defaults/validation/JSON/DevAPI
- [x] Generated output is semantically equivalent to the old hand-written
  code (diff reviewed: per-field checks instead of combined conditions,
  uniform parse var names, explicit tutorial_done default)
- [x] All DevAPI scenarios and the full pipeline gate pass
- [x] `game-state-management` skill documents the generated-vs-manual split

## Open questions

## Log

- 2026-06-12: Markerized `tools/state_codegen/game_state.c.in` (7 markers),
  added render functions and marker expansion to
  `tools/state_codegen/generate_state.py`; header enum-name declarations now
  generated per enum.
- 2026-06-12: Zero-touch proof: temporarily added `probe.value`
  (int, nested, 0..100) and `probe_ready` (bool) to the schema only;
  regen + build clean; DevAPI run: set probe.value=42 OK, 101 rejected
  ("integer value out of range"), probe_ready=true OK. Fields reverted.
- 2026-06-12: Evidence: diff old vs new generated `game_state.c` — 133
  mechanical lines, semantics equal; `smoke_test 9123`,
  `state_roundtrip 9124` (ALL PASSED), `settings_modal 9125`,
  `ui_button_text 9126` all exit 0; `node tools/pipeline_validate.mjs` ok
  (includes skills eval PASS x6 after skill doc update).
