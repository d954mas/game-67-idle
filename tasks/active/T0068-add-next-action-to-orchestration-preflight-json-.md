---
id: T0068
title: Add next action to orchestration preflight JSON failures
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

`orchestration-check --json` failures now return structured `problem` objects,
but the machine-readable object does not include the next operator action. The
human CLI path prints hints for missing packet fields, while JSON consumers need
to infer the same recovery step from `code` and `missingFields`.

Add a compact `problem.nextAction` string for orchestration preflight JSON
failures so the orchestrator can surface or follow the recovery command without
parsing prose. Keep existing human output unchanged.

## Done when

- [x] `orchestration-check <task> --json` preflight failures include
      `problem.nextAction`.
- [x] Current-selector JSON failures keep their structured contract and include
      an appropriate `problem.nextAction`.
- [x] Successful JSON output remains unchanged except for no new problem data
      when `problem` is `null`.
- [x] Human non-json output remains clear and unchanged in shape.
- [x] Focused taskboard/facade validation and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: add machine-readable next action guidance to orchestration
  preflight JSON failures
  allowed files: tools/taskboard/cli.mjs, tools/taskboard/lib.mjs,
  tools/taskboard/test.mjs, tools/ai.test.mjs,
  tasks/active/T0068-add-next-action-to-orchestration-preflight-json-.md
  tool-use guard: exact paths/discovery before reads; use Select-Object
  -Skip/-First for line windows; trace/status commands include evidence source
  and --json-output where applicable
  expected output: failing `orchestration-check --json` responses carry a
  compact `problem.nextAction` for machine consumers, while human CLI output
  and successful JSON responses remain compatible
  evidence command: node --test tools/taskboard/test.mjs; node --test tools/ai.test.mjs; node tools/ai.mjs orchestration-check --current --json; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: JSON failure contracts include actionable next-step guidance,
  focused tests cover preflight and selector cases, and review validation passes
  independent reviewer: subagents audit JSON contract shape and status/facade
  implications
- reviewer Hilbert: PASS; `problem.nextAction` belongs on both preflight and
  current selector failures, with focused type/command assertions and no full
  sentence coupling.
- reviewer Gauss: PASS; taskboard owns the full JSON contract, facade needs
  only pass-through coverage, and status `next_action` does not need changes.
- evidence: PASS `node --test tools/taskboard/test.mjs` (75/75).
- evidence: PASS `node --test tools/ai.test.mjs` (21/21).
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`
  returned `{ "ok": true, "file": "tasks\\active\\T0068-add-next-action-to-orchestration-preflight-json-.md", "problem": null }`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`; telemetry
  agents 60/60, unresolved failures 0, agent tool-use clean tail 20.
- evidence: PASS `node tools/taskboard/cli.mjs validate`.
- evidence: PASS `node tools/ai.mjs validate --review`.
- evidence: PASS `git diff --check` (only global git ignore permission warning).
