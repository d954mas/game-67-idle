---
name: nt-game-items
description: "Use for the items catalog CLI, ownership API, reason tags, quarantine/reconcile, or items state and migrations."
---

# NT Game Items

Workflow router for the split items feature. The invariant core lives in
`features/items-core/`; content, state, reasons, and seed remain consumer-owned.

## Start

1. Read `features/items-core/README.md` for the public surface, tools,
   validation, compatibility, and extension boundaries.
2. Read `features/items-core/INSTALL.md` for wiring, verification, and removal.
3. Read `features/items-core/feature.json` for the versioned machine contract.
4. Read the consumer's `src/features/items/README.md`, then inspect its catalog,
   lock, field schema, and state schema as relevant.

## Catalog workflow

For single-source Lua catalogs, use the focused semantic CLI. It always takes
an explicit game root and delegates evaluation/validation to the canonical
sandbox and Snapshot:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> list
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> inspect --item <item-id> --level-from <n> --level-to <n>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> dependencies --item <item-id>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> source --item <item-id>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> validate
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> build --out-dir <build-dir>
```

Use `chart` and `requirements` for bounded reports; inspect command help for
their filters instead of loading the full Snapshot.

Until T0386 removes the legacy JSON catalog, use its existing op-layer only for
that legacy path:

Use the module op-layer; do not add a second catalog parser:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_ops.py list --catalog <game>/content/items.json --json
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_ops.py validate --catalog <game>/content/items.json --schema <game>/content/item_fields.schema.json --baseline <game>/content/items.lock.json --state-schema <game>/state/items.schema.json --json
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_ops.py schema --schema <game>/content/item_fields.schema.json --json
```

Pass consumer paths explicitly. Follow the consumer README for additions,
removals, version bumps, migrations, and lock handling. Regenerate catalog
tables and run registered tests after catalog changes.

## Runtime and state workflow

1. Use the public ownership API; do not mutate raw items state.
2. Keep reason tags and new-game seed in the consumer.
3. Route save-shape and migrations through `nt-game-state-management`.
4. Run module validation and the consumer's relevant runtime/save tests.

## Routing

- Core behavior and validation: `features/items-core/README.md`.
- Installation and test wiring: `features/items-core/INSTALL.md`.
- Version and dependencies: `features/items-core/feature.json`.
- Content, lock, quarantine, reasons, and migrations: the consumer's local
  items README and owned content/state files.
- Live proof: `nt-runtime-automation`.
- Acceptance evidence: `nt-quality-checks`.
