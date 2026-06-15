---
id: T0046
title: Add fun and reference-feel owner for the core moment
status: done
epic: E003
priority: P0
tags: [quality, feel, ai-workflow]
created: 2026-06-15
updated: 2026-06-15
---

## What

No skill owns "is it fun?" or "does it play like the reference in motion?".
"fun" appears only as a risk to file (primary-gdd-pipeline:183), and the 6-axis
visual rubric scores static screenshots only (no motion / juice / game-feel /
reference resemblance in play). The Splash Rods catch moment was never built as
a felt reveal ("the catch moment is missing a premium reveal", fishing failure
:55-56). Add a single cheap, mandatory owner for core-moment feel: a short
play-in-motion check (does the core action reward; does it resemble the
reference in motion, not just a still). Keep it lightweight - observation
against a short recording/sequence, not a new artifact battery.

## Done when

- [x] `game-feature-iteration` step 6 owns the core-moment feel + reference-in-motion check, with a cheap 3-criteria procedure against a short capture.
- [x] The check is mandatory for the first playable slice and explicitly cheap (observation against a short capture, no manifest/provenance load).
- [x] Ties into the visual definition of done (T0045): AGENTS.md "done" includes "the core moment feels right".
- [x] `node tools/skills_eval.mjs` (9/9) + `node tools/taskboard/cli.mjs validate` (ok) pass.

## Open questions

- RESOLVED: section inside `game-feature-iteration` (not a separate skill), to avoid skill sprawl.

## Log

- 2026-06-15: Created from full pipeline review. Gap: nothing in the pipeline tests for fun or reference-in-motion feel.
- 2026-06-15: Added the core-moment feel check to `game-feature-iteration` step 6 (3 felt criteria: immediate readable response, payoff reads as one satisfying moment, resembles the reference in motion) - explicitly cheap, mandatory for the first playable slice, and named as this skill's ownership of "is it fun / does it play like the reference". Added an Implementation Rule: build the core moment as a felt reveal, not a silent state transition (citing the fishing catch-moment failure). skills_sync regenerated; skills_eval 9/9; taskboard validate ok.
