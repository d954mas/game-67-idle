# Items Viewer — Slice 1 UI build spec (2026-07-08)

Round 1.5 of T0316. Rework the items surface from a card wall into the Canvas
two-pane shell: toolbar + dense sortable master TABLE + read-only schema-driven
INSPECTOR, with Items / Containers / Kinds tabs and client-side search/filters.
**Read-only, client-only.** No editing, no write op, no server/ops/api change.

Authoritative inputs this spec obeys:
- `tmp/items_viewer_ux_review_2026-07-08.md` (two-report UX review: S1-S7 critique,
  target wireframe, Canvas reuse map with canvas.css line refs). Followed; every
  deviation is stated inline with justification.
- `ai_studio/taskboard/items/active/T0316-ai-studio-content-editor.md`, the "Решения
  лида 2026-07-08 (после приёмки фазы 1; UX-разбор...)" block (ratified "делай").
- Fresh data model (commit `af8c75f3f`): `stack` is one authored int
  (`0`=unlimited, `1`=unique/instance, `N>1`=cap); `list --json` emits it raw;
  schema v2 `core.stack.type=="i64"` + a `note`.

---

## 1. Goal / Non-goals

**Goal.** Make the surface usable at 6 and at 300 items (fixes review S1-S4, S6):
scan/find/filter/sort in a dense table; read the full record in a schema-driven
inspector; Containers and Kinds as sibling read-only tabs; Summary demoted to a
thin health strip; noise (absent optionals, `created` in the row) killed. Adopt
Canvas's UI/UX idiom by reusing its patterns/classes.

**Non-goals (Slice 1).** No editing surface of any kind: no inputs that write, no
dirty state, no Review-changes/diff panel, no toasts, no Save, no destructive
confirm, no add-new. No `ops.mjs`/`api.mjs`/`icon_preview.mjs`/`items_ops.py`
change. No validation logic in JS (issues come from `view.validate`). No new
dependency, no build step, no framework. No windowing/virtualization (one design
for 6 and 300; render all rows). **A gallery/card view toggle is an explicit
non-goal** — the table REPLACES the card grid (review verdict); a toggle is
deferred to a later slice and is not "nearly free" (it would need a second
renderer + a view-state axis).

Reuse-map rows that belong to the EDIT slices (toasts / `#history-panel` diff /
`.chat-clear-confirm` / `.new-card` / `.inline-input`) are **out of scope here** —
do not pull them in.

## 2. Client-only confirmation (no server change)

The `/api/items-viewer/catalog` view object already carries everything the
two-pane UI needs — verified in `ops.mjs` `loadCatalogView` return (ops.mjs:229-243)
and `list --json` record shape (`items_ops.py:130-183`):

- `items[]` — each: `id, display_name, icon_asset_id, kind, created, tags[],
  base_value, stack` (raw int) `, blocks[]` (present block names), and optional
  `equip{slot}` / `use{effect_id,params}` / `currency{hud_hint,cap}`.
- `containers[]` (`id, capacity, accept_policy, hidden`), `item_kinds[]`
  (`id, label`), `namespace`, `schema` (`{core, blocks, ...}` or `null`),
  `lock.status_by_id` + `lock.removed`, `validate.{available,ok,errors,warnings,reason}`,
  `icons.{page_data_uri, regions, reason}`, `meta.hasItems`, optional `content_error`.

The `/items` 302 redirect preserves the query string (server.mjs:74-76), so all
deep-link params (below) survive it. **Therefore Slice 1 touches only
`site/{items.html,items.js,items.css}` + `README.md`.** `ops.mjs`, `api.mjs`,
`icon_preview.mjs`, `items_ops.py`, and both test files stay byte-identical.

## 3. Target DOM structure (concrete)

Keep the studio shell chrome exactly as today (`<div class="studio-shell">` +
`<aside class="studio-sidebar">` with the same nav; `is-active` on the Items item).
The page still links **only** `studio_shell.css` + `items.css` and loads
`studio_shell.js` + `items.js` (module). **Do not link `canvas.css`** — it would
drag Canvas's `:root` vars and `html,body` font/height rules onto the page (§5).

`<main class="studio-content items-content">` becomes a full-height flex column
(§5). Its children:

```
main.studio-content.items-content
├─ div.iv-toolbar                         (#top-bar idiom: canvas.css:220-253)
│  ├─ div.iv-catalog  ( <label for=catalogSelect> + <select id=catalogSelect> )
│  ├─ div.iv-tabs[role=tablist]           (.insp-segmented: canvas.css:1343-1362)
│  │  └─ button.iv-tab[data-tab=items|containers|kinds][role=tab]  (.insp-seg-btn)
│  ├─ div.iv-spacer                       (#top-bar .spacer: canvas.css:244-246)
│  └─ div#healthStrip.iv-health           (validate badge + namespace + counts + issues rollup)
├─ div#catalogIssues.iv-catalog-issues.is-hidden   (catalog-level id:null issues; §8)
├─ div#topBanner.iv-top-banner.is-hidden[role=status]  (content_error / "Items not connected")
├─ div#filterRow.iv-filters               (Items tab ONLY; hidden on other tabs)
│  ├─ input#searchInput.iv-search[type=search]
│  ├─ select#kindFilter        (All + view.item_kinds)
│  ├─ select#statusFilter       (All + lock statuses present in data)
│  ├─ label.iv-toggle > input#issuesOnly[type=checkbox] + "Issues only"
│  └─ span#filterCount.iv-filter-count      ("N of M")
└─ div.iv-body                            (#ws-body flex: canvas.css:257-261)
   ├─ div.iv-master                       (scroll column; #layers-panel region idiom: canvas.css:290-298)
   │  ├─ table.iv-table.iv-master-table#masterTable   (extends local .iv-table: items.css:142-160)
   │  │  ├─ thead > tr > th.iv-th(.sortable)[data-sort][aria-sort]
   │  │  └─ tbody#masterBody > tr.iv-row[data-id][tabindex=-1]
   │  ├─ div#noMatch.iv-empty.is-hidden    ("No items match the filters")
   │  └─ section#removedSection.iv-removed.is-hidden   (Items tab only, below table; §8)
   └─ aside#inspector.iv-inspector        (#inspector idiom: canvas.css:845-853, scrollbar 1799-1831)
      └─ (either .insp-group sections OR .insp-nothing empty state)   (canvas.css:855-922)
```

**Master row** (Items tab):
```
tr.iv-row[data-id="tmpl.sword"][tabindex=-1]
  td.iv-col-icon   > span.iv-thumb  ( <canvas> crop, or "?" placeholder — renderIconSlot, ~24px .thumb: canvas.css:461-468 )
  td.iv-col-id     ( monospace )
  td.iv-col-name
  td.iv-col-kind   ( item_kinds label, fallback raw id )
  td.iv-col-value.num
  td.iv-col-stack.num  ( glyph, §7 )
  td.iv-col-status > span.iv-chip.iv-chip-lock.iv-chip-lock-<status>   (.iv-chip base items.css:328-338; per-status color items.css:341-343; .iv-chip-lock is an unstyled markup hook)
  td.iv-col-issue  > span.iv-issue-dot.iv-issue-dot-<error|warning>   (or empty)
```
`tr.iv-row.selected` → `background:#24405f` (Canvas `.layer-row.selected`,
canvas.css:454-456). Hover → `--studio-surface-soft`.

**Containers tab** — full-width `table.iv-table` (no inspector), columns
`id · capacity · accept_policy · hidden` (`capacity==0` → "unlimited"; `hidden`
→ "yes"/"no"). This is the existing Summary containers table, relocated verbatim.

**Kinds tab** — full-width `table.iv-table`, columns `id · label · items`, where
`items` = count of catalog items whose `kind === id` (cheap derived rollup; makes
the tab worth visiting). A table, not a bare list — same idiom as the other two.

**Inspector** (Items tab only; hidden on Containers/Kinds so their table goes
full width). Header (`.insp-name` idiom) = larger icon (~44px, same renderIconSlot
path) + `display_name` + `id` (mono) + kind chip + lock chip. Then collapsible
`.insp-group` sections (canvas.css:861-922), each a header button with chevron:
- **Core** — schema.core fields **excluding `CHROME_KEYS ∪ {stack}`**, hide-absent-optionals.
  `CHROME_KEYS` (items.js:143) = `{id, display_name, icon_asset_id, kind, blocks}`
  (all shown in the header) and does NOT contain `stack`; add `stack` to the Core
  exclusion set explicitly, or a literal `schema.core` iteration would render
  `stack` as a Core row AND again in the Stack section (double-render bug). So Core
  = `{created, tags, base_value}` + any game-specific extra core scalars.
- **Stack** — the `stack` value + its read-only gloss (§7); the SOLE renderer of
  `stack`. (Own section per scope + review wireframe; the future segmented control
  lands in the EDIT slice.)
- one section **per present block** (Equip / Use / Currency), from `item.blocks[]`,
  rendered by the ported `renderBlock` (schema.blocks[name].fields, generic).
- **Lock / issues** — lock status + this item's routed validate issues (full
  `[rule] msg` lines with field, `.insp-pack-error`/`.insp-pack-banner` idiom:
  canvas.css:2171-2185), or "no issues". **Per-field inline issue marks are absent
  by design in this slice**: issues render only as the `[rule] msg` list in this
  section (many `issue.field` values don't map to a rendered read-only row anyway,
  e.g. `id`/`kind` live in the header). Field-anchored red marks arrive with the
  editable inspector (Slice 2).
- Empty state when nothing selected: `.insp-nothing` "Select an item" (canvas.css:855-859).

## 4. CSS reuse & token-remap strategy (the decision the reuse map leaves open)

**Copy the Canvas *patterns* into `items.css` under `iv-` classes, remapped to the
`--studio-*` tokens the page already uses. Do NOT import canvas.css and do NOT
edit canvas.css/canvas.html.** Rationale: the two files use different token sets
(`canvas.css :root` `--panel/--line/...` vs `studio_shell.css` `--studio-*`), and
linking canvas.css would also import its `html,body`/`.studio-content.canvas-content`
rules. So lift the *structural* CSS (flex/grid/padding/radius/sizing/selection
accent) and substitute colors:

| canvas token | items token |
|---|---|
| `--panel` | `--studio-surface` |
| `--panel-soft` | `--studio-surface-soft` |
| `--line` | `--studio-line` |
| `--text` | `--studio-text` |
| `--muted` | `--studio-muted` |
| `--blue` | `--studio-blue` |
| `--cyan`/`--green` | `--studio-teal` |
| selection `#24405f` | keep literal `#24405f` (raw hex in Canvas too; reads well on the studio bg) |

Reuse the **existing** local classes as-is (already `--studio-*`): `.iv-table`
(items.css:142-160), `.iv-chip` base (328-338) + `.iv-chip-lock-<status>` variants
(341-343; the bare `.iv-chip-lock` is only a markup hook, no own rule), `.iv-badge*`
(100-113), `.iv-issue*` (167-185), `.iv-field*` (121-136), `.iv-icon-*` (261-298),
`.iv-removed-*` (196-226), `.is-hidden` (80-82). Drop the card-grid rules
(`.items-card-grid`, `.iv-card*` 228-373) — the card renderer is replaced.

Full-height app rule (mirrors `main.studio-content.canvas-content`, canvas.css:49-54,
but written locally):
```
main.studio-content.items-content { height:100vh; overflow:hidden; display:flex;
  flex-direction:column; gap:0; }
```
`.iv-body { flex:1; min-height:0; display:flex; }`; `.iv-master { flex:1;
min-width:0; overflow:auto; }` (own scroll, `scrollbar-gutter:stable`);
`#inspector.iv-inspector { width:320px; flex:none; overflow:auto; border-left:1px
solid var(--studio-line); }`. Master table gets `overflow-x:auto` inside
`.iv-master` so it never scrolls the page body.

**Scale mechanics (this is what makes it "usable at 300", not just 6).** Two
required rules, else the claim is hollow:
- **Sticky header**: `.iv-master-table thead th { position:sticky; top:0; z-index:1;
  background:var(--studio-surface); }` (opaque background so scrolled rows don't
  bleed through) — columns + sort carets stay visible while the tbody scrolls
  inside `.iv-master`.
- **Truncation / width stability**: `table-layout:fixed` on `.iv-master-table` (or
  explicit `min/max-width` per column), and the `id` + `name` cells get Canvas's
  own `.layer-name` idiom — `white-space:nowrap; overflow:hidden;
  text-overflow:ellipsis` (canvas.css:489-495) — so a long monospace id can never
  force horizontal scroll. `title` attr on truncated cells for the full value.

Responsive bands (a 320px inspector + 8-column table is broken well above the
shell's 860px collapse):
- **≥ ~1024px**: full two-pane as specified.
- **< ~1024px**: drop the 100vh/overflow lock and go single-column — stack the
  inspector under the master table and let the content pane scroll (shell default).
  (Equivalently the executor may keep the table full-width and hide the inspector
  behind the row selection, but the stacked fallback is simplest.)
Desktop-first tool; keep the fallback thin, just not broken in the mid band.

## 5. Interaction contract

### 5.1 Catalog select
Unchanged data path: `loadCatalogs()` fills the dropdown; selecting one calls
`loadCatalog(id)` → `/api/items-viewer/catalog?id=<kind>:<id>`. On catalog change
reset selection to the sort's first row (or `?item` if still valid — but ids
differ per catalog, so effectively first row), reset filters to defaults, keep the
current tab and sort column/dir (the fixed column spine is identical across
catalogs). Decode `icons.page_data_uri` once before render (keep existing logic).

### 5.2 Tabs
`[Items] [Containers] [Kinds]`, single-select segmented control. Switching tabs
swaps the master body and shows/hides `#filterRow`, `#removedSection`, and
`#inspector` (Items only). No refetch.

### 5.3 Search + filters (Items tab, client-side over the already-fetched view)
- **search**: case-insensitive substring over `id` + `display_name`.
- **kind**: `All` or one `item_kinds[].id`.
- **status**: `All` or one lock status **present in the data** (derive options
  from the distinct values in `lock.status_by_id`; don't offer `removed` if none).
- **issues only**: checkbox; keep rows that have ≥1 routed validate issue.
- **Combination = AND** across the four (intersection); each individual filter is a
  single choice (no OR within). This is the expected "narrow it down" model
  (Airtable/CastleDB). `#filterCount` shows "N of M" (filtered / total).
- 0 matches → show `#noMatch` ("No items match the filters"); distinct from the
  catalog-empty / not-connected message.

### 5.4 Sort
Click a sortable header to sort; click the active header again to flip direction.
**Default `id` ascending.** Sortable columns: `id, display_name, kind, base_value,
stack, status` (chrome `icon` and `issue` columns are NOT sortable). Comparators:
- string columns (`id, display_name, kind`, `status`) → `localeCompare`,
  case-insensitive-ish via `localeCompare` default; use the kind **label** text for
  the kind column so the visible order matches the cells.
- numeric columns (`base_value, stack`) → numeric compare on the raw value. `stack`
  sorts by the stored int (`0` unlimited sorts as `0`); a "semantic infinity" sort
  is a deferred nicety — documented, predictable.
- **Tie-break: `id` ascending** (stable), always.
Active header shows a caret (`▲` asc / `▼` desc) and sets `aria-sort`.

### 5.5 Selection
Click a row → `state.selectedId = data-id` → inspector renders that item; set
`.selected` + `aria-selected="true"` on the row (cheap AT win), cleared on all
others. Selection is by id and persists across sort/filter changes. **If the
selected id is not in the visible (filtered) set**: no row carries
`.selected`/`aria-selected`, and keyboard nav treats it as no anchor (§5.6). The
inspector keeps the last-selected item's read-only detail (valid data; picking a
visible row replaces it). On load: select `?item` if it resolves, else the first
row of the current sort/filter, else `null` → empty state. (Auto first-row
selection so the inspector is populated on open, not a dead pane; empty state then
only appears for a zero-item or not-connected catalog.)

### 5.6 Keyboard nav (in scope, minimal)
When a `.iv-row` is focused/selected on the Items tab: `ArrowDown`/`ArrowUp` move
selection to the next/previous **visible** (filtered+sorted) row and `scrollIntoView`
(no wrap). If nothing is selected **or the selected id is not in the visible set**,
`ArrowDown` selects the first visible row (`ArrowUp` the last). `Enter` on a focused
row is a no-op (selection already follows focus). No Home/End, no type-ahead —
bounded. ~15 lines; a real win for a keyboard-driven lead. **Optional (not
mandated)**: wrap sortable `th` in a `<button>` (or `tabindex=0`+Enter/Space) for
keyboard-activated sort — leave to the executor's judgment.

### 5.7 Deep-link / view state in URL query
All view state lives in the URL query, updated via `history.replaceState` (no
history spam, no reload) and read on initial load; survives the `/items` 302:
- `cat=<kind>:<id>` (selected catalog), `tab=items|containers|kinds`,
  `item=<id>` (Items selection), `q=<search>`, `kind=<id>`, `status=<status>`,
  `issues=1`.
Worth it: the plumbing is shared across params, it makes every acceptance
screenshot a pure URL (deterministic, reproducible — see §11), and it makes any
filtered/selected view shareable. Unknown/stale params are ignored defensively
(e.g. `item` not in catalog → fall back to first row).

## 6. Rendering rules — hide-absent-optionals (kills S3)

Replace the old "print every schema key" rule (which regenerated em-dash noise)
with, for BOTH table cells and inspector rows:
- **Required field** (`spec.required === true`): always render. If the value is
  absent, show `"—"` (em-dash) / "(missing)" — a missing required field is a
  validation signal worth showing. (Won't happen for the guaranteed core spine.)
- **Optional field** (`required !== true`): render the row ONLY if the value is
  present and non-empty (not `undefined`/`null`; for `list<string>` not `[]`; for
  `object` not `{}`). Otherwise omit the row entirely — no em-dash noise.
This predicate lives in the section/field renderers; `humanize(key)` stays the
single label seam (swap when a `ui:{}` namespace ships real labels).

## 7. Rendering rules table (field type → table cell / inspector row)

| field (type) | table cell | inspector |
|---|---|---|
| `id` (string, req) | `.iv-col-id`, monospace | header id line (mono) |
| `display_name` (string, req) | `.iv-col-name` | header title |
| `icon_asset_id` (string, req) | drives `.iv-col-icon` (~24px crop via `renderIconSlot`, or "?" + reason) | header icon (~44px, same path) |
| `kind` (string, req) | `.iv-col-kind`, item_kinds **label** (fallback raw id) | header kind chip |
| `base_value` (i64, req) | `.iv-col-value.num` (right-aligned) | Core row |
| `stack` (i64, req) | `.iv-col-stack.num` glyph (below) | **Stack** section: value + gloss |
| `created` (string, req) | **not a column** (demoted) | Core row |
| `tags` (list<string>, opt) | **not a column** | Core row, joined `", "`; omitted if empty (optional) |
| `equip`/`use`/`currency` (block) | **not a column** | one `.insp-group` per present block; `renderBlock` |
| object w/o schema fields (e.g. `use.params`) | — | recurse keys (keep the `[object Object]` guard, items.js:100-105) |
| lock status (derived, not schema) | `.iv-col-status` chip | Lock section |
| validate issues (derived, not schema) | `.iv-col-issue` dot | Lock section (full messages) |

**Icon rendering — `renderIconSlot` gains a size/variant parameter** (it currently
hardcodes a 46px slot and ALWAYS appends `.iv-icon-caption`, items.js:64-85 +
items.css:289-298, which overflows a 24px table cell). Two variants:
- **compact** (table `td.iv-col-icon`, ~24px, `.thumb` sizing canvas.css:461-468):
  crop only, NO caption; the honest degradation reason moves to the slot's `title`
  tooltip. The crop math itself is unchanged — the region rects are already
  server-inset (icon_preview.mjs) and correct.
- **header** (inspector, ~44px): the fuller slot; caption optional.
The `"?"` placeholder + `icons.reason` path is preserved in both (as glyph+tooltip
compact, glyph+caption header). No pixel/crop logic changes.

**Stack glyph** (single authored int): `0` → `"∞"` (∞), `1` → `"—"` (—),
`N>1` → `"×"+N` (×N). Inspector **gloss**: `0` → "Unlimited stack", `1` →
"Unique / not stackable", `N` → "Stacks up to N". (Follows the fresh-data-model
mapping from the task; the review wireframe's separate `≤N` "cap" glyph is
dropped — the unified int has no cap-vs-size distinction to encode, so one glyph
family. Justified deviation.)

**Issue dot** severity: red (`.iv-issue-dot-error`) if the item has ≥1 routed
**error**, else amber (`-warning`) if ≥1 warning, else no dot.

### Schema-driven genericity — preserved (no per-game hardcoding)

- **Table = a fixed INDEX spine** of chrome columns (`icon`, `status`, `issue` —
  viewer concerns from `icons`/`lock`/`validate`, not schema fields) + the
  **guaranteed-required catalog core** scalar columns (`id, display_name, kind,
  base_value, stack` — all `required:true` in the core contract, present in every
  item). Column **headers** come from `humanize(key)` for the schema-spine columns
  (honest, swappable label seam) and fixed strings for chrome columns. Cells read
  the item field **defensively** (a hypothetical game whose schema lacks a spine
  key renders `"—"`, never a crash). This is NOT hardcoding a specific game's
  fields — it is the fixed catalog spine the items feature guarantees; a curated
  index column set is exactly what CastleDB/Airtable do.
- **Inspector = fully schema-driven**, identical genericity to the current card
  renderer: it iterates `schema.core` (minus `CHROME_KEYS ∪ {stack}`, §3) +
  `item.blocks[]` against `schema.blocks[*].fields`, via the ported `renderTypedValue` /
  `renderRawValue` / `renderBlock` / `field` / `humanize`. Any game-specific EXTRA
  core field or block therefore still appears in the inspector — it simply isn't a
  table *column*. No per-game field name, label, or value is hardcoded anywhere.

## 8. Removed section + catalog-level issues placement

- **Catalog-level issues** (`id == null`): render in `#catalogIssues` (a thin strip
  below the toolbar, always visible when present, `.iv-issue*` styling). The health
  strip's issues rollup (§9) links/scrolls to it. These have no owning row.
- **Removed / homeless** (ids in `lock.removed{}` that are NOT current items, plus
  validate issues whose `id` is not in `items[]` — the removed-without-reaction
  family): keep the existing `renderRemovedSection` behavior verbatim, relocated to
  a collapsible `#removedSection` **under the Items table** (Items tab only,
  collapsed by default, shown only when non-empty). Removed ids have no item record,
  so they can't be table rows or a status-filter value — they need their own list.
  Justified over a "removed" filter value for exactly that reason.
- Note the coexisting case: an id present in BOTH `items[]` and `lock.removed`
  (the legal "removed-def-restored" restoration) IS a current row and shows lock
  chip `removed` + its warning inline; the status filter value `removed` surfaces
  those current rows. The `#removedSection` list is only for truly-gone ids. Both
  coexist (mirrors ops.mjs:124-143 + items.js issue routing).
- Reuse the existing inline issue-routing partition in `items.js` (the ~10-line
  vanilla-JS mirror of `routeIssues`, README "Why the site can't import ops.mjs") —
  still cannot import `ops.mjs` (Node-only). Keep it.

## 9. Health strip (replaces the Summary panel)

`#healthStrip` in the toolbar, one thin row:
- validate badge — reuse `.iv-badge` + `.iv-badge-ok`/`-fail`/`-unknown`
  (items.css:100-113) with the existing `badgeClass`/`badgeText` logic.
- namespace. **Guard `view.namespace` absent**: it is `null` in the not-connected
  branch (ops.mjs:179) and the `content_error` branch (ops.mjs:222) — render `"—"`
  or omit the segment, never the literal string "undefined"/"null".
- counts: `N items · N containers · N kinds`.
- **lock-status rollup**: `N shipped · N draft · N removed` (from
  `lock.status_by_id`; omit zero buckets). This is the other half of the ratified
  S6 fix — "filter + rollup" — that the filter (§5.3) alone did not deliver.
- issues rollup: `#issuesRollup` "N issues" (errors+warnings total); when
  catalog-level issues exist it reveals `#catalogIssues` (§8).
`content_error` / `hasItems:false` keep their own `#topBanner` line (existing
`renderTopBanner` logic: "Catalog broken (...)" / "Items not connected for this
game."). The old `renderSummary` containers table moves to the Containers tab (§3).

**Empty new tabs on a not-connected / empty catalog.** `containers` and
`item_kinds` are `[]` (ops.mjs:181-182, the not-connected branch), so the Containers
and Kinds tables render a single empty-state row ("No containers." / "No item
kinds.", `.iv-empty` idiom), never a bare header with no body. The `#topBanner`
still carries the "Items not connected" line; the health strip shows validate
"unavailable", `0 items · 0 containers · 0 kinds`, and no lock rollup.

## 10. File-change list

| file | change |
|---|---|
| `site/items.html` | **rewrite** the `<main>` inner DOM (§3); keep shell chrome, links, script tags. |
| `site/items.js` | **substantial rewrite**: new state (`catalog, tab, sort{col,dir}, filters{q,kind,status,issuesOnly}, selectedId`), tab/table/inspector/health-strip/filters/sort/keyboard/URL-state rendering. **Keep & reuse**: `make`, `field`, `humanize`, `setStatus`, `resolveIcon`, `renderTypedValue`, `renderRawValue`, `renderBlock`, the inline issue-routing partition, `loadCatalog`/`loadCatalogs` fetch + one-time icon decode. **`renderIconSlot` is parameterized, not verbatim** (below). |
| `site/items.css` | **substantial rewrite**: add two-pane/toolbar/tabs/master-table/sortable-header/inspector-group/keyboard-focus rules (Canvas patterns remapped, §4); keep the reused local classes; drop the `.iv-card*`/`.items-card-grid` rules. |
| `README.md` | **update**: Phase-1 boundary → Slice 1 (table+inspector+filters+tabs, Canvas idiom); the reuse+remap decision; ops/api/icon_preview/items_ops unchanged; the issue-routing duplication still holds; the URL deep-link params. |
| `ops.mjs`, `api.mjs`, `icon_preview.mjs`, `items_ops.py`, `tests/*` | **UNTOUCHED** (client-only). |

## 11. Acceptance gates

1. **Node tests green, untouched.** From repo root: `node --test
   ai_studio/assets/items_viewer/tests/` → ops.test.mjs (15) + icon_preview.test.mjs
   (10) all pass, and `git diff --stat` shows zero changes under `tests/`,
   `ops.mjs`, `api.mjs`, `icon_preview.mjs`, `items_ops.py`. (They import the
   Node/ops modules only — no DOM — so a site-only rewrite cannot affect them;
   confirmed.) NOTE: ops.test.mjs:129-132 asserts `view.icons.reason===undefined`
   and that every `icon_asset_id` resolves to a packed region — this REQUIRES the
   template's `native-debug` pack to be built. On a fresh tree that one test fails;
   that is an env precondition, NOT a Slice-1 regression (this slice touches no icon
   code path). Build the template once (`cmake --build .../native-debug`) so the
   suite is fully green, or record the pack-absent reason with the rest green; never
   "fix" it in site code.
2. **Headless-Chrome screenshots**, ~`1500x940`, against the running shell (`node
   ai_studio/studio_shell/server.mjs`, port 8765) + Chrome `--headless=new`
   (per the web-wasm-headless-verify pattern). Use deep-link URLs (§5.7) so each
   shot is deterministic. Save under `tmp/t0316_slice1_ui/`. Shots (each with a
   visual checklist):
   - **A — Items, row selected**
     `/items?cat=template:template&tab=items&item=tmpl.sword`.
     Checklist: dense table, 6 rows; icon column renders (real crop OR "?" — icons
     are orthogonal to this slice, see below); sort caret on `Id` (default asc);
     inspector populated for Iron Sword with Core / Stack / Equip / Lock sections;
     selected row shows the `#24405f` accent; no card grid anywhere; health strip
     shows validate OK + `tmpl` + `6 items · 2 containers · 4 kinds`.
   - **B — Containers tab** `...&tab=containers`.
     Checklist: full-width containers table (backpack cap 20 / purse "unlimited",
     accept_policy, hidden yes/no); no inspector pane; filter row hidden.
   - **C — Filtered** `...&tab=items&q=gold` (prefer `q=` — the search substring is
     data-stable; `kind=<id>` is fine too but the kind id is not test-pinned, so if
     using `kind=` the executor must confirm the live id first).
     Checklist: table shows only the matching subset; `#filterCount` "N of 6";
     health-strip counts still reflect the full catalog; the search/kind/status
     controls reflect the URL state.
   - **D — Degraded / honest empty** `/items?cat=game:fixture-game&tab=items`.
     Checklist: `#topBanner` "Items not connected for this game."; empty master
     (no rows), inspector empty state "Select an item"; validate badge
     "unavailable"; page does not error. (This is the honest-degradation proof for
     a not-connected catalog. Separately: if the template's `native-debug` pack
     is not built, shots A-C legitimately show "?" icon placeholders WITH the
     `icons.reason` caption/tooltip — that is itself the no-pack honest
     degradation; real-pixel icons are the already-accepted icons mini-round's
     gate, not this slice's.)
   - **E — Kinds tab** `/items?cat=template:template&tab=kinds`.
     Checklist: full-width kinds table, 4 rows (Currency/Consumables/Weapons/
     Materials), the derived `items` count column populated; no inspector; filter
     row hidden.
   - **F — Scale (sticky header + truncation)** — inject ~300 items and screenshot
     the Items tab scrolled partway down. Checklist: `thead` stays pinned with sort
     carets visible while the tbody scrolls; long `id`/`name` cells ellipsis-clip;
     the page body itself never gains a horizontal scrollbar. **Deterministic
     method** (pick the cleanest, restore after): preferred = transiently replace
     `templates/template/content/items.json` with a generated ~300-item file (the 6
     base items x ~50, ids suffixed `_NN` to stay namespace-valid, `created` kept),
     take the shot, then `git checkout -- templates/template/content/items.json` and
     confirm `git status` is clean. (validate may go red under the synthetic file —
     irrelevant; this shot proves the table CSS at scale, not validity.) Do NOT add
     a permanent app seam to duplicate rows.
   - Narrow-width shot (mid-band fallback, §4) is **optional**.
3. **Untouched surfaces.** `/` (home), `/taskboard/`, `/asset_viewer/`, `/canvas`
   render unchanged; `git status`/`git diff --stat` shows only
   `site/items.html`, `site/items.js`, `site/items.css`, `README.md` modified
   (plus this spec, already committed by the pipeline). No file outside
   `items_viewer/site` + its README changes.

## 12. Executor constraints

- No commits, no taskboard edits. Evidence (screenshots, test output, `git
  diff --stat`) under `tmp/t0316_slice1_ui/` (not committed).
- Vanilla JS, bare ESM, **no framework / no dependency / no build step**. Match the
  current file style: the `make(tag,class,text)` helper, small pure render
  functions, module-scope `state`/`els`, `document.getElementById` wiring.
- **Source stays ASCII.** The current file already uses one non-ASCII glyph
  (em-dash). For all display glyphs use `\u` escapes so the source bytes are ASCII
  while the rendered text is the symbol: `∞` (∞), `×` (×), `—` (—),
  `▲`/`▼` (▲/▼ sort carets), `●` (● issue dot if used). Comments,
  identifiers, and any log/evidence text are plain ASCII.
- Reuse existing helpers rather than re-derive; do not fork validation into JS.
- Preserve the honest icon degradation reasons and the one-time
  `icons.page_data_uri` decode-before-render.
- **Required structure seams** (named so Slice 2 grows cleanly, not a refactor):
  - `visibleRows(state)` — ONE pure function returning the filtered+sorted item
    array; table render, keyboard nav, and `#filterCount` all read from it (never
    three divergent copies of the filter/sort logic).
  - `renderInspector(item)` — the single function that builds the inspector for one
    item; Slice 2 turns its rows editable in place.
  - a state↔URL read/write pair — e.g. `readStateFromUrl()` (on load) and
    `writeStateToUrl()` (via `replaceState` on any state change), the only two
    places that touch `location.search`.

## 13. Slice plan — ONE executor slice

Ship Slice 1 as a single executor pass. It is one cohesive client-side rewrite of
three files + a README, no server/data/test change, sharing one `state` object and
one render pipeline; splitting (e.g. shell+table vs inspector+filters) would only
add a throwaway intermediate seam over shared state. One pass → deep review.

## 14. Design-call ledger (decisions this spec owns)

- **URL query for view state** (cat/tab/item/q/kind/status/issues), `replaceState`,
  read on load, survives the /items 302 → deterministic screenshots + shareable
  views; unknown params ignored. (Not JS-only: the plumbing is shared and cheap.)
- **Sort default `id` asc**, flip on re-click; tie-break always `id` asc (stable);
  sortable = id/name/kind/value/stack/status; `stack` sorts by raw int.
- **Filter combination = AND**; each filter single-choice; status options derived
  from statuses present; `#filterCount` "N of M".
- **Selection** by id, persists across sort/filter; auto-select first row on load
  (or `?item`); empty state only for zero-item / not-connected.
- **Keyboard nav IN**: Arrow Up/Down over visible rows + scrollIntoView (bounded).
- **Health strip** = validate badge + namespace (guarded vs null) + counts +
  lock-status rollup + issues rollup; catalog-level issues in a strip below the
  toolbar; content_error/not-connected keep `#topBanner`.
- **Kinds tab = table** (id/label/items count), not a bare list.
- **Removed section** lives under the Items table (collapsible), not a filter
  value; catalog-level issues in the toolbar strip.
- **Table columns = fixed chrome + guaranteed-required core spine** (defensive);
  **inspector = fully schema-driven** → genericity preserved, no per-game hardcoding.
- **Stack glyph** 0→∞ / 1→— / N→×N; inspector adds a semantic gloss line.
- **CSS**: copy Canvas patterns as `iv-` classes remapped to `--studio-*`; do not
  link/edit canvas.css.
- **Inspector Items-only**; Containers/Kinds tabs are full-width tables (their
  fields fit the row; no detail pane needed read-only).
- **Gallery/card toggle = deferred non-goal** (not nearly free).

## 15. Open questions

None. All design calls are taken with defaults above. The one environmental caveat
(icons show "?" unless the template `native-debug` pack is built) is not a blocker:
icon pixels are the already-accepted icons mini-round's gate, and the "?" state is
a valid honest-degradation screenshot for this slice.
