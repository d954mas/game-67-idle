---
id: T0357
title: Consolidate Studio Python and CMake toolchains with build benchmarks
status: done
project: P001
epic: E015
priority: P1
tags: [python, cmake, performance]
created: 2026-07-10
updated: 2026-07-11
---

## What

Make the root `.venv` the one predictable Python entry point for ordinary
Studio tooling, mechanically split oversized game/template CMake ownership,
and benchmark startup/build cost before adding abstraction.

## Done when

- [x] `studio.config` exposes the Python path and every ordinary Studio command
      resolves the same root `.venv`; top-level requirements are pinned and
      checked, while pip-resolved transitive dependencies are not claimed as a
      reproducible full lock.
- [x] Heavy specialist tools may keep isolated environments, but no bundled
      Python or second general-purpose environment is introduced.
- [x] The documented Windows repair path fixes the user-level Python association
      without requiring the broken system Python in normal operation.
- [x] Game/template CMake is split into explicit game-owned include files by
      concern, with byte-equivalent targets/options and no hidden framework.
- [x] Cold, warm, and no-op command/build timings are captured beside the code;
      any enforced regression threshold is based on a stable local baseline.

## Open questions

None.
## Log

- 2026-07-10: Reuse the existing root `.venv`; do not repair the workflow by
  relying on global Python or by vendoring a runtime.
- 2026-07-10: T0353 consumes the stabilized toolchain/build path; T0395 benchmarks the resulting end-to-end loop.
- 2026-07-10: Resolved planning detail: mechanical CMake include boundaries are GameOptions, GameAssets, GameCodegen, GamePlatform, and GameTests with the root file as conductor.
- 2026-07-11: Checkpoint: studio.config already points at the working root .venv (Python 3.12.4), but ordinary commands still hard-code the broken system py -3.12 launcher. Template and web-dressup CMake remain monolithic; template CMake also contains preserved unstaged T0393 audio WIP. Starting incremental Python resolver/pin check, then mechanical GameOptions/GameAssets/GameCodegen/GamePlatform/GameTests splits and measured baselines. Audio behavior/content will remain unchanged and unstaged.
- 2026-07-11: Added one strict cross-platform root-venv resolver, canonical runner/check/setup commands, six pinned ordinary-Studio dependencies, and active-contract regression scanning. Image bridge, video helpers, Items Viewer, Canvas generators, Runtime Automation, feature metadata, and current game/template instructions no longer rely on ambient Python or the broken launcher. Specialist GPU/video environments remain explicit exceptions.
- 2026-07-11: Split web-dressup and template CMake ownership into exactly GameOptions, GameAssets, GameCodegen, GamePlatform, and GameTests plain include files. Root files remain conductors with no shared framework. The committed template conductor and include files are derived from the pre-audio base; the working T0393 audio overlay remains unstaged.
- 2026-07-11: Verification: focused Node suites 124/124 pass; root Python 3.12.4 reports prefix_ok with 6/6 exact pins; Runtime Automation and feature/DevAPI Python suites 129/129 pass; web native CTest 23/23 passes; CMake conductor/target/CTest parity checks pass; Architecture Map strict reports 343 mapped / 775 scanned with 0 unmapped, missing, duplicate, or invalid-description issues; cached diff check passes.
- 2026-07-11: The preserved current T0393 overlay still makes full template build stop at its pre-existing undefined audio_miniaudio_test_per_clip_limit and audio_miniaudio_test_total_limit symbols. T0357 base-only conductor configure/game-build and declaration parity are green; no T0393 file or audio behavior is claimed as T0357 evidence.
- 2026-07-11: Benchmark v2 records three Windows samples with toolchain/revision/dirty context and no enforced threshold. Web medians in ms: Python 67.1, cold configure 12432.1, cold build 49646.5, warm 709.0, no-op 349.0. Template medians: 41.5, 6508.6, 28166.7, 640.8, 350.8; template evidence explicitly includes the preserved audio overlay.
- 2026-07-11: Independent review cycle 1 found the Windows-only resolver HIGH plus actionable dependency, parity, hermetic-test, benchmark, URL-path, and stale-comment issues; all were fixed. Cycles 2-3 converged at 0 HIGH and 0 actionable MEDIUM/LOW across architecture, correctness, ownership, tests, process, performance, and context cost. Lead completion audit then migrated remaining active ordinary Python contracts and added a focused regression scan.
- 2026-07-11: Quality: QTECH_001=pass; evidence: strict Python prefix/pin checks, cross-platform resolver tests, active-contract scan, Node/Python suites, native CTest, CMake parity, measured benchmarks, Architecture Map strict validation, and independent diff review.
- 2026-07-11: Closed after canonical cross-platform Python, mechanical CMake ownership splits, measured build baselines, strict integration checks, and three review cycles passed.
- 2026-07-11: Wave 1 integration correction: renamed the six exact top-level
  pins to `requirements.direct.txt` and documented that transitive packages are
  pip-resolved, removing the earlier full-lock implication without changing the
  installed dependency set.
