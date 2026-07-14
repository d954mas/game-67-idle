---
id: T0413
title: Reduce Architecture Map to module ownership and stable boundaries
status: done
project: P001
epic: E018
priority: P0
tags: [architecture, map, simplification]
created: 2026-07-13
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=One-file map 28/28; strict mapped 49 of 890 scanned with all issue counters zero; architecture domain pass; two independent security re-reviews PASS after fail-closed fixes."}]}
---

## What

Turn Architecture Map into a short authored module index. Record owners,
public surfaces, canonical contracts/stores, stable external boundaries, and
the verification domain; let a module directory cover its implementation and
tests instead of repeating the filesystem.

Each authored node must answer only: what is the module, who owns it, what is
its public entry, where is its canonical contract/store, and which stable
boundary/verification domain applies.

## Done when

- [x] Authored test, fixture, implementation-file, generated, history,
      research, and migration-guard nodes are removed.
- [x] New tests under an owned subtree require no map edit while new top-level
      modules/features/games still fail strict ownership coverage.
- [x] Existing subtree/directory coverage is reused; no map generator, test
      scanner, or second ownership registry is introduced.
- [x] Redundant roles, tags, descriptions, and exact child-count assertions are
      removed; loader confinement, malformed-input, and API error tests remain.
- [x] The 38 fragments are collapsed to one small file or at most twelve
      owner-domain fragments, chosen after measuring the reduced data.
- [x] Before/after fragment count, authored node count, serialized bytes, and
      rendered context bytes are recorded; all shrink materially, but no
      permanent line-count or exact-node-count guard is added.
- [x] Strict validation, renderer loading, and owner routing remain green.
- [x] Validation code and tests remain in the repository but are not authored
      as visible map nodes.

## Open questions

- Single file versus owner fragments is a measured end-state choice, not a
  blocking architecture decision.

## Log

- 2026-07-13: Accepted direction: tests do not belong in the authored map.
- 2026-07-13: Single-file owner/boundary map implementation in progress: 38 fragment files removed and focused validator/renderer regressions running.
- 2026-07-14: Chose one file after reduction. Storage fell from 39 JSON files,
  4,951 lines, 167,364 authored bytes, 373 nodes, and 44 authored test nodes to
  1 JSON file, 589 lines, 22,878 authored bytes, 50 nodes, and 0 test nodes.
  The serialized API model fell from 122,046 to 15,417 bytes; the rendered
  context proxy fell from 183,523 to 21,613 bytes.
- 2026-07-14: Added `subtree`, `direct-files`, and `self` coverage semantics.
  Known module internals are covered without leaves; new Studio modules,
  templates, feature packs, games, and extensions remain fail-closed.
- 2026-07-14: Focused Architecture Map tests, strict validation, single-file
  API loading, renderer syntax, private-mount exclusion, and loader confinement
  are green. Status remains `doing` until the two independent reviews finish.
- 2026-07-14: Independent reviews found three fail-open gaps: literal `ref`
  keys survived the single-file loader, authored `entry`/`contract`/`store`
  locators were not checked for existence, and private-mount discovery errors
  fell back to an unexcluded scan. Added RED regressions, then made all three
  paths fail closed with contained API errors. The focused suite is now 28/28,
  strict validation reports zero missing locators, and the architecture domain
  remains green. Status stays `doing` for the required repeated reviews.
- 2026-07-14: Final close: 39 to 1 JSON, 373 to 50 nodes, 44 to 0 authored test nodes, authored bytes 167364 to 22878. Strict validation and architecture domain pass; both independent security re-reviews PASS after literal-ref, missing-locator, and private-mount containment fixes.
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=One-file map 28/28; strict mapped 49 of 890 scanned with all issue counters zero; architecture domain pass; two independent security re-reviews PASS after fail-closed fixes.
