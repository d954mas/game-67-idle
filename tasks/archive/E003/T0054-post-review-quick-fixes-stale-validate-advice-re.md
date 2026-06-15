---
id: T0054
title: Post-review quick fixes stale validate advice readme wording tmp prune
status: done
epic: E003
priority: P1
tags: [validation, profiling, docs, context, speed]
created: 2026-06-15
updated: 2026-06-15
---

## What

Quick wins from the post-implementation review of the T0043-T0053 milestone:
1. Six profiler strings still emitted the removed `ai.mjs validate --change <kind>
   --risk <risk>` syntax (a no-op after T0047): followups.mjs x2, review.mjs x3,
   reflection_draft.mjs x1. The tool that exists to reduce waste was teaching a
   dead command.
2. `tools/README.md` slice_hygiene description listed profiler-guard evidence in
   the pre-handoff checklist without marking it advisory (T0044 demoted it).
3. `tmp/` held ~98M of closed-prototype scratch (rune_marches, NanoAlpha,
   ui_generation_pipeline_2026-06-14, roblox_fishing).

## Done when

- [x] All 6 profiler strings updated to the current syntax (`ai.mjs validate` quick; `--full` for broad/final gates). No `validate --change` remains in live `tools/ai_profile/*.mjs`.
- [x] `tools/README.md` marks the slice-hygiene profiler guard optional/advisory (never blocks the slice).
- [x] Closed-prototype `tmp/` scratch pruned (tmp 120M -> 22M).
- [x] Tests pass: `node --test tools/ai_profile/test.mjs` 66/66; `node tools/taskboard/cli.mjs validate` ok.

## Open questions

## Log

- 2026-06-15: Created from the post-implementation review of T0043-T0053.
- 2026-06-15: Fixed the 6 stale `validate --change/--risk` advice strings in followups.mjs/review.mjs/reflection_draft.mjs to the quick/`--full` syntax; softened tools/README.md slice-hygiene profiler-guard to advisory; pruned closed-prototype tmp dirs (120M -> 22M). ai_profile tests 66/66, taskboard ok.
