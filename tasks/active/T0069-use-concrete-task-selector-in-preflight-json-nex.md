---
id: T0069
title: Use concrete task selector in preflight JSON next action
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

T0068 added `problem.nextAction` to `orchestration-check --json` failures, but
the preflight failure string still says to rerun `orchestration-check <task-id>
--json` even when the failing task id is known. That is less useful for a
manager/orchestrator than a concrete command.

Make preflight JSON failure guidance use the resolved task id when available,
falling back to `<task-id>` only when the task has no id. Keep success JSON and
human CLI output compatible.

## Done when

- [x] Preflight JSON failures for tasks with ids include a concrete
      `node tools/ai.mjs orchestration-check Txxxx --json` next action.
- [x] The no-id fallback still uses `<task-id>` instead of emitting an empty or
      malformed command.
- [x] Current-selector JSON failures and success JSON remain unchanged.
- [x] Focused taskboard/facade validation and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: make preflight JSON nextAction use a concrete task selector when
  available
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs,
  tools/ai.test.mjs,
  tasks/active/T0069-use-concrete-task-selector-in-preflight-json-nex.md
  tool-use guard: exact paths/discovery before reads; use Select-Object
  -Skip/-First for line windows; trace/status commands include evidence source
  and --json-output where applicable
  expected output: failing preflight JSON for known tasks returns a direct
  `node tools/ai.mjs orchestration-check Txxxx --json` nextAction while no-id
  docs keep the placeholder fallback
  evidence command: node --test tools/taskboard/test.mjs; node --test tools/ai.test.mjs; node tools/ai.mjs orchestration-check --current --json; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: known-task preflight failures have concrete nextAction,
  no-id fallback is tested, and review validation passes
  independent reviewer: subagents audit JSON contract and test scope
- reviewer Peirce: PASS; facade coverage can stay minimal, but added a
  concrete preflight failure pass-through test because T0069 promises facade
  validation.
- reviewer Aristotle: PASS; use only real frontmatter `id` for concrete
  nextAction, keep `<task-id>` fallback for no-id docs, and prevent `--file`
  path fallback from becoming a fake positional id.
- evidence: PASS `node --test tools/taskboard/test.mjs` (77/77).
- evidence: PASS `node --test tools/ai.test.mjs` (22/22).
- evidence: PASS `node tools/ai.mjs orchestration-check --current --json`
  returned `{ "ok": true, "file": "tasks\\active\\T0069-use-concrete-task-selector-in-preflight-json-nex.md", "problem": null }`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`; telemetry
  agents 62/62, unresolved failures 0, agent tool-use clean tail 22.
- evidence: PASS `node tools/taskboard/cli.mjs validate`.
- evidence: PASS `node tools/ai.mjs validate --review`.
- evidence: PASS `git diff --check` (only global git ignore permission warning).
