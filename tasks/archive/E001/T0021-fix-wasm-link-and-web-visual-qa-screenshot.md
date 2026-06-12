---
id: T0021
title: Fix WASM link and web visual QA screenshot
status: done
epic: E001
priority: P1
tags: [fantasy-pocket-rpg, wasm, qa]
created: 2026-06-11
updated: 2026-06-11
---

## What

WASM target links reliably and produces a browser/mobile portrait screenshot for the playable slice.

## Done when

- [x] `cmake --build --preset game-wasm-qa` completes without timeout
- [x] local web server can serve `build/game_67_idle/wasm-qa/index.html`
- [x] browser/mobile portrait screenshot proves nonblank readable UI
- [x] task T0005 can close its web/mobile proof criterion

## Open questions

## Log

- 2026-06-11: Created after `game-wasm-debug` and `game-wasm-qa` both reached the final Emscripten compile/link stage but timed out before producing `index.html`. Native desktop and native portrait evidence are already available.
- 2026-06-11: Fixed `wasm-qa` as a fast visual QA tier: `cmake --build --preset game-wasm-qa` now completes in 3-5s after CMake creates a placeholder pack for clean web builds and the QA preset sets explicit Emscripten env/debug-save settings. Web proof captured with `C:\Users\ROG\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe tmp/web_mobile_visual_audit.mjs`; served `index.html`, `index.js`, and `index.wasm`; screenshot `build/captures/web_mobile_wasm_qa.png` is nonblank/readable at 390x844.
