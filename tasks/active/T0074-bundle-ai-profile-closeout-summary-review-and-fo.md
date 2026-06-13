---
id: T0074
title: Bundle AI profile closeout summary review and followups
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Make `tools/ai_profile/closeout.mjs` produce the complete scratch reflection
bundle in one command: closeout event, summary, review markdown, review JSON,
follow-up markdown, and follow-up JSON.

## Done when

- [x] `closeout.mjs` writes summary, review markdown/JSON, and follow-up
      markdown/JSON by default under `tmp/session_profiles/`.
- [x] `closeout.mjs` has opt-out flags for review/followups and path override
      flags for generated artifacts.
- [x] Closeout records include the generated bundle paths as evidence unless
      the caller supplies custom evidence.
- [x] Profiling docs and reflection skill rules prefer the bundled closeout
      over manually running summary/review/followups.
- [x] Validation passes for syntax, live closeout bundle output, skill eval,
      taskboard, diff check, and reusable pipeline.

## Open questions

- None.

## Log
- 2026-06-13: Started closeout bundle work so one command writes summary, review markdown/JSON, and follow-up drafts for later reflection.
- 2026-06-13: Implemented bundled closeout outputs in `tools/ai_profile/closeout.mjs`: default `.summary.md`, `.review.md`, `.review.json`, `.followups.md`, and `.followups.json`; added `--no-review`, `--no-followups`, and path override flags.
- 2026-06-13: Live bundle evidence: `node tools/ai_profile/closeout.mjs --profile tmp/session_profiles/closeout_bundle_test.jsonl --work-item T0074 --iteration closeout-bundle --notes "Validate bundled closeout artifacts"` wrote all five artifacts and recorded them as evidence paths.
- 2026-06-13: JSON evidence: `node -e "const r=require('./tmp/session_profiles/closeout_bundle_test.review.json'); const f=require('./tmp/session_profiles/closeout_bundle_test.followups.json'); ..."` confirmed review schema `1`, `wall_clock_coverage`, followup schema `1`, and suggestions.
- 2026-06-13: Opt-out evidence: `node tools/ai_profile/closeout.mjs --profile tmp/session_profiles/closeout_summary_only_test.jsonl --no-review ...` wrote summary only; `node tools/ai_profile/closeout.mjs --profile tmp/session_profiles/closeout_no_followups_test.jsonl --no-followups ...` wrote summary plus review MD/JSON only.
- 2026-06-13: Validation note: `node tools/skills_eval.mjs` initially failed because the rewritten reflection skill lacked the exact `--json-output` anchor; restored it and reran `node tools/skills_sync.mjs` and `node tools/skills_eval.mjs` successfully.
- 2026-06-13: Final validation passed: `node tools/taskboard/cli.mjs validate`; `node --check tools/ai_profile/closeout.mjs`; `node --check tools/skills_eval.mjs`; `git diff --check`; `node tools/pipeline_validate.mjs`.
- 2026-06-13: Started closeout bundle work so one command writes summary, review markdown/JSON, and follow-up drafts for later reflection.
- 2026-06-13: Moved to review after bundled closeout implementation, live bundle and opt-out evidence, skill/taskboard checks, diff check, and final reusable pipeline validation passed.
