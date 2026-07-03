---
id: T0246
title: "Canvas: sliceRegions - single-region slice mints a loose element, no wrapper group (group only for 2+ crops)"
status: done
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Lead 2026-07-03: 'если я делаю один вырез региона из картинки, то группа не нужна'. Fix: in ops.mjs sliceRegions, when created.length === 1 skip the wrapper group (loose element at its grid spot, provenance/journal unchanged); 2+ crops keep the group (lead's 2026-07-02 rule). Update slice tests. BLOCKED on ops.mjs until T0238 lands - orchestrator applies inline right after.
- 2026-07-03: Applied inline right after T0238 freed ops.mjs: created.length>1 gates the wrapper group; run/args summaries carry group_id null for single crops. 311/311.
