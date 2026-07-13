---
id: T0364
title: Define Items Lua schema and typed runtime API seam
status: done
project: P001
epic: E016
priority: P0
tags: [items, lua, schema, c, architecture]
created: 2026-07-10
updated: 2026-07-10
---

## What

Specify the typed embedded Items Lua declaration API and prove how a reusable
stable L1 core, generated typed accessors, and compact runtime data coexist
inside one logical Items package.

## Done when

- [x] Core invariants and game extensions are separated explicitly; game Lua
      cannot redefine `def_id`, storage, or ownership semantics.
- [x] Stable `field_id`, type/unit/range, required capability, UI metadata,
      C type/rounding, source provenance, and schema evolution are specified.
- [x] Schema owns field metadata/label keys; views own only layout/order/chart
      composition by field ID. Generated LuaLS stubs come from this contract.
- [x] `kind` and tags do not repeat one fact; tags are orthogonal traits.
- [x] One generated header/include prototype compiles `features/items-core` with
      typed weapon/level/cost views without adding game fields to every game or
      requiring a generic property bag.
- [x] The header generates strong typed 64-bit constants for every item `def_id`
      using the engine hash algorithm, independently rejects both 64-bit hash
      collisions and sanitized generated-C-name collisions at build time, and
      optionally registers debug labels like the atlas generator.
- [x] The public contract remains one Items-facing catalog/API with checked
      generated accessors; a generic property bag is rejected.
- [x] Fixtures cover a currency, fixed sword, levelled sword, item ref, separate
      acquisition cost, resource-only composite cost, explicit
      free transition, `levels.single`, literal/generated/mixed
      levels, target-level cost semantics, and one override.
- [x] Unknown declarations, duplicate IDs/field IDs, invalid block cardinality,
      and unsupported C types fail with stable source diagnostics.
- [x] `def_id` preserves the current ASCII namespace regex
      `^[a-z][a-z0-9_]*\.[a-z][a-z0-9_]*$`; broader Unicode/normalization is not
      introduced without a separate versioned identity migration.

## Open questions

- Closed by the compiling proof: consumers include only
  `features/items/items.h`; an opt-in build selects one public
  `items_game.gen.h`, while `items_game.internal.gen.h` remains build-local.

## Log

- 2026-07-10: Re-scoped after single-source Items Lua review. Both independent
  architecture reviews identified the current fixed `game_item_def_t` seam as
  the first P0 blocker.
- 2026-07-10: Lead approved the reviewed Items Lua concept and started the
  T0364 vertical schema/C API proof.
- 2026-07-10: Implemented the bounded dual-schema proof: sealed core/game field
  ownership, complete field/view/evolution contract, LuaLS annotations,
  deterministic diagnostics, exact XXH64 IDs/collision gates, opaque cost
  transitions, generic typed capability codegen, and one shared `items_api.c`
  compiled against core-only and weapon catalogs.
- 2026-07-10: Verification: Python generator suite 19/19; full native-debug
  build passed; full CTest 25/25; taskboard validation passed; two independent
  final subagent gates returned SHIP.
- 2026-07-10: Quality: QTECH_001=pass; evidence: dual generated schemas compile
  with the shared Items core under the warning gate, engine-hash/runtime API
  assertions pass, and the complete Windows native-debug suite is green.
- 2026-07-10: T0364 accepted after complete QTECH proof and final SHIP reviews.
