# items — feature reference

`src/features/items/` — L1 foundation feature (`items.h` first line:
`// feature-layer: L1`). Depends only on the L0 shell (game_save toolkit +
`gsj_` JSON helpers + engine); never includes another `src/features/*`
header (grep-gate G10). No `#if FEATURE_GAME_STATE` — state is always on
(T0327 И2-0), items is one version. Ships with build_spec
`templates/design/build_spec_t0327_i2_2026-07-07.md` §2/§6-§8 as the source
design; this file is the feature-local operational reference. For agent
workflow ("how do I..."), use the `nt-game-items` skill — this README is
the WHAT, the skill is the HOW-TO; do not duplicate content between them.

## What it is

One catalog (item/container/currency definitions, compiled from
`content/items.json`) + one ownership model (who has what, in which
container) + one typed event (`items.txn`). No UI, no DevAPI commands of its
own, no kind of gameplay logic — items NEVER executes effects, it only
tracks and reports ownership. `use.effect_id` is a pointer for a future
effects layer to interpret, not something items itself runs.

## Public API (`items.h`)

**Catalog (const tables, always compiled):**
- `item_core(def_id)` / `item_at(index)` / `items_def_count()` — lookups.
- `items_with_tag(tag, out, out_cap)` — tag filter.
- `item_is_currency(def)` — has a `currency` block.
- `item_container_def(container_id)` — container definition lookup.

**Ownership (over the generated `items` save fragment):**
- `items_add(container_id, def_id, count, reason)` /
  `items_remove(container_id, def_id, count, reason)` — one verb for stacks
  AND currencies (gold is just an item with a `currency` block).
- `items_count(container_id, def_id)` / `items_can_afford(container_id, def_id, n)`.
- `items_move(from, to, entry_key, count, reason)` — the ONLY place ownership
  changes container (stacks: strict remove+add re-key; uniques: field
  re-parent). `entry_key` is a def_id for a stack, an instance_id for a
  unique.
- `items_instance_create(container_id, def_id, reason)` /
  `items_instance_destroy(instance_id, reason)` — unique-instance pool
  (equip-block items); always requires a container destination.
- `items_purse(def_id)` — convenience for `items_count("purse", def_id)`.

**`reason` is mandatory on every mutation**, format `verb:subject`, verb from
the closed, append-only list in `reason_tags.h` (debug-assert in
non-release builds; no-op in release). Prices/costs are lists of
`{def_id, count}` checked one at a time with `items_can_afford` — there is
no single "price" type.

## State fragment (`state/items.schema.json`, `--fragment items`)

Ownership is ONE flat map `owned: map<string, ItemOwned>` — NOT one map per
container. A "container" is a runtime VIEW (filter of `owned` by the
`.container` field) over const container definitions in the catalog, not a
schema construct; new containers are data (add a row to `items.json`), never
a schema change. Key convention (the one piece of "magic", §2.3 of the build
spec):
- **stack** (stackable/currency def): key = `"<container>/<def_id>"` — the
  KEY is authoritative; `.container` must always match the key's prefix.
  Built by exactly ONE helper (`build_stack_key` in `items_containers.c`) —
  never hand-build this key elsewhere.
- **unique instance** (equip-block def): key = instance_id
  (`"<def_id>#<seq>"`); the `.container` FIELD is authoritative (the key does
  not encode a container).

`version: 1`, no `migrations` section yet (skeleton only). To add v2:
1. Bump `"version": 2` in `state/items.schema.json`.
2. Add `"migrations": [{"to_version": 2, "fn": "items_migrate_1_to_2"}]`.
3. Write `bool items_migrate_1_to_2(cJSON *frag, char *err, int cap)` in a
   new `items_migrations.c`, operating on the RAW fragment JSON (level-1
   plumbing; semantic reshaping is level-2, owned by the game's own copy).
4. Add the new file to `CMakeLists.txt` `target_sources`.
5. Regression anchor: `tests/fixtures/items_v1.json` must still load through
   the full migration chain once v2 exists.

## Quarantine (`items_reconcile`, runs after every load)

A record whose `def_id` is no longer in the catalog gets `quarantined: true`
— it is NEVER deleted (deletion would be unrecoverable save corruption if
the catalog change was temporary). Quarantined records are excluded from
`items_count`/`items_can_afford` but keep their `.container` home and their
`count`, and survive `to_json`/`from_json` round-trips. If the def_id
reappears in the catalog, the next `reconcile()` un-quarantines it.

**Budget cost:** quarantined records still occupy a slot in `owned`, which is
capped by `state/items.schema.json`'s `owned.max_count` (currently 64,
compiled into `ITEMS_STATE_MAX_OWNED`). A game with catalog content that
churns heavily (defs removed/renamed often) can exhaust this budget on
quarantine alone — raise `owned.max_count` in that game's copy of the schema
if this becomes a real constraint. This is a per-game tuning knob, not
something items works around automatically.

**Capacity + UI (lead, 2026-07-07):** a quarantined record occupies its
container-capacity slot (it lives in that container and returns there on
restoration — no overflow case exists by construction). DELIBERATE removal of
a shipped def belongs in a migration step (delete or convert to compensation,
e.g. gold by `base_value`) — migrations run before reconcile, so quarantine is
only the safety net for UNhandled catalog changes. Inventory UI guidance for
future games (no inventory screen exists in the template): render a
quarantined record as a greyed "?" placeholder slot, no interactions — hiding
it would show "20/20 used, 19 visible" and read as a bug. Final call at the
first game with an inventory UI.

## Art

`art_needs: NONE` by design in И2 — items is 100% data/tracking, it has no
visuals of its own (`icon_asset_id` in the catalog is a logical string id,
not an art handle). No `feature.json` needed. When a real UI consumer shows
up (И3, e.g. `resource_panel`), THAT feature declares `art_needs` and reads
icon handles from the game's own atlas — items itself never writes to a
pack (see `src/features/README.md` "Ассеты (декларативная модель)" for the
convention and its grep-gate).

## Content workflow (catalog authoring)

1. Edit `content/items.json` (+ `content/item_fields.schema.json` if adding a
   new field/block shape). **New item def:** set `created` (ISO date,
   `"YYYY-MM-DD"`) — required (`validate` rules `created-missing`/
   `created-invalid`; lead-ratified 2026-07-07, git history was rejected as
   the source of truth for this since copy-then-own resets it,
   `games/new_game.mjs`). The future T0316 web editor will set `created`
   automatically; hand-authored entries must set it themselves.
2. Build codegen: `py -3.12 tools/generate_items_catalog.py --catalog
   content/items.json --schema content/item_fields.schema.json --out-dir
   <dir>` — emits the compile-time const tables (`items_catalog.gen.{h,c}`).
   Also runs a lightweight sanity net so a broken catalog fails the build,
   not just the runtime. `created` is authoring metadata only — never
   compiled into the tables.
3. Run the STRICT gate before shipping: `py -3.12 tools/items_ops.py validate`
   — a strict superset of the generator's sanity net (same source of truth,
   imported not re-parsed) PLUS: the `created` field, the lock-file removal
   workflow against `content/items.lock.json` + `state/items.schema.json`
   (below), full `<namespace>.<slug>` charset check, the composite-key
   length hard rule (`len(container) + 1 + len(def_id) <= 63`, since stack
   keys are `"<container>/<def_id>"` under `string_max=64`), an `equip` ⇒
   not `stack.unlimited` sanity check, and an advisory display_name-keying
   lint. See the `nt-game-items` skill for the full CLI reference (`list`/
   `validate`/`schema`, `--json`). **This gate ALSO runs automatically in
   `ctest`** (target `items_ops_validate`) — a destructive change without a
   reaction now fails the build's test suite by itself, not only a manual
   run you might forget to do.

### Lock workflow (`content/items.lock.json`) — destructive-change guard

**Lead-ratified 2026-07-07: deleting/renaming a SHIPPED def_id is destructive
(existing saves may already reference it) and must FORCE an explicit
developer reaction — not just log a warning.** `items.lock.json` v2 has two
sections:
- `def_ids` — ids currently live and shipped (append-only: once here, stays
  until deliberately moved to `removed`).
- `removed` — ids DELIBERATELY removed from the catalog after shipping, keyed
  by def_id: `{"fragment_version": N, "note": "..."}`. `fragment_version`
  (required, integer `>= 2`) records WHICH `state/items.schema.json`
  `version` bump + migration step actually handled the removal, and must be
  `<= ` the CURRENT items fragment version — i.e. DELIVERED, not merely
  declared. `note` is OPTIONAL free-text documentation; `validate` never
  requires it.

**Fresh games start empty.** `games/new_game.mjs` resets a new game's copy of
`content/items.lock.json` to an empty baseline (`def_ids: []`, `removed: {}`)
right after copying the template (copy-then-own) — the TEMPLATE's own lock
legitimately lists ITS shipped demo defs (`tmpl.gold` etc.), but a brand-new
game has shipped NOTHING to ITS OWN players yet.

**Everyday cases:**
- **Adding a new item and shipping it — READ THIS ONE TWICE.** Once a def_id
  has gone out in a release, you MUST append it to `def_ids`. **This is the
  ONE manual, unenforced link in the whole chain** — nothing currently
  catches "you forgot to append a newly-shipped id to the lock" (only the
  REVERSE mistake, removing a locked id, is machine-enforced). Miss this step
  and the destructive-change guard is quietly blind to that def_id until
  someone notices. Never remove an entry from `def_ids` once it has actually
  shipped — move it to `removed` instead (below).
- **Editing an item that is still unreleased** (not yet in `def_ids`): free
  to rename/remove, no guard fires — it never shipped.
- **Restoring a previously removed def:** if a def_id in `removed` reappears
  in the catalog, `validate` only WARNS (`removed-def-restored`) —
  restoration is legal, `reconcile()` un-quarantines any matching saved
  records. Move the id back to `def_ids` and drop its `removed` entry once
  you are sure the restoration is permanent. **If a def might come back,
  prefer a no-op/quarantine migration step over a real delete step:** dropping
  the `removed` receipt later does NOT undo a migration step that has already
  shipped and run — a delete step that already executed cannot be un-run by
  editing the lock file after the fact.

**Deliberately removing a shipped def_id — the forced step-by-step:**
1. Remove the def_id from `content/items.json`.
2. Run `validate` — it goes RED with `removed-without-reaction` (the id is
   still in `def_ids`, missing from the catalog, and not yet in `removed`).
   The error message spells out the next three steps.
3. In `content/items.lock.json`: move the id from `def_ids` to `removed`
   with `fragment_version = <current items fragment version> + 1` — or just
   `= <current version>` if you already bumped `state/items.schema.json`'s
   `"version"` earlier in THIS SAME release for an unrelated shape migration.
4. Bump `state/items.schema.json`'s `"version"` to that same number, and add
   the migration step for it (`"migrations": [{"to_version": ..., "fn":
   "..."}]`, see "Migration skeleton" above) — a REAL step: delete or
   convert the orphaned records (e.g. convert a currency to something else
   by `base_value`), or an explicit no-op step recording the conscious
   decision that `reconcile()`'s quarantine is the intended handling. The
   generator enforces `version == len(migrations)+1` (§0 п.3 of the build
   spec), so the version bump ITSELF forces you to add the step — you cannot
   silently skip it.
5. Run `validate` again — green once the fragment_version in `removed`
   matches (`<=`) the now-bumped `state/items.schema.json` version.

**Batching:** many removals in one release can share the SAME
`fragment_version` — one version bump, one migration step, covering all of
them. Example: deleting 10 defs in one release moves all 10 ids into
`removed` with the SAME `fragment_version` (say `3`), and ONE migration step
(`items_migrate_2_to_3`) handles all 10 together. `validate` never requires
distinct versions per entry.

**Data-shape guards:** an id listed in BOTH `def_ids` and `removed`
simultaneously is `lock-inconsistent` (fix `items.lock.json` by hand — it
must be exactly one of "currently shipped" or "documented removal"); a
`removed` entry with a missing/non-integer/`< 2` `fragment_version` is
`lock-invalid`.

**Exception (rare):** if a def_id in `def_ids` never actually shipped in any
released build, you may remove it from `def_ids` directly instead of going
through `removed` — but treat that as the exception for authoring mistakes,
not the primary path for a real, shipped removal.
