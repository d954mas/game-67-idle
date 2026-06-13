---
id: T0025
title: Validate native PC release package
status: review
epic: ""
priority: P1
tags: [release, native, packaging, validation]
created: 2026-06-12
updated: 2026-06-13
---

## What

Create and validate a repeatable native PC release package path for 67 World.
The package must use the current `game_seed` build outputs, include the
runtime art pack, and leave a runnable artifact folder that can be handed to a
tester without relying on chat-only commands.

## Done when

- [x] Native release configure/build passes with `cmake --preset native-release`
      and `cmake --build --preset native-release`.
- [x] A package command stages `game_seed.exe`, required `assets/`, and a
      short run/readme file into `build/release/67-world-pc/67-world`.
- [x] VS Code native build/run/release/package tasks reference current
      `game_seed` presets, targets, and output paths.
- [x] Release artifact validation checks that the executable and
      `assets/world67_art.ntpack` exist and are non-empty.
- [x] Native runtime can load package-local `assets/world67_art.ntpack` when
      launched from the release folder, while keeping the existing dev/build
      art-pack fallback.
- [x] Package command creates tester-facing `README.txt`, `release_manifest.json`,
      `CHECKSUMS.txt`, and `67-world-pc.zip`.
- [x] Packaged executable smoke-test launches the release build from the
      release folder and verifies zip contents, checksums, process liveness,
      and screenshot capture. First-loop gameplay/audio remain covered by the
      DevAPI child-test readiness scenario because release builds intentionally
      do not expose DevAPI.

## Open questions

None.

## Log

- 2026-06-12: Started from release gate after native visual density reached
  review. Existing CMake release preset works, but VS Code tasks/launch still
  referenced old `game_67_idle` paths and non-existent presets.
- 2026-06-12: Added `tools/package_native_release.mjs`; updated
  `.vscode/tasks.json` and `.vscode/launch.json` to current `game_seed`
  outputs and pack-builder target names. Evidence passed:
  `cmake --preset native-release`; `cmake --build --preset native-release`;
  `node tools/package_native_release.mjs`; package contains
  `build/release/67-world-pc/67-world/67-world.exe` (707072 bytes) and
  `assets/world67_art.ntpack` (20995020 bytes). JSON configs parse and `rg`
  found no stale `game_67_idle`, `game-native`, `pack-native`,
  `build_game_67_idle`, or `game-wasm` references in `.vscode`.
- 2026-06-13: Reopened package polish after finding the release folder could be
  file-present but not truly self-contained: native runtime loaded the dev
  build-pack path before the packaged `assets/` path. Added package-local art
  pack loading with dev fallback, richer release package metadata/checksums/zip,
  and `tools/devapi/scenarios/package_release_smoke.py` to launch the packaged
  exe from its own folder.
- 2026-06-13: Evidence passed after package polish:
  `node --check tools/package_native_release.mjs`;
  `py -3.12 -m py_compile tools/devapi/devapi_client.py tools/devapi/scenarios/package_release_smoke.py tools/devapi/scenarios/child_test_readiness.py`;
  `cmake --build --preset native-release`;
  `node tools/package_native_release.mjs` produced
  `build/release/67-world-pc/67-world/67-world.exe` (713728 bytes),
  `assets/world67_art.ntpack` (20995020 bytes), and
  `build/release/67-world-pc/67-world-pc.zip` (21712166 bytes);
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9288 build/captures/scenarios/package_release_smoke_v3.png`
  passed from package cwd; `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/package_release_smoke_v3.png`
  passed.
- 2026-06-13: Evidence passed for updated package readiness gate:
  `cmake --build --preset native-debug`;
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9290 build/reports/child_test_readiness_v10_package.json build/captures/scenarios/child_test_readiness_v10_package`
  passed with package manifest/checksums/zip ok; pixel health passed for all
  five v10 child-test screenshots.
