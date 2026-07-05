---
id: T0259
title: "Canvas store: relocate journal/snapshots/.lock off YandexDisk to local cache (review item 3, lead approved)"
status: review
project: P001
epic: E010
priority: P1
tags: [canvas, store, perf]
created: 2026-07-03
updated: 2026-07-03
---

## What

Relocate Canvas journal, snapshots, and lock files off YandexDisk into local
cache storage so heavy history I/O no longer syncs through cloud folders.

## Done when

- [x] Store migration lands with cache location pinned outside the repo.
- [x] Existing project history is read through the moved cache path.
- [ ] The latent snapshotForEntry follow-up is accepted, deferred, or split.

## Open questions

## Log
- 2026-07-03: Landed 6986a581 + config pin f3723576 (AppData/Local, off sweepable repo tmp/). My review + suites 540/540 + 51/51. Live-verified on real Demo: history read triggered move-on-first-access, YandexDisk dir now project.json+files only, 213 snapshots in cache, undo chain reads. :8780 restarted, smoke 200/200. Worker flagged latent snapshotForEntry {} silent-wipe path in ops.mjs (unreachable in cross-machine flow) - follow-up candidate.
