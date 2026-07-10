---
id: T0357
title: Consolidate Studio Python and CMake toolchains with build benchmarks
status: backlog
project: P001
epic: E015
priority: P1
tags: [python, cmake, performance]
created: 2026-07-10
updated: 2026-07-10
---

## What

Make the root `.venv` the one predictable Python entry point for ordinary
Studio tooling, mechanically split oversized game/template CMake ownership,
and benchmark startup/build cost before adding abstraction.

## Done when

- [ ] `studio.config` exposes the Python path and every ordinary Studio command
      resolves the same root `.venv`; dependencies are pinned and checked.
- [ ] Heavy specialist tools may keep isolated environments, but no bundled
      Python or second general-purpose environment is introduced.
- [ ] The documented Windows repair path fixes the user-level Python association
      without requiring the broken system Python in normal operation.
- [ ] Game/template CMake is split into explicit game-owned include files by
      concern, with byte-equivalent targets/options and no hidden framework.
- [ ] Cold, warm, and no-op command/build timings are captured beside the code;
      any enforced regression threshold is based on a stable local baseline.

## Open questions

- Which existing CMake file boundaries produce the smallest mechanical split?

## Log

- 2026-07-10: Reuse the existing root `.venv`; do not repair the workflow by
  relying on global Python or by vendoring a runtime.
