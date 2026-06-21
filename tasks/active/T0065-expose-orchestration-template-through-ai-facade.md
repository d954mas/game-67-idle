---
id: T0065
title: Expose orchestration template through AI facade
status: review
epic: ""
priority: P2
tags: [pipeline, orchestration, taskboard, subagents]
created: 2026-06-21
updated: 2026-06-21
---

## What

`status --agent-rollup` now tells the orchestrator to create/refine a `doing`
pipeline/orchestration task with a complete orchestration packet when no current
task exists. The copyable packet template still lives only under
`node tools/taskboard/cli.mjs orchestration-template`, while the surrounding
workflow uses `node tools/ai.mjs status` and `node tools/ai.mjs
orchestration-check`.

Expose the template through `node tools/ai.mjs orchestration-template` so the AI
facade has the full status -> template -> check loop.

## Done when

- [x] `node tools/ai.mjs orchestration-template` forwards to the taskboard
      template command.
- [x] AI facade usage/tests include the new command without duplicating
      taskboard template logic.
- [x] No-current clean-tail status guidance can point at the AI facade template
      command before preflight.
- [x] Focused AI/profile tests, taskboard validation, and review validation
      pass.

## Open questions

## Log

- orchestration: used
  objective: expose the orchestration packet template through the AI facade and
  connect no-current status guidance to the full facade loop
  allowed files: tools/ai.mjs, tools/ai.test.mjs,
  tools/ai_profile/status.mjs, tools/ai_profile/test.mjs,
  tasks/active/T0065-expose-orchestration-template-through-ai-facade.md
  tool-use guard: exact paths/discovery before reads; safe line ranges; trace
  source plus --json-output
  expected output: `node tools/ai.mjs orchestration-template` prints the
  taskboard packet template, and no-current clean-tail status names the template
  command before `orchestration-check`
  evidence command: node --test tools/ai.test.mjs; node --test tools/ai_profile/test.mjs; node tools/ai.mjs orchestration-template; node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --parent-thread-id 019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd C:\projects\game-67-idle --no-import-codex-session --verbose; node tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: facade command works, status guidance points to it in the
  no-current branch, focused tests cover both, and review validation passes
  independent reviewer: subagents audit facade pattern and test coverage

- reviewer Maxwell: PASS; confirmed thin facade forwarding is the right shape
  and requested exact stdout equality with `tools/taskboard/cli.mjs
  orchestration-template` to prove no duplicated template.
- reviewer Hypatia: PASS; confirmed no-current profile guidance should mention
  `node tools/ai.mjs orchestration-template`, while unique/ambiguous guidance
  should stay focused on concrete/placeholder preflight.
- PASS node --test tools/ai.test.mjs (19/19).
- PASS node --test tools/ai_profile/test.mjs (48/48).
- PASS node tools/ai.mjs orchestration-template.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose` printed
  `node tools/ai.mjs orchestration-check T0065 --json` in Next Action
  (54/54 telemetry agents; clean tail 14).
- PASS node tools/taskboard/cli.mjs validate.
- PASS node tools/ai.mjs validate --review.
- PASS git diff --check.
