---
id: T0004
title: Poki audience test packet
status: dropped
epic: E001
priority: P1
tags: [poki, playtest, ftue, telemetry]
created: 2026-06-13
updated: 2026-06-15
---

## What

Prepare a lightweight test packet for casual web/mobile audience validation:
FTUE script, success metrics, screenshot checklist, survey prompts, and
telemetry events for the first Rune Marches slice.

## Done when

- [x] Poki/platform requirements or constraints are captured from usable
  sources or marked as assumptions.
- [x] First-session success metrics are defined for first action, first reward,
  first upgrade, confusion/stall points, and session length.
- [x] Test script covers desktop browser and mobile portrait.
- [x] Telemetry event names are mapped to the native/web implementation.
- [x] Native DevAPI exposes first-session telemetry counters for the mapped
  FTUE milestones.
- [x] A native automation probe exports a first-session telemetry report.
- [x] A short feedback survey asks about fantasy clarity, next action,
  difficulty, visual readability, and desire to continue.

## Open questions

- What sample size and age/tone boundaries should be used for the first test?
- Is the test intended for a private prototype link, Poki review, or an
  informal audience test first?

## Log

- 2026-06-13: Backlog task created from user goal to test on a Poki audience.
- 2026-06-13: Created first-session test packet, Poki source notes, and
  telemetry event map. Evidence:
  `gamedesign/projects/rune-marches/playtest/first_session_poki_packet.md`,
  `gamedesign/projects/rune-marches/sources/poki_platform_notes.md`, and
  `gamedesign/projects/rune-marches/data/playtest_telemetry.json`. Validation:
  passive-profiled JSON parse and `node tools/taskboard/cli.mjs validate`.
- 2026-06-13: Connected native runtime telemetry counters to the first-session
  event map. `game.rune.telemetry` now reports session, FTUE, combat, quest,
  route, level-up, and upgrade milestone counts; `game.rune.telemetry.reset`
  starts a clean telemetry session. Evidence: passive-profiled native build,
  smoke/full DevAPI probes, and Rune Marches desktop + portrait scenarios with
  telemetry assertions.
- 2026-06-13: Added `tools/playtest/rune_marches_probe.py` as the native
  automation-proxy report runner. It drives the first-session path, reads
  `game.rune.telemetry`, writes `tmp/rune_marches/playtest_probe_report.json`,
  captures `tmp/rune_marches/playtest_probe.png`, and exits nonzero if required
  milestones are missing. Evidence: passive-profiled probe run, native rebuild,
  desktop + portrait scenarios, JSON parse, and taskboard validation.
- 2026-06-15: Dropped during pipeline cleanup: Poki audience packet is historical Rune Marches work, not active validation lane.
