---
id: T0386
title: Cut over Items JSON and schema to single-source modular Lua
status: doing
project: P001
epic: E016
priority: P1
tags: [items, lua, migration]
created: 2026-07-10
updated: 2026-07-16
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"semantic CLI is the only catalog read path, tests prove invalid Lua has no JSON fallback and Viewer omits catalog containers"},{"id":"QCLR_001","outcome":"pass","evidence":"browser evidence at 320 and 1440 px shows readable catalog selection, summary, statuses, capabilities, and six item cards, DOM widths equal viewport at all checked widths"}]}
---

## What

After the vertical proof, reproduce the complete current template Items catalog
and contracts in Lua, prove parity, switch every consumer once, and delete the
old JSON/schema/parser path without a compatibility fallback.

## Done when

- [ ] Inventory all `items.json`, `item_fields.schema.json`, generator, ops,
      CMake, public header, runtime, tests, Viewer, docs, lock, new-game, and
      progression-reference consumers before cutover.
- [ ] Lua fixtures reproduce all six item definitions, kinds, core blocks,
      diagnostics, lock/removal behavior, and Viewer output. Runtime containers
      remain independent E019 work and are never copied into Lua.
- [ ] New strict storage-mode enforcement and saved-level bounds/migration
      behavior have explicit fixtures; deliberate behavior changes are not
      mislabeled byte parity.
- [ ] CLI, Viewer, build codegen, tests, docs, and skills switch to Snapshot in
      one cutover; `items.json`, `item_fields.schema.json`, old JSON parser, and
      stale instructions are removed.
- [ ] `items.lock.json` remains the sole extended release receipt across the
      cutover and proves parity for shipped/removed IDs and migration reactions.
- [ ] Migration preserves the read-only Viewer first, then enables T0366/T0316
      semantic editing only after Lua source spans/write refusal are proven.
- [ ] No code reads both Lua and JSON, and rollback is defined only before the
      cutover; after cutover version control is the rollback.
- [ ] Windows/Linux full CI passes and focused timing shows no unexplained
      regression; final performance-budget ratification belongs to T0380.

## Open questions

- Migration starts after T0364/T0381/T0382/T0383/T0365 prove the authoring seam;
  E019 runtime/state work does not block catalog cutover.

## Log

- 2026-07-14: Absorbed item-specific GDD/docs/skill cleanup from T0378 and
  removed the false dependency on the independent E019 runtime rewrite.

- 2026-07-10: Replaced partial migration of selected numeric fields. The lead
  selected complete single-source Items Lua, so the correct migration is one
  explicit catalog/schema cutover and deletion.
- 2026-07-16: Started after T0383/T0365 closure. First slice inventories every JSON/schema/parser/runtime/Viewer consumer and resolves the legacy container boundary before any deletion.
- 2026-07-16: Slice 1 isolated Lua release-receipt validation from the legacy JSON op-layer. items_cli now imports items_receipt directly; items_ops delegates to the same module. RED dependency test, 14 semantic CLI tests, 29 legacy op-layer tests, benchmark contract, and verify --changed passed.
- 2026-07-16: Slice 2 added the six template definitions as modular Lua plus manifest. Parity proof preserves ids/kinds/storage/core values, deliberately renames display_name/icon_asset_id/hud_hint to name/icon/hud, removes tags that duplicated kind, excludes container rows, and represents the shipped unique sword as levels.single({}) so the v4 release receipt validates.
- 2026-07-16: Slice 3 made Snapshot reject unknown top-level item fields and validate authored metadata/capability shapes, replacing arbitrary property-bag tests with contract fields. Snapshot, semantic CLI, sandbox, and runtime-package suites pass.
- 2026-07-16: Slice 4 let the runtime package consume the complete validated Snapshot contract while projecting only current runtime facts. Template semantic build now emits a six-item blob/header; metadata remains in Snapshot but still affects the package content fingerprint, avoiding speculative wire sections.
- 2026-07-16: Slice 5 added a build-local production CMake target for the semantic Lua route. RED CMake contract test, items_catalog_gen build, six-item Snapshot inspection, and independent 568-byte package verification passed. Deferred lead questions recorded for final runtime cutover: keep name/icon/tags and inactive capabilities Snapshot-only until a concrete consumer exists; decide whether currency.cap must enter the typed runtime package temporarily or be removed by an explicit E019 behavior migration.
- 2026-07-16: Slice 6 wired the generated compact catalog into game.ntpack as the sole items/catalog blob input. RED/green CMake ownership test passed; game_asset_packs built 18 assets and binary inspection proved the 568-byte packed payload is byte-identical to the semantic build output (SHA-256 DEC43895AC9142E199ED7ECD5E1E680C49A7E6A36A46E41C961B22E2799480F5). Quality: QTECH_001=pass; evidence: focused CMake test plus pack build and entry/payload inspection.
- 2026-07-16: Slice 7 removed progression's independent items.json read: progression codegen now accepts only items.snapshot.v1 via --items-snapshot, CMake orders it after the semantic Items export, and progression-core 3.0.0 docs/seed declare the breaking contract. Six generator tests, generated target, two native progression tests, feature contracts, and verify --changed (features/template-release) passed. Quality: QTECH_001=pass; evidence: malformed-schema fixture plus real Snapshot codegen and native tests.
- 2026-07-16: Slice 8 extended the existing bounded semantic list result with explicit Snapshot card metadata needed by the read-only Viewer while still excluding level tables and acquire transitions. No new command or generic property bag was added; all 17 semantic CLI tests pass.
- 2026-07-16: Slice 9: Items Viewer now consumes only semantic CLI Snapshot summaries plus the release receipt; removed legacy items_ops/items.json/field-schema paths and catalog container projection. Verification: 19/19 viewer tests, studio verify --changed pass, browser template:template showed 6 cards and validate OK at 320/768/1024/1440 px with no horizontal overflow, API 200, zero console warnings/errors. Deferred lead questions remain recorded from Slice 8.
- 2026-07-16: Quality: QTECH_001=pass; QCLR_001=pass; evidence: QTECH_001=semantic CLI is the only catalog read path, tests prove invalid Lua has no JSON fallback and Viewer omits catalog containers; QCLR_001=browser evidence at 320 and 1440 px shows readable catalog selection, summary, statuses, capabilities, and six item cards, DOM widths equal viewport at all checked widths
- 2026-07-16: Slice 9 review follow-up: removed the Viewer-owned items.snapshot.viewer.v1 adapter and the semantic schema subprocess. The bounded UI now renders explicit Snapshot summary fields and derives kind labels from list output, reducing each catalog load to list + validate. RED/green contract tests passed; final browser HTTP probe confirmed schema=false, containers=false, 6 cards, validate OK, zero console output, and no overflow.
- 2026-07-16: Slice 10: preserved the currently enforced currency.cap behavior in the compact runtime package instead of silently dropping it during JSON cutover. Package wire format is now v2 with one explicit currency flag and int64 cap in the item row; public runtime API exposes items_is_currency/items_currency_cap, while inactive metadata remains Snapshot-only. RED/green Python and native corruption tests passed; production six-item package verifies at 616 bytes with ABI 809fdbec9b1d36e3; runtime package/resource CTests and verify --changed pass. Re-ran the current five-command CLI benchmark after its prior list-source change: 30 logical reads, 2972 stdout bytes, 331 stderr bytes, 1854.198 ms advisory wall time. Quality: QTECH_001=pass; evidence: package inspector, forged kind/flag/cap rejection, native typed API assertions, production verify, and changed-gate.
- 2026-07-16: Slice 10 review correction: the core currency-block probe is named items_has_currency, not items_is_currency, so it cannot collide with a future generated level-capability accessor for required_for=[currency]. Native package/resource tests pass after the rename.
- 2026-07-16: Slice 11: ownership/reconcile now resolve item storage, stack caps, currency flags, and currency caps exclusively through the bound compact runtime package. Fixed backpack/purse container policy stays local to ownership pending E019 instead of expanding the item blob. Converted test_items_fragment, test_progression, and test_template_composition to bind the production Lua-derived package; removed legacy generated-C catalog linkage from those consumers. Verification: focused build passed; ctest test_items_fragment/test_progression/test_template_composition 3/3 passed.
- 2026-07-16: Slice 11 verification addendum: studio.mjs verify --changed passed all changed domains (work-management 4.298s, features 49.048s, template-release 2.676s). Local diff review found no unresolved correctness issue; stale test linkage comments were corrected.
