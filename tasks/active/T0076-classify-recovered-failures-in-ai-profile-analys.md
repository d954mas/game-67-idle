---
id: T0076
title: Classify recovered failures in AI profile analysis
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, analysis, reflection, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Classify failed AI profile command records as recovered when the same normalized
command later passes, so profile status and retrospectives distinguish
resolved rework from unresolved failures.

## Done when

- [x] `review.mjs` reports recovered failed records separately from unresolved
      failed records in markdown and JSON.
- [x] `status.mjs` reports recovered and unresolved failed record counts, and
      does not treat recovered-only failures as the top next action.
- [x] `followups.mjs` generates separate draft actions for unresolved failures
      and recovered failures.
- [x] Profiling docs and reflection skill rules explain recovered failures.
- [x] Validation passes for syntax, live review/status/followups JSON,
      taskboard, skill eval, diff check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started recovered-failure classification so failed commands that later pass stop looking like unresolved profile health issues.
- 2026-06-13: Implemented recovered/unresolved failure classification in `review.mjs` by matching failed command records against later passing records with the same normalized command.
- 2026-06-13: Updated `status.mjs` to report `failed records: total (recovered, unresolved)` and only prioritize failures when unresolved failures remain.
- 2026-06-13: Updated `followups.mjs` to generate a separate `Classify recovered AI profile failures` draft action from `recovered_failed_records`.
- 2026-06-13: Live review/status evidence: `node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.review.md --json-output tmp/session_profiles/session_profile_2026-06-13.review.json` and `node tools/ai_profile/status.mjs --json-output tmp/session_profiles/session_profile_2026-06-13.status.json` reported `2 recovered, 0 unresolved`.
- 2026-06-13: Follow-up evidence: `node tools/ai_profile/followups.mjs tmp/session_profiles/session_profile_2026-06-13.review.json --output tmp/session_profiles/session_profile_2026-06-13.followups.md --json-output tmp/session_profiles/session_profile_2026-06-13.followups.json` generated `Classify recovered AI profile failures`.
- 2026-06-13: Final validation passed: `node --check tools/ai_profile/review.mjs`; `node --check tools/ai_profile/status.mjs`; `node --check tools/ai_profile/followups.mjs`; `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`; `git diff --check`; `node tools/pipeline_validate.mjs`.
- 2026-06-13: Started recovered-failure classification so failed commands that later pass stop looking like unresolved profile health issues.
- 2026-06-13: Moved to review after recovered/unresolved failure classification, live review/status/followups JSON proof, skill/taskboard checks, diff check, and final reusable pipeline validation passed.
