# Items Lua single-source concept — reviewed architecture

Status: accepted architecture, implemented through the E016/T0386 cutover.
This document records the design rationale; current commands and wiring live in
the module README and INSTALL manual.

## 1. Product goal

An item is authored once and remains easy for both a developer and an agent to
inspect, edit, validate, graph, compile, and use. AI Studio does the expensive
work at design/build time. The shipped game receives typed constants and compact
validated data; it contains no Lua VM, formula evaluator, or text-config parser.

The complete flow is:

```text
game-owned Items Lua
        ↓ deterministic build-time evaluator
normalized Items Snapshot
        ├── focused UI/CLI projection
        ├── generated typed C API + item ID constants
        └── compact binary asset `items/catalog`
                    ↓ game builder chooses physical pack
                runtime catalog bind

game runtime/state (separate concern)
        └── dynamic containers → ordered entries → stacks/instances
```

Lua defines item types and values. It does not enumerate living inventories,
merchants, chests, or their contents.

## 2. Canonical authoring ownership

The only current definition of an item is modular game-owned Lua:

```text
<workspace-game>/design/
  items/
    schema.lua
    kinds.lua
    currencies.lua
    materials.lua
    weapons.lua
    requirements.lua
    views.lua
  balance/
    formulas/
      item_growth.lua
```

There is no required `containers.lua`. Concrete containers are runtime
instances created by game/world/entity code. A game may later define optional
container archetype or inventory-capacity balance data in an owning sibling
domain, but those are templates for runtime creation, not live containers and
not part of the canonical Items catalog.

Items owns a complete item definition. Balance may provide pure formula helpers
without creating a second record of the item. The exact Lua file split is
game-owned; modules must remain small, deterministic, and source-navigable.

T0386 removed the old authoring sources after migration parity was proven:

```text
content/items.json
content/item_fields.schema.json
```

Generated snapshots, headers, stubs, debug C fixtures, and blobs are disposable
build artifacts, never hand-edited truth.

### Deliberately separate facts

- `state/items.schema.json` defines mutable runtime ownership/container state,
  migrations, and budgets. It does not define attack, prices, or level curves.
- The existing `items.lock.json` is retained and extended in place as the one
  release receipt. It remembers shipped/removed `def_id`, stable `field_id`,
  storage mode, and shipped level bounds without duplicating current values.
  Current Lua cannot reconstruct deletion history.
- Assets and localization stay in their owning systems.

## 3. Deterministic Lua evaluator

The source is real Lua with a typed embedded API, not JSON-shaped syntax and not
a new expression language. It runs in a fresh restricted build-time process:

- no filesystem, network, shell, environment, clock, random, FFI, debug,
  bytecode, or dynamic package loading;
- explicit module allowlist and deterministic module order;
- no unordered author-visible `pairs`/`next`, mutable globals, mutable formula
  upvalues, metatable tricks, or unrestricted snapshot traversal;
- CPU, memory, instruction, recursion, output-row, and output-byte limits;
- exact source file/line provenance for declarations, formulas, and errors;
- full isolated evaluation in v1; incremental evaluation is admitted only after
  purity instrumentation and full-rebuild parity prove it safe;
- identical normalized output across repeated Windows/Linux runs.

Cycles, duplicate IDs, unknown fields, invalid references, non-finite values,
range failures, unsafe integer conversion, and overflow fail early.
`def_id` retains the current ASCII namespace contract
`^[a-z][a-z0-9_]*\.[a-z][a-z0-9_]*$`; Unicode expansion or normalization would
require a separate versioned identity migration.

## 4. Explicit schema

The schema is declared explicitly and never inferred from currently populated
rows:

```lua
-- design/items/schema.lua
local field = require("studio.field")
local items = require("studio.items")

items.extend_schema({
  level_row = {
    attack = field.i64({
      id = "game.weapon.level.attack",
      required_for = { "weapon" },
      min = 0,
      max = 1000000,
      label_key = "item.attack",
      unit = "damage",
    }),

    cost_to_reach = field.cost_list({
      id = "items.level.cost_to_reach",
      required = false,
    }),
  },
})
```

`field_id` is compatibility identity. Schema owns identity, type, unit, range,
rounding, and label key. `views.lua` owns only layout, ordering, and chart
composition by `field_id`. A view cannot redefine field meaning.
The `items.*` field namespace is sealed for built-in core semantics. Game or
feature extensions use an owning namespace such as `game.weapon.*` and cannot
redeclare a sealed field.

`kind` is the primary presentation/capability category. Tags contain orthogonal
traits such as `melee` or `flammable`, not another copy of `weapon`.

## 5. Item declarations

```lua
-- design/items/materials.lua
local items = require("studio.items")

items.define({
  id = "game.wood",
  created = "2026-07-10",
  name = "Wood",
  icon = "icons/wood",
  kind = "material",
  stack = 999,
})
```

```lua
-- design/items/currencies.lua
local items = require("studio.items")

items.define({
  id = "game.gold",
  created = "2026-07-10",
  name = "Gold",
  icon = "icons/gold",
  kind = "currency",
  stack = 0,

  currency = {
    hud = "counter",
    cap = 0,
  },
})
```

`stack = 0` means unlimited count per stack, `stack = 1` means a unique
instance, and `stack > 1` is the maximum count of one stack entry. Runtime must
enforce the selected storage route. Currency remains an item; a particular game
decides which runtime container holds a particular actor's currency.

A fixed weapon still uses the same level-row schema as an upgradeable weapon:

```lua
-- design/items/weapons.lua
local items = require("studio.items")
local levels = require("studio.levels")

local gold = items.ref("game.gold")

items.define({
  id = "game.iron_sword",
  created = "2026-07-10",
  name = "Iron Sword",
  icon = "icons/iron_sword",
  kind = "weapon",
  tags = { "melee" },
  stack = 1,

  equip = { slot = "weapon" },
  levels = levels.single({ attack = 15 }),
  acquire = {
    cost = items.cost(gold, 100),
  },
})
```

`levels.single` normalizes to one `game.weapon.level.attack` row. There is no competing
top-level attack schema. `acquire.cost` is a data price to create/receive the
item; it does not imply persistent unlock state.

## 6. Levels, formulas, and costs

Each row describes the state at that target level. Upgrading `1 → 2` reads row
2. Level 1 has no `cost_to_reach`; reaching the initial level is not an upgrade.

```lua
local items = require("studio.items")
local levels = require("studio.levels")

local gold = items.ref("game.gold")
local metal = items.ref("game.metal")

items.define({
  id = "game.iron_sword",
  created = "2026-07-10",
  name = "Iron Sword",
  icon = "icons/iron_sword",
  kind = "weapon",
  tags = { "melee" },
  stack = 1,
  equip = { slot = "weapon" },

  levels = levels.table({
    [1] = { attack = 10 },
    [2] = {
      attack = 15,
      cost_to_reach = items.costs({
        { item = gold, count = 100 },
        { item = metal, count = 5 },
      }),
    },
    [3] = {
      attack = 20,
      cost_to_reach = items.free(),
    },
  }),
})
```

For `levels.table`, maximum level is the largest contiguous numeric key; a
second authored `max_level` would duplicate truth. Normal value columns cover
`1..max_level`; `cost_to_reach` covers `2..max_level`.

Formula mode declares its bound explicitly:

```lua
levels = levels.generate({
  max_level = 100,

  attack = function(level)
    return 10 + (level - 1) * 5
  end,

  cost_to_reach = function(target_level)
    return items.cost(gold, 100 + (target_level - 2) * 25)
  end,

  overrides = {
    [20] = { attack = 112 },
  },
})
```

Mixed mode keeps each column explicit:

```lua
levels = levels.columns({
  max_level = 5,
  attack = levels.linear({ start = 10, step = 5 }),
  cost_to_reach = levels.values({
    [2] = items.cost(gold, 100),
    [3] = items.cost(gold, 150),
    [4] = items.cost(gold, 300),
    [5] = items.cost(gold, 700),
  }),
})
```

`levels.table` means full literal rows, `levels.generate` means formula-driven
rows, `levels.columns` means mixed columns, and `levels.values` means one
literal column.

The evaluator materializes every mode into the same bounded target-level rows.
The default boundary is 1000 rows per series plus project-wide row/byte budgets
and an explicit override. Millions or infinite levels require a deliberate
runtime formula/large-number architecture.

Costs contain only resource definitions and positive counts. They never name a
living `purse`, `backpack`, player, or merchant container. Duplicate item
requirements normalize with checked addition and deterministic ordering.
Unique instances are not cost resources in v1.

## 7. Evaluation and normalized Snapshot

Evaluation is order-independent and phased:

1. Freeze built-in core and game schema extensions.
2. Register kinds, every `def_id`, and raw item declarations.
3. Resolve item references and record dependency edges.
4. Evaluate pure formulas in a fixed order and materialize bounded tables.
5. Run structural, release, asset, numeric, and game-requirement validation.
6. Freeze one normalized Items Snapshot and build fingerprint.
7. Derive focused UI/query projections and the runtime package from that same
   Snapshot.

`items.ref(id)` is an immutable evaluator handle. Missing/removed IDs fail at
the referring Lua source. The Snapshot records source spans, dependencies,
literal/formula/override provenance, schema metadata, normalized values, and
bounded diagnostics. It is a cache/build result, not another authoring source.

## 8. Numbers

- Lua authoring and analysis may use fractional `double` values.
- Exported deterministic formulas use Studio math, not host libm operations
  whose last bits can vary across platforms.
- Every runtime field declares C type, range, unit, and rounding.
- NaN, infinity, missing rounding, fractional output for integer fields, and
  overflow are errors.
- Cross-backend ordinary integer arithmetic stays within the exact IEEE-754
  range. Larger exact arithmetic needs an explicit value type.
- Runtime stack counts remain `int64_t`; they are state, not balance values.

## 9. Generated C identity and API

The build generates strong 64-bit item IDs like the engine atlas generator:

```c
typedef struct item_id {
    uint64_t value;
} item_id_t;

typedef struct item_def_ref {
    uint32_t _index;
} item_def_ref_t;

#define ITEM_GAME_GOLD \
    ((item_id_t){0xE662E696028B01C4ULL}) /* game.gold */

#define ITEM_GAME_IRON_SWORD \
    ((item_id_t){0xB36736FA950BF10DULL}) /* game.iron_sword */
```

IDs use the exact engine contract: validated UTF-8 `def_id`, XXH64, seed 0, and
no path normalization. The generator fails on either a 64-bit hash collision or
a generated C-identifier collision after sanitization. Debug builds register
string labels for diagnostics. Lua, saves,
migrations, DevAPI, and human output retain stable `def_id` strings. Generated
hashes remove handwritten C strings; runtime dense indices remain private to one
bound catalog.

Adding/removing an item changes the generated ID header and may recompile its
consumers. This is an accepted correctness trade-off. Value-only changes do not
change the ID or schema header. Header sharding is a later measured optimization.

Ordinary required APIs crash early on missing catalog data:

```c
item_def_ref_t sword = items_get(ITEM_GAME_IRON_SWORD); /* NT_ASSERT if absent */
item_weapon_level_t level = items_weapon_level(sword, 12); /* returned copy */
```

Expected absence has separate explicit surfaces:

```c
bool items_exists(item_id_t id);
bool items_try_get(item_id_t id, item_def_ref_t *out);
bool items_try_get_string(const char *def_id, item_def_ref_t *out);
```

Try/string APIs are for save loading, migrations, optional content, DevAPI, and
external data, not routine gameplay. Public value accessors return small copies,
not pointers into blob memory. Variable-length costs/tables are traversed by
opaque handles and copy-out element functions.

Every catalog function asserts that a catalog is already bound. `exists` means
only “absent from this valid bound catalog”, not “catalog unavailable”. String
lookup hashes first and then compares the original UTF-8 `def_id`, so a hostile
or external colliding string cannot resolve to another item.

## 10. Runtime package and pack placement

The production target is hybrid:

- small generated headers contain schema ABI, typed structs/accessors, and item
  ID constants;
- one compact normalized binary asset contains item/core/level/cost/string data;
- generated C literal arrays remain a tiny-fixture/reference exporter and
  benchmark candidate, not the scalable default.

The exporter emits a logical asset:

```text
items/catalog
```

The game builder decides whether that blob lives in the current game pack, a
separate Items pack, a platform-specific pack, or the tiny fallback embedding.
Items runtime never hardcodes a pack filename. A separate pack is an optional
iteration/cache optimization, not an architectural invariant.

```c
typedef enum items_catalog_bind_error {
    ITEMS_CATALOG_BIND_OK = 0,
    ITEMS_CATALOG_BIND_BAD_MAGIC,
    ITEMS_CATALOG_BIND_BAD_VERSION,
    ITEMS_CATALOG_BIND_ABI_MISMATCH,
    ITEMS_CATALOG_BIND_CONTENT_MISMATCH,
    ITEMS_CATALOG_BIND_BAD_LAYOUT,
    ITEMS_CATALOG_BIND_NO_MEMORY,
} items_catalog_bind_error_t;

bool items_catalog_try_bind(
    const uint8_t *bytes,
    uint32_t byte_count,
    items_catalog_bind_error_t *out_error);

void items_catalog_bind_required(
    const uint8_t *bytes,
    uint32_t byte_count); /* NT_ASSERT on failure */
```

The wire format is fixed-width little-endian with explicit offsets, strides,
alignment, and zeroed padding. It never serializes native structs. Before
publishing any view, bind validates magic, format version, payload size at most
`UINT32_MAX - sizeof(NtBlobAssetHeader)`, checked 64-bit
`offset + count * stride` before narrowing, string/index bounds, and:

- `schema_abi_fingerprint`: little-endian `uint64_t XXH64(seed=0)` of the
  canonical schema-ABI descriptor (stable field IDs, types, section layout,
  accessor ABI); present in generated header and blob;
- `content_fingerprint`: little-endian `uint64_t XXH64(seed=0)` over the exact
  Items payload byte range `[0, payload_size)`, excluding the outer
  `NtBlobAssetHeader`, with the one eight-byte `content_fingerprint` field
  treated as all-zero bytes. Runtime recomputes and compares it.

The pack integrity check is an outer transport check. Items copies/decodes once
into one aligned owned buffer because the resource blob view has no permanent
lifetime guarantee. There are no per-row allocations.

The runtime content fingerprint is an integrity/content-identity digest, not a
freshness oracle: an older internally valid blob can still validate. The game
builder must compare the selected `items/catalog` fingerprint with the current
Snapshot/export result before writing the chosen pack, and publication deploys
that validated pack set atomically. No second hand-authored manifest is added.

Bind is atomic: failure publishes nothing and frees all temporary memory.
Rebind is forbidden in v1; item definition refs remain valid until explicit
`items_catalog_shutdown()`. Generated ABI/ID headers are write-if-different so a
value-only edit leaves their bytes and mtimes unchanged.

Startup order is:

```text
mount the game-selected pack(s)
→ resolve logical asset `items/catalog`
→ validate/copy/bind catalog
→ load persistent container/entry state
→ reconcile saved def_id strings
→ start game
```

Pre-bind catalog access asserts; it never behaves like an empty catalog.

The blob uses flat normalized sections:

```text
header
unique string table
core item rows
per-item capability spans
all weapon level rows
all composite cost entries
other typed domain sections
```

Each logical row exists once. Items point to level spans; levels point to cost
spans. A cost entry stores only an internal item index and `int64_t count`, not
a runtime container.

## 11. Runtime containers and persistent entries

Concrete containers are game-created runtime entities. Examples include player
inventory, a wallet, merchant stock, a chest, equipment, or temporary loot.
One hundred merchants create one hundred container instances; Lua does not list
them.

Items runtime owns the container registry and invariants. Game/world/entity code
owns lifecycle intent and stores a stable reference to its container.

```c
container_ref_t stock = items_container_create_persistent(
    (item_container_desc_t){
        .capacity = 30,
        .accept_policy = ITEMS_ACCEPT_ANY,
    }
);

merchant.inventory_container_id = items_container_id(stock);
```

Persistent IDs are simple opaque `uint32_t` values unique within one save/world:

```c
typedef struct container_id { uint32_t value; } container_id_t;
typedef struct item_entry_id { uint32_t value; } item_entry_id_t;
```

Zero and `UINT32_MAX` are reserved. State persists `last_container_id` and
`last_entry_id`; allocation uses `1..UINT32_MAX - 1`, never wraps or reuses an
ID, and refuses before the representable range is exhausted. Runtime refs are
dense index+generation handles and are never serialized.

The current Game State generator cannot yet express this shape: it lacks `u32`
and nested bounded object collections. A P0 generator proof must add exact u32,
bounded `list<object>`, nested validation/serialization, and preserve the
current schema-v2 identity model: stable paths plus reserved removed names. It
must not smuggle in a new numeric-field-ID schema dialect.
Runtime storage must use separate bounded global container and entry pools so
memory is `MAX_CONTAINERS + MAX_ENTRIES`, not their Cartesian product. JSON and
DevAPI still project entries nested under their owning containers.

Every item stack or unique instance is an entry nested inside exactly one
container. Entries have globally unique numeric IDs and explicit slot numbers;
JSON object key order is never inventory order.

```json
{
  "last_container_id": 2,
  "last_entry_id": 44,
  "containers": [
    {
      "container_id": 1,
      "capacity": 20,
      "accept_policy": "any",
      "entries": [
        {
          "entry_id": 42,
          "slot": 0,
          "def_id": "game.wood",
          "count": 20
        },
        {
          "entry_id": 43,
          "slot": 1,
          "def_id": "game.wood",
          "count": 20
        },
        {
          "entry_id": 44,
          "slot": 4,
          "def_id": "game.iron_sword",
          "level": 3,
          "durability": 0.8
        }
      ]
    }
  ]
}
```

The item definition is not copied into state. `def_id` remains a string in the
save for migration, quarantine, diagnostics, and restoration; load resolves it
to the generated hash/dense item ref. Attack, icons, kinds, prices, and level
tables remain in the catalog.

### Entry and slot semantics

- `def_id` identifies the item definition.
- `entry_id` identifies one runtime stack or unique instance across all
  containers in the save.
- `slot` is the authoritative position/order inside one container.
- A unique item has one entry ID; two swords share `def_id` but have different
  entry IDs, levels, and durability.
- A stack has one entry ID regardless of count. Individual identical units do
  not receive IDs.
- Splitting a stack creates a new entry ID. Merging keeps the destination ID and
  destroys the source ID. Moving a whole entry preserves its ID.
- In v1, stacks merge only when their saved per-entry state is compatible.

Runtime derives global lookup indices such as `entry_id → container/slot` for
speed. Those caches are rebuilt after load and never become save truth.
Canonical state serialization orders containers by `container_id` and each
container's entries by `(slot, entry_id)`, independent of pool order.

Capacity is the number of addressable slots. Every entry satisfies
`0 <= slot < capacity`, and slots are unique within a container. Automatic
placement chooses the lowest free slot. Reorder/move accepts an explicit
destination slot or an explicit auto-slot mode. Shrink is legal only when
`max_occupied_slot < new_capacity`; occupancy count alone is insufficient.
There is no unlimited sentinel: `capacity = 0` means zero addressable slots.
Legacy `capacity = 0` (unlimited) must map to an explicit finite game-selected
capacity during migration; it is never reinterpreted implicitly.

### Container lifecycle

V1 provides persistent and ephemeral creation, but their lifetimes do not mix.
Persistent containers/entries serialize. Ephemeral containers and entries are
runtime-only and have no persistent IDs. Moving a persistent entry into an
ephemeral container is forbidden because autosave would lose it. Acquiring from
ephemeral loot into a persistent container creates a new persistent entry ID.
A request for a persistent ID from an ephemeral runtime ref is a developer
error and asserts.
A later run/session save document may define a different lifetime boundary.

Capacity is mutable runtime state. Increasing it succeeds. Decreasing below
occupied slots fails atomically; Items never evicts, clamps, or silently moves
entries. Invalid handles or impossible required operations assert; expected
gameplay refusal uses `exists/can/try` APIs.

Acceptance policy is immutable after creation in v1 because changing it could
make existing contents illegal. Start with built-in serializable policies such
as `any` and `currency_only`; add kind/tag/slot policies only when demanded by a
real game. No arbitrary runtime Lua predicate is introduced.

Destroying a non-empty container is forbidden. Game code must explicitly
transfer, drain, drop, or destroy its contents through an atomic domain action.
Every saved entry referencing a missing container is corruption/migration
failure, never silently orphaned.

A structurally valid entry whose saved `def_id` is missing or marked removed is
quarantined without changing `entry_id`, slot, count, or per-instance state. It
still occupies its slot/capacity, is unavailable to gameplay/payment, and is
restored when a compatible definition returns. Duplicate IDs/slots, invalid
counters/counts, or a missing owning container are structural corruption and
reject the staged load; quarantine is not a general corruption fallback.

An owner fragment stores only `container_id`; the Items registry does not
duplicate an owner pointer. Game-owned create/destroy actions update both
in-memory fragments before one save-envelope commit. Because fragments can
still load or reset independently, raw DevAPI writes to ownership references
or fragment resets that would break integrity are refused. Template default
stages all affected fragments and rejects the whole load before publication on
dangling owner references or unreferenced persistent containers. A game may
provide explicit versioned recovery with fixtures; neither side is silently
invented, cleared, reattached, or discarded.

## 12. Payment, acquisition, and upgrade

One authored price can be paid by different actors. Therefore the Lua price
contains resources only, while runtime supplies a deterministic payment scope:

```c
item_payment_scope_t payment = {
    .containers = { player.wallet, player.inventory },
    .container_count = 2,
};
```

The ordered scope defines where to look and spend; Items never performs a
hidden global search. The scope count is bounded; overflow or duplicate source
containers are developer errors and assert. Within each source container, payment consumes
compatible stack entries by ascending slot, which also defines which stack ID survives.
`items_try_pay_cost` normalizes/checks the complete plan,
preflights all source entries, and commits all removals atomically.

`items_try_acquire(destination, item, payment, reason)` validates payment,
destination capacity/policy, and item storage route before one commit.
An absent `acquire.cost` means acquisition is unavailable through this generic
verb; free acquisition must be authored explicitly as `items.free()`.
When destination is also a payment source, acquisition first creates the full
virtual payment plan, applies it to projected state, then plans placement/merge
and ID allocation against that projection. Only the finished combined plan is
committed to real state.
`items_try_upgrade_instance(entry, target_level, payment, reason)` accepts only
`target_level == current_level + 1` in v1, obtains
`cost_to_reach[target_level]`, pays,
changes level, marks dirty, and emits one final event as one commit. Failure at
any stage leaves all containers and entries unchanged.

Trading existing merchant stock is Shop/game orchestration over payment plus
atomic transfer; it is not global catalog acquisition.

## 13. Viewer and agent workflow

The normalized Snapshot, not browser code, evaluates item Lua. Viewer and CLI
request focused slices and can show identity, literals/derived fields, levels,
charts, dependencies, source, validation, and release status without loading
the whole economy into model context.

Developer UI and AI use one typed semantic operation layer:

```text
Developer UI --+
               +→ semantic Items ops → canonical Lua → validate/build
AI CLI/chat ---+
```

Safe writes cover recognized literals, explicit level cells, built-in curve
parameters, and explicit overrides. They require expected source hash, preview
source+semantic diff, validation, atomic same-file batch, inverse patch, and
conflict refusal. Arbitrary functions, helpers, aliases, conditions, loops,
shared mutable tables, identity/storage changes, and unsupported source shapes
route to source/agent editing. V1 refuses multi-file edit batches until a real
journal/recovery protocol exists.

Generated LuaLS stubs provide autocomplete from the schema. They are disposable
tooling output, not another schema source.

Runtime container/entry inspection is a separate state/DevAPI projection. It
shows container IDs, capacity, policy, slot order, entry IDs, resolved def IDs,
and quarantine/errors; it does not edit item definitions.
It is always bounded: `container list` is paginated/filterable, `container
inspect <id>` returns an explicit entry range/filter, and hard row/byte/context
budgets reject unbounded projection.

## 14. Requirements and diagnostics

Game balance requirements are pure Lua checks beside the data. They declare
warning/error severity and return bounded structured expected/actual evidence.
Structural type/reference/cycle/range/overflow failures are always errors.
Requirements receive bounded query handles/host aggregates rather than an
unrestricted million-row Lua table and never mutate source or Snapshot.

## 15. Build, CI, and performance proof

The build-cache fingerprint covers Lua sources, stable schema IDs/API version,
Lua backend/version, evaluator version, release receipt, relevant state-schema
compatibility input, and export configuration. It is distinct from runtime ABI
and content fingerprints.

Windows/Linux CI proves:

- sandbox and determinism, including mutable closures, post-define mutation,
  metatables, and differing map insertion order;
- schema/reference/cycle/source diagnostics and hash-collision rejection;
- numeric conversion, rounding, overflow, and normalized Snapshot equality;
- generated item IDs, typed API compile, and copy-out accessors;
- blob corruption/truncation/layout/fingerprint rejection;
- catalog-before-state startup and save migration/reconcile;
- persistent/ephemeral container lifecycle, globally unique numeric IDs, slot
  range/uniqueness/order, first-free placement, split/merge/move semantics,
  persistent/ephemeral crossing refusal, capacity/policy enforcement,
  and derived-index rebuild;
- atomic composite pay/acquire/upgrade failure with exact before/after equality;
- Viewer/CLI focused-query and safe-writer contracts;
- C-array versus blob generation/compiler/linker/access measurements;
- current-pack versus separate-pack rebuild/startup/cache measurements;
- cold, warm, no-op, candidate incremental, 1K/100K/1M evaluator/export tests
  for pinned PUC Lua/LuaJIT candidates.

Benchmarks report evaluator/process time, generation, compiler/linker, chosen
pack contribution, HTTP transport estimate, transient/steady memory, catalog
lookup/accessor latency, state load/index rebuild, and compact agent context
bytes. Browser rendering of one million rows is not required.

## 16. Migration and minimal vertical proof

Migration is one explicit cutover, not dual truth:

1. Freeze current JSON/catalog/state fixtures and generated C behavior.
2. Prove schema/Snapshot/runtime package for one currency and sword.
3. Generate item hash constants and assert/exists/try lookup contracts.
4. Extend the state generator for exact u32 plus bounded nested object
   collections and prove flat bounded C pools with nested JSON projection.
5. Prove one persistent runtime container with nested numeric entries/slots and
   one ephemeral container.
6. Add the versioned `owned`/fixed-container → `containers[].entries[]` state
   migration. It uses a frozen game-owned mapping from old container strings to
   numeric ID/capacity/policy, sorts old map keys for deterministic slots/IDs,
   maps legacy unlimited capacity to an explicit finite project budget, writes
   both `last_*_id` counters to the assigned maxima,
   updates persistent external owner references, and never reads mutable current
   catalog/balance data. Event payload and consumer changes are a separate
   code/API cutover because events are transient, not migrated save rows.
7. Prove payment scope plus atomic paid/free acquire and upgrade failures.
8. Reproduce the six template item definitions in Lua and validate parity.
9. Switch CLI/Viewer/build to Snapshot once.
10. Delete old Items JSON/schema/parser without compatibility fallback.
11. Run full Windows/Linux CI and performance budgets.

The minimal proof contains:

- one explicit Items schema extension with stable `field_id`, plus Game State
  schema-v2 stable paths/reserved-name evolution;
- currency, stackable material, fixed sword, and three-level sword;
- literal/generated/mixed levels, one override, composite cost, explicit free;
- generated item hash constants with collision proof and debug labels;
- logical blob asset placed by builder in the selected pack;
- required assert API plus optional exists/try and copy-return accessor;
- persistent player inventory and merchant/chest container instances;
- two wood stacks with different numeric entry IDs/slots and two swords sharing
  `def_id` but having different durability;
- split, merge, reorder, move, resize, save/load, and derived-index proof;
- deterministic runtime payment scope and atomic upgrade failure/success;
- focused Viewer/CLI projection and one safe semantic edit;
- Windows/Linux determinism and timing evidence.

## 17. Explicit non-goals

- Runtime Lua, JSON config parsing, or formula evaluation.
- Concrete container instances in item-authoring Lua or the item catalog blob.
- Global singleton `purse`, `backpack`, or `items_purse()` in reusable core.
- Hidden global search for payment resources.
- Arbitrary runtime container-policy callbacks.
- Dynamic container archetype system before a real game requires it.
- A generic runtime property bag.
- Arbitrary Lua round-trip editing.
- Automatic million-level materialization.
- Cloud/liveops or remote overrides in v1. Value-only remote overrides remain
  captured separately as post-v1 idea T0389.
- Silent fallback to old JSON after cutover.
