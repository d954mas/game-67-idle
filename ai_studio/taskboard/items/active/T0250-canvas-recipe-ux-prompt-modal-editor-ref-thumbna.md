---
id: T0250
title: "Canvas: recipe UX - prompt modal editor + ref thumbnails in generation meta"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Landed f32ad5c6: prompt Edit modal (patchRecipeAction seam, Esc/overlay/Cancel discard, Ctrl+Enter save) + Generation section (engine/at/prompt View modal, refs_snapshot thumbnails). Orchestrator review fixes: readOnly not disabled, stopPropagation on Esc/Ctrl+Enter, no-op save guard. Gates: node --check OK, recipe+tree 45/45, full suite 392/392. Awaiting lead F5 verify.
