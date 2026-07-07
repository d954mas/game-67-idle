---
name: nt-game-items
description: "Use when working with the template's items feature (L1): item/container/currency catalog content, the read-only op-layer CLI (list/validate/schema), the ownership API (add/remove/move/count/can_afford, purse, unique instances), reason tags, quarantine/reconcile, or the items save fragment and its migration skeleton."
---

# NT Game Items

Use this as the agent-facing router for the `items` feature: the invariant
ownership core lives in the root in-place module `features/items-core/`
(`src`/`scripts`), and the game-owned corner (`reason_tags.h` +
`items_on_new_game` seed) plus content/state config live in
`templates/template/src/features/items/` and `templates/template/{content,state}/`
(T0337 split — see "Boundary" below). Keep canonical code, schemas, and
scripts in those folders, not in this skill folder.

## Start

1. Read the design doc: `templates/design/item_system_design_2026-07-06.md`
   (§1 core+blocks+currencies, §3 containers, §6 save/migrations, §9
   ship-with-fixes, §10 mutations/reason/txn). This doc has not yet moved to a
   permanent home (out of T0327 И2 scope) -- this skill points at its current
   path.
2. Read `templates/template/src/features/items/README.md` first for
   everything feature-specific: public API summary, state-fragment shape,
   quarantine semantics + its `owned.max_count` budget cost, art_needs,
   content workflow, and the lock workflow (`content/items.lock.json`). This
   skill is the HOW-TO (commands, workflow order); the README is the WHAT
   (reference) — read it instead of re-deriving the same facts from source.
3. Inspect `templates/template/content/items.json` +
   `templates/template/content/item_fields.schema.json` before catalog
   content changes; inspect `templates/template/state/items.schema.json`
   before ownership/save-shape changes.

## Two halves (design §7)

**Working with the CATALOG (data, offline/tooling)** -- go through the
op-layer, never hand-parse `items.json`:

```
py -3.12 features/items-core/scripts/items_ops.py list     [--catalog content/items.json] [--json]
py -3.12 features/items-core/scripts/items_ops.py validate [--catalog content/items.json] [--schema content/item_fields.schema.json] [--baseline content/items.lock.json] [--state-schema state/items.schema.json] [--json]
py -3.12 features/items-core/scripts/items_ops.py schema   [--schema content/item_fields.schema.json] [--json]
```

(T0337 moved the op-CLI out of the template into the `items-core` module;
pass every path explicitly when running from a directory other than the
module itself — the script's own argparse defaults are relative to
`features/items-core/`, not the game, so a missing `--catalog`/`--schema`/
`--baseline`/`--state-schema` resolves inside the module and either hard-
fails or silently skips a check, §5.6/R7 of `templates/design/build_spec_t0337_2026-07-07.md`.)

- `list`/`schema`/`validate` are the ONLY ops in И2 (read-v1, LEAN §3);
  upsert/deprecate are editor-era (T0316 web editor), not built yet.
- **New item def: set `created` (ISO date `"YYYY-MM-DD"`), required**
  (`validate` rules `created-missing`/`created-invalid`; lead-ratified
  2026-07-07 — git history was rejected as unreliable, copy-then-own
  `games/new_game.mjs` resets it). `list --json` returns it; never compiled
  into the C tables (authoring metadata only).
- `validate` is the STRICT gate; rule ids surfaced in `--json` output:
  `generator-check` (reuses `generate_items_catalog.py`'s own sanity net —
  never forked into a second parser), `namespace`, `created-missing`,
  `created-invalid`, `composite-key-length`, `equip-unlimited` (hard errors),
  the lock-file removal-workflow rules `removed-without-reaction` /
  `removed-version-not-shipped` / `lock-invalid` / `lock-inconsistent` (hard
  errors, lead-ratified 2026-07-07 — deleting a SHIPPED def_id is destructive
  and must FORCE a reaction), plus advisory warnings `display-name-keying`,
  `rename-guard-skipped` (default `content/items.lock.json` absent), and
  `removed-def-restored` (a removed id reappeared — legal, not an error).
  Full rule semantics and the step-by-step removal recipe (including
  BATCHING many removals under one shared `fragment_version`/migration
  step): README.md "Content workflow" / "Lock workflow".
- **This CLI is wired into `ctest`** (target `items_ops_validate`) — the
  guard is no longer voluntary: a destructive change without a reaction
  fails the build automatically. `items_ops_test.py` (a second ctest target)
  is the committed proof that the lock-workflow rules themselves fire
  correctly, against temp fixtures.
- Exit codes: 0 OK, 1 validation FAIL, 2 usage/IO error (also: an explicitly
  passed `--baseline`/`--state-schema` path that does not exist, OR a
  baseline with the wrong shape, is an IO error; a MISSING default
  `--baseline` is not — it just skips the lock-file checks with a warning).
  `--json` gives the future Node web editor subprocess parity with this CLI
  (same source of truth, no second data model); errors/warnings are
  structured `{rule, id, field, msg}` objects, not free strings.
- Editing the catalog content itself? Also see
  `features/items-core/scripts/generate_items_catalog.py` (compile-time
  codegen: `items.json` -> `items_catalog.gen.{h,c}` const tables) — this is
  a SEPARATE codegen from the save-state generator; do not mix the two.

**Working IN THE GAME (runtime, via the feature API)** -- use
`features/items-core/include/features/items/items.h` (spelling
`features/items/items.h` preserved through the game's include-path, §2.2 of
`templates/design/build_spec_t0337_2026-07-07.md`), never raw
`items_state`/`owned[]` access from game code:

- `items_add`/`items_remove`/`items_move`/`items_count`/`items_can_afford` —
  one verb per operation across stacks AND currencies (gold is just an item
  with a `currency` block; no separate currency API).
- Unique instances (equip-block items) always need a container DESTINATION:
  `items_instance_create(container_id, def_id, reason)` /
  `items_instance_destroy(instance_id, reason)`.
- Prices/costs are lists of `{def_id, count}` pairs checked with
  `items_can_afford` per entry — there is no single "price" type.
- Currencies go through `purse` by default (`items_purse(def_id)` reads
  `count("purse", def_id)`); do not hardcode a currency container id in game
  code, let the catalog's `accept_policy` route it.
- `reason` is mandatory on every mutation, format `verb:subject`, verb from
  the closed list in `templates/template/src/features/items/reason_tags.h`
  (debug-assert in non-release builds).
- **Fractional/idle production is NOT built into `count` (int64 everywhere,
  including currencies).** The pattern (not implemented in И2, documented
  here per design §OQ5): accumulate a `float`/`double` accumulator in game
  glue code, and only call `items_add`/`items_remove` once the accumulator
  crosses `>= 1`, flushing the integer part back into `count`. Never store
  fractional amounts in the save.

## Migration skeleton (§9, not built in И2 — no migrations exist yet)

`state/items.schema.json` is `version: 1`, no `migrations` yet. Full v2
recipe (schema bump, `items_migrate_1_to_2`, `items_migrations.c`, CMake
wiring, `items_v1.json` regression anchor): README.md "Migration skeleton" —
do not re-derive the steps here, follow that section.

## Rules

- `items.h` is the ONLY public header (feature-layer L1), now living in the
  root module `features/items-core/include/features/items/items.h`
  (spelling preserved, §2.2 of `templates/design/build_spec_t0337_2026-07-07.md`):
  it depends only on the L0 shell (game_save toolkit + `gsj_` json helpers +
  engine), never on another feature.
- Never key game logic off `display_name` — it is display-only; key off `id`.
- The `<container>/<def_id>` composite key is built by exactly ONE helper
  inside `items_containers.c` (now `features/items-core/src/items_containers.c`)
  — do not hand-build it elsewhere; use `items_ops.py validate`
  (`features/items-core/scripts/items_ops.py`) to catch length overflows
  before they reach a save.
- `reconcile()` quarantines orphaned def_ids, never deletes (README.md
  "Quarantine" — includes the `owned.max_count` budget cost of churny
  catalogs).
- items has zero DevAPI commands of its own — it is reachable through the
  universal `game.state.*` commands (`game.state.get {path:"items"}`,
  `game.state.schema`, `game.state.patch`). For runtime/live proof, use
  `nt-runtime-automation`; for save/fragment mechanics, stay in
  `nt-game-state-management`.

## Boundary

Keep this skill as a router. Catalog content and state schema/migrations
belong in `templates/template/{content,state}/`; the game-owned corner
(closed reason-verb list, `on_new_game` seed, integration tests) in
`templates/template/{src/features/items,tests}/`; the ownership core,
content codegen, and op-layer CLI now live in the root in-place module
`features/items-core/{src,scripts}/` (T0337 — see
`features/items-core/README.md` + `INSTALL.md`, and the root
`features/README.md` "Categories" for the decisive rule).
