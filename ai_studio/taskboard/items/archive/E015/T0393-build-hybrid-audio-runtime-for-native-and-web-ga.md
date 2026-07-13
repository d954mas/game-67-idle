---
id: T0393
title: Build hybrid audio runtime for native and web games
status: done
project: P001
epic: E015
priority: P0
tags: [audio, template, feature, native, web, performance]
created: 2026-07-10
updated: 2026-07-12
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

## Format policy

- Short SFX ship as PCM WAV in v1; music ships as MP3.
- OGG is not a v1 delivery format.
- Ordinary game builds do not transcode audio. The committed delivery bytes
  are the bytes packed into the game-selected ntpack, preserving hashes and
  deterministic build time.
- Asset IDs and public gameplay APIs remain codec-neutral. Owning metadata
  records media type/codec, and pack validation checks extension, magic,
  provenance, hash, and decoder support.
- Native miniaudio and target browsers must decode the exact committed WAV/
  MP3 fixtures. Unsupported input fails; there is no silent fallback format.

## Done when

- [x] A focused fake-backend test proves clip lifecycle, fixed limits,
      generation-safe handles, master/music/sfx gains, pause/enable semantics,
      failure behavior, and cleanup before a real backend is added.
- [x] Windows/Linux use one pinned, licensed miniaudio implementation TU;
      unused capture/encoding/effects/codecs/backends are disabled and headless
      or missing-device startup degrades honestly without breaking CI.
- [x] Emscripten uses a thin WebAudio backend with SFX/music/master gain graph,
      async decode status, one-shot sources, pause/resume, user-gesture unlock,
      and cleanup; it does not compile miniaudio into WASM.
- [x] The template owns a small `game_audio` cue/music catalog and exposes only
      init/update/shutdown, gesture, settings, pause, play-cue, play/stop-music,
      and status operations to gameplay.
- [x] One short CC0 WAV SFX and one CC0 MP3 music fixture have license,
      provenance, SHA-256, origin, media-type metadata, and native/browser decode
      proof; `build_packs.c` stores both delivery files in the current
      `game.ntpack` under codec-neutral BLOB IDs and CMake rebuilds the pack when the source changes.
- [x] Persisted settings and platform lifecycle callbacks reach the same audio
      runtime; browser autoplay, decode failure, no-device, and disabled paths
      have explicit tests or evidence.
- [x] Windows and Linux builds/tests pass; a real browser smoke proves audible
      state transitions after user gesture without console errors.
- [x] A checked-in benchmark reports native/web build-time delta, binary/pack
      size delta, init/load latency, first-play latency, and memory/voice caps.
- [x] Full template/Studio validation is green and two independent read-only
      reviews find no blocking correctness, boundary, or context-cost issue.

## Open questions

None.
## Log

- 2026-07-10: Lead rejected deferral because games need sound before engine
  audio is available and accepted the hybrid miniaudio-native/WebAudio-web
  design. T0362 retains only the still-undiscussed quality/performance topics.
- 2026-07-10: Paused during final plan convergence. Partial audio WIP is preserved in the working tree, but execution resumes only after T0357, T0353, and T0397 provide canonical build/verify/integrity contracts.
- 2026-07-10: Audio format/backend decisions are closed in the task contract; no remaining planning question.
- 2026-07-10: Format policy ratified for auditability: WAV SFX, MP3 music, no OGG and no automatic build transcoding in v1.
- 2026-07-12: Checkpoint after T0397 at `19b15dcc9`. Resuming the preserved audio WIP on the real worktree under the approved dependency reorder T0397 -> T0393 -> T0353; preserve unrelated E017/game planning and implementation files, use exact staging only, and do not touch `external/neotolis-engine`.
- 2026-07-12: TDD/review fixes: added exact native MP3 decode proof, cross-init stale clip/voice regression, WebAudio running-state semantics, strict autoplay browser assertions, audio pack rebuild dependencies, and restored the T0357 modular CMake ownership split. `audio-core` advanced compatibly to 1.0.1 and the template dependency seed is synchronized.
- 2026-07-12: Windows evidence: native game build green; full template CTest 30/30. Linux evidence: strict audio-core runner 28 C tests green; clean Release consumer game build 301/301 and focused consumer CTest 6/6 green. The separate Debug consumer attempt exposed a pre-existing sanitizer-runtime linkage mismatch in test targets; Release and the strict warning-gated runner prove T0393 on Linux without changing unrelated sanitizer ownership.
- 2026-07-12: Real headed Chrome smoke with enforced user-gesture autoplay passed through a real canvas click: exact delivery bytes 8098 WAV and 28883 MP3 decoded, two sources started (SFX non-loop and music loop), pause/resume/stop transitions passed, final context running, and console/page/network errors were zero.
- 2026-07-12: Benchmark evidence: reproducible native plus paired Emscripten runners, raw Web evidence, merged raw/result consistency verifier PASS, native clean build +11.1317%, web clean build -2.3978%, executable/pack/JS/WASM deltas recorded, real-device init/load/unlock/first-play latency recorded, and 64 clip/32 voice plus 128 MiB per-clip/256 MiB aggregate decoded-PCM caps documented.
- 2026-07-12: Integration gates: scoped asset integrity 2/2; global integrity 110/110 with zero issues; feature contracts 6 modules/2 pointers; Architecture Map strict 0 issues; Taskboard validation green; staged diff check green.
- 2026-07-12: Three independent review/fix cycles closed all architecture, correctness, ownership, tests, process, performance, and context-cost findings. Final closure rechecks report 0 HIGH, 0 MEDIUM, 0 LOW actionable.
- 2026-07-12: Quality: QTECH_001=pass; QASSET_001=pass; evidence: Windows/Linux native and consumer builds/tests, exact native/browser decode of committed WAV+MP3, strict real-gesture Chrome smoke, reproducible benchmark verifier, full feature/integrity/map/taskboard gates, and two independent final reviews with 0 actionable findings.
- 2026-07-12: Quality: QTECH_001=pass; QASSET_001=pass; evidence: Windows 30/30; Linux strict audio plus Release consumer build and 6/6; headed Chrome real-gesture WAV+MP3 smoke; benchmark verifier; feature/map/taskboard/integrity gates; final reviews 0 actionable
