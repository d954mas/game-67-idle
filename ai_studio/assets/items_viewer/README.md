# Items Viewer

Read-only web surface that shows the item catalog of any registered game or
template — "see the items", the cheapest, most valuable slice of the future
item editor (T0316 phase 1). Full contract and design rationale:
`docs/build_spec_phase1_2026-07-08.md`. Icon previews (mini-round, real
`icons/<name>` art for `templates/template`): `docs/build_spec_icons_2026-07-08.md`.

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
- **`icon-link` write command.** `icon_asset_id -> file` binding is authored
  directly in `content/items.json` (slash form, `icons/<name>`) and resolved
  by reading the BUILT pack (see "Icon preview" below) — there is still no
  write op-layer command to author that binding from the viewer itself; that
  remains a phase-2+ `items_ops` command (spec §5 of the phase-1 doc).
- **A CLI client.** `items_ops.py` (`list` / `validate` / `schema --json`) IS
  the agent client — parity with the web client holds by construction because
  both read the exact same subprocess. There is no `cli.mjs` in this module
  (decision (б); confirmed out in spec §8).
- **Caching.** Every `/catalog` request re-spawns `py -3.12 items_ops.py`
  (list/schema/validate — up to 3 short spawns). Catalogs are tiny (a handful
  of items); add an mtime cache only if this ever measurably feels slow.

## Layout

- `ops.mjs` — pure logic, no HTTP. Merges `listRegisteredTemplates`
  (`assets/backlog/storage/sources/`) and workspace game mounts
  (`ai_studio/workspace/games.mjs`) into the dropdown list. Public games are
  visible by default; private game catalogs require explicit `include-private`
  or direct `game:<id>` selection and pass the private game preflight before
  exposure. It then spawns `items_ops.py` (`execFile("py", ["-3.12", ...])`,
  cwd = repo root, every path ABSOLUTE) for `list`/`schema`/`validate --json`;
  reads `items.lock.json` directly (a separate artifact — see "Lock status"
  below); folds everything into one catalog view object. `loadCatalogView(root,
  folderAbs, meta)` is decoupled from the registry lookup so tests can point
  it at a throwaway temp folder. All `node:test` coverage lives in `tests/`.
- `api.mjs` — `createItemsViewerApi(root)`, an `async (req,res,url) => bool`
  handler mounted by Studio Shell on `/api/items-viewer/` (mirrors
  `assets/canvas/api.mjs`'s shape — HTTP <-> ops marshalling only, no items
  logic). Two read-only GET endpoints; see the spec §3 for the full response
  shapes and exit-code -> HTTP-status mapping.
  - `GET /api/items-viewer/catalogs` — the dropdown list; add
    `?include-private=true` only when private mounted games should be listed.
  - `GET /api/items-viewer/catalog?id=<kind>:<id>` — the whole view for one
    catalog in one fetch.
- `site/` — one page (`items.html` + `items.js` + `items.css`), bare ESM, no
  framework, no build step. Studio Shell also serves a short `/items` route
  (query string preserved) that 302-redirects to
  `/ai_studio/assets/items_viewer/site/items.html`; see
  `ai_studio/studio_shell/server.mjs`.
- `icon_preview.mjs` — pure, no HTTP/subprocess: parses a BUILT
  `game.ntpack` + its `<atlas>_pageN.png` debug image + the generated
  `game_assets.h` into `view.icons` (`{page_data_uri, page_w, page_h, regions,
  reason?}`). See "Icon preview" below.
- `tests/` — `node:test` suite for `ops.mjs` (`ops.test.mjs`) and
  `icon_preview.mjs` (`icon_preview.test.mjs`). The live template
  (`templates/template/content/items.json`, 6 items) is the committed
  happy-path fixture for `ops.mjs`; every failure branch (malformed
  catalog/schema/state schema, missing `content/`, exit-code branches) builds
  a throwaway temp folder from the template's own valid schema files.
  `icon_preview.test.mjs` uses a REAL two-atlas (`ui`+`icons`) pack captured
  from a native-debug build, committed under `tests/fixtures/icon_pack/`
  (`game.ntpack`, `icons_page0.png`, and a trimmed `game_assets.h.slice`).

## Icon preview

`view.icons` (added to every `loadCatalogView` result by `ops.mjs`, populated
by `icon_preview.mjs`) reads the catalog folder's BUILT asset pack — never a
second parser of `content/items.json`, and never a git-committed binary — to
turn each item's `icon_asset_id` (e.g. `icons/gold`) into a pixel rect on the
atlas builder's debug-PNG page:

- Tries `build/native-debug/pack/` then `build/devapi-debug/pack/` under the
  catalog folder for `game.ntpack` + the generated `src/generated/game_assets.h`
  (both are build products; `game_assets.h` is GITIGNORED and only exists
  after a native build has run at least once).
- Parses `NtPackHeader`/`NtAssetEntry` (`nt_pack_format.h`) to find EVERY
  `asset_type==ATLAS` entry (a template pack has two: `ui` and `icons` — the
  first-entry-only bug this guards against is the main risk called out in the
  build spec), then each atlas's `NtAtlasRegion`/`NtAtlasVertex`
  (`nt_atlas_format.h`) for pixel rects. `name_hash` is a 64-bit hash — read
  with `DataView.getBigUint64`, never `Number()`/`parseInt()` (silent
  precision loss above 2^53).
- Version-asserts `NtPackHeader.version`/`NtAtlasHeader.version` FIRST; a
  format newer than this parser degrades to a `reason` string instead of
  misreading a changed byte layout.
- Every rect gets a 2px inner inset: the builder's debug-PNG draws a 2px
  magenta `{255,0,255,255}` outline at each region's boundary
  (`nt_builder_atlas.c`) — without the inset every crop would show a magenta
  ring.
- The debug PNG is STRAIGHT alpha (copied before the pack encoder's
  premultiply step) — `site/items.js` crops it with a bare
  `ctx.drawImage(...)`, no alpha division. Dividing by alpha here (treating it
  as premultiplied) would burn semi-transparent edges; `tests/icon_preview.test.mjs`
  pins this by asserting `page_data_uri` round-trips the source PNG bytes
  exactly (no server-side pixel processing at all).
- No pack built yet, or the pack exists but the debug-PNG page is missing
  (`debug_png` off) — two DISTINCT `reason` strings, never conflated, so a
  missing icon reads as "why", not just "?".

`site/items.js` decodes `page_data_uri` ONCE per catalog load (before
rendering any card) and every card's icon slot is a small `<canvas>` cropped
from that single decoded image — `resolveIcon(view, assetId)` is the
one-line seam that looks up `view.icons.regions[assetId]`.

Recommended long-term replacement for `icon_preview.mjs` (not built this
round — see `docs/build_spec_icons_2026-07-08.md` §3b/§8): a native studio
tool over the engine's own public runtime atlas reader
(`nt_atlas_find_region`/`nt_atlas_get_region`, `engine/atlas/nt_atlas.h`)
instead of a hand-rolled binary format parser in JS.

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
