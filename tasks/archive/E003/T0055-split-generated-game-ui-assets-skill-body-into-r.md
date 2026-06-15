---
id: T0055
title: Split generated-game-ui-assets skill body into references
status: done
epic: E003
priority: P2
tags: [skills, context, subtraction]
created: 2026-06-15
updated: 2026-06-15
---

## What

After the T0048/T0049 cuts, `generated-game-ui-assets/SKILL.md` is still the
largest skill at ~531 lines (2x the next, visual-art at 269). Reference-manual
depth that loads every UI session — Slice9 Rules, Atlas/Reuse Rules, Icon/Sprite
Rules — should move to `references/` (the pattern primary-gdd-pipeline already
uses), keeping the SKILL body to the workflow + tier routing + pointers.

## Done when

- [x] SKILL body trimmed 531 -> 385 lines; the 4 deep rule sections (Slice9, Atlas/Reuse, Icon/Sprite, Responsive) moved to `references/ui-asset-rules.md` (176 lines, content intact) with a References pointer table. (~200 was not reachable without gutting the multi-step Workflow + Gate Tiers, which must stay in the body.)
- [x] skills_eval body needles preserved; 2 needles retargeted to phrases that survive in the body (`drawing primitives` -> `procedural shapes`; reworded a wrapped `runtime integration` line), each asserting the same rule. Justified in the commit.
- [x] `node tools/skills_sync.mjs` + `node tools/skills_eval.mjs` (9/9) + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

## Log

- 2026-06-15: Captured from the post-implementation review (remaining over-sized skill).
- 2026-06-15: Moved Slice9/Atlas/Icon/Responsive deep rules from generated-game-ui-assets SKILL.md (531->385) into references/ui-asset-rules.md, following the primary-gdd-pipeline references pattern. Preserved skills_eval needles (2 retargeted, justified). skills_eval 9/9, taskboard ok.
