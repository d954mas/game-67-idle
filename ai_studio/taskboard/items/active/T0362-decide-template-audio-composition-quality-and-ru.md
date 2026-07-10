---
id: T0362
title: Decide template audio composition quality and runtime performance seams
status: idea
project: P001
epic: E015
priority: P1
tags: [template, audio, quality, performance]
created: 2026-07-10
updated: 2026-07-10
---

## What

Close the audit topics that were identified but not discussed far enough to
authorize implementation: reference-template audio/composition ownership,
remaining quality-rule coverage, and runtime/template performance seams.

For each topic, inspect current code and existing cards first, present one
concrete problem and alternatives, then let the lead choose no-op, refactor, or
new work. Do not create a generic framework from this holding card.

## Done when

- [ ] Each topic has current code/benchmark evidence and links to overlapping
      existing work.
- [ ] The lead accepts a concrete scoped task or records an evidence-backed
      no-op; unreviewed topics remain `idea`.

## Open questions

- Does the reference template need a reusable audio/composition feature, or is
  the current engine/game boundary already sufficient?
- Which audit quality gaps are real after the Taskboard and CI tasks land?
- Which runtime/template path is actually slow under measurement, and is it
  Studio-owned, feature-owned, game-owned, or engine-owned?

## Log

- 2026-07-10: These areas were still in the original full-audit queue when the
  conversation pivoted into Balance. They are preserved as undecided work, not
  accepted findings.
