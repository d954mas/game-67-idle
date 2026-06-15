---
id: T0049
title: Tier UI asset gates and collapse final-art report battery
status: backlog
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

- [ ] Three explicit tiers defined; a normal UI iteration runs <=3 commands (draft/integrate); the full battery is opt-in / final-only.
- [ ] One orchestrator runs intake -> build -> audit once and emits a single pass/fail; redundant edge/intake/atlas re-scans deduped.
- [ ] Edge-proof / composition-proof artifacts are generated only on failure, not per asset.
- [ ] Asset-tool tests + `node tools/taskboard/cli.mjs validate` pass; an example UI iteration shows the reduced command count.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: ~14 gates per UI iteration; 17-report final-art battery; duplicate edge/atlas scans.
