# items-core

Reusable in-place L1 module for item definitions and ownership. A consuming
game compiles the sources from this directory against its generated save
fragment and owns only authoring modules, reason tags, seed logic, migrations,
and release history.

## Purpose

Provide one deterministic authoring/build/runtime path for item definitions and
one reusable ownership implementation without moving game policy into the
module.

## Architecture

There is one catalog path:

```text
items.lua.json + design/items/*.lua
  -> isolated Lua 5.4 evaluator
  -> normalized items.snapshot.v1
  -> generated items_catalog.gen.{h,c}
  -> compiled into the game
  -> save load/reconcile and gameplay
```

## Two declaration spaces, one evaluator

The manifest is `studio.lua.sandbox.v1` because the evaluator is not the item
model. `studio.items` declares items; `studio.tracks` declares progression
tracks, and both reuse `studio.levels`, `studio.field`, `studio.math`, and the
cost primitives without change. A track is not a kind of item: it has no
storage, no acquisition, and its own generator downstream. What they share is
the way a levelled thing is authored and checked.

A track declares `id` (the key of its save record, so only its charset is
checked -- renaming one forgets every player's earned levels), `kind` (which
binds its columns through the same `required_for` registry items use), `mode`,
and `levels`. Row 1 is the un-upgraded state and carries the track's zero
contribution; the levels it can reach are the rows above it. Exactly one advance
is representable per mode: `manual`/`auto` carry `cost_to_reach` (a price in
items), `threshold` carries `xp_to_reach` (the track's own accumulator). Naming
the other one fails at authoring time. A row may also `grant` items on being
reached, written with the same quantity primitive a price is; an `auto` track
that grants back at least what a level charged of the same resource is rejected,
because that shape never stops buying.

The Snapshot grows a `tracks` section beside `items`, part of the content hash.
The generated C item catalog ignores it, and ignores any field no item kind
requires — those columns belong to the neighbouring space and have their own
generator.

## Exact and fractional columns

`field.i64` declares an exact column and must declare `rounding: "exact"`.
`field.f64` declares a fractional one and must not declare a rounding policy at
all: the value is computed once on the build machine and baked with round-trip
precision, so there is no runtime rounding to have a policy about.

`studio.math` follows the column: `add`/`sub`/`mul`/`min`/`max` take two
operands of the same kind and return that kind, `idiv` is exact-only, `div` and
`pow` are fractional-only, and `tofloat` is the one place a value stops being
exact. Mixed operands are an authoring slip, not a promotion. Every fractional
result is checked finite, so an overflow to infinity or a negative base under a
fractional exponent fails at build time instead of baking a nonsense literal.

The evaluator, Snapshot, and generated C are build-local. Nothing ships in the
asset pack and there is no bind step, so the catalog is available from the
first instruction. There is no JSON catalog, field-schema JSON, runtime blob,
fallback parser, or dual-read mode.

The Snapshot retains authoring metadata and source spans for tools. The
generated catalog projects only runtime-consumed identity, kind, storage,
levels, typed fields, costs, and currency caps. New generated data needs a
concrete runtime consumer.

## Contents

```text
include/features/items/items.h       typed catalog and ownership API
src/items_api.c                      typed reads over the generated catalog
src/items_containers.c               ownership and fixed backpack/purse policy
src/items_reconcile.c                quarantine and unique-sequence reseed
scripts/items_lua_sandbox.py         isolated evaluator
scripts/items_snapshot.py            normalized model and focused queries
scripts/items_c_catalog.py           typed C catalog generator
scripts/items_cli.py                 single AI/UI/build authoring surface
scripts/items_receipt.py             release-history validation and sealing
scripts/items_xxh64.py               neutral hash primitive for catalog tools
```

`feature.json.outputs` lists the Snapshot family; `feature.json.catalog_outputs`
lists the generated C catalog that `items_api.c` implements the base API over.
See `docs/items_typed_catalog_contract.md`.

## Public surface

The typed catalog API uses strong item IDs and opaque references:

- `items_get`, `items_exists`, `items_try_get`, `items_try_get_string`
- `items_core`
- `items_acquire_transition`, generic level transition queries, `items_cost_count`, `items_cost_at`
- generated capability accessors when the game declares typed level fields
- `items_has_currency`, `items_currency_cap`

The ownership API is:

- persistent or ephemeral `items_try_container_create`, destroy-empty, and resize;
- stack add/remove/count/afford operations against an explicit container ref;
- atomic composite payment, from a baked catalog cost or a caller-built list,
  and paid/explicit-free catalog acquisition;
- atomic paid/explicit-free next-level upgrade for unique instances;
- unique entry create/destroy and whole/split/merge move operations;
- persistent ID lookup plus generation-checked runtime refs.

`stack == 0` is unlimited, `stack == 1` is unique, and `stack > 1` is a finite
per-container cap. Currency caps are also enforced by ownership. Mutations
require a game-owned `verb:subject` reason and emit typed `items.txn`, `items.payment`, or
`items.move` events.

Both payment entries plan the whole cost against a scope of up to
`ITEMS_PAYMENT_SCOPE_MAX` containers and then commit once, so a shortfall on any
requirement leaves every other one untouched. `items_try_pay_cost` takes a baked
catalog cost; `items_try_pay_stacks` takes
`def_id`/count pairs authored outside the catalog. Both paths are bounded by
`ITEMS_PAYMENT_MAX_REQUIREMENTS`, so every payment's composition fits its audit
record whole. A caller-built list carries
stack resources only, at most one requirement per item, and non-negative counts;
a count of zero is a free position that is dropped, and a list left with nothing
to pay succeeds having taken nothing while still recording the payment with zero
requirements. A valid scope is required either way, so a caller that owes
nothing still names where it would have paid from. Everything the catalog path
asserts, the list path answers with a result code.

`items_can_pay_stacks` answers the same question without taking or recording
anything, for callers that must reserve their own budget before spending:
check, allocate, then pay. Both go through one planner and only the payment has
a commit tail, so the two can disagree only if the state moved between them.

`items.payment` is the single success record for a composite payment. Stable
`cost_fingerprint`, `scope_fingerprint`, and `source_fingerprint` fields identify
the ordered item-ID/count requirements, ordered persistent container IDs,
and ordered container-ID/entry-ID/slot/item-ID/applied-count plan. Requirement
order is the order the cost declares them in, so the same price declared in a
different order is a different fingerprint; `requirement_count == 0` is what
marks a payment that owed nothing. They use
FNV-1a over the named `items.payment.*.v1` domain followed by canonical
little-endian 64-bit fields. `requirement_count`, `scope_count`, and
`source_entry_count` describe the bounded plan; `requested_units` is the
overflow-checked sum of the declared requirements and `applied_units` is
counted off the committed plan rows, so the two are equal on success by
agreement rather than by construction. A refusal emits no event. A committed
mutation always emits its record without first asking whether the frame's event
log has room; running out of room is a developer error that game-events asserts
on, not a runtime condition ownership degrades around. `items.acquire` is the sole success
event for the combined verb and records the acquired item and destination/entry
IDs plus the same payment fingerprints and totals; explicit-free acquisition
sets paid false and all payment fields to zero.
`items.upgrade` follows the same single-result rule and records the stable item
and entry identity, exact from/target levels, and payment summary. Targets must
be exactly next-level and fit the persisted entry-level bound before payment.

Games create concrete inventory, wallet, equipment, merchant, and chest
containers. Capacity is a finite slot range; built-in serializable policy is
fixed at create time. Persistent containers and entries live in generated
nested state pools, while separately bounded ephemeral pools never enter a
save. The reusable core has no global backpack, purse, or hidden payment scope.

Bounded runtime inspection uses caller-owned buffers. Container listing filters
policy, lifetime, and empty rows, then paginates persistent IDs followed by
ephemeral runtime refs. One-container entry inspection requires an explicit
`[slot_begin, slot_end)` range and can filter definition or quarantine state.
Every query supplies row, projected-byte, and scanned-context budgets below the
hard `64` row, `32 KiB`, and `2048` context-row ceilings.

`items_reconcile()` quarantines unknown definitions without deleting saved
records, restores records whose definition returns, and reseeds unique IDs from
loaded `<def_id>#<seq>` keys.

## Semantic CLI

All commands require an explicit game root and return bounded structured JSON:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> list
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> inspect --item <item-id>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> detail --item <item-id>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> source --item <item-id>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> schema
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> chart --item <item-id> --field <field-id>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> requirements
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> validate
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> build --out-dir <build-dir>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game-root> seal-receipt
```

Focused edit commands (`level-set`, `curve-set`, `override-set`, max-level
append/truncate, and bounded `batch`) require the exact source hash, preview by
default, preserve source structure, validate a temporary copy, and apply by
exclusive lock plus atomic replace. They refuse formulas or shapes they cannot
edit safely.

`validate` evaluates globally, checks requirements and the release receipt, and
can return one affected dependency neighborhood. `build` performs the same
checks before atomically replacing changed Snapshot and catalog outputs.
`seal-receipt` is the only release-history write path and is idempotent.

## Determinism and safety

Every evaluation starts a fresh memory-bounded `lupa.lua54` worker. Author code
gets deterministic Studio declarations and checked math, but no filesystem,
environment, network, clock, randomness, dynamic loading, bytecode, debug/FFI,
or mutable global state. Manifests allowlist modules. Source, instruction,
recursion, memory, time, row, and output budgets fail with structured source
diagnostics.

The Snapshot sorts identities, validates typed fields and provenance, derives
dependencies, bounds focused queries, and rejects unknown property bags. The
catalog generator rejects identity collisions, invalid currency blocks, unknown
cost references, missing capability members, and budget overruns before
emitting a line of C, so a rejected catalog cannot reach the compiler.

## Validation

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_lua_sandbox_test.py
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_snapshot_test.py
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli_test.py
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_c_catalog_test.py
cmake --build templates/template/build/native-debug --target game test_items_api test_items_fragment
ctest --test-dir templates/template/build/native-debug -R "items|progression|template_composition" --output-on-failure
node features/validate_contracts.mjs
```

See [INSTALL.md](INSTALL.md) for consuming-game wiring and
[`benchmarks/README.md`](benchmarks/README.md) for the measured pipeline cost.

## Compatibility

The feature manifest uses exact SemVer. PATCH releases preserve behavior and
wire/API contracts, MINOR releases add backward-compatible surface, and MAJOR
releases may remove or change public commands, APIs, or catalog contracts.
Consumers pin both the version and repository revision.

5.1.0 lets a track row `grant` items on being reached, written with the same
quantity primitive a price is, and accepts a dot-separated track id: a track id
is the key of its save record, and renaming one forgets earned levels. An `auto`
track that grants back at least what a level charged of the same resource is
rejected -- that shape never stops buying.

5.0.0 renames the authoring manifest schema to `studio.lua.sandbox.v1` and adds
the `studio.tracks` declaration space, the `tracks` Snapshot section, and
`field.f64` fractional columns. Every consumer edits the `schema` line of its
own `items.lua.json`; every Snapshot content hash moves, because `tracks` joined
the hashed authoring payload. The item C catalog, `items.snapshot.v1`, and the
runtime ABI are unchanged.

4.2.0 gives `items.payment` the composition it never carried: `cost[]` records
naming each charged resource, its amount, and what the whole scope held before.
`ITEMS_PAYMENT_MAX_REQUIREMENTS` now bounds a baked catalog cost too, not only an
author-built list, so every payment can be audited whole; a longer price is
rejected with `ITEMS_RESULT_INVALID_ARGUMENT` instead of silently under-reported.

4.1.0 adds `items_can_pay_stacks`, the non-committing answer to the same
question `items_try_pay_stacks` answers.

4.0.0 removes `ITEMS_RESULT_AUDIT_UNAVAILABLE` along with the audit-capacity
pre-checks behind it; mutations commit and emit.

3.1.0 adds `items_try_pay_stacks`, an atomic multi-item payment whose
requirements are authored outside the baked catalog.

3.0.0 makes the generated C catalog the production runtime data path. The typed
header a consumer includes is `items_catalog.gen.h`, catalog identity and
currency queries answer without a bind step, and the `api_proof*` commands are
replaced by `c_catalog*`.

Version `6.0.0` puts track ids under the lock ratchet: the release receipt gains
`track_ids: {active, reserved}` beside `field_ids`, and the baseline schema_version
moves to 5. A track id is the key of its save record, so dropping or renaming one
forgets every player's levels for it; that now fails `validate` until the id is
moved to `reserved` on purpose, and a reserved id cannot come back. Existing locks
must be resealed (`items_cli seal-receipt`).

Version `5.2.0` teaches `diff_snapshots` about tracks. A price edit moves
`content_hash`, and the diff -- which walked items and requirements only -- used to
report no changes for it. Diff entries for tracks carry a `track` key beside the
existing `item` and `requirement` ones. The self-paying gate for `auto` tracks also
treats an absent price as zero, so a free level granting the same resource is
refused where it used to pass.

## Extension points

Extend through game-owned Lua fields/modules, reason tags, seed logic, save
migrations, and release history. Add generated fields only for a concrete runtime
consumer. A game with fundamentally different ownership semantics should own a
game-local implementation instead of adding speculative switches here.
