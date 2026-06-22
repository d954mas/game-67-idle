---
id: T0050
title: Classify agent tool-usage failures
status: done
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Agent rollup still mixes real unresolved validation failures with subagent
tool-usage mistakes such as reading missing paths, using unsupported PowerShell
parameters, or calling orchestration trace without an evidence source. Those
operator/tooling mistakes should remain visible, but should not look like
unresolved product or pipeline validation failures.

## Done when

- [x] Transcript-derived agent failures can be classified as agent tool-usage
      failures from output text.
- [x] Agent rollup reports a separate agent tool-usage failure count and
      samples.
- [x] Agent tool-usage failures no longer inflate unresolved agent failure
      counts.
- [x] Validation/test failures that are not tool-usage mistakes remain
      unresolved unless otherwise recovered.
- [x] Independent reviewer confirms the classification scope and risks.
- [x] Focused tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: separate subagent tool-usage mistakes from unresolved validation
  failures in the orchestration profile rollup
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs,
  tasks/active/T0050-classify-agent-tool-usage-failures.md
  expected output: `status --agent-rollup` reports agent tool-usage failures
  separately and leaves genuine validation failures in unresolved samples
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs
  status --agent-rollup --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose; node
  tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: live agent rollup distinguishes tool-usage failures from
  unresolved validation failures without hiding either
  independent reviewer: Archimedes audits the next profiling improvement and
  risks

- reviewer: Archimedes recommended classifying operator/tool-usage failures
  separately, keeping samples visible, avoiding wrapper-output recovery, and
  leaving real test/build failures unresolved unless explicitly recovered.
- PASS: `node --test tools/ai_profile/test.mjs`
  result: 39 tests passed, including transcript tool-usage separation and
  parent-recovered regression coverage.
- PASS: `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok
  --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`
  result: strict live rollup passed; unresolved agent failures dropped from 8
  to 1, agent tool-usage failures reported as 7, and parent-recovered remained
  4.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`;
  output showed `agent tool-usage failures: 7`.
- PASS: `node --test tools/bootstrap/export_base.test.mjs`
  result: portable export budget passed after keeping
  `docs/ai-pipeline/profiling-reuse.md` under the export hot-doc cap.
- PASS: `node tools/context_budget.mjs --review`
  result: review context budget passed; `docs/ai-pipeline/profiling-reuse.md`
  measured 2595 chars.
- PASS: `node tools/taskboard/cli.mjs validate`
  result: ok, no problems found.
- PASS: `node tools/ai.mjs validate --review`
  result: reusable pipeline quick+review validation passed.
