# items-core

In-place L1 module (see `features/README.md` — decisive rule: same-`.c`-across-games
+ data-only customization = module, not a copy-then-own feature). Precedent:
`features/game-state` — one copy of the source lives here, each consuming
game/template compiles it in-place against ITS OWN generated headers and content
(`../../features/items-core/` from any `templates/<x>` or `games/<id>`, depth-2
invariant). Extracted in T0337 out of `templates/template/src/features/items/`.

## What it is

One catalog (item/container/currency definitions, compiled from a game's
`content/items.json`) + one ownership model (who has what, in which container) +
an op-layer CLI. No UI, no DevAPI commands of its own, no gameplay
logic — items NEVER executes effects, it only tracks and reports ownership.

Successful mutations emit typed events: `items.txn` for add/remove/create/destroy
and `items.move` for container transfers.

## Contents

```text
features/items-core/
  include/features/items/items.h   public API, spelling preserved (see "Include spelling" below)
  src/
    items_api.c                    T0364 typed catalog API proof core (opt-in until cutover)
    items_catalog.c                catalog lookup over the codegen tables
    items_containers.c             ownership (add/remove/move/count/purse/instances/seq)
    items_reconcile.c              post-load quarantine + unique-seq reseed
  scripts/
    generate_items_catalog.py      content codegen (content/items.json -> const C tables)
    generate_items_api_proof.py    normalized proof Snapshot -> typed C API/reference data/LuaLS
    generate_items_api_proof_test.py  schema/API proof regression suite
    items_ops.py                   op-layer CLI (read operations + receipt upgrade)
    items_ops_test.py              self-contained unittest for items_ops.py's rules
    items_lua_sandbox.py           isolated deterministic Lua declaration evaluator
    items_lua_sandbox_test.py      sandbox, limits, diagnostics, and fixture proof
    items_snapshot.py              evaluation JSON -> deterministic Snapshot + focused query
    items_snapshot_test.py         Snapshot hash, dependency, bounds, and CLI proof
  feature.json
  README.md   (this file)
  INSTALL.md
```

## Public API (`items.h`)

**Catalog (const tables, always compiled):**
- `item_core(def_id)` / `item_at(index)` / `items_def_count()` — lookups.
- `items_with_tag(tag, out, out_cap)` — tag filter.
- `item_is_currency(def)` — has a `currency` block.
- `item_container_def(container_id)` — container definition lookup.

**Ownership (over the game's generated `items` save fragment):**
- `items_add(container_id, def_id, count, reason)` /
  `items_remove(container_id, def_id, count, reason)` — one verb for stacks AND
  currencies (gold is just an item with a `currency` block).
- `items_count(container_id, def_id)` / `items_can_afford(container_id, def_id, n)`.
- `items_move(from, to, entry_key, count, reason)` — the ONLY place ownership
  changes container.
- `items_instance_create(container_id, def_id, reason)` /
  `items_instance_destroy(instance_id, reason)` — unique-instance pool.
- `items_purse(def_id)` — convenience for `items_count("purse", def_id)`.

Stack operations reject `stack == 1`; instance operations reject `stack != 1`.
A finite authored `stack > 1` is enforced as the per-container stack cap.

`reason` is mandatory on every mutation, format `verb:subject`, verb from a
closed, append-only list — see "Reason verbs" below.

`items_reconcile()` runs after every load: a record whose `def_id` is no
longer in the catalog is quarantined (never deleted), and a formerly-removed
def_id that reappears is un-quarantined. It also reseeds the unique-instance
sequence counter above the highest `<def_id>#<seq>` key already present in a
loaded save, so a freshly created unique after a process restart can never
collide with one already on disk.

## Stack authoring contract

The game-owned `content/items.json` uses one required integer `stack` value:

- `0`: unlimited stack;
- `1`: unique/non-stackable instance;
- `N > 1`: capped stack of `N` items.

The generator validates `stack >= 0` and derives the compiled
`stackable`/`max_stack`/`unlimited` fields without changing their C ABI. An
item with an `equip` block must use `stack == 1`. `items_ops.py list --json`
returns the authored integer, never a derived object.

## Tools (`scripts/`)

- `generate_items_catalog.py --catalog <items.json> --schema <field_schema.json> --out-dir <dir>` —
  emits `items_catalog.gen.{h,c}` (compile-time const tables) plus a
  lightweight sanity net.
- `items_ops.py list|validate|schema [--json]` — read-only catalog operations.
- `items_ops.py upgrade-receipt ...` — one-shot, write-if-different v2/v3 lock
  to v4 release-receipt upgrade. It seeds shipped storage/level bounds from the
  frozen JSON catalog and retains `removed` history in the same lock file.
  V4 separates active and reserved stable field IDs so a later Lua schema
  rename/removal cannot erase identity history.
  Older removed definitions whose metadata cannot be recovered are marked
  `storage: unknown`, `level_count: null` rather than guessed.
  Later runs are no-op.
- `items_ops.py validate-evaluation-receipt ...` — read-only compatibility
  check for canonical evaluator JSON against shipped field/item history.
- `items_ops.py seal-evaluation-receipt ...` — atomic write-if-different release
  seal. It adds new active field/item IDs and raises shipped level bounds, but
  refuses unreacted removal/rename, storage changes, level shrink, or state
  schema regression.

Every path is passed explicitly by the caller. `validate` is a strict superset
  of the generator's sanity net (imports it,
  never re-parses) plus the destructive-change (lock-file) guard, full
  shipped storage/level compatibility checks,
  `<namespace>.<slug>` charset check, composite-key length rule, and an
  advisory display-name-keying lint. **Every path argument (`--catalog`,
  `--schema`, `--baseline`, `--state-schema`, `--src-dir`) must be passed
  explicitly by the caller** — the script's own argparse defaults are
  script-relative (`Path(__file__).parent.parent`), which resolves inside
  THIS module, not the calling game; a module CLI takes paths only from the
  caller (CWD/args), never from `__file__` (see INSTALL.md).
- `items_ops_test.py` — unittest against temp fixtures, proves the lock-workflow
  rules themselves fire correctly (does not touch a game's real `content/*`,
  except the reused `content/item_fields.schema.json`, treated as the generic
  stable field-shape contract).

### Deterministic Lua evaluator

`items_lua_sandbox.py evaluate` is the build-time boundary for Lua Items and
Balance declarations. Every call starts a new Python worker containing a
memory-bounded `lupa==2.8` `lupa.lua54` runtime. Author code receives only
deterministic primitives, `require`, `studio.items`, `studio.levels`,
`studio.field`, and the checked integer `studio.math` surface. Filesystem,
process/environment, clock,
random, network, dynamic loading, bytecode, debug/FFI/JIT, unordered iteration,
mutable globals, and unapproved/mutable formula upvalues are absent.

The manifest allowlists module names and files; entries are sorted before
evaluation, module exports are read-only, and `items.define` deep-copies its
input. `items.extend_schema` deep-copies authentic `field.i64` handles; finalize
registers and sorts field IDs, members, kinds, and every `def_id` before it
resolves immutable `items.ref` handles. Schema and item module order therefore
does not affect output; sealed/duplicate fields, missing refs, and duplicate IDs
fail at their Lua source. Source-byte, instruction, recursion,
memory, wall-time, output-row, and output-byte budgets fail as structured
`items.lua.error.v1` diagnostics.
Successful output is canonical `items.lua.evaluation.v1` JSON with registered
`fields`/`kinds`, the backend fingerprint, and the honest Lua file/line of each
field registration and `items.define` call. Typed field-value validation remains
the Snapshot boundary. Each source is an exact bounded call-line span with its
UTF-8 snippet; the evaluator does not pretend to reconstruct a multiline Lua
expression or a symbolic formula.
Level tables require contiguous keys, levelled items use unique storage, and
level 2+ rows require a paid or explicit-free transition. `levels.generate`
uses an explicit `max_level` and named formula columns; formulas may capture
only immutable Studio APIs/item refs, cannot change those captures, and run
after schema/definition registration has closed. Raw Lua arithmetic/bitwise
operators are rejected in favor of `studio.math`, and both formula and override
integers must remain in its exact range. `levels.columns` uses the same explicit
bound with authentic `levels.linear`/`levels.values` handles and deterministic
overrides. Evaluation records the authored mode and one bounded provenance tag
for every materialized level value; an override changes only the selected tags
to `override`. Composite
costs accept only stackable resources and merge duplicate refs with checked,
deterministic sums.

`studio.requirements` runs named warning/error checks after materialization.
Checks receive only bounded typed `q.level(item_ref, field_handle, level)` reads
and an authentic result constructor; those reads record actual item
dependencies automatically. Expected/actual evidence is bounded JSON-safe data.
An optional waiver names the requirement and records a non-empty reason plus
`reviewed_by`; it does not mutate the authored item data or hide the raw result.

Lua 5.4 was selected over bundled LuaJIT 2.1 on the representative
currency/fixed-sword/levelled-sword workload: both exposed the required hooks,
while Lua 5.4 needed no JIT policy or Lua 5.1 integer compatibility layer. The
committed fixture under `tests/fixtures/lua_sandbox/` is the Windows/Linux
parity receipt.

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_lua_sandbox.py evaluate --root <game-root> --manifest <game-root>/items.lua.json
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_lua_sandbox_test.py
```

### Deterministic Snapshot and focused query

`items_snapshot.py build` turns only canonical evaluator JSON into one
`items.snapshot.v1` document. It sorts item identities, hashes normalized
content and registered field schemas, validates required i64 level fields
against their declared kinds and ranges, requires complete mode-consistent
per-value provenance, retains the evaluator fingerprint,
and derives inputs/dependents from actual typed references. Complete item/field
source spans remain separate from the content hash. `query` returns one item with its Lua
definition location plus an optional field and level range; a selected level
field also includes only its schema, registration location, and selected value
provenance. The Snapshot also carries derived `items.runtime_export.v1`
metadata (field IDs, item storage, and level counts); the compact runtime
package itself is produced by the next export stage. More than 1000
level rows requires an explicit smaller range. `diff` compares only normalized
item and requirement data, emits stable identity-relative JSON Pointer paths,
ignores source-only movement,
and stops after 1000 changes by default. `chart` accepts only a registered
numeric level field, returns at most 200 points by default, preserves range
endpoints, and discloses full level/value bounds plus its stable downsampling
method and counts.
Requirement results and reviewed waivers are semantic Snapshot content;
definition/waiver spans are not. `requirements` returns at most 1000 stable
diagnostics and can filter by the actual queried item dependency and severity.

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_snapshot.py build --evaluation <evaluation.json> --out <snapshot.json>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_snapshot.py query --snapshot <snapshot.json> --item <item-id> --inputs --dependents
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_snapshot.py chart --snapshot <snapshot.json> --item <item-id> --field <field> --max-points 200
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_snapshot.py requirements --snapshot <snapshot.json> --item <item-id> --severity warning
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_snapshot.py diff --before <old-snapshot.json> --after <new-snapshot.json>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_snapshot_test.py
```

### T0365 compact runtime package

`items_runtime_package.py` consumes only `items.snapshot.v1`. It writes one
fixed-width little-endian `items/catalog` blob plus an ABI-stable generated
header. Item/field/level/value/cost sections use checked offset/count spans;
strings are deduplicated, padding is zero, and schema/content/item identities
use the engine-compatible seed-0 XXH64 contract. Generated outputs are replaced
atomically only when their bytes change, so a value-only balance edit changes
the blob without touching the header. The Python inspector is the wire-format
reference. The opt-in native binder validates the generated schema/item ABI,
content digest, UTF-8 strings, ranges, canonical spans, indices, alignment, and
default 64 MiB budget on an owned copy before publishing it. Its lifecycle is
main-thread startup/shutdown only. The public resource adapter remains the next
T0365 slice.

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_runtime_package.py build --snapshot <snapshot.json> --out <items.catalog> --header-out <items_catalog_abi.gen.h>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_runtime_package_test.py
cmake --build templates/template/build/native-debug --target test_items_runtime_package
ctest --test-dir templates/template/build/native-debug -R "^test_items_runtime_package$" --output-on-failure
```

### T0364 typed API proof

`generate_items_api_proof.py` is a bounded reference exporter, not the Lua
evaluator or production blob builder. It consumes the normalized fixtures in
`tests/fixtures/items_api_*_proof.json` and atomically emits build-local
`items_game.gen.h`, `items_game.internal.gen.h`, `items_game.gen.c`, and
`items_game.luau`. The core-only and weapon CTest targets compile the same
`src/items_api.c` against different generated capability schemas. See
`docs/items_lua_schema_api_contract_2026-07-10.md` for the exact boundary.

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/generate_items_api_proof_test.py
cmake --build templates/template/build/native-debug --target test_items_api_core_only test_items_api
```

## Layer

L1 foundation — depends only on the L0 shell (game-state's `gsj_*` JSON toolkit
+ the engine), never on another feature. `features/progression-core` (L2)
depends on this module (`progression.h` includes `features/items/items.h`);
the reverse edge does not exist — items code never mentions progression
(grep-gated).

## Include spelling (two physical roots, one logical prefix)

The public header keeps its historical spelling, `features/items/items.h`,
even though it physically lives under `features/items-core/include/`. The
consuming game adds `features/items-core/include` to its include path
(`ITEMS_CORE_INC`, ahead of its own `src`) so `#include "features/items/items.h"`
resolves to the module; the game's OWN `src/features/items/reason_tags.h`
resolves from the game's `src` on the same logical prefix. See INSTALL.md. A
spelling rename would touch every consumer and break byte-identical relocation.

## Reason verbs

The verb half of `reason` (`verb:subject`) comes from a closed, append-only
list that is **owned by the consuming game**, not this module —
`src/features/items/reason_tags.h` in the game's own tree, included via the
game's include path as documented in `INSTALL.md`. `items_add`/`items_remove`/etc.
in this module call `items_reason_check(reason)`, which resolves to the
game's header at compile time. This keeps the ownership core byte-identical
across every consumer while letting each game define its own verb vocabulary;
the check is a debug-only assert (zero cost in release builds).

## Migrations

Item save-fragment migrations are always game code, never module code — a
migration step encodes a specific game's save history. This module owns
`items_reconcile()` (quarantine/seq-reseed, invariant across every game); the
consuming game owns `state/items.schema.json`'s `version`/`migrations` and the
step bodies. See the game-side `src/features/items/README.md` for the
migration-skeleton recipe.

## Backdoor (documented, not built)

A game with a fundamentally different ownership semantics (e.g. no per-instance
uniques, or a different stacking rule) is not expected to fork this module by
adding a switch here — LEAN forbids speculative generalization for a single
consumer. Instead it copies `src/`+`include/` out of this module into its own
`src/features/items/` tree and owns that copy going forward (copy-then-own,
same escape hatch `settings`/`resource_panel` already use). No code in this
module supports that fork; it is a documented possibility, not a feature.

## Purpose

Provide reusable item catalog, ownership operations, state-fragment generation,
and read-only authoring tools.

## Public surface

`include/features/items/items.h`, generated outputs, and commands declared in
`feature.json` are public. Game reason tags, seed content, and migrations are not.

## Validation

Run the `test`, `api_proof_test`, `lua_sandbox_test`, `snapshot_test`, and
`runtime_package_test` commands from `feature.json`, then
`node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. Patch preserves the public contract,
minor adds backward-compatible surface, and major permits breaking changes.
Consumers pin both this version and an exact repository revision.

## Extension points

Extend through game-owned fields, reason tags, catalogs, save fragments, and
the documented fork escape hatch; game policy stays outside the core.
