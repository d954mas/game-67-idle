---
name: nt-game-items
description: "Maintain item catalogs, ownership operations, reason tags, reconciliation, and item-state migrations."
---

# NT Game Items

The invariant core lives in `features/items-core/`; content, state, reasons,
and seed remain consumer-owned.

## Start

Use the consumer's `src/features/items/README.md` and affected content/state
files. Read relevant `features/items-core/README.md` sections for core contracts,
`features/items-core/INSTALL.md` for wiring changes, and
`features/items-core/feature.json` for versions/dependencies.

## Catalog workflow

For single-source Lua catalogs, use the semantic CLI with an explicit game
root. It delegates evaluation/validation to the canonical sandbox and Snapshot:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/scripts/items_cli.py --project-root <game> validate
```

Add `--affected <item-id>` for an item dependency report; omit it for catalog-wide
validation, including tracks or exports. Choose `list`, `inspect`, `dependencies`,
or `source` for discovery; `chart` and `requirements` provide bounded reports.
Use command help for filters. Run
`build --out-dir <build-dir>` when generated output is needed.

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

- Use the public ownership API, never raw items-state mutations. Keep reason
  tags and new-game seed in the consumer.
- Route save-shape changes and migrations through `nt-game-state-management`.
- The generated catalog is compiled in; no bind step is needed.
- Validate affected catalog changes and run relevant consumer runtime/save tests
  for changed ownership or persistence behavior. Core changes need module checks.

Report the changed contracts, checks, and gaps. Use `nt-runtime-automation` for
live proof and `nt-quality-checks` for acceptance evidence when applicable.
