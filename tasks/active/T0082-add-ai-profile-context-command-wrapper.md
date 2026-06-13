---
id: T0082
title: Add AI profile context command wrapper
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, context, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a profiling wrapper for read-only context commands. It should run a command
such as `node tools/taskboard/cli.mjs context`, print the command output, and
append one profile context record with measured output size so later
reflection can account for command-provided context, not only files.

## Done when

- [x] `tools/ai_profile/context_command.mjs` runs a command after `--`,
      preserves stdout/stderr, exits with the wrapped command status, and
      records command text, duration, output character count, context risk, and
      work-item/iteration metadata.
- [x] AI profile tests cover successful and failing wrapped context commands.
- [x] Profiling docs and reflection skill mention `context_command.mjs` for
      `taskboard context`, generated summaries, and other command-produced
      context.
- [x] Portable pipeline validation runs the expanded AI profile tests in the
      source repo and exported base.
- [x] Validation passes for AI profile tests, taskboard, skill eval, diff
      check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started after `status.mjs` correctly identified missing
  context-input details as the next current issue; `context.mjs` measures local
  files, but command-produced context still requires manual metadata.
- 2026-06-13: Added `tools/ai_profile/context_command.mjs` to run a command,
  print stdout/stderr, preserve exit status, and append measured command output
  as one `context_inputs` entry.
- 2026-06-13: Extended `tools/ai_profile/test.mjs` with success and failure
  coverage for command-produced context, including output chars, command exit
  code, tool attribution, and inherited work-item metadata.
- 2026-06-13: Updated profiling docs, reflection skill, skill eval anchors,
  and iteration log so command-produced context is measured with
  `context_command.mjs`.
- 2026-06-13: Evidence: `node --check tools/ai_profile/context_command.mjs`;
  `node --check tools/skills_eval.mjs`; `node --test
  tools/ai_profile/test.mjs` passed 9 tests; smoke `node
  tools/ai_profile/context_command.mjs --phase context --intent "Load current
  task digest with measured command context" --reason "T0082 smoke taskboard
  context" -- node tools/taskboard/cli.mjs context` recorded 9127 chars;
  `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node
  tools/taskboard/cli.mjs validate`; `node tools/pipeline_validate.mjs`
  passed, including exported AI profile tests.
- 2026-06-13: Moved to review after context command wrapper implementation, docs/skill updates, smoke context capture, ai profile tests, and reusable pipeline validation passed.
