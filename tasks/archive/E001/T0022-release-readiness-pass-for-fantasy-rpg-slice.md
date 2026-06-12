---
id: T0022
title: Release readiness pass for fantasy RPG slice
status: done
epic: E001
priority: P1
tags: [fantasy-pocket-rpg, release, qa]
created: 2026-06-11
updated: 2026-06-11
---

## What

Run a release-readiness pass for the current fantasy RPG slice and produce a short ship/blocker list.

## Done when

- [x] native release build completes
- [x] full DevAPI playtest passes on the release or release-like build
- [x] web/mobile QA screenshot still passes after release-readiness changes
- [x] visual/gameplay review records ship blockers versus polish-only issues
- [x] taskboard has follow-up tasks for every blocker

## Open questions

## Log

- 2026-06-11: Created after T0005/T0021 closed automation proof; this is the next concrete step toward release rather than broad polish.
- 2026-06-12: Started release-readiness pass with independent code, gameplay, and visual/UX review agents plus local build/playtest verification.
- 2026-06-12: Fixed P0/P1 blockers found during review: Hunter's Ford now renders as enabled `Travel` after unlock; Search no longer unlocks camp loot before the ruin is resolved; Search now grants `has_dragon_marked_shard` and visible Resolve pressure; Calm Beast can fail and bite back; `native-qa` has a separate output tier; `game-wasm-release` now produces `index.html/js/wasm`; release pack is non-empty.
- 2026-06-12: Evidence: `cmake --build --preset game-native-release --clean-first`, `py -3.12 tools/devapi/smoke_test.py 9144`, `py -3.12 tools/devapi/agent_playtest.py 9145 --full-loop --exe build/game_67_idle/native-qa/game_67_idle.exe --out-dir build/captures/release_readiness_native_qa`, `py -3.12 tools/devapi/scenarios/state_roundtrip.py 9146`, `cmake --build --preset game-wasm-release`, web release screenshot `build/captures/web_mobile_wasm_release.png`, pack `build/game_67_idle/game_67_idle.ntpack` is 163 bytes.
- 2026-06-12: Remaining release follow-ups are tracked: T0023 web runtime backgrounds, T0024 build-tree asset header, T0025 optimized WASM release without workaround, T0026 real packaged asset format.
