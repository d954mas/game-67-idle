# Template Items integration

The reusable evaluator, package runtime, ownership operations, and reconcile
logic live in `features/items-core`. This directory contains only game-owned
policy:

- `reason_tags.h`: closed mutation-reason verbs;
- `items_bootstrap.c`: initial grants for a normal new game;
- this integration and migration note.

## Authoring and build

The canonical catalog is `items.lua.json` plus the allowlisted modules under
`design/items/`. Use the single semantic CLI:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root templates/template list
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root templates/template validate
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root templates/template build --out-dir templates/template/build/items-catalog
```

CMake performs the same build into its generated directory, packs the resulting
blob as `items/catalog`, and compiles the generated ABI header. Runtime startup
binds that blob before save load/reconcile or gameplay. There is no JSON or
generated-table fallback.

## Ownership

Stack APIs reject `stack == 1`; instance APIs require it. The template keeps
the current fixed ownership policy from Items Core: a 20-record `backpack` and
an unlimited currency-only `purse`. Currency and finite stack caps come from
the bound catalog package.

Every mutation uses a `verb:subject` reason from `reason_tags.h`. Normal new
games are seeded in `items_bootstrap.c`; `--fresh-state` intentionally runs only
the generated reset defaults and skips those grants.

## Release receipt and destructive changes

`content/items.lock.json` records shipped field IDs, item storage, and maximum
shipped level counts. `validate` fails when an authored change is incompatible
with that history. After the matching save migration is implemented and the
release candidate is verified, seal the compatible new bounds once:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root templates/template seal-receipt
```

The write is atomic and idempotent. Removed item IDs stay in `removed`; removed
field IDs move from `receipt.field_ids.active` to `reserved`. Never erase
identity history.

## Save migrations

The save fragment is generated from `state/items.schema.json`. A destructive
catalog change must first raise the fragment version and add an ordered,
idempotent migration step. Migration bodies are game code because they encode
this game's shipped history. `items_reconcile()` then quarantines any remaining
unknown definitions without deleting player data and restores them if the
definition returns.

Verification:

```powershell
cmake --build templates/template/build/native-debug --target game test_items_fragment test_progression test_template_composition
ctest --test-dir templates/template/build/native-debug -R "items|progression|template_composition" --output-on-failure
```
