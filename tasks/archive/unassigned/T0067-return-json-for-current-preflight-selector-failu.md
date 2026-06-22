---
id: T0067
title: Return JSON for current preflight selector failures
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

`orchestration-check --current --json` is now the recommended preflight command
after creating/refining exactly one current orchestration task. When no current
task exists, it still exits through plain stderr:

`error: no current doing pipeline/orchestration task; create or set exactly one
task to doing first`

That is clear for humans but weak for machine-backed orchestration. Make the
`--json` form return structured `{ ok: false, file: null, problem: ... }` for
current-selector state failures, while keeping non-json CLI output unchanged.

## Done when

- [x] `orchestration-check --current --json` returns parseable JSON for the
      no-current case.
- [x] `orchestration-check --current --json` returns parseable JSON for the
      multiple-current case.
- [x] Existing successful `--current --json` behavior remains unchanged.
- [x] Non-json selector failures still print clear stderr for humans.
- [x] Focused taskboard/facade validation and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: make current-task preflight selector failures machine-readable
  under --json
  allowed files: tools/taskboard/cli.mjs, tools/taskboard/test.mjs,
  tools/ai.test.mjs,
  tasks/active/T0067-return-json-for-current-preflight-selector-failu.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: current-selector failures return structured JSON when
  `--json` is passed, while non-json CLI failures remain human-readable
  evidence command: node --test tools/taskboard/test.mjs; node --test tools/ai.test.mjs; node tools/ai.mjs orchestration-check --current --json; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: no-current and multiple-current selector failures are covered
  by parseable JSON tests, successful current preflight still passes, and review
  validation passes
  independent reviewer: subagents audit selector JSON contract and tests
- reviewer Sagan: PASS; confirmed the minimal selector JSON contract, requested
  success `problem: null`, no-current/multiple-current JSON assertions, and
  non-json multiple-current stderr coverage.
- reviewer Schrodinger: PASS; facade coverage is sufficient for the no-current
  JSON failure path; added `taskIds: []` assertion for parity with taskboard.
- evidence: PASS `node --test tools/taskboard/test.mjs` (75/75).
- evidence: PASS `node --test tools/ai.test.mjs` (21/21).
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`
  returned `{ "ok": true, "file": "tasks\\active\\T0067-return-json-for-current-preflight-selector-failu.md", "problem": null }`.
- evidence: PASS `node tools/taskboard/cli.mjs orchestration-check --current`
  returned human-readable ok output for T0067.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`; telemetry
  agents 58/58, unresolved failures 0, agent tool-use clean tail 18.
- evidence: PASS `node tools/taskboard/cli.mjs validate`.
- evidence: PASS `node tools/ai.mjs validate --review`.
- evidence: PASS `git diff --check` (only global git ignore permission warning).
