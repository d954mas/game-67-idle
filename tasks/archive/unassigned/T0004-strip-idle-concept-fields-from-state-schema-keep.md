---
id: T0004
title: Strip idle-concept fields from state schema, keep neutral seed
status: done
epic: ""
priority: P1
tags: [state, pipeline]
created: 2026-06-12
updated: 2026-06-12
---

## What

The 2026-06-12 pipeline review found that `state/game_state.schema.json`
still carried the dead idle-concept skeleton (fields 15-35: seed_points,
click_power, income, milestones, jobs, feedback), contradicting the "no
active concept" rule and biasing the next concept. Remove them, keeping the
neutral seed fields 1-14 (test/render fields, settings, tutorial, wallet,
items/inventory/equipment as pattern examples).

Removed field ids are kept in `reserved` so future schemas never reuse them.
No version bump: there are no external saves, and the parser ignores unknown
keys in old dev saves.

## Done when

- [x] Schema contains only fields 1-14; ids 15-35 moved to `reserved`
- [x] Codegen REQUIRED_FIELDS list and `game_state.c.in` template cleaned of
  removed fields (6 template blocks: defaults, validate, to_json, get_path,
  set_path, from_json)
- [x] `src/main.c` placeholder uses only remaining fields (palette from
  test_ui_clicks)
- [x] DevAPI scripts (smoke_test, agent_playtest, state_roundtrip) exercise
  the same contract surfaces via remaining fields
- [x] Native build + all DevAPI scenarios + full pipeline gate pass

## Open questions

## Log

- 2026-06-12: Removed fields 15-35 from schema, added 21 reserved entries.
  Cleaned `tools/state_codegen/generate_state.py` REQUIRED_FIELDS and six
  per-field blocks in `tools/state_codegen/game_state.c.in`. `src/main.c`:
  seed_click no longer writes seed_points/visual_stage; palette now derives
  from test_ui_clicks. Rewrote idle-field sections of
  `tools/devapi/scenarios/state_roundtrip.py` onto wallet/test_ui_clicks/
  tutorial.done (same coverage: int set, fractional reject, patch, bool
  get/set/restore, non-bool reject, save/reset/load roundtrip). Updated
  smoke_test.py and agent_playtest.py checks to wallet.soft. Fixtures were
  already neutral.
- 2026-06-12: Evidence: codegen regenerated (h/c/schema.gen.h); `cmake
  --build --preset native-debug` clean; `smoke_test 9123`,
  `state_roundtrip 9124` (ALL PASSED, runtime schema shows 22 reserved
  entries and 14 fields), `full_probe 9123`, `settings_modal 9125`,
  `ui_button_text 9126` all exit 0; `node tools/pipeline_validate.mjs` ok.
