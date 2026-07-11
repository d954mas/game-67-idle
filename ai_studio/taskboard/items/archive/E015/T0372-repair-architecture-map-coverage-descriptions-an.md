---
id: T0372
title: Repair Architecture Map coverage descriptions and validators
status: done
project: P001
epic: E015
priority: P1
tags: [architecture-map, validation]
created: 2026-07-10
updated: 2026-07-11
---

## What

Repair the current Architecture Map validators and shorten human descriptions
without coupling that content rewrite to the storage split.

## Done when

- [x] The current 13/15 baseline is green; hard-coded root-child counts are
      replaced by meaningful invariants and CLI imports have no side effects.
- [x] Coverage truth uses `git ls-files`; generated/untracked hygiene is a
      separate optional report.
- [x] Required agent/runtime ownership surfaces are represented.
- [x] Descriptions are architectural, at most 240 characters, and exclude
      commands, flags, routes, test cases, and UI micro-behavior.
- [x] A focused review proves the shortened text still lets a human locate each
      component and understand its responsibility.

## Open questions

## Log

- 2026-07-10: Owns validator/content quality only; `T0354` owns recursive
  storage and `T0371` owns graph deletion.
- 2026-07-11: Checkpoint: Architecture Map scope is clean; focused baseline is exactly 13/15 with two failures caused by hard-coded 11-child assertions while the merged tree has 12. Starting validator, ownership-surface, and bounded description repair only; T0371 graph deletion and T0354 storage split remain out of scope.
- 2026-07-11: Replaced child-count snapshots with unique required-ownership invariants and import-side-effect proof. Coverage now derives from `git ls-files`; optional hygiene uses `git ls-files --others --exclude-standard` and does not affect strict status.
- 2026-07-11: Added concise description-policy validation and represented Codex/Claude role catalogs, Runtime Automation, and coarse tracked ownership for existing Canvas Chat, Items Viewer, video, feature, game, and template surfaces without changing graph or tree-storage architecture.
- 2026-07-11: Verification: Architecture Map suite 22/22 pass; live `--strict --hygiene` reports 332 mapped / 763 scanned with 0 unmapped, missing, duplicate, missing-description, or invalid-description issues; 12 current untracked hygiene entries are non-gating and ignored `.venv` content is absent; doc-reference tests 7/7 pass; scoped `git diff --check` passes.
- 2026-07-11: Independent review cycle 1 found 0 HIGH and 4 actionable findings: tautological descriptions, ignored-tree hygiene noise, missing real-repo strict proof, and CLI help drift. All were fixed. Cycle 2 reported 0 HIGH and 0 actionable MEDIUM/LOW across architecture, correctness, ownership, tests, process, performance, and context cost.
- 2026-07-11: Quality: QTECH_001=pass; evidence: focused validator tests, real-repository strict/hygiene report, import smoke, documentation references, and independent diff review.
- 2026-07-11: Closed after tracked coverage, description policy, strict live validation, documentation checks, and two independent review cycles passed.
