---
id: T0059
title: Add orchestration packet preflight check
status: done
epic: ""
priority: P2
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Substantial orchestration tasks already need a complete packet before
`review`/`done`, but the main agent only gets feedback at closeout time.
Add a lightweight taskboard preflight command that can validate a task's
orchestration packet before launching subagents. The preflight should require
the template-default `tool-use guard` and a machine evidence command source,
without requiring later PASS evidence.

## Done when

- [x] `node tools/taskboard/cli.mjs orchestration-check --file <task.md>`
      validates a task packet before closeout.
- [x] Preflight rejects missing `tool-use guard` and missing/invalid machine
      evidence command source.
- [x] Existing `validate` closeout compatibility remains unchanged.
- [x] Focused taskboard tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: add a packet preflight check before subagent launch
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/cli.mjs,
  tools/taskboard/test.mjs, docs/ai-pipeline/subagent-protocol.md,
  tasks/active/T0059-add-orchestration-packet-preflight-check.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: taskboard CLI can check a packet before closeout without
  requiring PASS evidence
  evidence command: node --test tools/taskboard/test.mjs; node tools/taskboard/cli.mjs orchestration-check --file tasks/active/T0059-add-orchestration-packet-preflight-check.md; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: preflight catches packet defects early and taskboard
  closeout validation still passes
  independent reviewer: Laplace audits surface, compatibility risk, and tests

- reviewer Laplace: PASS for lib-level validator plus CLI sibling to
  `orchestration-template`; keep `tool-use guard` required only in preflight,
  not legacy closeout validation; preflight must not require PASS evidence.
- evidence: PASS `node --test tools/taskboard/test.mjs` (65/65).
- evidence: PASS `node tools/taskboard/cli.mjs orchestration-check --file
  tasks/active/T0059-add-orchestration-packet-preflight-check.md --json`
  returned `ok: true`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose` (46
  subagent sessions; telemetry agents 46/46; clean tail 6).
- evidence: PASS `node tools/taskboard/cli.mjs validate`.
- evidence: PASS `node tools/ai.mjs validate --review`.
