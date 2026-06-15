---
id: T0036
title: Make broken profiling coverage actionable
status: done
epic: E003
priority: P1
tags: [ai-profile, pipeline]
created: 2026-06-15
updated: 2026-06-15
---

## What

Make low wall-clock profiling coverage actionable in the normal profiler status
readout. A broken profile must show the concrete missing-time gaps and next
checkpoint action, not only a generic coverage percentage.

## Done when

- [x] `node tools/ai.mjs status` reports largest coverage gaps when whole-session
      or current-scope wall-clock coverage is low.
- [x] The same gap data is present in status JSON for review/automation.
- [x] Profiler tests cover low-coverage gap reporting.
- [x] Task/status docs name the broken-profile evidence expectation.

## Open questions

- none; this is a tooling guardrail fix from the fishing review.

## Log

- 2026-06-15: Started after review showed the fishing profile had too little
  wall-clock coverage to support reliable bottleneck conclusions.
- 2026-06-15: Added largest coverage gaps to `tools/ai_profile/status.mjs`
  passive/verbose output and JSON, fixed passive next-action wording for
  partial/broken profiles, and covered low-coverage gap reporting in
  `tools/ai_profile/test.mjs`.
- 2026-06-15: Validation passed:
  `node tools/ai.mjs validate --change profiling --change docs --change taskboard --risk medium`;
  `node tools/ai.mjs status --require-current-scope-usable`;
  `node tools/taskboard/cli.mjs validate`; `git diff --check -- ...`.
