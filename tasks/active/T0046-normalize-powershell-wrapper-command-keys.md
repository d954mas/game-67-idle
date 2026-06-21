---
id: T0046
title: Normalize PowerShell wrapper command keys
status: review
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration]
created: 2026-06-21
updated: 2026-06-21
---

## What

`node tools/ai.mjs status --agent-rollup` currently reports repeated
PowerShell wrapper snippets such as `$i=0;` as command keys. That makes
orchestration profiling noisy because loop/setup wrappers outrank the actual
tool commands agents ran.

Normalize safe leading PowerShell variable-assignment wrappers before command
key extraction so status rollups group by the real tool command.

## Done when

- [x] Command rollups no longer use `$i=0;` as the key for a wrapped tool
  command.
- [x] Existing `$env:...;` and POSIX env assignment normalization still works.
- [x] Search-command classification still treats no-match search exits as pass
  after safe wrapper stripping.
- [x] Independent reviewer confirms the normalization rule is narrow enough.
- [x] Focused tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: remove noisy PowerShell wrapper command keys from profiling rollups
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs, tasks/active/T0046-normalize-powershell-wrapper-command-keys.md
  expected output: `node tools/ai.mjs status --agent-rollup` groups wrapper commands by the real tool instead of `$i=0;`
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove wrapper normalization and review validation remains green
  independent reviewer: Helmholtz audits PowerShell wrapper normalization risks and tests
- reviewer PASS: Helmholtz confirmed `commandKey()` was using `$i=0;` as the
  rollup key and recommended stripping only leading PowerShell scalar setup
  statements before tokenization.
- evidence: PASS `node --test tools/ai_profile/test.mjs` passed 34/34.
- evidence: PASS `node --test tools/ai_profile/test.mjs tools/ai.test.mjs`
  passed 51/51.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` showed `Get-Content` instead of `$i=0;` in Most-Run Commands and Agent Profile Rollup.
- evidence: PASS `node tools/taskboard/cli.mjs validate` reported no problems.
- evidence: PASS `node tools/ai.mjs validate --review` passed quick+review validation.
- evidence: PASS `git diff --check` reported no whitespace errors.
