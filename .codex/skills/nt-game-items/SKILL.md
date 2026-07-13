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

Use the module op-layer; do not add a second catalog parser:

```powershell
py -3.12 features/items-core/scripts/items_ops.py list --catalog <game>/content/items.json --json
py -3.12 features/items-core/scripts/items_ops.py validate --catalog <game>/content/items.json --schema <game>/content/item_fields.schema.json --baseline <game>/content/items.lock.json --state-schema <game>/state/items.schema.json --json
py -3.12 features/items-core/scripts/items_ops.py schema --schema <game>/content/item_fields.schema.json --json
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
