---
id: T0075
title: Add AI profile status helper
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a lightweight `tools/ai_profile/status.mjs` command that reports current
profile health during a work session without closing the session.

## Done when

- [x] `status.mjs` reads a profile JSONL and reports records, latest event,
      closeout/bundle presence, work-item coverage, context-input gaps, and
      wall-clock coverage.
- [x] `status.mjs` suggests one concise next profiling action, such as adding
      `--work-item`, using `context.mjs`, or running `closeout.mjs`.
- [x] `status.mjs --json-output` writes machine-readable status for future
      dashboards/automation.
- [x] Profiling docs and reflection skill rules mention the status helper for
      mid-session checks.
- [x] Validation passes for syntax, live status markdown/JSON, skill eval,
      taskboard, diff check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started profile status helper so agents can see current telemetry health and next profiling action before final closeout.
- 2026-06-13: Implemented `tools/ai_profile/status.mjs`, a read-only mid-session profile health check with markdown output and optional `--json-output`.
- 2026-06-13: Live status evidence: `node tools/ai_profile/status.mjs --json-output tmp/session_profiles/session_profile_2026-06-13.status.json` reported records, latest event, closeout/bundle status, work-item coverage, missing context inputs, failed records, wall-clock coverage, and next action.
- 2026-06-13: Missing-profile evidence: `node tools/ai_profile/status.mjs --profile tmp/session_profiles/does_not_exist_status_test.jsonl --json-output tmp/session_profiles/does_not_exist_status_test.status.json` returned a start-profiling next action without writing profile records.
- 2026-06-13: JSON evidence: `node -e "const s=require('./tmp/session_profiles/session_profile_2026-06-13.status.json'); ..."` confirmed schema `1`, valid profile, bundle complete, work-item coverage, failed record count, and next action.
- 2026-06-13: Validation passed so far: `node --check tools/ai_profile/status.mjs`; `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`.
- 2026-06-13: Final validation passed: `node --check tools/ai_profile/status.mjs`; `node tools/ai_profile/status.mjs --json-output tmp/session_profiles/session_profile_2026-06-13.status.json`; `node tools/taskboard/cli.mjs validate`; `node tools/skills_eval.mjs`; `git diff --check`; `node tools/pipeline_validate.mjs`.
- 2026-06-13: Started profile status helper so agents can see current telemetry health and next profiling action before final closeout.
- 2026-06-13: Moved to review after status helper implementation, live markdown/JSON status proof, missing-profile proof, skill/taskboard checks, diff check, and final reusable pipeline validation passed.
