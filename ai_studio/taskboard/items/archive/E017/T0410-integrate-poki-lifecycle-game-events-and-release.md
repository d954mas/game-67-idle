---
id: T0410
title: Integrate Poki lifecycle Game Events and release telemetry
status: done
project: P004
epic: E017
priority: P0
tags: [poki, analytics, platform]
created: 2026-07-11
updated: 2026-07-13
---

## What

Expose the approved Poki Game Events through the shared platform facade and
collect a release-safe 500-player funnel without PII or a custom backend dependency.

## Done when

- [ ] Shared facade supports `measure(category, what, action)` with Poki, mock and local game-event evidence.
- [ ] Once-guards prevent duplicate FTUE, round, recipe, Lookbook and active-time milestones.
- [ ] Exact triples include the six literal recipe IDs from the accepted GDD; expected first-session mock trace is asserted.
- [ ] Poki lifecycle starts after first input and pauses correctly across menu/ad/focus transitions.
- [ ] First styling input starts gameplay and round 1; pressing AWAKEN stops gameplay through runway and Recipe Card; Restyle resumes only on interactive Dress Room and starts the next round.
- [ ] Mock trace and Poki Inspector Event Log show the expected ordering without duplicates.
- [ ] No PII, random user identifier, full event-bus forwarding or analytics hard dependency ships.

## Open questions

- None for implementation; dashboard presentation is verified in Inspector without changing the frozen event names.

## Log
- 2026-07-13: Closure: waived; reason: prototype closed by lead before acceptance; evidence: lead decision 2026-07-13; committed pause 4697cd445 and .planning/.continue-here.md record incomplete gates
- 2026-07-13: Quality: not-applicable; reason: closure records cancellation and does not claim implementation or acceptance
