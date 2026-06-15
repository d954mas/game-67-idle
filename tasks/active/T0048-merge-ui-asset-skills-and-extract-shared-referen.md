---
id: T0048
title: Merge UI asset skills and extract shared reference deconstruction
status: backlog
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

- [ ] One canonical asset skill; the other reduced to a pointer (or removed) - no duplicated gate list.
- [ ] One shared reference-deconstruction reference file; primary-gdd / visual-art / feature-iteration link to it instead of restating (~150 dup lines cut).
- [ ] Heavy Lock/Ladder/Evidence-Board applies only in central/deep mode; quick-check is one short paragraph.
- [ ] `node tools/skills_sync.mjs` regenerates `.claude`; `node tools/skills_eval.mjs` + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Evidence: 3 art/asset skills = 1216 lines (~31% of skills); reference block duplicated in 3 skills.
