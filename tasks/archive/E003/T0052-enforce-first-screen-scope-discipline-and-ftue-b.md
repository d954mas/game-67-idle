---
id: T0052
title: Enforce first-screen scope discipline and FTUE beat cap
status: done
epic: E003
priority: P2
tags: [gdd, quality, scope]
created: 2026-06-15
updated: 2026-06-15
---

## What

Rune Marches FTUE ballooned to 14 beats ("FTUE Is Overloaded... a casual player
will not understand the hierarchy", rune :65-72) while the look never reached the
bar; work optimized for state coverage over first-screen player read (rune
:50-52). Add a scope rule: the first screen = one goal, one primary action; a
hard cap on FTUE beats for a first slice; split runtime content from
first-session presentation.

## Done when

- [x] AGENTS.md Direction + `primary-gdd-pipeline` Stop-And-Reframe state: a first playable slice has one goal and one primary action, with the first-session/FTUE chain capped at <=3 beats (default).
- [x] The rule to split runtime content (world/routes/systems) from first-session presentation is recorded in both places.
- [x] Ties to the T0045 visual-first freeze (AGENTS: "This pairs with the visual-first freeze").
- [x] `node tools/skills_eval.mjs` 9/9 + `node tools/taskboard/cli.mjs validate` ok.

## Open questions

- RESOLVED: beat cap defaulted to <=3 for the first playable; the lead may raise it for a specific game.

## Log

- 2026-06-15: Created from full pipeline review. Evidence: 14-beat FTUE in Rune Marches; scope creep into systems over first-screen read.
- 2026-06-15: Added first-screen scope discipline to AGENTS.md Direction (one goal + one primary action; FTUE <=3 beats default; split runtime content from first-session presentation; pairs with visual-first freeze) and a matching Stop-And-Reframe trigger in primary-gdd-pipeline. skills_eval 9/9, taskboard ok.
