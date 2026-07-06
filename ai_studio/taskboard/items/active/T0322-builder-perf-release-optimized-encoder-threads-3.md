---
id: T0322
title: "builder perf: release-optimized encoder + threads (39s cold / 1s warm vs 2-3.7m)"
status: done
project: P001
epic: ""
priority: P1
tags: [template, engine, builder, perf, vibejam-retro]
created: 2026-07-06
updated: 2026-07-06
---

## What

VibeJam evidence: `game_asset_packs` took 2.0-3.7 min per rebuild (sessions 31dc,
32a3) after Basis ETC1S landed. Root causes: (1) builder libs (incl. basisu C++
encoder) compiled at -O0 inside the game's native-debug tree; (2) neither game
nor template called `nt_builder_set_threads*` — encode ran single-threaded.
Content cache was already on and correct. Lead decision: builder is an offline
tool — always build it with full optimizations.

## Done when

- [x] Builder targets compile with -O2 even in Debug trees — implemented as a
      CONSUMER-SIDE override in template + rb-dark-rpg CMakeLists (foreach
      target_compile_options after add_subdirectory of the builder). Engine
      left untouched: its own CI already builds pack builders with a release
      preset ("release, no ASAN — fast encoding", ci.yml:325), so no engine
      change is warranted. Verified: -O2 present on nt_basisu_encoder compile
      lines in game + both template build dirs with engine tree pristine.
- [x] `nt_builder_set_threads_auto(ctx)` in template AND rb-dark-rpg build_packs.c.
- [x] Measured on the real jam corpus (115 assets): cold cache full re-encode
      **39s**, warm cache **1s** (was 2.0-3.7 min single change).

## Open questions

## Log

- 2026-07-06: lead directive "сборщик ассетов нужно явно собирать в релизной
  сборке со всеми оптимизациями". Implemented engine CMake foreach -O2 override
  + threads_auto in game/template. Verified: build_game_packs relinked, cold
  re-encode 39s, warm rebuild 1s (games/rb-dark-rpg native-debug). Engine change
  lives in submodule working tree — needs engine commit/PR (pre-commit checks:
  CMake-only change; build verified via game tree). Note: leftover measurement
  backup at games/rb-dark-rpg/build/_cache_bak_measure (gitignored) — safe to
  delete manually.
