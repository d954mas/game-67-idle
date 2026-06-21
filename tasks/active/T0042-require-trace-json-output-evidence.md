---
id: T0042
title: Require trace json output evidence
status: review
epic: ""
priority: P1
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

`orchestration-trace` can emit a durable JSON artifact with `--json-output`,
but taskboard machine-evidence matching currently accepts trace commands that
only print to stdout. Require `--json-output` for trace machine evidence so
review/closeout has an inspectable artifact path, not only a copied PASS line.

## Done when

- [x] Trace evidence without `--json-output` is rejected for newer substantial
  orchestration tasks.
- [x] Trace evidence with matching source and `--json-output` is accepted.
- [x] Existing strict `status --agent-rollup --require-agent-rollup-ok`
  evidence remains accepted.
- [x] Focused taskboard tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: require durable JSON artifact paths for orchestration-trace machine evidence
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0042-require-trace-json-output-evidence.md
  expected output: taskboard rejects source-only orchestration-trace evidence and accepts trace evidence only with matching `--json-output`
  evidence command: node --test tools/taskboard/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove trace JSON artifact matching and review validation remains green
  independent reviewer: Mill audits whether trace `--json-output` should be required
- reviewer: PASS Mill confirmed the gap: clean HEAD accepted
  `orchestration-trace` without durable `--json-output`, leaving closeout with
  only stdout/PASS text instead of an inspectable trace artifact.
- evidence: PASS `node --test tools/taskboard/test.mjs tools/ai_profile/test.mjs` (89 tests)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/context_budget.mjs --review`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (29 subagent sessions)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `git diff --check`
