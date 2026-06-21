---
id: T0061
title: Expose orchestration preflight through AI facade
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

`orchestration-check` is now the preflight for substantial subagent work, but
it is only exposed through `tools/taskboard/cli.mjs`. Add a thin
`tools/ai.mjs orchestration-check` facade so the main AI pipeline command
surface includes preflight, trace, status, and validation entrypoints together.

## Done when

- [x] `node tools/ai.mjs orchestration-check <task-id>` forwards to the
      taskboard preflight command.
- [x] The facade preserves structured `--json` output and task resolution.
- [x] Existing facade commands continue to pass.
- [x] Focused facade/taskboard tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: expose orchestration packet preflight through the AI facade
  allowed files: tools/ai.mjs, tools/ai.test.mjs,
  tasks/active/T0061-expose-orchestration-preflight-through-ai-facade.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: `node tools/ai.mjs orchestration-check T0061 --json`
  forwards to taskboard and returns structured preflight output
  evidence command: node --test tools/ai.test.mjs; node --test tools/taskboard/test.mjs; node tools/ai.mjs orchestration-check T0061 --json; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: AI facade exposes preflight without duplicating taskboard
  validation logic
  independent reviewer: Heisenberg audits facade scope and compatibility risk

- reviewer Heisenberg: PASS for a thin facade branch; keep taskboard as source
  of truth and pass argv through without local option parsing.
- evidence: PASS `node --test tools/ai.test.mjs` (18/18).
- evidence: PASS `node --test tools/taskboard/test.mjs` (69/69).
- evidence: PASS `node tools/ai.mjs orchestration-check T0061 --json`
  returned `ok: true` and resolved
  `tasks\active\T0061-expose-orchestration-preflight-through-ai-facade.md`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose` (48
  subagent sessions; telemetry agents 48/48; clean tail 8).
- evidence: PASS `node tools/taskboard/cli.mjs validate`.
- evidence: PASS `node tools/ai.mjs validate --review`.
