---
id: E016
title: Balance Workbench and single-source Items Lua
status: active
project: P001
priority: P2
tags: [balance, items, lua, game-design, tooling]
created: 2026-07-10
updated: 2026-07-10
---

## Goal

Create a local, agent-first design/build module that lets a game author item
definitions, formulas, bounded level tables, scenarios, requirements, and graph
views in modular Lua, then produces deterministic focused views and a compact
typed runtime package.

The complete reviewed Items concept with code examples is:
`features/items-core/docs/items_lua_single_source_concept_2026-07-10.md`.

## In scope

- Modular game-owned Items/Balance Lua authoring and typed schema declarations.
- Deterministic isolated evaluator, normalized snapshot, focused agent queries,
  Viewer/grid/charts/diff/what-if plus restricted semantic editing, and a typed
  compact runtime package.
- Items JSON/schema parity migration, release/save compatibility, Windows/Linux
  verification, and performance/context benchmarks.
- Generated item hash constants plus runtime persistent/ephemeral containers,
  ordered nested entries, numeric IDs, and atomic payment scopes.

## Ratified product model

- Game-owned Items Lua is the only canonical current item definition source.
  `content/items.json` and `content/item_fields.schema.json` are removed after
  parity and explicit cutover.
- Items and Balance are sibling design domains. Items may import pure shared
  formulas from Balance, but no second Balance attachment repeats item fields.
- Lua is design/build-time only. Runtime contains a generated typed API and
  validated compact blob data, with no Lua VM, bytecode, or formula interpreter.
- One evaluator produces a normalized Items/Balance Snapshot. Viewer, focused
  CLI queries, requirements, and runtime exporters consume it without
  recalculating math.
- The existing Items Viewer remains read-only for the first migration slice.
  The target product edits recognized literals, explicit table cells, built-in
  curve parameters, and overrides through the same semantic operations used by
  AI. Arbitrary Lua functions remain source/agent edited.
- Generated snapshots, projections, indices, and C files are cache/build
  artifacts, never hand-edited authoring sources.
- Concrete inventories, wallets, merchants, chests, slots, stacks, and unique
  instances are runtime/state entities created by the game, never Lua catalog
  rows.

## Deliberate non-duplication boundaries

- `state/items.schema.json` remains separate because it stores dynamic
  containers, ordered nested entries, counts, per-instance level/durability,
  quarantine, persistent ID counters, and migrations.
- The existing `items.lock.json` remains separate and is extended in place as
  the one Items release receipt because current Lua cannot recover the history
  of shipped/removed `def_id`, schema `field_id`, storage mode, or shipped level
  bounds after deletion/change.
- Assets and localization stay in their owning systems.

These are different facts, not duplicate item definitions.

## Lua and schema contract

- Real Lua runs inside a restricted, fresh process with an embedded typed API.
- The built-in Items API fixes identity and ownership invariants. Game Lua
  explicitly declares typed schema extensions with stable `field_id`, units,
  ranges, runtime type/rounding, UI metadata, and typed capability blocks.
- Schema is explicit and never inferred from current rows.
- Modules are small and deterministic. Author code has no filesystem, network,
  shell, environment, time, random, FFI, debug, dynamic package loading,
  bytecode, unordered iteration, mutable globals, or arbitrary snapshot walk.
- V1 performs a full isolated evaluation. Incremental evaluation is not claimed
  until purity instrumentation and full-rebuild parity prove it safe.
- Formula inputs are immutable typed handles. `items.ref(id)` resolves in the
  evaluator and missing/removed references fail at source. Runtime gameplay uses
  generated strong 64-bit item hashes and private dense indices; save/DevAPI
  retain stable strings.
- Requirements are pure Lua functions with static severity and structured
  evidence. V1 runs all requirements; manually duplicated dependency lists do
  not control validation.

## Level and numeric contract

- Levels support explicit tables, generated formulas/curves, and mixed columns.
  `levels.table` derives max level from contiguous keys; generated/mixed modes
  declare `max_level` explicitly. `levels.columns` mixes columns and
  `levels.values` supplies one literal column; overrides remain in the same Lua
  source.
- Level tables are v1-only for unique instances (`stack == 1`). The save owns
  the current instance level; the catalog owns values at valid levels.
- Runtime uses checked accessors. An out-of-range saved level is never silently
  clamped. Reducing shipped `level_count` or changing stack/instance storage
  requires a release receipt reaction and save migration.
- Each row describes that level. `cost_to_reach[target_level]` belongs to the
  target row, so upgrading `1 -> 2` reads row 2. Level 1 omits the field;
  omission at level 2+ is an error unless the transition is explicit
  `items.free()`.
- Item acquisition cost is a separate `acquire.cost`, not an overloaded level-1
  value or an implied persistent unlock. A cost is a typed list of stackable
  resource definitions/counts: `items.cost` is single-resource, `items.costs`
  is composite, and `items.free` is explicit. Duplicate items normalize with
  checked sums. Runtime supplies an explicit ordered actor payment scope; T0388
  owns atomic payment/acquisition/upgrade with no hidden global search.
- Default materialization is 1000 rows per series plus project row/byte budgets
  and an explicit override. Millions/infinite levels require a deliberate game
  runtime formula or large-number architecture.
- Fractional authoring uses `double`. Exported values declare C type, rounding,
  range, and unit. NaN/infinity/ambiguous rounding/overflow fail early.
- Ordinary cross-backend integer arithmetic stays within the exact IEEE-754
  range. Larger exact arithmetic is a separate explicit value-type decision.
- Exported formulas use deterministic Studio math, not host libm behavior whose
  last bits may differ across OS/runtime versions.

## One logical runtime package

The existing universal `game_item_def_t` cannot safely acquire every game's
`attack`, cost, and level columns, while naive C literal arrays can produce huge
source/relink costs. The reviewed target boundary is:

```text
small generated schema C headers/types/accessors/item hashes
        +
one compact normalized logical blob asset `items/catalog`
        =
one Items runtime package with ABI/content fingerprints and API
```

The blob stores unique strings once and uses flat typed sections plus
`offset/count` spans: core rows, per-item fields, all level rows, and all cost
entries. Cost entries reference fingerprint-scoped item indices/counts, not
runtime containers or repeated strings; save identity remains stable `def_id`.
The wire format
is fixed-width little-endian with explicit layout, never native-struct bytes.
Runtime validates header/version/schema-ABI/content fingerprints/offsets/bounds
before publishing spans, then copies/decodes once into one aligned owned buffer
through the existing resource blob API. Game builder chooses the current or a
separate pack; Items never hardcodes a pack filename. Catalog bind gates state
load/reconcile;
pre-bind access fails fast. Runtime does not parse config semantics or allocate
per row. A generic
property bag and independent combat/economy sources remain
rejected. Tiny C-array embedding is a fallback/reference exporter only.

The first proof benchmarks C arrays versus compact blob on Windows/Linux:
generation, compiler/linker, current-versus-separate-pack rebuild, startup binding,
raw/pack/transport/transient/steady bytes, lookup strategy, and access latency.
It must settle generated-header/include, startup-order, and pack-loading seams
with `features/items-core` before final format ratification.

## Runtime container and entry contract

- The game creates persistent/ephemeral containers for player inventories,
  wallets, merchants, chests, equipment, or temporary loot. Reusable core has no
  global `purse`, `backpack`, or concrete container catalog.
- Persistent container and entry IDs are opaque non-zero `uint32_t`, unique and
  monotonic within one save. Zero and `UINT32_MAX` are reserved; state stores
  separate `last_container_id`/`last_entry_id`, and allocation refuses before
  wrap. Runtime refs are dense index+generation handles and are never serialized.
- State nests `entries[]` inside `containers[]`. Every entry has globally unique
  `entry_id` and explicit `slot`; entries do not repeat `container_id`, and JSON
  key order is never inventory order.
- T0391 first extends schema-v2 for exact u32 and bounded nested objects while
  preserving stable paths/reserved names and keeping runtime memory in separate
  global container/entry pools.
- One stack has one entry ID regardless of count. Split creates a new ID; merge
  keeps destination ID; moving a whole stack or unique instance preserves ID.
  Two unique swords share `def_id` but have distinct IDs/state.
- Runtime derives global lookup caches after load. Save truth remains the nested
  container aggregate.
- Capacity is mutable. Shrink below occupied slots refuses atomically. V1
  capacity means addressable slots, so every slot is `< capacity`, automatic
  placement is first-free, and shrink requires the highest occupied slot below
  the new bound. V1 policy is built-in/serializable and immutable after
  creation; non-empty destroy requires explicit transfer/drain.
- Persistent entries cannot move into ephemeral containers; acquisition from an
  ephemeral source creates a new persistent ID. Ephemeral objects have no
  persistent ID. Owner fragments store only `container_id`; domain operations
  update both fragments before one envelope save, and post-load reconciliation
  applies explicit game policy after independent fragment load/reset.

## Agent and Viewer contract

Focused operations return one bounded slice, not the whole game economy:

```text
items schema [path]
items list [--fields ...] [--kind ...]
items inspect <def_id> [--level N] [--deps]
items validate [<def_id>|--affected <source>]
items build
items source <def_id> [field]
```

V1 `--affected` may still perform a full evaluator run while returning focused
diagnostics. Stable compact JSON is the CLI/browser parity contract. The Viewer
shows computed rows, recorded handles/dependencies, source span/snippet, and
charts; it does not pretend to recover a symbolic expression from arbitrary Lua.

Arbitrary Lua write-back is rejected. The target shared UI/CLI operation layer
edits only proven-safe literals, table cells, curve parameters, and overrides
with expected hash, review diff, inverse patch, and conflict refusal. V1 batches
are atomic within one Lua file; multi-file write requires a later journal.
Ephemeral what-if values never become build inputs unless converted into a
reviewed Lua patch.

## Risk-first plan

1. `T0369`: close the single-source decision and remaining state/history
   boundaries; amend T0316 and old design wording.
2. `T0364`: specify the explicit Items Lua schema and prototype stable core plus
   generated typed capability blocks/include seam.
3. `T0363`: benchmark pinned PUC Lua/LuaJIT candidates with the representative
   declaration/normalization workload; choose only provisionally.
4. `T0382`: build the deterministic isolated sandbox/module loader for the
   ratified declaration fixture.
5. `T0381`: build phased item registration, typed refs, stable schema/release
   receipt checks, and storage/level compatibility validation.
6. `T0383`: normalize one logical snapshot with source provenance and bounded
   focused queries.
7. `T0365`: compare C arrays with the compact typed blob, then generate the
   selected Items runtime package with flat spans, checked accessors, budgets,
   wire format, logical pack asset, generated hashes, startup gate, and separate
   fingerprints.
8. `T0391`: extend Game State codegen for exact u32 and bounded nested container
   aggregates with flat bounded runtime pools.
9. `T0390`: build dynamic runtime containers, ordered nested numeric entries,
   derived indices, lifetime rules, and cross-fragment reconciliation.
10. `T0392`: migrate fixed containers/flat `owned` and persistent references,
   then cut transient event producers/consumers to numeric entry identities.
11. `T0388`: add explicit payment-scope atomic payment, acquisition, and unique-instance
   upgrade verbs before game code can consume composite costs.
12. `T0366`: provide compact agent-first inspect/validate/build/source operations
   and update the Items/Balance skills without duplicating contracts.
13. `T0384`: add deterministic math, checked numeric conversion, requirements,
   process limits, and Windows/Linux proof.
14. `T0367`: retain the read-only migration slice, then specify shared semantic
   literal/table/curve/override edits for developer UI and AI alongside grid,
   charts, source navigation, diff, and ephemeral what-if.
15. `T0386`: reproduce all six template items, kinds, lock behavior,
   and C output in Lua; switch CLI/Viewer/build once, then delete JSON/schema
   and the old parser without compatibility fallback.
16. `T0379`/`T0380`: run hands-on competitor workflow comparison and full cold,
    warm, no-op, 1K/100K/1M production/agent benchmarks before budgets/backend
    become final.

`T0385` is intentionally closed: the former Items JSON + Balance Lua
cross-owner transaction disappears under one canonical Lua source.

## Minimal vertical proof

- one Items schema extension with stable `field_id` plus Game State schema-v2
  stable paths/reserved-name evolution;
- one currency, one fixed sword, and one levelled sword;
- one typed item reference and one cost list;
- one deterministic three-level integer table with one explicit override;
- one warning requirement;
- one focused normalized projection;
- one generated typed runtime package with compact flat sections, checked
  level/cost accessors, separate ABI/content fingerprints, and
  C-array-versus-blob benchmark;
- generated strong item hashes, collision rejection, required assert API, and
  explicit exists/try/string boundary;
- one persistent inventory plus merchant/chest container, two ordered wood
  stacks and two unique swords with distinct durability/entry IDs;
- split/merge/move/reorder/resize/save/load plus a separately frozen old-state
  migration and event-consumer cutover proof;
- exact-u32/nested-state generator proof with bounded flat runtime pools;
- one two-source payment-scope atomic upgrade failure/success fixture;
- one saved-level bounds/migration fixture;
- read-only Viewer source/grid/chart proof plus one shared literal and one table
  cell semantic edit with expected hash/diff/undo and formula refusal;
- Windows/Linux repeat and timing evidence.

Exponential/libm formulas, UI write-back, runtime Lua, cloud/liveops, arbitrary
domains, literal million-row browser grids, Monte Carlo, and automatic optimizers
are excluded from the proof.

## Competitive position

- Unity/Godot/Balancy are stronger for direct human Inspector/CMS editing; the
  target product therefore includes restricted semantic literal/table/curve/
  override editing, while still refusing an unsafe general Lua writer.
- Unreal validates typed rows/handles and has mature table/curve tooling; adopt
  typed generated access, multi-curve grids, and explicit interpolation.
- Defold/Roblox show that modular Lua code-as-data is practical but also expose
  runtime require/cache/cycle/state risks; keep our evaluator build-only,
  isolated, and fail-early.
- Machinations is a reference for scenarios and charts, not a canonical Items
  store. Keep graph truth and cloud CMS/liveops out of v1.
- Our differentiator is local/private Git ownership, deterministic checked C
  export, source provenance, cross-platform fingerprinting, and compact agent
  tools.

## Out of scope

- Cloud/liveops balance service or remote canonical CMS.
- Lua in runtime builds.
- Runtime Lua/JSON/text-config parsing or semantic validation.
- Concrete container instances or source containers in item-authoring Lua/blob.
- Global core `purse`, `backpack`, `items_purse()`, or hidden payment search.
- Arbitrary runtime container policy callbacks.
- A graph/editor cache as a second source of truth.
- Arbitrary Lua round-trip editing in the browser.
- A generic runtime property bag.
- Silent fallback to old JSON after cutover.
- Treating the provisional Lua backend choice as final before full measurements.
- Value-only remote overrides in v1; T0389 keeps the post-v1 idea.

## Log

- 2026-07-10: Initial E016 proposed Items JSON plus Balance Lua attachments and
  separate domain C tables after three reviews.
- 2026-07-10: Lead rejected two-place Items ownership and selected modular Items
  Lua as the sole current definition source, with read-only Viewer and generated
  runtime constants.
- 2026-07-10: Independent architecture and adversarial reviews returned
  ACCEPT-WITH-FIXES. Integrated blockers: stable core plus generated typed
  blocks, strict stack/instance semantics, saved-level compatibility, cost-list
  rather than one price, deterministic isolated full evaluation, no manual
  requirement dependency truth, and honest source-span rather than symbolic
formula recovery.

The shared writer distinguishes literal `level-set`, explicit `override-set`,
and built-in `curve-set`. Atomic batches use expected source hash and return
source+semantic diff plus inverse patch; unsupported source refuses. Structural
row edits require a source-preserving CST/proven canonical writer, not line spans
alone. Generated LuaLS stubs improve IDE autocomplete without becoming schema
truth.
- 2026-07-10: Official-source competitor review covered Unity, Unreal, Godot,
  Defold, Roblox, Balancy, and Machinations. Integrated stable schema IDs,
  level overrides, semantic snapshot diff, and ephemeral what-if preview.
- 2026-07-10: Lead confirmed the concept should serve both developers and AI.
  Added shared semantic editing for safe Lua shapes, explicit/mixed/generated
  level modes, target-level `cost_to_reach`, separate acquisition cost, composite
  resource costs, explicit free transitions, and no magic numeric sentinel.
- 2026-07-10: Replaced naive per-item/per-level C literal arrays with a runtime
  format decision: benchmark small generated C APIs plus one compact flat blob
  against C arrays; runtime binds validated spans and never parses Lua/JSON.
- 2026-07-10: Second runtime/red-team review selected the blob plus small C API
  as production default and added explicit cost sources, T0388 atomic verbs,
  dedicated Items pack, catalog-before-save startup, fixed wire encoding,
  separate ABI/content fingerprints, and same-file-only v1 edit batches.
- 2026-07-10: Lead corrected the container boundary and C ergonomics. Concrete
  containers moved from Lua/catalog to dynamic runtime/state aggregates; costs
  became resource-only with runtime payment scope; state nests globally unique
  numeric entries/slots; generated item hashes and assert-first APIs mirror the
  atlas pattern; pack placement became a game-builder choice.
