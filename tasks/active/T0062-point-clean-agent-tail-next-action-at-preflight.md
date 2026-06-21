---
id: T0062
title: Point clean agent tail next action at preflight
status: review
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Agent status now detects when old classified subagent tool-use failures are
followed by a clean delegated-agent tail. Its `Next Action` still says only to
keep hints and watch the next run. Since the pipeline now has
`node tools/ai.mjs orchestration-check <task-id> --json`, point the clean-tail
next action at that preflight command so the next delegated packet gets checked
before launch.

## Done when

- [x] Clean-tail `status --agent-rollup` next action names
      `node tools/ai.mjs orchestration-check <task-id> --json`.
- [x] Unresolved agent failure priority remains unchanged.
- [x] Short clean-tail behavior still asks to apply prevention hints.
- [x] Focused profile tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: connect clean-tail status guidance to orchestration preflight
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs,
  tasks/active/T0062-point-clean-agent-tail-next-action-at-preflight.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: clean-tail next action points at `tools/ai.mjs
  orchestration-check` before the next delegated run
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: live status next action mentions preflight and tests cover
  clean-tail, short-tail, and unresolved-priority branches
  independent reviewer: Wegener audits status wording and branch risks

- reviewer Wegener: PASS for changing only clean-tail `nextAction`; keep
  unresolved-agent priority and avoid claiming that environment blockers are
  absent because that branch is lower priority.
- evidence: PASS `node --test tools/ai_profile/test.mjs` (45/45).
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose` printed
  `node tools/ai.mjs orchestration-check <task-id> --json` in Next Action
  (49 subagent sessions; telemetry agents 49/49; clean tail 9).
- evidence: PASS `node tools/taskboard/cli.mjs validate`.
- evidence: PASS `node tools/ai.mjs validate --review`.
