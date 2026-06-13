---
id: T0073
title: Add gap coverage analysis to AI session profile review
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, analysis, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Teach AI profile review to report wall-clock span, merged profiled time,
coverage ratio, and largest unprofiled gaps so retrospectives can distinguish
measured work from missing telemetry.

## Done when

- [x] `review.mjs` computes wall-clock coverage from record timestamps and
      duration intervals without breaking existing JSONL profiles.
- [x] Markdown review output shows wall-clock span, merged profiled time,
      coverage percentage, and largest gaps.
- [x] JSON review output includes a `wall_clock_coverage` object suitable for
      follow-up tools.
- [x] `followups.mjs` can draft an action from `low_profile_coverage`.
- [x] Profiling docs and reflection skill rules require large gaps or low
      coverage to be explained or fixed with sparse checkpoints.
- [x] Validation passes for syntax, live review JSON, skill eval, taskboard,
      diff check, and the reusable pipeline gate.

## Open questions

- None.

## Log
- 2026-06-13: Started gap/coverage analysis for profile review so long sessions expose unprofiled wall-clock gaps instead of only summed command duration.
- 2026-06-13: Implemented wall-clock coverage in `review.mjs`: timestamp/duration intervals, merged profiled time for parallel command safety, coverage percentage, largest gaps, `low_profile_coverage` finding, and `wall_clock_coverage` JSON.
- 2026-06-13: Added `followups.mjs` drafts for `low_profile_coverage` and `missing_work_item_metadata` so structured review findings produce reviewable next actions instead of manual markdown scanning.
- 2026-06-13: Live review evidence: `node tools/ai_profile/review.mjs tmp/session_profiles/session_profile_2026-06-13.jsonl --output tmp/session_profiles/session_profile_2026-06-13.review.md --json-output tmp/session_profiles/session_profile_2026-06-13.review.json` reported `Profile covers 1.2% of a 54.7m wall-clock span`; JSON check confirmed `wall_clock_coverage` and `low_profile_coverage`.
- 2026-06-13: Live follow-up evidence: `node tools/ai_profile/followups.mjs tmp/session_profiles/session_profile_2026-06-13.review.json --output tmp/session_profiles/session_profile_2026-06-13.followups.md --json-output tmp/session_profiles/session_profile_2026-06-13.followups.json` generated `Raise AI profile wall-clock coverage`.
- 2026-06-13: Validation note: `node tools/skills_eval.mjs` initially failed because the reflection skill lacked the exact `low_profile_coverage` anchor; added it and reran successfully.
- 2026-06-13: Final validation passed: `node tools/taskboard/cli.mjs validate`; `git diff --check`; `node --check tools/ai_profile/review.mjs`; `node --check tools/ai_profile/followups.mjs`; `node tools/skills_sync.mjs`; `node tools/skills_eval.mjs`; `node --test tools/taskboard/test.mjs`; `node tools/pipeline_validate.mjs`.
- 2026-06-13: Started gap/coverage analysis for profile review so long sessions expose unprofiled wall-clock gaps instead of only summed command duration.
- 2026-06-13: Moved to review after wall-clock coverage review output, JSON proof, follow-up draft support, skill/taskboard checks, diff check, and final reusable pipeline validation passed.
