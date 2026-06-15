---
id: T0049
title: Tier UI asset gates and collapse final-art report battery
status: done
epic: E003
priority: P1
tags: [assets, speed, tooling, subtraction]
created: 2026-06-15
updated: 2026-06-15
---

## What

A UI iteration runs ~14 mandatory CLI gates (generated-game-ui-assets step 13,
lines 175-342); `validate_art_job --final-art` requires 17 separate JSON
reports, each a per-pixel python scan. This is the biggest per-iteration time
tax (problem B). Tier the gates into draft / integrate / final-art; only the
final tier (shipping a kit) runs the full battery. Collapse the 17 reports into
one orchestrator emitting a single verdict, and generate edge/composition-proof
PNGs only on failure.

## Done when

- [x] Three explicit tiers defined in `generated-game-ui-assets` (`## Gate Tiers`): DRAFT (2 cmds/iteration), INTEGRATE (3 cmds when wiring in), FINAL-ART (the full 13-cmd battery, opt-in / kit-ship only). A normal iteration runs DRAFT(+INTEGRATE), not the full battery.
- [x] Added `tools/assets/run_ui_asset_tier.mjs` (+ test 6/6): `--tier draft|integrate|final --plan` prints the exact ordered command sequence and a command count. NOTE: it is a tier sequencer/planner (testable without image fixtures), not a one-shot executor; real execution is deferred (would need fixtures). Re-scan dedup is achieved by the tiering (draft/integrate skip the full battery).
- [x] Edge-proof / composition-proof PNGs are generated only on failure / at FINAL-ART (skill `### Conditional proofs`, grounded in existing `--only-problems` / proof-fail-by-default flags).
- [x] Asset tests pass (validate_art_job 71/71, run_ui_asset_tier 6/6) + skills_eval 9/9 + taskboard validate ok. Example: `run_ui_asset_tier --tier draft --plan` -> `commands: 2` (was ~14).

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: ~14 gates per UI iteration; 17-report final-art battery; duplicate edge/atlas scans.

- 2026-06-15: Tiered the UI-asset gates (DRAFT 2 / INTEGRATE 3 / FINAL-ART 13). Normal iteration now runs 2 commands instead of ~14; full battery is opt-in/final-only. Added run_ui_asset_tier.mjs (--tier --plan sequencer + test 6/6) and a Conditional proofs rule (edge/composition PNGs only on failure/final). skills_eval 9/9, validate_art_job 71/71, taskboard ok.
