---
id: T0030
title: Improve orchestration packet feedback
status: review
epic: E001
priority: P1
tags: [pipeline, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Reduce orchestration friction by making the taskboard guard show the exact
orchestration packet shape and missing fields when a substantial
pipeline/orchestration task is moved to `review` or `done`.

### Scope

- Keep this as taskboard/pipeline tooling only.
- Improve feedback for malformed `orchestration: used` evidence.
- Keep the subagent protocol in `docs/ai-pipeline/subagent-protocol.md` as the
  source of truth for the required packet.
- Use subagents for independent review/research during the iteration.

### Out Of Scope

- No game runtime changes.
- No transcript-level proof of actual subagent tool calls yet.
- No broad rewrite of taskboard status classification.

## Done when

- [x] Failed orchestration evidence reports which required packet fields are
      missing or invalid.
- [x] CLI remediation includes a compact copyable packet template.
- [x] Taskboard tests cover malformed packet feedback, not just pass/fail.
- [x] Protocol/docs mention the diagnostic command or failure output.
- [x] Evidence records subagent packets and an independent verifier.
- [x] `node --test tools/taskboard/test.mjs` and
      `node tools/taskboard/cli.mjs validate` pass.

## Open questions

- None for this iteration.

## Log

- 2026-06-21: Started after the Dragon Grove orchestration test exposed a
  concrete friction point: the guard correctly rejected a malformed packet, but
  the CLI hint was too generic to show the exact missing label/value problem.
- orchestration: used
  objective: improve orchestration packet diagnostics and machine-readable feedback
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/cli.mjs, tools/taskboard/test.mjs, docs/ai-pipeline/subagent-protocol.md, tasks/active/T0030-improve-orchestration-packet-feedback.md
  expected output: missing-field diagnostics, copyable packet template, JSON validation path, focused tests
  evidence command: node --test tools/taskboard/test.mjs; node tools/taskboard/cli.mjs orchestration-template; node tools/taskboard/cli.mjs validate --json; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: taskboard guard remains compatible for string callers while JSON callers get stable orchestration fields
  independent reviewer: Faraday identified the CLI template gap; Beauvoir required structured JSON feedback and transition-path coverage
- 2026-06-21: Implemented `validateStoreDetailed`, structured orchestration
  problems (`code`, `taskId`, `missingFields`, `template`), `validate --json`,
  `set --json`, and `orchestration-template`.
- 2026-06-21: Focused evidence passed:
  `node --test tools/taskboard/test.mjs` (38/38),
  `node tools/taskboard/cli.mjs orchestration-template`, and
  `node tools/taskboard/cli.mjs validate --json`.
- 2026-06-21: Review evidence passed:
  `node tools/taskboard/cli.mjs validate`,
  `node --test tools/taskboard/test.mjs`, `git diff --check`, and
  `node tools/ai.mjs validate --review`.
