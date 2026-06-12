---
id: T0032
title: Harden state codegen schema-derived validation
status: done
epic: E001
priority: P1
tags: [state, tooling, release, validation]
created: 2026-06-12
updated: 2026-06-12
---

## What

Make `tools/state_codegen/generate_state.py` enforce the state schema as the true source of truth instead of relying on hard-coded templates with substring coverage checks.

This was identified during T0031 code review. The current generator is useful for the active slice but does not strictly validate field ids, defaults, min/max ranges, migration declarations, or generated ordering against `state/game_state.schema.json`.

## Done when

- [x] generator validates every persisted field property used by runtime code
- [x] generated `GAME_STATE_VERSION`, fields, defaults, ranges, and migrations come from schema data or are checked exactly against it
- [x] removed/renamed fields require an explicit reserved/migration decision
- [x] generation fails on schema/template drift with actionable errors
- [x] state roundtrip coverage includes a generator drift failure case or equivalent unit coverage
- [x] generated unsafe debug file saves use temp-file replacement or are intentionally restricted
- [x] migration fixtures are compared directly in automation, not only spot-checked

## Open questions

## Log

- 2026-06-12: Captured from T0031 code review residual risk after v2 migration/autosave/keyed-save fixes landed.
- 2026-06-12: Started hardening pass.
- 2026-06-12: Implemented strict state codegen contract checks for schema identity, version, field history/id order, defaults including zero/false drift, min/max, serializers, migration order, and DevAPI save/load wiring. Removed stale `game_state.c.in` dependency/file, made generated unsafe saves use temp-file replacement, split persisted save JSON from DevAPI view JSON, removed v2 field ownership from v0->v1 migration, and made migration fixture comparison exact by persisted schema fields.
- 2026-06-12: Review cycle: two subagent reviews found stale template dependency, derived fields in saves, weaker default/id/migration checks, subset fixture comparison, and task closure gaps; all blocking findings were fixed. Final subagent review reported no blocking findings.
- 2026-06-12: Evidence: `py -3.12 tools/state_codegen/generate_state.py --self-test`; `py -3.12 -m py_compile tools/state_codegen/generate_state.py tools/devapi/scenarios/state_roundtrip.py`; `cmake --build --preset game-native-debug`; `py -3.12 tools/devapi/scenarios/state_roundtrip.py 9179`; `cmake --build --preset game-native-qa`; `node tools/taskboard/cli.mjs validate`.
