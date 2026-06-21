---
id: T0051
title: Recover agent test failures from parent superset test passes
status: review
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Agent parent recovery currently matches exact normalized commands. That leaves
single-file `node --test <file>` subagent failures unresolved even when the
parent later passes a wider `node --test <file> <other-file>` command. Treat
explicit test-file supersets as recovery evidence without parsing wrapper
stdout or broadly trusting unrelated validation commands.

## Done when

- [x] A failed single-file `node --test <file>` can be recovered by a later
      parent `node --test` pass that includes the same explicit file.
- [x] Parent passes before the failed test do not recover it.
- [x] Failed multi-file `node --test` commands are not partially recovered by a
      parent run that covers only one failed file.
- [x] Non-test commands and wrapper validation output are not affected.
- [x] Independent reviewer confirms the recovery rule and risks.
- [x] Focused tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: recover single-file agent test failures from later parent
  node-test superset passes
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs,
  tasks/active/T0051-recover-agent-test-failures-from-parent-superset.md
  expected output: live agent rollup has no unresolved agent failures when a
  later parent `node --test` command explicitly covers the failed test file
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs
  status --agent-rollup --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose; node
  tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove only explicit single-file node-test
  failures recover from later parent superset passes
  independent reviewer: Lovelace audits superset test recovery safety and risks

- reviewer: Lovelace recommended the narrow node-test file superset recovery
  rule, with safeguards against globs, wrapper commands, stdout parsing, and
  filtered runs such as `--test-name-pattern`.
- PASS: `node --test tools/ai_profile/test.mjs`
  result: 40 tests passed, including parent superset recovery, before-failure
  non-recovery, multi-file fail non-recovery, and filtered parent pass
  non-recovery.
- PASS: `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok
  --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`
  result: strict live rollup passed with `unresolved=0`,
  `parent_recovered=5`, `parent_exact=4`, `parent_node_test_file=1`, and
  `agent_tool_usage=7`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`;
  output showed no unresolved agent failures and
  `parent-recovered agent failures: 5`.
- PASS: `node tools/taskboard/cli.mjs validate`
  result: ok, no problems found.
- PASS: `node tools/ai.mjs validate --review`
  result: reusable pipeline quick+review validation passed.
