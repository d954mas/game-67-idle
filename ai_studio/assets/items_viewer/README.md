# Items Viewer

Read-only web surface that shows the item catalog of any registered game or
template — "see the items", the cheapest, most valuable slice of the future
item editor (T0316 phase 1). Full contract and design rationale:
`docs/build_spec_phase1_2026-07-08.md`.

## Owner and boundary

This module owns the catalog VIEW: the merged-registry lookup, the HTTP
contract that turns `items_ops.py` output into one view object, and the thin
page. It does **not** own the item data model or the op-layer — that is
`features/items-core/scripts/items_ops.py` (read: T0327; write: not built
yet). No new parser of `items.json` lives here (decision (а) — see the spec
§1/§8): every catalog field comes from `items_ops.py list/schema --json`,
verbatim.

## Phase-1 boundary (read this before extending)

In scope: pick a catalog, render its items/containers/item_kinds, show lock
status (shipped/draft/removed) and `validate` issues per item + catalog
summary, schema-driven field rendering (no hardcoded template fields).

Explicitly **not** built here — do not scaffold these without a new spec:

- **Writing.** No edit forms, no `upsert`/`deprecate`. The write op-layer does
  not exist in `items_ops.py` yet.
- **Icon resolve.** `resolveIcon()` in `site/items.js` always returns `null`
  (an honest placeholder) — no gallery search, no `icon_asset_id -> file`
  binding. That binding is `icon-link`, a phase-2+ `items_ops` command (spec
  §5). The seam is one function wide; wiring it is all phase 2 needs to
  change here.
- **A CLI client.** `items_ops.py` (`list` / `validate` / `schema --json`) IS
  the agent client — parity with the web client holds by construction because
  both read the exact same subprocess. There is no `cli.mjs` in this module
  (decision (б); confirmed out in spec §8).
- **Caching.** Every `/catalog` request re-spawns `py -3.12 items_ops.py`
  (list/schema/validate — up to 3 short spawns). Catalogs are tiny (a handful
  of items); add an mtime cache only if this ever measurably feels slow.

## Layout

- `ops.mjs` — pure logic, no HTTP. Merges `listRegisteredTemplates`/
  `listRegisteredGames` (`assets/backlog/storage/sources/`) into the dropdown
  list; spawns `items_ops.py` (`execFile("py", ["-3.12", ...])`, cwd = repo
  root, every path ABSOLUTE) for `list`/`schema`/`validate --json`; reads
  `items.lock.json` directly (a separate artifact — see "Lock status" below);
  folds everything into one catalog view object. `loadCatalogView(root,
  folderAbs, meta)` is decoupled from the registry lookup so tests can point
  it at a throwaway temp folder. All `node:test` coverage lives in `tests/`.
- `api.mjs` — `createItemsViewerApi(root)`, an `async (req,res,url) => bool`
  handler mounted by Studio Shell on `/api/items-viewer/` (mirrors
  `assets/canvas/api.mjs`'s shape — HTTP <-> ops marshalling only, no items
  logic). Two read-only GET endpoints; see the spec §3 for the full response
  shapes and exit-code -> HTTP-status mapping.
  - `GET /api/items-viewer/catalogs` — the dropdown list.
  - `GET /api/items-viewer/catalog?id=<kind>:<id>` — the whole view for one
    catalog in one fetch.
- `site/` — one page (`items.html` + `items.js` + `items.css`), bare ESM, no
  framework, no build step. Studio Shell also serves a short `/items` route
  (query string preserved) that 302-redirects to
  `/ai_studio/assets/items_viewer/site/items.html`; see
  `ai_studio/studio_shell/server.mjs`.
- `tests/` — `node:test` suite for `ops.mjs`. The live template
  (`templates/template/content/items.json`, 6 items) is the committed
  happy-path fixture; every failure branch (malformed catalog/schema/state
  schema, missing `content/`, exit-code branches) builds a throwaway temp
  folder from the template's own valid schema files.

## Why the site can't import ops.mjs

`ops.mjs` imports `node:child_process`/`node:fs` to spawn `items_ops.py` and
read `items.lock.json` — it cannot load in a browser, and this module has no
bundler (deliberately — "no build step, no bundler", spec §4). So the one
genuinely shared piece of logic, the **issue routing rule** ("an issue
attaches to a card IFF its `id` is present in `items[]`", spec §4), is a
small pure function in *both* places: `ops.mjs` exports `routeIssues()` for
`node:test` coverage of the contract; `site/items.js` re-implements the same
~10-line partition in vanilla JS to render cards/summary/Removed section. This
is a deliberate, tiny duplication of a trivial array partition — not a second
data model of `items.json` (decision (а) is about the catalog data, not this).

## Lock status (shipped / draft / removed)

`ops.mjs` reads `<folder>/content/items.lock.json` directly as plain JSON —
never through `items_ops.py` — and derives, per item currently in the
catalog: `shipped` if the id is in `def_ids`, `removed` if the id is a key in
`removed` (checked in that order — the same id in *both* is the legal-but-
flagged `removed-def-restored` case), else `draft`. This three-state
derivation is **viewer-only**: `items_ops.py` has no `lock --json` command
today (the T0316 card names it as the parity-restoring follow-up). Until it
ships, an agent driving `items_ops.py` directly has no equivalent of this
status map — only the web page shows it.

## Content errors vs. validate degradation

Two different things can go wrong with a catalog, and the response tells them
apart (spec §3):

- **`content_error`** (top-level, 200) — the game's OWN `items.json` or
  `item_fields.schema.json` is broken (malformed JSON, missing file). `list`
  or `schema` exited 2. Whichever of the two still parsed keeps rendering
  (they read different files, independently).
- **`validate.available: false`** (200) — `validate` itself couldn't run
  (broken/missing `state/items.schema.json`, or an explicit `--baseline` miss
  — narrower than `content_error`, spec §8). Items/schema can still be fully
  fine and rendering.
- **`validate.ok: false`** (200) — `validate` ran fine and found real issues
  (exit 1). Not a degradation; this is the normal "catalog has problems" case,
  surfaced as errors/warnings on the owning cards or in the Summary/Removed
  section.
- **500** — the only case that's an actual viewer bug: `py` could not be
  spawned at all, or printed unparseable JSON on an otherwise-successful
  exit.
