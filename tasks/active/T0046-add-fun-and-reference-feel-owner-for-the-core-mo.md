---
id: T0046
title: Add fun and reference-feel owner for the core moment
status: backlog
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

- [ ] One skill section (or a small skill) owns core-moment feel + reference-in-motion, with a cheap procedure (short capture/sequence + 2-3 felt criteria).
- [ ] The check is mandatory for the first playable slice and explicitly cheap (no manifest/provenance load).
- [ ] It ties into the visual definition of done (T0045): "feels right" is part of done.
- [ ] `node tools/skills_eval.mjs` + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

- Separate tiny skill vs a section inside `game-feature-iteration`? Lean: a section, to avoid skill sprawl.

## Log

- 2026-06-15: Created from full pipeline review. Gap: nothing in the pipeline tests for fun or reference-in-motion feel.
