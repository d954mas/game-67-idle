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
4. Read the consumer's `src/features/items/README.md`, then inspect its Lua
   manifest/modules, release lock, and state schema as relevant.

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
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> validate --affected <item-id>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> build --out-dir <build-dir>
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> seal-receipt
```

Use `chart` and `requirements` for bounded reports; inspect command help for
their filters instead of loading the full Snapshot.

For a supported existing literal, get the hash from `source`, preview a
`level-set`, `curve-set`, `override-set`, `max-level-append`, or
`max-level-truncate`, inspect both diffs, then repeat with `--apply`. Max-level
edits require explicit generate/columns literals and remain receipt-gated.
Never remove an `.items-edit.lock` until no writer process is active.
Unsupported source shapes route to direct source/agent editing.
Use `batch --patch-file` only when every typed operation resolves to the same
Lua file; preview and store its returned inverse batch before `--apply`.

There is no legacy parser or fallback catalog. Pass the consumer root
explicitly. Follow its README for additions, removals, version bumps,
migrations, and release-lock handling. `seal-receipt` is the only release
history write path; run it only after the matching migration and checks pass.

## Runtime and state workflow

1. Use the public ownership API; do not mutate raw items state.
2. Keep reason tags and new-game seed in the consumer.
3. Route save-shape and migrations through `nt-game-state-management`.
4. Bind the packed `items/catalog` blob before save load/reconcile or gameplay.
5. Run module validation and the consumer's relevant runtime/save tests.

## Routing

- Core behavior and validation: `features/items-core/README.md`.
- Installation and test wiring: `features/items-core/INSTALL.md`.
- Version and dependencies: `features/items-core/feature.json`.
- Content, lock, quarantine, reasons, and migrations: the consumer's local
  items README and owned content/state files.
- Live proof: `nt-runtime-automation`.
- Acceptance evidence: `nt-quality-checks`.
