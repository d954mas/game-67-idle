---
id: T0048
title: Show hidden unresolved agent failure sample count
status: done
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

`status --agent-rollup` caps rendered unresolved agent failure samples but does
not say how many unresolved failures remain hidden. When the count is larger
than the rendered sample cap, the lead can underestimate the remaining
orchestration cleanup.

Show a compact remainder line after rendered unresolved agent failure samples
that counts unresolved failures not shown.

## Done when

- [x] Non-verbose status shows the first 3 unresolved agent samples and a
  hidden unresolved-failure count when more exist.
- [x] Verbose status shows up to 10 unresolved agent samples and a hidden
  unresolved-failure count when more exist.
- [x] Status output does not add noisy remainder lines when all samples render.
- [x] Independent reviewer confirms wording/noise risk.
- [x] Focused tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: show when unresolved agent failure samples are capped in status output
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs, tasks/active/T0048-show-hidden-unresolved-agent-failure-sample-coun.md
  expected output: `status --agent-rollup` prints a compact hidden unresolved failure count when rendered samples are capped
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: focused tests prove capped sample remainder rendering and review validation remains green
  independent reviewer: Lorentz audits unresolved sample cap UX and edge cases
- reviewer PASS: Lorentz confirmed the cap UX gap and recommended the wording
  `failure(s) not shown` because JSON stores only the first 10 concrete sample
  rows while `unresolved_failed_records` counts all unresolved failures.
- evidence: PASS `node --test tools/ai_profile/test.mjs` passed 37/37.
- evidence: PASS `node --test tools/ai_profile/test.mjs tools/ai.test.mjs`
  passed 54/54.
- evidence: PASS `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session` printed `... 9 more unresolved agent failure(s) not shown`.
- evidence: PASS `node tools/taskboard/cli.mjs validate` reported no problems.
- evidence: PASS `node tools/ai.mjs validate --review` passed quick+review validation.
- evidence: PASS `git diff --check` reported no whitespace errors.
