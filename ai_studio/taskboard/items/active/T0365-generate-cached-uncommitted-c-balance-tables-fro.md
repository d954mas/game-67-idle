---
id: T0365
title: Benchmark and build the compact typed Items runtime package
status: doing
project: P001
epic: E016
priority: P1
tags: [items, codegen, c, cache]
created: 2026-07-10
updated: 2026-07-15
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"Two independent native parser reviews ACCEPT after generated expectation, UTF-8, field-count, value-range, size-gate, dependency, and INT64_MIN fixes; Python items-core 86/86; warning-gated native test_items_runtime_package CTest 1/1; feature contracts pass; studio verify changed passed features/template-release; git diff check clean"}]}
---

## What

Compare generated C literal arrays with a compact normalized binary blob, then
turn a validated single-source Items Snapshot into the measured typed runtime
package without runtime Lua/JSON parsing.

## Done when

- [ ] Export consumes only the normalized snapshot and performs no Lua/formula
      evaluation or semantic validation of its own.
- [ ] A Windows/Linux benchmark compares C arrays with the compact blob for
      generation/link time, incremental rebuilds, package/resident size, startup
      binding, and checked access. Blob plus small generated C API remains the
      default unless measurements disprove it.
- [ ] The package follows the canonical E016 format: fixed-width little-endian
      flat core/field/level/cost sections, deduplicated strings, checked spans,
      stable item hashes, and separate schema-ABI/content fingerprints.
- [ ] Binding through the public resource-blob path validates the complete
      header, fingerprints, bounds, indices, alignment, and content digest before
      atomically publishing one owned buffer; failure publishes nothing.
- [ ] Generated typed APIs distinguish definition and runtime-entry refs, assert
      required absence/unbound use, expose bounded `exists`/`try` alternatives,
      and reject hash/name collisions.
- [ ] Value-only changes rebuild catalog data without relinking ABI-stable C;
      generated headers are write-if-different and no handwritten item IDs remain.
- [ ] `items/catalog` placement remains a game-builder choice; pre-bind access
      fails, bind precedes state load, and publishing validates the catalog
      against the current Snapshot without a second authored manifest.
- [ ] Default row/byte budgets fail early unless explicitly overridden; runtime
      contains no Lua/JSON parser, semantic evaluator, independently authored
      combat/economy table, or per-row allocation, and generated values match
      focused Snapshot inspection.

## Open questions

- Blob plus small typed C API is the production default unless the benchmark
  disproves it; C arrays remain the tiny-fixture/reference exporter.
- Exact pack placement is a game-builder decision and loading must use existing
  public resource boundaries by logical asset ID.

## Log

- 2026-07-14: Collapsed repeated wire/API clauses into seven measurable
  outcomes; detailed invariants remain canonical in E016 and the concept doc.

- 2026-07-10: Replaced the former separate Balance C-table plan after the lead
  selected one canonical Items Lua definition and one logical catalog package.
- 2026-07-10: Lead questioned generated-source duplication/size and runtime
  parsing. Re-scoped to benchmark naive C arrays against small typed generated
  APIs plus one compact flat blob; runtime binds validated spans, not configs.
- 2026-07-10: Runtime-package re-review initially suggested a dedicated pack;
  the lead later made physical placement a game-builder choice while retaining
  the logical asset, catalog-before-state gate, wire/fingerprint checks, and
  honest pack/transport/memory metrics.
- 2026-07-15: Started after T0383 implementation reached review. First slice will define and measure the minimal Snapshot-only compact package boundary; Linux benchmark evidence remains a later proof gate, not a reason to duplicate evaluator logic.
- 2026-07-15: Slice 1: added Snapshot-only items runtime package v1 builder plus strict Python reference inspector. The 504-byte vertical fixture uses fixed-width little-endian aligned sections, deduplicated strings, stable XXH64 item/schema/content fingerprints, flat item/field/level/value/cost rows, exact ownership coverage, and early row/byte budgets. Unsupported item capabilities fail instead of being silently dropped; C binding and generated ABI headers remain next.
- 2026-07-15: Quality: QTECH_001=pass; evidence: Runtime package review ACCEPT after canonical span ownership, full FIELD validation, fail-early budgets, and positive-cost fixes; package tests 5/5 including resigned corruption; all items-core 83/83 before final positive-cost guard plus targeted 5/5 after; studio verify --changed passed features domain; git diff check clean
- 2026-07-15: Slice 2: added structured build CLI, atomically write-if-different blob/header outputs, and an ABI header containing only schema/item/field identity. Value-only edits leave header bytes/mtime unchanged. Review fixes reject all input/output path overlap before writes, normalize argument/encoding failures to JSON, and publish the additive surface as items-core 1.8.0 with exact template/receipt pins.
- 2026-07-15: Quality: QTECH_001=pass; evidence: Slice 2 review ACCEPT; runtime package 8/8; items ops 29/29; all items-core 86/86; feature contracts pass; studio verify --changed passed features and template-release; git diff check clean
- 2026-07-15: Slice 3: added opt-in native package bind over the exact Python-generated fixture. Bind copies first, validates magic/version/generated item+field ABI/content digest/canonical layout/UTF-8/ranges/indices/spans/default 64 MiB gate, and publishes atomically only on success; rebind is refused, shutdown permits a later bind, and the lifecycle is explicitly main-thread serialized. Public resource acquisition remains next.
- 2026-07-15: Quality: QTECH_001=pass; evidence: Two independent native parser reviews ACCEPT after generated expectation, UTF-8, field-count, value-range, size-gate, dependency, and INT64_MIN fixes; Python items-core 86/86; warning-gated native test_items_runtime_package CTest 1/1; feature contracts pass; studio verify changed passed features/template-release; git diff check clean
- 2026-07-15: Slice 4: added the opt-in public resource-blob adapter. It maps missing/not-ready/wrong-type deterministically, checks stable asset type before readiness to avoid retrying a terminal mismatch, delegates ready bytes directly to the copying binder, and leaves pack placement/request timing game-owned. Review ACCEPT after the combined wrong-type+not-ready regression. Native runtime package/resource CTests 2/2, studio verify --changed passed features/template-release, and git diff check clean.
