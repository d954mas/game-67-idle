---
id: T0354
title: Recursively split Architecture Map storage with exact visual parity
status: done
project: P001
epic: E015
priority: P1
tags: [architecture-map, tree, validation]
created: 2026-07-10
updated: 2026-07-12
---

## What

Recursively split the oversized `tree.json` representation by owning component
without changing the merged model or the current browser tree.

## Done when

- [x] Tree storage is recursively split by owning components; large parts such
      as `module-assets.json` no longer act as a second monolith.
- [x] Merged tree deep-equals the accepted pre-refactor representation and the
      browser visual tree is unchanged.
- [x] Split/import failures are actionable and no new runtime/service abstraction
      is introduced for what remains a storage-only decomposition.

## Open questions

## Log

- 2026-07-10: Baseline command: `node --test
  ai_studio/architecture_map/tests/validate_map.test.mjs
  ai_studio/architecture_map/tests/tree_split.test.mjs` = 13 pass, 2 fail
  because real root children are 12 while tests hard-code 11.
- 2026-07-10: Validator/description repair and legacy graph deletion were split
  into `T0372` and `T0371` to keep exact parity independently provable.
- 2026-07-10: Execute after T0372 makes the baseline validators green and T0371 removes the legacy graph; then prove recursive storage split with exact tree parity.
- 2026-07-12: Checkpoint after T0352 at a289f898d. Implement recursive
  Architecture Map storage split with exact merged-model and browser parity;
  preserve existing planning WIP and unrelated dirty worktree.
- 2026-07-12: TDD and implementation: recursive referrer-relative parts now
  materialize identically in Node and browser fallback. Assets and Hot owner
  monoliths became 761/756-byte indexes over 31 bounded owner parts; the largest
  leaf is 18,787 bytes. The root index stayed unchanged.
- 2026-07-12: Exact parity evidence: merged JSON is 131,378 bytes with accepted
  SHA-256 `5d90a3c0b69df82dd7fd06d35f53caa1e028433385994b6d9d6de48ed1fecfa6`;
  property order, child order, API payload, and absence of `parts` markers are
  executable-test covered.
- 2026-07-12: Review/fix cycle 1 corrected browser absolute/cross-origin ref
  acceptance, root `index` leakage, traversal diagnostics, and storage context
  invariants. Cycle 2 converged with two independent reviewers reporting 0 HIGH
  and 0 actionable MEDIUM/LOW.
- 2026-07-12: Integrated verification: Architecture Map 28/28; strict validation
  mapped 358/scanned 859 with all issue buckets 0; docs 11/11; diff check pass.
  Real Studio Shell browser parity remained root 12/12, Assets 7/7, Canvas
  19/19, map validation 0 issues, and 0 console warnings/errors.
- 2026-07-12: Quality: QTECH_001=pass; evidence: Architecture Map 28/28; strict validation 358/859 with 0 issues; exact merged SHA-256 parity; two clean review-cycle-2 reports; live browser root 12/12, Assets 7/7, Canvas 19/19, 0 console warnings/errors
