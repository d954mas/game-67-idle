---
id: T0052
title: Enforce first-screen scope discipline and FTUE beat cap
status: backlog
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

- [ ] gdd/feature skills state: a first slice = one goal + one primary action; an explicit FTUE beat cap (e.g. <=N) for the first playable.
- [ ] A rule to split runtime content from first-session presentation is recorded.
- [ ] It ties to the freeze in T0045 (no content expansion while the first screen fails).
- [ ] `node tools/skills_eval.mjs` + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

- Pick the concrete beat cap N (lead decision).

## Log

- 2026-06-15: Created from full pipeline review. Evidence: 14-beat FTUE in Rune Marches; scope creep into systems over first-screen read.
