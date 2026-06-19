---
id: T0005
title: Disambiguate art-cluster skills by asset-path signal + owns/hands-off line
status: backlog
epic: E001
priority: P2
tags: [pipeline, skills]
created: 2026-06-19
updated: 2026-06-19
---

## What

The art/asset skills share trigger words (fake shots, sprites, UI kit, slice9)
in their descriptions and none names a sibling, so an auto-selector sees only
ambiguity. Tighten each art-cluster description to a non-overlapping trigger
surface and add one "owns X; hands off Y to <skill>" line, routing on the
touched asset path / file type. Borrowed: path-scoped rules + one-modality-per-
generator.

## Done when

- [ ] generated-game-ui-assets / game-visual-art-direction / game-asset-pipeline descriptions are non-overlapping and each names its sibling boundary
- [ ] skills presence check and skills_sync stay green

## Open questions

## Log
