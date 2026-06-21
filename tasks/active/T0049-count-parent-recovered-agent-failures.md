---
id: T0049
title: Count parent-recovered agent failures
status: review
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Agent rollup currently marks a failed subagent command as unresolved unless the
same subagent later passes the same command. In orchestration workflows the
parent often fixes or validates the same command after the subagent returns, so
those failures should be visible as recovered by the orchestrator rather than
remaining unresolved.

## Done when

- [x] Agent rollup excludes failures from unresolved samples/count when the
      parent session later passes the same command.
- [x] Agent rollup exposes a compact parent-recovered count.
- [x] Parent passes before the agent failure do not recover the later failure.
- [x] Environment-blocked agent failures remain separate.
- [x] Independent reviewer confirms recovery rule and edge cases.
- [x] Focused tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: classify agent failures recovered by later parent-session
  validation
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs,
  tasks/active/T0049-count-parent-recovered-agent-failures.md
  expected output: `status --agent-rollup` reduces unresolved agent failures
  when the parent later passes the same command and reports the
  parent-recovered count
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs
  status --agent-rollup --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session; node
  tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove parent recovery classification and review
  validation remains green
  independent reviewer: Singer audits parent recovery rule and edge cases

- reviewer: Singer confirmed timestamp-based parent recovery, warned against
  coarse `commandKey()` matching, and recommended explicit parent-recovered
  counters.
- decision: keep environment-blocked failures separate for this change because
  T0049's contract is to avoid hiding dependency blockers; parent recovery for
  stale environment blockers can be a separate policy change.
- PASS: `node --test tools/ai_profile/test.mjs`
  result: 38 tests passed; includes parent-recovered, earlier-parent-pass, and
  environment-blocked separation coverage.
- PASS: `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok
  --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`
  result: strict agent rollup passed; live status reported
  `parent-recovered agent failures: 4` and unresolved agent failures dropped
  from 12 to 8.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`;
  output showed `parent-recovered agent failures: 4`.
- PASS: `node tools/taskboard/cli.mjs validate`
  result: ok, no problems found.
- PASS: `node tools/ai.mjs validate --review`
  result: reusable pipeline quick+review validation passed.
