---
id: T0069
title: Add automatic AI context input capture
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, context, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a low-overhead helper for profiling context reads.

The helper should measure local file character counts and append a profile
record with `files_read` and `context_inputs`, so agents do not need to type
manual `path:chars:reason` values for common context files.

## Done when

- [x] A local command records measured context inputs for one or more files.
- [x] Context risk is auto-assigned from total measured size unless explicitly
      provided.
- [x] Profile review lists the specific medium/high context records that are
      missing `context_inputs`.
- [x] Profiling docs and reflection skill tell agents to use the helper for
      local medium/high context reads.
- [x] Skill eval includes the new helper anchors.
- [x] Validation covers syntax, helper execution, profile review output,
      skill sync/eval, taskboard validation, diff whitespace, and portable
      pipeline validation.

## Open questions

- Should a future version support stdin/web/thread excerpts, or should those
  stay explicit `event.mjs` records because size and source meaning are less
  reliable?

## Log

- 2026-06-13: Added `tools/ai_profile/context.mjs` and updated review output to
  list missing context-input details by line/intent.
- 2026-06-13: Validation passed:
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/context.mjs`;
  `node tools/ai_profile/run.mjs ... -- node --check tools/ai_profile/review.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/context.mjs --phase context --intent "Measure profiling docs context" --path AI_PIPELINE_SESSION_PROFILING.md --path .codex/skills/chat-session-reflection/SKILL.md --reason "T0069 validation"`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tools/ai_profile/session_profile_example.jsonl --output tmp/session_profiles/session_profile_example.review.md`;
  `node tools/ai_profile/run.mjs ... -- node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.review.md`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_sync.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/skills_eval.mjs`;
  `node tools/ai_profile/run.mjs ... -- node tools/taskboard/cli.mjs validate`;
  `node tools/ai_profile/run.mjs ... -- git diff --check`;
  `node tools/ai_profile/run.mjs ... -- node tools/pipeline_validate.mjs`.
