---
id: T0062
title: Add low-overhead AI profile event and command wrappers
status: review
epic: ""
priority: P1
tags: [pipeline,profiling,telemetry,tools,automation]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add low-overhead profiling wrappers so future AI development sessions collect
useful reflection data while work happens, without forcing the agent to write
manual logs after every command.

This must stay universal and project-agnostic: no current-game assumptions, no
network dependency, no account, no service, no server.

## Done when

- [x] `event.mjs` appends sparse non-command checkpoint records to the default
      ignored session profile path.
- [x] `run.mjs` wraps commands, preserves stdio, records duration, exit code,
      command text, tool type, and pass/fail result.
- [x] Shared validation/default-path logic is reusable by current and future
      profile tools.
- [x] Profiling docs tell agents to use wrappers for substantial commands and
      sparse checkpoints, instead of reconstructing after the fact.
- [x] `chat-session-reflection` points future long sessions to the wrappers.
- [x] Validation proves scripts parse, example profile still summarizes, wrapper
      output summarizes, skills/taskboard remain valid, and portable pipeline
      export still includes profiling tools.
- [x] Summary closeout can write an ignored `.summary.md` artifact so
      retrospectives do not depend on copied terminal output.

## Open questions

- Should future work add an optional OpenTelemetry export, or is local JSONL +
  summary enough until a backend is explicitly requested?

## Log

- 2026-06-13: Added `tools/ai_profile/profile_lib.mjs`,
  `tools/ai_profile/event.mjs`, and `tools/ai_profile/run.mjs`.
- 2026-06-13: Updated `AI_PIPELINE_SESSION_PROFILING.md`, `AI_PIPELINE.md`,
  and `chat-session-reflection` to prefer live low-overhead capture over
  post-hoc reconstruction.
- 2026-06-13: Hardened `--context-input path:chars:reason` parsing so Windows
  absolute paths with drive-letter colons do not break context profiling.
- 2026-06-13: Added `--output <summary.md>` to
  `summarize_session_profile.mjs` so session closeout can create an ignored
  summary artifact for later reflection.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/profile_lib.mjs`;
  `node --check tools/ai_profile/event.mjs`;
  `node --check tools/ai_profile/run.mjs`;
  `node --check tools/ai_profile/summarize_session_profile.mjs`;
  `node tools/ai_profile/event.mjs --profile tmp/session_profiles/context_path_test.jsonl ... --context-input C:\projects\game-67-idle\tasks\STATUS.md:42000:windows-path-test`;
  `node tools/ai_profile/summarize_session_profile.mjs tmp/session_profiles/context_path_test.jsonl`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_sync.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_eval.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
