---
id: T0052
title: Prevent common subagent tool-usage failures
status: done
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Agent rollup now classifies subagent tool-usage failures, but the orchestrator
still has to translate samples into prompt/tooling fixes by hand. Surface
compact guidance from the profiler so the next subagent packet can directly
avoid repeated missing-path reads, invalid PowerShell command shapes, and
orchestration-trace invocations without evidence sources.

## Done when

- [x] Agent rollup emits guidance for each observed agent tool-usage failure
      reason.
- [x] Guidance is rendered in human output near the tool-usage samples.
- [x] Missing-path guidance points to path discovery before file reads.
- [x] Invalid PowerShell guidance points to supported read/range patterns.
- [x] Missing orchestration evidence guidance points to required trace source
      flags.
- [x] Independent reviewer confirms the prevention placement and risks.
- [x] Focused tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: turn agent tool-usage failure buckets into actionable prevention
  guidance for future subagent packets
  allowed files: tools/ai_profile/status.mjs, tools/ai_profile/test.mjs,
  tasks/active/T0052-prevent-common-subagent-tool-usage-failures.md
  expected output: `status --agent-rollup` renders compact guidance for
  missing-path, invalid-shell-command, and missing-evidence-source failures
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs
  status --agent-rollup --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose; node
  tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: live rollup gives concrete prevention guidance without
  changing failure classification counts
  independent reviewer: McClintock audits prevention placement and risks

- reviewer: McClintock recommended profiler/status prevention hints as the best
  placement, because taskboard validation and subagent protocol already reject
  or document the orchestration-trace command shape.
- PASS: `node --test tools/ai_profile/test.mjs`
  result: 40 tests passed, including reason-based prevention hints and a
  copyable orchestration-trace evidence-source command.
- PASS: `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok
  --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`
  result: strict live rollup passed, rendered three prevention hints for
  missing local paths, invalid shell command parameters, and missing
  orchestration evidence source.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`;
  output showed `prevent missing local file/path`, `prevent invalid shell
  command/parameter`, and `prevent missing orchestration evidence source`.
- PASS: `node tools/taskboard/cli.mjs validate`
  result: ok, no problems found.
- PASS: `node tools/ai.mjs validate --review`
  result: reusable pipeline quick+review validation passed.
