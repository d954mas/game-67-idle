---
id: T0005
title: Automation and proof
status: done
epic: E001
priority: P1
tags: [fantasy-pocket-rpg, devapi, qa]
created: 2026-06-11
updated: 2026-06-11
---

## What

Desktop harness can click the critical path and capture screenshots.

## Done when

- [x] emulated input path passes
- [x] desktop screenshot captured
- [x] mobile/web screenshot captured if web target exists

## Open questions

## Log

- 2026-06-11: Seeded from implementation_tasks.json phase list.
- 2026-06-11: Native desktop proof passes through DevAPI: `py -3.12 tools/devapi/smoke_test.py 9123`, `py -3.12 tools/devapi/agent_playtest.py 9126 --full-loop`, `py -3.12 tools/devapi/full_probe.py 9127`, screenshot `build/captures/fantasy_slice_text_full_loop.png`. Web/mobile screenshot remains a later target-specific validation step.
- 2026-06-11: Native visual proof expanded with runtime backgrounds and portrait layout: `py -3.12 tools/devapi/scenarios/capture_ruins_background.py 9135 build/captures/fantasy_slice_runtime_bg_ruins.png`, `py -3.12 tools/devapi/scenarios/capture_portrait_layout.py 9133 --dir build/captures/portrait_layout --size 390x844`. WASM web screenshot is not closed: `game-wasm-debug` and `game-wasm-qa` reached compile/link but timed out on the final Emscripten stage; tracked as T0021.
- 2026-06-11: Web/mobile proof closed via T0021. `cmake --build --preset game-wasm-qa` passes, and `C:\Users\ROG\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe tmp/web_mobile_visual_audit.mjs` captured readable 390x844 screenshot `build/captures/web_mobile_wasm_qa.png`.
