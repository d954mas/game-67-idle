---
id: T0365
title: Benchmark and build the compact typed Items runtime package
status: backlog
project: P001
epic: E016
priority: P1
tags: [items, codegen, c, cache]
created: 2026-07-10
updated: 2026-07-14
---

## What

Compare generated C literal arrays with a compact normalized binary blob, then
turn a validated single-source Items Snapshot into the measured typed runtime
package without runtime Lua/JSON parsing.

## Done when

- [ ] Export consumes only the normalized snapshot and performs no Lua/formula
      evaluation or semantic validation of its own.
- [ ] Windows/Linux benchmark compares C arrays and compact blob for generation,
      compiler/linker, incremental/no-op, raw/packed/resident bytes, startup
      binding, and checked access latency at representative and stress sizes.
- [ ] Small generated headers/types/accessors compile with the T0364 reusable
      core; value-only changes can regenerate/repack data without relinking when
      the selected format supports it.
- [ ] The compact candidate stores strings once and uses flat core/field/level/
      cost sections with item indices plus `offset/count` spans, not one array or
      repeated string per item/level.
- [ ] `cost_to_reach[target_level]` has an explicit inapplicable/free/paid kind
      and a composite stackable-resource span with item indices/counts only;
      level 1 is inapplicable, `items.free()` is explicit, and checked accessors
      expose no raw offsets or indexing.
- [ ] Export groups duplicate item requirements with checked addition, rejects
      non-positive/overflowing counts, and T0388 supplies runtime payment scope.
- [ ] Runtime output uses declared checked C types/rounding; invalid domain,
      safe-integer breach, or overflow fails before compile.
- [ ] Blob wire format is fixed-width little-endian with explicit section
      offsets/strides/alignment/padding and never serializes native C structs;
      all `offset + count * stride` checks use checked uint64/size arithmetic and
      payload fits `UINT32_MAX - sizeof(NtBlobAssetHeader)` before narrowing.
- [ ] Blob/runtime binding uses the existing public builder/resource blob path,
      validates magic/version/schema-ABI/content fingerprints/size/alignment/
      offsets/counts/indices before publishing any span, and copies/decodes once
      into one aligned owned contiguous buffer because the pack-memory view is
      not a permanent lifetime guarantee; no per-row allocations.
- [ ] Export emits logical asset `items/catalog`; game builder chooses current
      or separate pack. Proof measures both layouts and gates startup as bind ->
      save load/reconcile -> game; pre-bind catalog access fails fast.
- [ ] Lua/save identity remains stable `def_id`; blob indices are private to one
      content fingerprint. Adding/removing an item changes the generated header
      set, while the hash of every unchanged `def_id` remains stable; generation
      rejects collisions and removes handwritten C IDs.
- [ ] Required get/access APIs assert on missing catalog items/levels and return
      small copies; expected absence has separate `exists`/`try`/string-boundary
      APIs and variable data uses opaque handles with copy-out elements.
- [ ] Cache fingerprint, runtime `schema_abi_fingerprint`, and runtime
      `content_fingerprint` are distinct; value-only changes rebuild the blob/
      pack without regenerating the ABI header or relinking C.
- [ ] ABI/content fingerprints are little-endian `uint64_t XXH64(seed=0)`.
      Schema ABI hashes the canonical ABI descriptor. Content hashes exact Items
      payload bytes `[0,payload_size)`, excludes the outer blob-asset header, and
      treats its one eight-byte content-fingerprint field as zero.
- [ ] Bind compares generated-header ABI to blob ABI and recomputes the exact
      content digest above; any failure publishes no partial catalog and frees
      temporary memory.
- [ ] Exact item-ID generation matches validated UTF-8 + XXH64 seed 0 with no
      path normalization, rejects both hash and sanitized-C-name collisions,
      and generates headers write-if-different.
- [ ] Public API distinguishes `item_def_ref_t` from runtime `item_entry_ref_t`;
      all catalog access asserts unbound, required get/level assert absence,
      `exists/try` model expected absence, and string lookup verifies equality
      after hashing.
- [ ] Game bootstrap obtains `items/catalog` through request/get-blob while the
      pack view is valid, then immediately calls atomic try/required bind. V1
      forbids rebind until explicit shutdown.
- [ ] Runtime content digest is documented as integrity/content identity, not
      freshness. Before pack write, builder compares the selected catalog digest
      with the current Snapshot/export; publishing the validated pack set is
      atomic and introduces no second hand-authored manifest.
- [ ] Default 1000-row plus total source/blob/packed/resident budgets fail early
      unless explicitly overridden; large internal artifacts may shard for build
      speed while runtime sees one logical package.
- [ ] Runtime contains neither Lua/JSON parser nor an independently authored
      combat/economy table; generated values match focused snapshot inspection.

## Open questions

- Blob plus small typed C API is the production default unless the benchmark
  disproves it; C arrays remain the tiny-fixture/reference exporter.
- Exact pack placement is a game-builder decision and loading must use existing
  public resource boundaries by logical asset ID.

## Log

- 2026-07-10: Replaced the former separate Balance C-table plan after the lead
  selected one canonical Items Lua definition and one logical catalog package.
- 2026-07-10: Lead questioned generated-source duplication/size and runtime
  parsing. Re-scoped to benchmark naive C arrays against small typed generated
  APIs plus one compact flat blob; runtime binds validated spans, not configs.
- 2026-07-10: Runtime-package re-review initially suggested a dedicated pack;
  the lead later made physical placement a game-builder choice while retaining
  the logical asset, catalog-before-state gate, wire/fingerprint checks, and
  honest pack/transport/memory metrics.
