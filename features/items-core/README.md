# items-core

In-place L1 module (see `features/README.md` — decisive rule: same-`.c`-across-games
+ data-only customization = module, not a copy-then-own feature). Precedent:
`features/game-state` — one copy of the source lives here, each consuming
game/template compiles it in-place against ITS OWN generated headers and content
(`../../features/items-core/` from any `templates/<x>` or `games/<id>`, depth-2
invariant). Extracted 2026-07-07 (`templates/design/build_spec_t0337_2026-07-07.md`,
increment M1) out of `templates/template/src/features/items/`.

## What it is

One catalog (item/container/currency definitions, compiled from a game's
`content/items.json`) + one ownership model (who has what, in which container) +
a read-only op-layer CLI. No UI, no DevAPI commands of its own, no gameplay
logic — items NEVER executes effects, it only tracks and reports ownership.

Successful mutations emit typed events: `items.txn` for add/remove/create/destroy
and `items.move` for container transfers.

## Contents

```text
features/items-core/
  include/features/items/items.h   public API, spelling preserved (see "Include spelling" below)
  src/
    items_catalog.c                catalog lookup over the codegen tables
    items_containers.c             ownership (add/remove/move/count/purse/instances/seq)
    items_reconcile.c              post-load quarantine + unique-seq reseed
  scripts/
    generate_items_catalog.py      content codegen (content/items.json -> const C tables)
    items_ops.py                   read-only op-layer CLI (list/validate/schema)
    items_ops_test.py              self-contained unittest for items_ops.py's rules
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

`reason` is mandatory on every mutation, format `verb:subject`, verb from a
closed, append-only list — see "Reason verbs" below.

`items_reconcile()` runs after every load: a record whose `def_id` is no
longer in the catalog is quarantined (never deleted), and a formerly-removed
def_id that reappears is un-quarantined. It also reseeds the unique-instance
sequence counter above the highest `<def_id>#<seq>` key already present in a
loaded save, so a freshly created unique after a process restart can never
collide with one already on disk.

## Tools (`scripts/`)

- `generate_items_catalog.py --catalog <items.json> --schema <field_schema.json> --out-dir <dir>` —
  emits `items_catalog.gen.{h,c}` (compile-time const tables) plus a
  lightweight sanity net.
- `items_ops.py list|validate|schema [--json]` — read-only op-layer CLI;
  `validate` is a strict superset of the generator's sanity net (imports it,
  never re-parses) plus the destructive-change (lock-file) guard, full
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
resolves from the game's `src` on the same logical prefix. See INSTALL.md and
`templates/design/build_spec_t0337_2026-07-07.md` §2.2 for the full rationale
(a spelling rename would have touched ~12 files across every consumer and
broken byte-identical relocation).

## Reason verbs

The verb half of `reason` (`verb:subject`) comes from a closed, append-only
list that is **owned by the consuming game**, not this module —
`src/features/items/reason_tags.h` in the game's own tree, included via the
game's include-path (§2.3 of the build spec). `items_add`/`items_remove`/etc.
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
