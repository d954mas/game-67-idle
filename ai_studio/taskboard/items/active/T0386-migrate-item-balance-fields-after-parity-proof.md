---
id: T0386
title: Cut over Items JSON and schema to single-source modular Lua
status: backlog
project: P001
epic: E016
priority: P1
tags: [items, lua, migration]
created: 2026-07-10
updated: 2026-07-10
---

## What

After the vertical proof, reproduce the complete current template Items catalog
and contracts in Lua, prove parity, switch every consumer once, and delete the
old JSON/schema/parser path without a compatibility fallback.

## Done when

- [ ] Inventory all `items.json`, `item_fields.schema.json`, generator, ops,
      CMake, public header, runtime, tests, Viewer, docs, lock, new-game, and
      progression-reference consumers before cutover.
- [ ] Lua fixtures reproduce all six item definitions, kinds, core blocks,
      diagnostics, lock/removal behavior, and Viewer output. After T0390 builds
      the target runtime, T0392 migrates concrete legacy containers into state;
      they are never copied into Lua.
- [ ] New strict storage-mode enforcement and saved-level bounds/migration
      behavior have explicit fixtures; deliberate behavior changes are not
      mislabeled byte parity.
- [ ] CLI, Viewer, build codegen, tests, docs, and skills switch to Snapshot in
      one cutover; `items.json`, `item_fields.schema.json`, old JSON parser, and
      stale instructions are removed.
- [ ] `items.lock.json` remains the sole extended release receipt across the
      cutover and proves parity for shipped/removed IDs and migration reactions.
- [ ] Migration preserves the read-only Viewer first, then enables T0366/T0367
      semantic editing only after Lua source spans/write refusal are proven.
- [ ] No code reads both Lua and JSON, and rollback is defined only before the
      cutover; after cutover version control is the rollback.
- [ ] Windows/Linux full CI and agreed performance budgets pass before closing.

## Open questions

- Migration starts only after T0364/T0381/T0382/T0383/T0365 and the ordered
  T0391 -> T0390 -> T0392 state/runtime/migration chain prove the complete seam
  and the lead accepts intentional runtime changes.

## Log

- 2026-07-10: Replaced partial migration of selected numeric fields. The lead
  selected complete single-source Items Lua, so the correct migration is one
  explicit catalog/schema cutover and deletion.
