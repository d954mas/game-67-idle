---
id: T0076
title: Validate orchestration allowed files bounds
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, subagents, taskboard]
created: 2026-06-21
updated: 2026-06-21
---

## What

Make orchestration preflight reject weak allowed-files scopes so new subagent packets must have repo-local, bounded, parseable file/path patterns instead of arbitrary text.

## Done when

- [x] orchestration-check rejects weak/broad/off-repo allowed files, accepts bounded repo-local paths/patterns, focused tests pass, validate --review passes, and task log records subagent plus machine evidence.

## Open questions

## Log

- orchestration: used
  objective: Make orchestration preflight reject weak allowed-files scopes so new subagent packets must have repo-local, bounded, parseable file/path patterns instead of arbitrary text.
  allowed files: tools/taskboard/lib.mjs; tools/taskboard/test.mjs; tools/taskboard/cli.mjs; docs/ai-pipeline/subagent-protocol.md; tasks/active/T*.md
  tool-use guard: exact paths/discovery before reads; use Select-Object -Skip/-First for line windows; trace/status commands include evidence source and --json-output where applicable
  expected output: Focused taskboard validator/test change that hardens allowed-files preflight for new orchestration packets without gameplay/runtime edits.
  evidence command: node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose
  stop condition: orchestration-check rejects weak/broad/off-repo allowed files, accepts bounded repo-local paths/patterns, focused tests pass, validate --review passes, and task log records subagent plus machine evidence.
  independent reviewer: two subagents inspect allowed-files grammar and taskboard compatibility before closeout; parent integrates findings
- subagent: Pascal reviewed allowed-files grammar; found field parsing captured tool-use guard, recommended exact files, final-segment globs, and scoped recursive globs such as tools/taskboard/**.
- subagent: Bohr reviewed enforcement surfaces; recommended shared lib predicate in preflight, bootstrap, and T0076+ lifecycle validation, plus doc wording update.
- implementation: orchestration-check and orchestration-bootstrap now reject unbounded/off-repo allowed files; validateStore rejects unbounded allowed files for T0076+ while preserving pre-T0076 compatibility.
- evidence: PASS `node --test tools/taskboard/test.mjs` (90/90)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `git diff --check`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose`
