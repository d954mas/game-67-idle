---
id: T0181
title: Clarify restricted asset helper ownership
status: done
epic: E001
priority: P2
tags: [assets, licensing, docs]
created: 2026-06-30
updated: 2026-06-30
---

## What
`restricted.mjs` is now a current Asset Storage helper surface used by asset
pulling and the public-repo license guard. The docs/map still call it a
compatibility facade, which makes the reviewed license module look like it is
kept only for legacy behavior. Clarify ownership without changing behavior.

## Done when

- [x] License README describes `restricted.mjs` as current helper surface.
- [x] Architecture tree node describes the same current helper ownership.
- [x] Validate map, docs, and taskboard.
- [x] Commit and push the slice.

## Open questions

- None.

## Log

- 2026-07-01: Started after live asset scan found `restricted.mjs` still
  described as a compatibility facade in current license docs/map.
- 2026-07-01: Updated License README and Architecture Map node to describe
  `restricted.mjs` as the current shared helper surface for pull/guard. Verified
  stale compatibility wording is gone from asset license docs/skill, then
  validated docs, map, and taskboard.
- 2026-07-01: Clarified restricted.mjs as current asset license helper surface;
  validated docs/map/taskboard.
