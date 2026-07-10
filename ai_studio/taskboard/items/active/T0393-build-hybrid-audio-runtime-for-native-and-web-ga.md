---
id: T0393
title: Build hybrid audio runtime for native and web games
status: doing
project: P001
epic: E015
priority: P0
tags: [audio, template, feature, native, web, performance]
created: 2026-07-10
updated: 2026-07-10
---

## What

Ship production-useful audio now without waiting for the engine audio module.
Create an invariant `features/audio-core/` L1 runtime with one private backend
contract: pinned miniaudio for Windows/Linux and a thin browser-native WebAudio
backend for Emscripten. Keep cue/music enums and asset composition game-owned.

Audio sources are game assets stored as generic blobs in the game-selected
ntpack and addressed through generated hashes. The public game API and cue
catalog must not expose miniaudio types, JavaScript handles, paths, or backend
selection so a future engine backend can replace the interim providers.

## Done when

- [ ] A focused fake-backend test proves clip lifecycle, fixed limits,
      generation-safe handles, master/music/sfx gains, pause/enable semantics,
      failure behavior, and cleanup before a real backend is added.
- [ ] Windows/Linux use one pinned, licensed miniaudio implementation TU;
      unused capture/encoding/effects/codecs/backends are disabled and headless
      or missing-device startup degrades honestly without breaking CI.
- [ ] Emscripten uses a thin WebAudio backend with SFX/music/master gain graph,
      async decode status, one-shot sources, pause/resume, user-gesture unlock,
      and cleanup; it does not compile miniaudio into WASM.
- [ ] The template owns a small `game_audio` cue/music catalog and exposes only
      init/update/shutdown, gesture, settings, pause, play-cue, play/stop-music,
      and status operations to gameplay.
- [ ] A CC0 audio sample has license, provenance, SHA-256, and origin metadata;
      `build_packs.c` stores it in the current `game.ntpack` as a codec-neutral
      BLOB ID and CMake rebuilds the pack when the source changes.
- [ ] Persisted settings and platform lifecycle callbacks reach the same audio
      runtime; browser autoplay, decode failure, no-device, and disabled paths
      have explicit tests or evidence.
- [ ] Windows and Linux builds/tests pass; a real browser smoke proves audible
      state transitions after user gesture without console errors.
- [ ] A checked-in benchmark reports native/web build-time delta, binary/pack
      size delta, init/load latency, first-play latency, and memory/voice caps.
- [ ] Full template/Studio validation is green and two independent read-only
      reviews find no blocking correctness, boundary, or context-cost issue.

## Open questions

- Long music may use MP3 only after the native decoder and target browser smoke
  prove the chosen file; WAV is the guaranteed reference fixture and OGG is not
  claimed in v1.
- A later engine implementation replaces only the private backend. It does not
  move game-owned cue names or asset composition into the engine.

## Log

- 2026-07-10: Lead rejected deferral because games need sound before engine
  audio is available and accepted the hybrid miniaudio-native/WebAudio-web
  design. T0362 retains only the still-undiscussed quality/performance topics.
