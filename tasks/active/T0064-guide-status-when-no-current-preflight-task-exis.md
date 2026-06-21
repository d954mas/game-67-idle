---
id: T0064
title: Guide status when no current preflight task exists
status: review
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

Clean-tail status guidance currently falls back to
`node tools/ai.mjs orchestration-check <task-id> --json` when no unique current
`doing` pipeline/orchestration task exists. That is correct but weak for the
common no-current-task state: the orchestrator should first create/refine a
tracked task with a complete orchestration packet, set it to `doing`, then run
the concrete preflight command before launching delegated work.

Keep the ambiguous multi-task fallback conservative.

## Done when

- [x] Clean-tail `status --agent-rollup` with no current orchestration task
      tells the agent to create/refine a task with an orchestration packet
      before running preflight.
- [x] Clean-tail status with exactly one current orchestration task still
      prints the concrete `orchestration-check <id> --json` command.
- [x] Clean-tail status with multiple current orchestration tasks still keeps
      the placeholder and avoids guessing.
- [x] Focused profile tests, taskboard validation, and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: make clean-tail status distinguish no-current-task from concrete
  preflight-task guidance
  allowed files: tools/taskboard/lib.mjs, tools/ai_profile/status.mjs,
  tools/ai_profile/test.mjs,
  tasks/active/T0064-guide-status-when-no-current-preflight-task-exis.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: no-current-task clean-tail status points at creating or
  refining a task with an orchestration packet, while unique-task status keeps
  printing a concrete preflight command
  evidence command: node --test tools/ai_profile/test.mjs; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: tests cover no-current, unique-current, and ambiguous-current
  guidance, and live status preflight no longer leaves the agent with only a
  placeholder in normal no-current-task state
  independent reviewer: Dirac audits UX wording/scope; Dewey audits test and
  validation coverage

- reviewer Dirac: PASS; requested distinct multi-current wording and the term
  "complete orchestration packet" rather than generic "complete packet".
- reviewer Dewey: PASS; confirmed the risk is profile next-action wording, not
  duplicating taskboard preflight tests; requested no-current stdout and
  ambiguous-current assertions.
- PASS node --test tools/ai_profile/test.mjs (48/48).
- PASS node --test tools/taskboard/test.mjs (69/69).
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose` printed
  `node tools/ai.mjs orchestration-check T0064 --json` in Next Action
  (52/52 telemetry agents; clean tail 12).
- PASS node tools/taskboard/cli.mjs validate.
- PASS node tools/ai.mjs validate --review.
- PASS git diff --check.
