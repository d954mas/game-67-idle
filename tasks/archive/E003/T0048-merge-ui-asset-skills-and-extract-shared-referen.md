---
id: T0048
title: Merge UI asset skills and extract shared reference deconstruction
status: done
epic: E003
priority: P1
tags: [skills, subtraction, assets, reference]
created: 2026-06-15
updated: 2026-06-15
---

## What

`generated-game-ui-assets` (538 lines) and `game-asset-pipeline` (389) duplicate
~90% of the same UI-asset gates. The reference-deconstruction block
(Lock/Intake/Definition-of-Ready/Digest/Source-Ladder/Evidence-Board) is pasted
near-verbatim across 3 skills (primary-gdd, visual-art, feature-iteration; ~150
duplicated lines). Merge the two asset skills into one canonical asset skill;
extract ONE shared reference-deconstruction reference file that all skills point
to. Make the heavy apparatus apply only in central/deep mode; quick-check is one
short paragraph.

## Done when

- [x] `generated-game-ui-assets` stays canonical; `game-asset-pipeline` reduced 389->192 lines - duplicated ~14-gate UI walkthrough replaced by a pointer to the canonical skill; unique source/pack hygiene + provenance kept.
- [x] The 3 skills (primary-gdd 227->174, visual-art 289->269, feature-iteration 185->154) replace the duplicated Reference Lock/Intake/Source-Ladder/Evidence-Board prose with a short digest + a pointer to the existing `gamedesign/knowledge/reference_deconstruction.md`.
- [x] Heavy reference apparatus now lives only in the shared doc; each skill keeps a short digest. ~383 lines cut net across the 5 files.
- [x] `node tools/skills_sync.mjs` regenerated `.claude`; `node tools/skills_eval.mjs` 9/9; `node tools/taskboard/cli.mjs validate` ok. (skills_eval config: dropped 3 UI-only needles from game-asset-pipeline whose content moved to the canonical skill.)

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: 3 art/asset skills = 1216 lines (~31% of skills); reference block duplicated in 3 skills.
- 2026-06-15: Deduplicated. (A) Reference-deconstruction block in primary-gdd/visual-art/feature-iteration replaced with a 3-6 line digest + pointer to `gamedesign/knowledge/reference_deconstruction.md` (preserving all skills_eval-required needles). (B) `game-asset-pipeline` collapsed its duplicated UI-gate walkthrough to a pointer to `generated-game-ui-assets` (canonical), keeping unique asset/pack hygiene; removed 3 UI-only needles from its skills_eval body config (moved to the canonical skill). Net -383 lines. skills_eval 9/9, taskboard ok. Spot-checked asset-pipeline structure stays coherent (Workflow/Rules/Production Gate/Pack Builder).
