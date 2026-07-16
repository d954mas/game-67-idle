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
  -> verified items/catalog package + items_catalog_abi.gen.h
  -> runtime bind
  -> save load/reconcile and gameplay
```

The evaluator, Snapshot, and package are build-local. The pack contains only
the compact runtime package. The game must bind it before loading or
reconciling Items state. There is no JSON catalog, field-schema JSON, generated
C table, fallback parser, or dual-read mode.

The Snapshot retains authoring metadata and source spans for tools. The package
projects only runtime-consumed identity, kind, storage, levels, typed fields,
costs, and currency caps. New wire data needs a concrete runtime consumer.

## Contents

```text
include/features/items/items.h       typed catalog and ownership API
src/items_runtime_package.c          package validation, bind, typed reads
src/items_runtime_resource.c         ready-blob resource adapter
src/items_containers.c               ownership and fixed backpack/purse policy
src/items_reconcile.c                quarantine and unique-sequence reseed
scripts/items_lua_sandbox.py         isolated evaluator
scripts/items_snapshot.py            normalized model and focused queries
scripts/items_runtime_package.py     package/header builder and verifier
scripts/items_cli.py                 single AI/UI/build authoring surface
scripts/items_receipt.py             release-history validation and sealing
```

`generate_items_api_proof.py` and `items_api.c` remain bounded proof fixtures
for schema-derived capability APIs. Production runtime data comes only from the
compact package.

## Public surface

The typed catalog API uses strong item IDs and opaque references:

- `items_get`, `items_exists`, `items_try_get`, `items_try_get_string`
- `items_core`
- `items_acquire_transition`, `items_cost_count`, `items_cost_at`
- generated capability accessors when the game declares typed level fields
- `items_has_currency`, `items_currency_cap`

The ownership API is:

- `items_add`, `items_remove`, `items_count`, `items_can_afford`
- `items_move`
- `items_instance_create`, `items_instance_destroy`
- `items_purse`

`stack == 0` is unlimited, `stack == 1` is unique, and `stack > 1` is a finite
per-container cap. Currency caps are also enforced by ownership. Mutations
require a game-owned `verb:subject` reason and emit typed `items.txn` or
`items.move` events.

The current ownership policy has two fixed containers: `backpack` accepts any
item and has 20 distinct-record slots; `purse` is unlimited and accepts only
currency items. Dynamic container definitions belong to the Containers work,
not to the item catalog wire format.

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
checks before atomically replacing changed Snapshot/package/header outputs.
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
package binder validates magic/version, ABI and content fingerprints, canonical
spans, indices, alignment, UTF-8, ranges, and size before publishing an owned
copy. Bind/read/shutdown are main-thread-only.

## Validation

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_lua_sandbox_test.py
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_snapshot_test.py
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli_test.py
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_runtime_package_test.py
cmake --build templates/template/build/native-debug --target game test_items_runtime_package test_items_runtime_resource test_items_fragment
ctest --test-dir templates/template/build/native-debug -R "items|progression|template_composition" --output-on-failure
node features/validate_contracts.mjs
```

See [INSTALL.md](INSTALL.md) for consuming-game wiring and
[`benchmarks/README.md`](benchmarks/README.md) for the measured package choice.

## Compatibility

The feature manifest uses exact SemVer. PATCH releases preserve behavior and
wire/API contracts, MINOR releases add backward-compatible surface, and MAJOR
releases may remove or change public commands, APIs, or package contracts.
Consumers pin both the version and repository revision.

## Extension points

Extend through game-owned Lua fields/modules, reason tags, seed logic, save
migrations, and release history. Add package fields only for a concrete runtime
consumer. A game with fundamentally different ownership semantics should own a
game-local implementation instead of adding speculative switches here.
