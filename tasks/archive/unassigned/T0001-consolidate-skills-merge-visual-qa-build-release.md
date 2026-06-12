---
id: T0001
title: "Consolidate skills: merge visual-qa/build-release/design-steward, move agents-best-practices to user level"
status: done
epic: ""
priority: P1
tags: [skills, pipeline]
created: 2026-06-12
updated: 2026-06-12
---

## What

Skill review (2026-06-12) found 10 skills with overlapping triggers and one
non-game skill. Consolidate to 6 game skills:

- merge `game-visual-qa` checklist into `game-runtime-automation`;
- merge `game-build-release` discovery/naming/validation into
  `game-feature-iteration`;
- move `game-design-steward` output template into
  `primary-gdd-pipeline/references/design-stewardship.md`;
- move `agents-best-practices` out of the project to the user-level skill dir
  (`~/.claude/skills/`), keep `agent-legibility-feedback-loops.md` content in
  `gamedesign/knowledge/`;
- fix stale preset name `game-native-debug` -> `native-debug` in
  `game-state-management`;
- update `AI_PIPELINE.md` flow table;
- extend `tools/skills_eval.mjs` to cover all remaining skills (was 3/10).

## Done when

- [x] `.codex/skills/` contains exactly 6 skills; merged content preserved in
  the absorbing skills
- [x] `agents-best-practices` available at user level; removed from project;
  legibility lesson kept in `gamedesign/knowledge/`
- [x] `game-state-management` references the real `native-debug` preset
- [x] `AI_PIPELINE.md` references only existing skills
- [x] `node tools/skills_eval.mjs` checks all 6 skills and passes
- [x] `node tools/skills_sync.mjs` regenerated `.claude/skills/` with no
  orphaned dirs; `node tools/pipeline_validate.mjs` passes

## Open questions

## Log

- 2026-06-12: Merged `game-visual-qa` checklist into
  `game-runtime-automation` (new "Visual QA" section + description triggers).
  Merged `game-build-release` into `game-feature-iteration` (new "Build,
  Launch, And Release Tasks" section + description triggers). Moved
  `game-design-steward` content to
  `primary-gdd-pipeline/references/design-stewardship.md`. Copied
  `agents-best-practices` (19 files) to `C:/Users/ROG/.claude/skills/` and
  removed it from the project; condensed legibility lesson saved as
  `gamedesign/knowledge/agent_legibility.md` (indexed in knowledge README).
  Fixed stale preset `game-native-debug` -> `native-debug` in
  `game-state-management/SKILL.md`. Updated `AI_PIPELINE.md` flow table rows
  3/5/6/7. Extended `tools/skills_eval.mjs` to 6 skill checks plus a
  completeness guard that fails on any skill without a check entry.
- 2026-06-12: Evidence: `node tools/skills_sync.mjs` -> 6 generated, 0
  skipped; `node tools/skills_eval.mjs` -> PASS x6; `node
  tools/pipeline_validate.mjs` -> ok (taskboard validate ok, 15/15 tests,
  export + exported eval/validate/tests all passed; export at
  `tmp/pipeline-validate-2026-06-12T12-02-36-012Z`).
