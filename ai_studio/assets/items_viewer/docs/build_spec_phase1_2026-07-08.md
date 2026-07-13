# Items Viewer — Phase 1 build spec (read-only catalog viewer)

Task T0316 phase 1 of 3. Author: deep-reasoner, 2026-07-08. Status: spec, review pending.
Module owns its own docs (precedent: `ai_studio/assets/canvas/docs/`).

Sources this spec sits on: taskboard `ai_studio/taskboard/items/active/T0316-ai-studio-content-editor.md`;
current items contract `features/items-core/README.md`; op-layer
`features/items-core/scripts/items_ops.py`; tool-parity precedent `ai_studio/assets/canvas/{ops.mjs,api.mjs,cli.mjs,site/}`;
host `ai_studio/studio_shell/server.mjs` + `ai_studio/studio_shell/README.md` ("Surface Rule").

## 1. Goal / scope / non-scope

Goal: a read-only web surface that shows the item catalog of any registered game or template —
"see the items" — the cheapest, most valuable slice of the future editor:
value first, universality by schema, never a second data model.

In scope (phase 1):
- Pick a catalog (dropdown over merged game + template registries).
- Render the catalog: core fields, block badges (equip/use/currency), item_kinds, containers, currencies inline.
- Show lock status per item (shipped / draft / removed-with-receipt) and `validate` issues per item + a catalog summary.
- Schema-driven presentation (field roster + section order from `schema --json`; labels humanized from keys — the schema carries no labels), not hardcoded template fields.

Hard non-scope (do not build, do not scaffold):
- No writing. No edit forms. No `upsert`/`deprecate` — the write op-layer does not exist yet.
- Phase 2 (core + kinds editing) and phase 3 (block forms) are out.
- `icon-link` (an `icon_asset_id -> file` binding) is a phase-2+ ops command; not invented here (§5).
- No new data model of items.json in JS (decision (а)); no second parser.

## 2. Module architecture + mounting

New module `ai_studio/assets/items_viewer/`, modeled on `assets/canvas/`:

- `ops.mjs` — pure logic, no HTTP. Merges both registries, resolves per-catalog paths, spawns
  `items_ops.py` (list/schema/validate) via `execFile("py", ["-3.12", script, ...])` and reads
  `items.lock.json` directly, folding everything into one catalog view object. All node:test coverage lands here.
  Every path handed to the tool is ABSOLUTE (`join(root, folder, "content", "items.json")`, etc.) and `cwd`
  is the repo root — NEVER rely on `items_ops.py`'s script-relative argparse defaults, which point into
  `features/items-core/content/` (nonexistent) after T0337. `py -3.12` is the tool's own documented
  invocation (`items_ops.py` docstring lines 15-17); see §8 for the honest env caveat.
- `api.mjs` — `createItemsViewerApi(root)` returning an `async (req,res,url) => bool` handler; marshals HTTP <-> ops only (canvas `api.mjs:239` shape).
- `site/` — one bare-ESM page (`items.html` + `items.js` + `items.css`), no framework (canvas `site/canvas.html`).
- `README.md` — module ownership + surface note.
- No `cli.mjs`. Decision (б), LEAN cut: the agent CLI **is** `items_ops.py` (`list`/`validate`/`schema --json`).
  Parity holds by construction — the web client and the agent client read the *same* subprocess. A second
  JS CLI would only re-wrap the Python one. Confirmed explicitly out (§8).

Mounting in `ai_studio/studio_shell/server.mjs` (exact anchors):
1. Import — after line 15 (`import { createCanvasApi } from "../assets/canvas/api.mjs";`):
   `import { createItemsViewerApi } from "../assets/items_viewer/api.mjs";`
2. Handler — after line 29 (`const handleCanvasApi = createCanvasApi(root);`):
   `const handleItemsViewerApi = createItemsViewerApi(root);`
3. Route — in the `/api/` block, beside the canvas branch (lines 154-157):
   `if (url.pathname.startsWith("/api/items-viewer/")) { handleItemsViewerApi(req, res, url); return; }`
4. Surface page — already served by the `/ai_studio/` static catch-all (lines 114-116) at
   `/ai_studio/assets/items_viewer/site/items.html`. Add a friendly short link mirroring canvas
   (`canvasRedirectLocation` lines 65-68 + its use lines 139-144): `/items` -> 302 to that page.
   Add one line to README "Surface Rule" list (lines 68-76): `/items -> Items Viewer surface`.
   The shell owns hosting only; this module owns catalog meaning/contract (Surface Rule).

## 3. HTTP contract: `/api/items-viewer/*`

Two GET endpoints (read-only; no POST/PUT/PATCH in phase 1).

`GET /api/items-viewer/catalogs` — the dropdown list. Merges `listRegisteredTemplates(root)` +
`listRegisteredGames(root)` (`assets/sources/ops.mjs`). Response:
```
{ catalogs: [ { id: "template:template", kind: "template", title: "Template",
                folder: "templates/template", hasItems: true, status: "active" },
              { id: "game:fixture-game", kind: "game", title: "Fixture Game",
                folder: "games/fixture-game", hasItems: false, status: "active" } ] }
```
`id` = `<kind>:<registryId>` (disambiguates a template and a game that share an id; mirrors gallery's
`game:${id}` convention, `assets/gallery/api.mjs:48`). `hasItems` = `existsSync(<folder>/content/items.json)`.
Games with no items appear with `hasItems:false` (decision (д)) — never hidden, never fatal.

`GET /api/items-viewer/catalog?id=<catalogId>` — the whole view for one catalog in one fetch (LEAN: page
renders from a single response). Ops derives paths from the registry folder:
`content/items.json`, `content/item_fields.schema.json`, `content/items.lock.json`,
`state/items.schema.json`, `src/features/items` (canonical invocation, `items_ops.py` docstring lines 13-17).
Response:
```
{ meta: { id, kind, title, folder, hasItems },
  namespace,
  items: [ <list --json item record> ],        // items_ops item_json_record, verbatim
  containers: [ {id,capacity,accept_policy,hidden} ],
  item_kinds: [ {id,label} ],
  schema: { core, blocks, containers, item_kinds, namespace_pattern, ... },  // schema --json, verbatim
  lock: { status_by_id: { "<id>": "shipped"|"draft"|"removed", ... },
          removed: { "<id>": {fragment_version, note?} } },
  validate: { ok, errors:[{rule,id,field,msg}], warnings:[...], available, reason? },
  content_error?: { source: "catalog"|"schema"|"state_schema", stderr } }
```
`lock.status_by_id`: `shipped` if id in `def_ids`, `removed` if key in `removed`, else `draft` (in catalog,
not yet in `def_ids`). Ops reads `items.lock.json` as plain JSON — a separate artifact, not a second model of
items (decision (а) unbroken; the three-state derivation is viewer-only — see §8 parity). `--baseline
<folder>/content/items.lock.json` is passed to `validate` ONLY when that file exists; if it does not, the flag
is OMITTED so `validate` still runs and emits its own `rename-guard-skipped` warning (lock checks skipped).
Never always-omit: a clean template DOES ship a lock, and always-omitting would fake that warning. The tool's
own default `--baseline` cannot rescue us — `DEFAULT_LOCK` points into `features/items-core/content/`
(nonexistent) after T0337.

Codes / degradation (matched to the tool's real exit behavior, `items_ops.py:112-118, 608-613, 632-634`):
- The page ALWAYS calls `/catalog` for the selected id (single code path). `hasItems:false` is a valid response
  shape (empty state), NOT a reason to short-circuit the fetch from a dropdown flag.
- 200 with `hasItems:false` when the game has no `content/items.json` (decision (д)) — not an error.
- 404 when `id` is not in either registry.
- CONTENT-INVALID (the game's own data is broken) is exit 2 from `list`/`schema`, surfaced as a VISIBLE
  content error, not a 500: a missing/malformed `items.json`, `item_fields.schema.json`, or
  `state/items.schema.json` (`load_json_or_die`, `:112-118` / `cmd_validate` `:632-634`), or an EXPLICIT
  `--baseline` path that does not exist (`resolve_baseline` `:608-613`). Ops returns 200 with
  `content_error:{source,stderr}` and renders whatever DID parse, so the broken game data is seen on the page
  and fixed — it is the game author's bug, not a viewer failure.
- `validate` runs independently: exit 1 is a validation FAIL, not a crash — its stdout still holds the full
  `{ok:false,errors,warnings}` JSON (the CLI prints it AND returns 1), which ops parses and returns (red on the
  owning card). Exit 2 from `validate` specifically (bad/missing `--state-schema`, or an explicit `--baseline`
  miss) -> `validate:{available:false, reason:<stderr>}` while `items`/`schema` still render. NOTE: a MISSING
  `src/features/items` does NOT fail validate — `lint_display_name_keying` (`items_ops.py:540`) silently skips
  an absent src dir, so it is not an exit-2 trigger.
- TOOL-FAILURE (viewer/env bug, not game data) is the ONLY 500: `py` cannot spawn (interpreter absent, ENOENT),
  or stdout is unparseable JSON on an otherwise-successful exit.

## 4. UI (one page, bare ESM)

Layout, top to bottom: shared studio sidebar (copy the `<aside class="studio-sidebar">` block + link
`studio_shell.css`/`.js`, like `canvas.html:7-40`; add an "Items" nav item) -> catalog dropdown ->
validate summary panel -> Removed/lock section -> card grid.

- Dropdown: from `/catalogs`. A `hasItems:false` entry is selectable and renders an honest empty state
  ("items not connected for this game") — the neutral fixture-game case. If the response carries `content_error`,
  show it prominently at the top ("catalog broken: <stderr>") — the game's own data is invalid and must be seen.
- Summary panel: `validate.ok`/`available` badge, error/warning counts, `namespace`, container list
  (id / capacity / accept_policy / hidden). Catalog-level and homeless issues live here (routing rule below).
- **Removed / lock section** (fixes the homeless-receipt gap): iterate `lock.removed`. Each entry has NO
  catalog item, so it can never be a card — render `id` + `fragment_version` + the human `note` receipt here.
  This is where "removed-with-receipt" status is actually visible, and where the `removed-without-reaction`
  family of lock errors lands (see routing).
- **Issue routing rule** (load-bearing): an issue (error or warning) attaches to a card IFF its `id` is present
  in `items[]`; otherwise it routes to the summary/Removed section. The whole `removed-without-reaction` /
  `removed-version-not-shipped` / `lock-invalid` family (`items_ops.py:331-333` and neighbors) keys on an id
  DELETED from the catalog, and catalog-level rules (`generator-check`, `rename-guard-skipped`) carry `id:null`
  — none of these have a card to live on, so §4-as-first-drafted made "removed-with-receipt" status and its red
  unreachable.

Card anatomy — split explicitly to kill the "designer order vs schema order" contradiction:
- **Promoted chrome, hand-rendered** at fixed positions (NOT schema-iterated): icon slot (§5),
  `display_name` as the card title, `id` as subtitle, kind chip (label resolved from `item_kinds`).
- **Generic rows, schema-iterated:** every OTHER key in `schema.core`, in schema order, as label/value rows.
  Per-type render rules: `object` -> recurse into its `.fields` (e.g. `use.params`);
  `list<string>` -> join (tags); `i64`/`string`/`bool` -> scalar; `enum` -> the value (e.g. container
  `accept_policy`). Values come from the `list --json` record; a schema key absent from the record renders as
  an em-dash.
- **Block badges:** from `item.blocks[]`; expand each with fields iterated from `schema.blocks.<b>.fields`
  (same per-type rules), values from `item.equip|use|currency`.
- **Lock chip:** shipped / draft (removed items have no card — they live in the Removed section).
- **Per-item issues:** `validate.errors`/`warnings` whose `id` is this item -> red / yellow on the card.
- Currencies render inline in the same grid (lead decision), tagged as currency.

Labels: `item_fields.schema.json` fields carry ONLY `{type, required}` — the schema has NO labels. The
optional `ui:{}` namespace is deferred and absent from the data today. Phase-1 label = humanize the key
(`display_name` -> "Display name", `base_value` -> "Base value"). Real labels arrive with `ui:{}`; the
humanizer is the single seam to swap then.

Degradation: if `schema` came back as `content_error` (schema exit 2), the grid still renders by iterating the
record's OWN keys directly (or shows "schema unavailable") — the page is never dead just because the schema
failed to load.

No build step, no bundler: `<script type="module" src="items.js">`, `fetch` the two endpoints. The page always
issues `/catalog` for the selected id (no dropdown-flag short-circuit).

## 5. Icons — decision: honest placeholder only (no resolve in phase 1)

Decision: render a placeholder showing the `icon_asset_id` text + a "no icon" glyph. Do **not** run the
gallery search in phase 1.

Rationale (deviates from decision (в)'s "best-effort resolve + placeholder" toward its own LEAN escape hatch
"maybe search is unnecessary in phase 1"): (1) miss rate is 100% today — no asset exists in the repo for any
`icon_asset_id`, and no `icon_asset_id -> file` mechanism exists (fact #4). (2) Wiring `searchAssets`
(`assets/catalog/search.mjs`) means an SQLite-indexed async fan-out per card for a guaranteed-empty
result — cost with zero payoff. (3) The real binding is `icon-link`, a phase-2+ ops command — resolving by
loose id/tag match now would invent an ad-hoc binding the design explicitly defers.

Keep the seam one function wide: `resolveIcon(assetId) -> null` (phase 1). Phase 2 wires it to `icon-link`
output; the page already renders "resolved img OR placeholder", so nothing else changes.

## 6. Gates

- `node:test` on `ops.mjs` (the contract lives here): registry merge (both sources, `hasItems` flag);
  parse of `list`/`schema`/`validate --json`; lock `status_by_id` mapping (shipped/draft/removed) + issue
  routing (in-catalog id -> card bucket, deleted/`null` id -> summary bucket); the THREE-branch exit handling —
  validate exit 1 parsed from stdout as `validate.ok:false` (not thrown), validate exit 2 -> `available:false`,
  list/schema exit 2 -> `content_error`, ENOENT/unparseable-stdout -> throw (500); conditional `--baseline`
  (present lock passed, absent lock omitted -> `rename-guard-skipped` warning); game folder with no
  `content/items.json` -> `hasItems:false`.
  Fixtures: the **live template** is the happy-path fixture (committed, stable, 6 items). Temp fixtures cover the
  failure branches; the CONTENT exit-2 branch is triggered by a MALFORMED `items.json` /
  `item_fields.schema.json` / `state/items.schema.json`, or a missing EXPLICIT `--baseline` — NOT by removing
  `src/features/items` (an absent src dir is silently skipped, `items_ops.py:540`, and never fails validate).
- Smoke: boot the server, `GET /api/items-viewer/catalog?id=template:template` returns 6 items and
  `validate.ok:true`. (Run via `start_site_windows.ps1`; `py -3.12` resolvable.)
- Manual lead acceptance in the browser (count-up precedent: judged by eye). **No** screenshot-SHA gate.

## 7. Slicing (3 commits)

1. `ops.mjs` + its `node:test` — subprocess wrappers, registry merge, lock read, list/schema/validate
   parse + degradation. Headless, no HTTP, no UI. Independently reviewable/green.
2. `api.mjs` + `server.mjs` mount (import + handler + route + `/items` redirect + README surface line) +
   the smoke test.
3. `site/` page (`items.html`/`items.js`/`items.css`) + module `README.md`; manual lead acceptance.

## 8. Risks / LEAN cuts (conscious NOs)

- **No CLI client** — the agent client is `items_ops.py`. Parity BY CONSTRUCTION holds for the THREE
  subprocess commands (`list`/`schema`/`validate --json`): the web client and the agent read the same tool.
  NARROWER than the first draft: the three-state `shipped/draft/removed` status is a viewer-only JS derivative
  of a direct `items.lock.json` read — the agent has NO equivalent op today. The parity-restoring follow-up is
  `items_ops lock --json` (named in the T0316 card); until it ships, the derivation stays viewer-only. Decision
  (а) is NOT broken: the lock is a separate artifact, not a second model of items.
- **No icon resolve** — placeholder only; `icon-link` is phase 2+ (§5).
- **No caching** — 6-item catalogs, ≤3 short `py` spawns per load (~sub-second). Skip caching entirely;
  add an mtime check later only if it ever feels slow. (Decision (а) invited this.)
- **`item_json_record` block-field curation gap (flag, do not fix here).** `list --json`
  (`items_ops.py:130-157`) hardcodes block extraction: `equip -> {slot}`, `use -> {effect_id,params}`,
  `currency -> {hud_hint,cap}`. The template schema declares exactly those fields, so schema-driven
  rendering is faithful for phase 1. But a *future* game whose schema adds equip stat fields
  (strength, etc.) would have those fields dropped by `list --json` — the page would show the
  schema-declared field with an empty value. Trigger to close it (extend `item_json_record` to pass
  through full block objects) is exactly when the second game's schema introduces block fields beyond the
  core set — which is the phase-2 editing milestone. Not changed here: `items_ops.py` is the feature's
  file (T0327), outside this task's edit scope.
- **`validate` degradation is narrow, not broad.** Only a missing/broken `state/items.schema.json` (or an
  explicit missing `--baseline`) turns `validate` off (`available:false`); a missing `src/features/items` does
  NOT (its lint silently skips, `items_ops.py:540`). A `validate` exit 1 is a real FAIL surfaced with issues,
  not a degradation.
- **`py -3.12` resolution — honest caveat.** We call `py -3.12` because that is `items_ops.py`'s OWN documented
  invocation (docstring lines 15-17). This is NOT the mechanism the rest of the pipeline uses: node asset tools
  spawn an absolute venv python from studio config, and `ctest` uses `${Python3_EXECUTABLE}` — nothing else
  spawns a bare `py` from node. Acceptable for a local single-user studio tool; the clean upgrade is to read a
  configured interpreter (`AI_PIPELINE_PYTHON` / studio `pythonPath`) instead of a bare launcher, kept as a
  one-line seam in ops.

Reassurances (checked, so NOT risks):
- `items_ops.py --json` prints ASCII (`json.dumps` default `ensure_ascii=True`), so the narrow-codepage
  (cp1251) console concern does not apply when we read stdout through a pipe — the bytes are pure ASCII.
  A 1MB `execFile` `maxBuffer` is ample for these tiny `--json` payloads.
- Read-only tool: no writes anywhere -> no concurrency, no project lock, no `withProjectLock` analog needed
  (unlike canvas).

Security / boundaries (decision (ж)): read-only FS, no writes anywhere. Catalog id -> registry lookup ->
folder confined under repo root via the server's `safeResolve` pattern (`server.mjs:56-61`). `py` is spawned
with a fixed `execFile` argv array (no shell, no interpolation) and ABSOLUTE paths under a registered
game/template folder, `cwd` = repo root. `external/neotolis-engine` untouched; canvas project (T0267, paused)
read as a pattern only, never modified.
