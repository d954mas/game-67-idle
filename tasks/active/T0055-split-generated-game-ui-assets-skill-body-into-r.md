---
id: T0055
title: Split generated-game-ui-assets skill body into references
status: backlog
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

- [ ] SKILL body trimmed to ~200 lines: workflow, gate tiers, failure response, and pointers; deep rule sections moved to `references/`.
- [ ] All skills_eval required body needles for this skill still present (update tools/skills_eval.mjs only if a needle's content intentionally moves to a reference, and justify).
- [ ] `node tools/skills_sync.mjs` + `node tools/skills_eval.mjs` (9/9) + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

## Log

- 2026-06-15: Captured from the post-implementation review (remaining over-sized skill).
