---
id: T0037
title: Match declared orchestration machine PASS evidence
status: done
epic: ""
priority: P1
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

T0031+ taskboard orchestration evidence currently requires a machine-looking
packet command and a later machine-looking `evidence: PASS` line, but those can
be different machine commands. Tighten the static guard so the later PASS
machine evidence semantically matches a declared machine evidence command from
the orchestration packet.

## Done when

- [x] A declared `orchestration-trace` cannot be closed by a later
  `status --agent-rollup` PASS.
- [x] Same-kind machine evidence with a different `--parent-thread-id` or
  `--trace-session` source is rejected.
- [x] Matching tolerates Markdown backticks, trailing prose, wrapped command
  lines, `--flag value` vs `--flag=value`, and slash differences.
- [x] Existing active T0031+ orchestration tasks validate under the stricter
  rule.
- [x] Focused taskboard tests and review validation pass.

## Open questions

- None.

## Log

- orchestration: used
  objective: require later machine PASS evidence to match the declared orchestration machine evidence
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs, tasks/active/T0037-match-declared-orchestration-machine-pass-eviden.md
  expected output: taskboard rejects unrelated machine PASS evidence and accepts semantically matching trace/status evidence despite harmless formatting differences
  evidence command: node --test tools/taskboard/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: matching stays static and compares command kind plus source identity, not full raw strings or arbitrary log prose
  independent reviewer: Zeno reviewed matching semantics and false-positive risks
- reviewer: PASS Zeno recommended source-identity matching: command kind plus
  `--parent-thread-id`, `--session`, or `--trace-session`, normalized for
  markdown punctuation, wrapping, slash differences, and harmless flags.
- evidence: PASS `node --test tools/taskboard/test.mjs` (52 tests)
- evidence: PASS `node tools/taskboard/cli.mjs validate --json`
- evidence: PASS `node tools/ai.mjs status --agent-rollup --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (23 subagent sessions)
- evidence: PASS `node tools/taskboard/cli.mjs validate`
- evidence: PASS `node tools/ai.mjs validate --review`
- evidence: PASS `git diff --check`
- evidence: PASS current strict `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` (26 subagent sessions); refreshed after T0039 strict evidence contract.
