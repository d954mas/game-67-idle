---
id: T0340
title: platform-sdk mock ads + template debug buttons
status: backlog
project: P001
epic: E014
priority: P1
tags: [platform-sdk, mock, web, testing]
created: 2026-07-08
updated: 2026-07-08
---

## What

Implement the first concrete runtime slice for `features/platform-sdk`: a mock platform SDK adapter and template debug/test controls that make ad flows verifiable before real portal SDK adapters are wired.

The old JS reference at `C:\projects\ai_games\game-ai-template\src\features\core\platform` is a behavior reference only. Its method set was: `whenReady`, `gameLoadingFinished`, `gameReady`, `gameplayStart`, `gameplayStop`, `commercialBreak`, `showRewarded`, `onPause`, `onResume`, `onAudioToggle`, `onVisibilityChange`, `measure`, `loadData`, `saveData`, `destroy`. Keep the new API aligned with `features/platform-sdk/references/contract.md`.

`mock` is a platform SDK adapter, not a target. It is used by `local` and `itch`; `local -> mock` should provide deterministic ad simulations for dev/test, while `itch -> mock` should be production-safe unsupported/no-op for ads.

## Done when

- [ ] The mock SDK adapter implements the wrapper contract path needed by the template: ready/gameReady/gameplay start-stop, pause/resume callbacks, interstitial, rewarded, event tracking, load/save no-op or local fixture behavior, and destroy.
- [ ] `local -> mock` interstitial displays a mock ad overlay and returns a structured result with `supported`, `shown`, and `reason`.
- [ ] `local -> mock` rewarded displays a mock rewarded overlay with success, skip/close, and decline/fail paths; reward is granted only on explicit success.
- [ ] Pause/resume callbacks fire exactly once around every mock ad path, including skip/fail paths.
- [ ] `itch -> mock` returns production-safe unsupported/no-op ad results without network or fake ad UI.
- [ ] Template debug/test UI exposes buttons for `Show interstitial ad` and `Show rewarded ad`, plus current target, current platform SDK, capabilities, and last ad result.
- [ ] Debug/test UI is excluded from production release artifacts; artifact inspection confirms debug button labels and `debug_test` placement string are absent.
- [ ] Tests cover target-to-SDK mapping, local mock interstitial, local mock rewarded success, local mock rewarded fail/decline, itch no-op behavior, and destroy listener cleanup.
- [ ] Validation evidence is recorded in the task log with exact commands/results.

## Open questions

- Should mock overlay support a visible countdown only, or also explicit skip/close for interstitial?
- Should `itch -> mock` ever allow debug overlay in non-release preview builds, or always no-op to mirror production?
- Where should the template debug/test UI live so it is available to copied games but gated out of release builds?

## Log

- 2026-07-08: Created from lead clarification: mock must be a real testable SDK adapter with ad UI/skip/reward paths, and the template needs debug buttons for interstitial/rewarded smoke checks.
