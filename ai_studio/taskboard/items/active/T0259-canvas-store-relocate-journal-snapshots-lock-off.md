---
id: T0259
title: "Canvas store: relocate journal/snapshots/.lock off YandexDisk to local cache (review item 3, lead approved)"
status: review
project: ""
epic: ""
priority: P1
tags: [canvas, store, perf]
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Landed 6986a581 + config pin f3723576 (AppData/Local, off sweepable repo tmp/). My review + suites 540/540 + 51/51. Live-verified on real Demo: history read triggered move-on-first-access, YandexDisk dir now project.json+files only, 213 snapshots in cache, undo chain reads. :8780 restarted, smoke 200/200. Worker flagged latent snapshotForEntry {} silent-wipe path in ops.mjs (unreachable in cross-machine flow) - follow-up candidate.
