---
id: T0288
title: "rb-dark-rpg: combat stage responsive review fixes"
status: backlog
project: P003
epic: E011
priority: P1
tags: [rb-dark-rpg, combat, visual, uiux, qa]
created: 2026-07-05
updated: 2026-07-05
---

## What

Second iteration after T0286. Review the animated clash stage in real desktop
and phone captures, then apply targeted layout/readability fixes without
changing combat math, reward flow, or adding final generated actor art.

This task exists because T0286 should stay focused on proving the stage
presenter. T0288 owns the post-review cleanup loop: compare captures, list
specific issues, fix only those issues, and recapture evidence.

Scope:

- review T0286 captures for 960x540, 640x360, and phone portrait;
- fix stage/log/stats overlap, clipped labels, weak impact readability, and
  touch-size regressions;
- preserve the separate stage, stats, and log zones created by T0286;
- keep all combat events driven by `game_combat_event_t`.

Out of scope:

- final sprite sourcing/generation;
- reward-result redesign;
- new combat mechanics, skills, status effects, or tuning beyond visual timing.

## Done when

- [ ] A short review note or task log lists concrete visual issues found after
      T0286, with desktop and phone evidence paths.
- [ ] Each must-fix review issue is either fixed or explicitly deferred to a
      named follow-up task.
- [ ] Gate and mill fight captures show no incoherent overlap between stage,
      HP bars, stats, damage markers, and last-three-event log.
- [ ] Phone portrait keeps the clash readable without hiding the current HP,
      impact damage, or latest log event.
- [ ] Existing combat/unit scenario checks still pass.
- [ ] Final evidence is captured under `tmp/quality/`.

## Open questions

- This task should start only after T0286 has a running implementation and
  initial visual evidence.

## Log

- 2026-07-05: Created as the iterative review/fix pass after the animated
  clash-stage implementation in T0286.
