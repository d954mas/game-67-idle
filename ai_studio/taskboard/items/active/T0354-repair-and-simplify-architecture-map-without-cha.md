---
id: T0354
title: Recursively split Architecture Map storage with exact visual parity
status: backlog
project: P001
epic: E015
priority: P1
tags: [architecture-map, tree, validation]
created: 2026-07-10
updated: 2026-07-10
---

## What

Recursively split the oversized `tree.json` representation by owning component
without changing the merged model or the current browser tree.

## Done when

- [ ] Tree storage is recursively split by owning components; large parts such
      as `module-assets.json` no longer act as a second monolith.
- [ ] Merged tree deep-equals the accepted pre-refactor representation and the
      browser visual tree is unchanged.
- [ ] Split/import failures are actionable and no new runtime/service abstraction
      is introduced for what remains a storage-only decomposition.

## Open questions

## Log

- 2026-07-10: Baseline command: `node --test
  ai_studio/architecture_map/tests/validate_map.test.mjs
  ai_studio/architecture_map/tests/tree_split.test.mjs` = 13 pass, 2 fail
  because real root children are 12 while tests hard-code 11.
- 2026-07-10: Validator/description repair and legacy graph deletion were split
  into `T0372` and `T0371` to keep exact parity independently provable.
