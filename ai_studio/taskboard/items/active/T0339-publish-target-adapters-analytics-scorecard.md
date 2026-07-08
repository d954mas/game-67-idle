---
id: T0339
title: publish-target adapters + analytics scorecard
status: backlog
project: P001
epic: E014
priority: P1
tags: [publish, web, analytics, monetization]
created: 2026-07-08
updated: 2026-07-08
---

## What

Build the first commercial-factory slice: define reusable publish-target adapters, a universal platform SDK wrapper, and a compact analytics scorecard so each web game can be packaged, checked, published or queued for a target, and judged by the same metrics.

Target platforms are `local`, `itch`, `poki`, `yandex`, and `playgama`. Platform SDK adapters are `mock`, `poki`, `yandex`, and `playgama`. `mock` is not a target: `local -> mock` and `itch -> mock` because itch has no required runtime game SDK. The old JS template path `C:\projects\ai_games\game-ai-template\src\features\core\platform` is a reference for shape only, not the source of truth for this repo. Playgama Bridge is one platform SDK adapter, not the universal wrapper.

This should sit above existing template web delivery work, not duplicate `tools/build_web.sh`, `serve_web.mjs`, or T0323's package/smoke implementation. It should consume those outputs and define target-specific acceptance.

## Done when

- [x] A `features/platform-sdk` L1 feature-pack scaffold exists with a platform SDK wrapper contract and stable game-facing methods for ready/loading, gameplay start/stop, pause/resume, interstitial, rewarded ad, banner capability, analytics event, save/load, language, and platform capabilities.
- [x] Per-target/per-SDK adapter notes exist for targets `local`, `itch`, `poki`, `yandex`, `playgama` and SDK adapters `mock`, `poki`, `yandex`, `playgama`, including SDK URL/loading policy, required callbacks/events, ad semantics, storage semantics, and unsupported/no-op behavior.
- [x] Contract requires build-time adapter selection so only the needed SDK adapter is included in a release build, plus runtime target/SDK identity and capabilities such as `externalLinksAllowed`.
- [ ] T0340 mock SDK + template debug buttons slice is completed or explicitly carved out from this broader publish/analytics task.
- [ ] A publish target manifest/adapter contract exists for `itch`, `poki`, `yandex`, and `playgama` with required files, zip layout, metadata, SDK/placeholder policy, and validation command.
- [ ] A task-local implementation plan identifies how `features/platform-sdk` is installed into templates/games, where publish adapters live, and how T0323's `package_web.sh`/web-load-smoke output is reused.
- [ ] Analytics scorecard fields are defined from existing `game_analytics`/event data: first-60s completion, session length, key drop-off, reward/upgrade interaction, ad-break opportunity, and continue/kill recommendation.
- [ ] A CLI or documented command path can produce a scorecard from a local NDJSON/export fixture without requiring a live portal account.
- [ ] Validation evidence is recorded: taskboard item validates, and any new docs/commands pass their narrow checks.

## Open questions

- Should the first runtime implementation be added directly under `features/platform-sdk`, or prototyped in a JS template and promoted once the adapter API is stable?
- Which Yandex SDK path is the default for builds: archive-hosted `/sdk.js`, custom-domain absolute SDK URL, or both as target modes?
- Do we need a separate `self_host` target now, or is `itch` plus `local` enough until the first published game?

## Log

- 2026-07-08: Created as first P1 slice under Commercial web game factory.
- 2026-07-08: Refined target set to itch, Poki, Yandex Games, Playgama, and local target using the mock SDK adapter. Playgama Bridge is treated as a separate adapter, not the universal wrapper.
- 2026-07-08: Reclassified platform SDK as an L1 feature pack under `features/platform-sdk`. Clarified target vs SDK axes: targets are local/itch/poki/yandex/playgama, SDK adapters are mock/poki/yandex/playgama.
- 2026-07-08: Added build-size/policy rule: release builds include only the selected SDK adapter, while runtime exposes target, SDK, and policy capabilities for UI decisions such as external links.
- 2026-07-08: Split concrete mock ad/debug-button runtime slice into T0340 so mock behavior is checked separately from the broad publish/analytics contract.
