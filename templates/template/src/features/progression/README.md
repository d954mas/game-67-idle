# progression — feature reference

`src/features/progression/` — L2 feature (`progression.h` first line:
`// feature-layer: L2`). Depends on the L0 shell AND on the L1 feature `items`
(`#include "features/items/items.h"`) — the ONE allowed feature edge in the
template (`build_spec_t0327_i3_2026-07-07.md` §2.2/§8 grep gates G10/G11):
`manual`/`auto` modes read and spend player currency (gold, xp, ...) through
items' public API (`items_purse`/`items_add`/`items_remove`). `items` NEVER
includes `progression.h` — the edge only ever points down.

## What it is

Named "tracks" (level + xp progress meters) declared as DATA in
`content/progression.json`, compiled to a const table by
`tools/generate_progression_tracks.py` (`progression_tracks.gen.{h,c}`),
mirroring items' content-catalog codegen. Runtime state (level, internal xp)
is one flat save fragment (`state/progression.schema.json`, `--fragment
progression`) — a `tracks: map<string, TrackState>` keyed by `track_id`,
exactly like items' `owned` map. No UI, no DevAPI commands of its own — the
fragment is reachable through the universal `game.state.*` surface the
instant it is registered.

## Three modes (one axis: `mode` in the catalog)

- **`manual`** — xp lives in purse (`currency_def`); `progression_level_up(track,
  reason)` spends `cost(level)` from purse on call. Does NOT tick, does NOT
  emit `progression.levelup` (the caller already knows the result).
- **`auto`** — xp lives in purse; `progression_update()` (the frame tick)
  auto-buys levels while purse can afford it.
- **`threshold`** — xp is an internal accumulator
  (`progression_add_xp(track, n, reason)`); `progression_update()`
  auto-levels while the accumulator covers `cost(level)`.

Both `auto` and `threshold` emit `progression.levelup {track, old_level,
new_level}` from inside `progression_update()`, never from a direct call.

## Curve = baked int64 table (zero float in C)

`content/progression.json` authors ONE curve preset per track — in И3 only
`curve.type: "exp"` is supported (`{base, growth_num, growth_den}`); any
other type is a loud generator `SystemExit` (`table`/`linear`/`poly` are a
deliberate LEAN cut, not silently ignored — add them with their own identity
test when a real game needs them). `tools/generate_progression_tracks.py`
bakes `cost[L] = floor(base * (growth_num/growth_den)**L)` via pure integer
arithmetic (`(base * growth_num**L) // growth_den**L` — FLOOR by
construction, no float-rounding risk) into a `static const int64_t
COST_<TRACK>[]` table at build time. The runtime (`progression.c`) only ever
reads `def->cost[level]` — there is no formula interpreter in C.
`tests/test_progression_curve.c` golden-asserts the demo track's baked
values (`50, 75, 112, 168, ...`).

`on_level_up` (authored per-level currency/xp-cascade rewards) is a real
RUNTIME feature (`progression_emit_t`, `apply_on_level_up`, cascade
resolution with a depth cap) but the codegen does NOT bake it in И3
(LEAN-порез A) — every generated track carries `.on_level_up = NULL,
.on_level_up_count = 0`; declaring `on_level_up` in
`content/progression.json` is a generator `SystemExit`. The runtime path is
exercised ONLY by the hand-written test catalog
(`tests/test_progression_catalog.c`) until a real game needs authored
cascades — bring the generator branch back with its own test at that point.

## HARD caps on the tick (T5 — not optional)

`progression_update()` resolves every auto/threshold track through
`resolve_track()`. Two ways a single frame could hang without a cap: (1) a
self-refunding `auto` track whose `on_level_up` gives back >= its own cost
(infinite `while`); (2) a track-to-track xp cascade (`on_level_up.to_track`)
that feeds back on itself (`A -> B -> A -> ...`). Both are closed by hard
caps (`PROGRESSION_MAX_LEVELUPS_PER_TRACK = 64` per track per frame,
`PROGRESSION_MAX_CASCADE_DEPTH = 8` recursion depth) — hitting either logs
`nt_log_warn` and drops the rest of that track's levels for the frame; the
frame always returns. `tests/test_progression.c` (via the pathological
tracks in `tests/test_progression_catalog.c`) proves both terminate.

## Lazy allocation (§2.1)

A track with no save record reads as level 0 / xp 0
(`progression_level`/`progression_xp_current` on an absent record are 0,
never a crash). A record is allocated ONLY right before the first real
mutation (`level_up`/`add_xp`/`set_level`, or the first level-up inside
`progression_update()`) — a tick over an empty-purse `auto` track or an
empty `threshold` track does NOT create a record. This keeps a fresh save's
`tracks` map empty, matching items' "no gratuitous ownership record"
discipline.

## reason contract (lighter than items)

Every mutation takes `reason` (`verb:subject`), asserted in debug builds
(`progression_reason_check`, no-op in release) — but progression does NOT
pull in items' closed verb list (`features/items/reason_tags.h` is
items-internal, not something progression should reach into). A spend into
purse forwards `reason` straight to `items_remove`/`items_add`, where the
FULL items verb-check already runs — the verb vocabulary lives in exactly
one place.

## `set_level` vs `reset` (Р6) — different primitives

- `progression_set_level(track, level, reason)` — clamps to `[0,
  max_level]`, leaves xp untouched. For a prologue ("start this hero at
  level 5").
- `progression_reset(track, reason)` — level=0 AND internal xp=0 (only
  `threshold` meaningfully has xp; `manual`/`auto` xp is the ignored
  default). Does NOT touch purse — a full currency prestige is game
  composition (`progression_reset` + a separate `items_remove`), not
  something progression does on its own (progression never owns purse
  balances, only reads/spends them).

There is no `level_down` — cut deliberately (build_spec §3 LEAN cut);
`set_level`/`reset` cover every ratified use case.

## State fragment (`state/progression.schema.json`, `--fragment progression`)

`tracks: map<string, TrackState { level: int, xp: i64 }>` — the SAME flat-map
shape as items' `owned`, no per-mode branching in the schema. `level` is
capped at schema max 9999 (`content/progression.json`'s `max_level` must
stay `<= 9999`, enforced by the generator — a higher cap would silently
clamp on save instead of failing the build). `xp` is meaningful ONLY for
`threshold` tracks; `manual`/`auto` tracks carry the ignored default 0 (xp
lives in purse for those two modes — an L2->L1 read, not schema state).

**No hooks** (`on_new_game`/`reconcile` both absent) — `progression` has no
`bootstrap.c`, unlike items. `reset()` alone is the correct "fresh game"
state (empty tracks = level 0 everywhere via the lazy-allocation default
above); a game that wants a strong starting hero calls
`progression_set_level` from its own bootstrap code, not from an
`on_new_game` hook here. An orphaned track record (its `track_id` removed
from `content/progression.json`) is harmless by construction —
`progression_update()` only ever iterates the CATALOG's tracks, never scans
`tracks` for orphans — so no reconcile/quarantine pass is needed (contrast
with items, where an orphaned owned-record represents lost player value and
must be quarantined, not ignored).

**`created`/lock-file: deliberately absent for `content/progression.json`**
(unlike `items.json`/`items.lock.json`). An orphaned items def_id means a
LOST count in the save (destructive, needs a guard); an orphaned track_id is
inert (ignored by `progression_update()`, per the paragraph above) — so
there is no destructive-removal case to guard against in И3. Cost of this
decision: removing a track from the catalog silently forgets the player's
earned levels for it, with no red gate. Accepted for the template fixture; a
real game with prestige/account-transfer across a shipped track roster
should add its own lock+migration workflow before removing a track that has
shipped.

## Content workflow (catalog authoring)

1. Edit `content/progression.json` (`namespace` + `tracks[]`; form mirrors
   `items.json`). `id` is a bare slug (`"hero"`, not `"tmpl.hero"`) — track
   ids are progression-internal, not items-namespaced.
2. Build codegen: `py -3.12 tools/generate_progression_tracks.py --catalog
   content/progression.json --items content/items.json --out-dir <dir>` —
   emits `progression_tracks.gen.{h,c}`. `--items` is the cross-check:
   `currency_def` (manual/auto tracks) must name an existing items def with
   a `currency` block.
3. `curve.type` must be `"exp"`; `mode` one of `manual|auto|threshold`;
   `max_level` in `[1, 9999]`; `on_level_up` must be ABSENT from the JSON
   (see above) — the generator rejects it loudly, not silently.

## Cross-dependency note (see also `items/README.md`)

`currency_def` in `content/progression.json` names a live items def_id.
items' own destructive-removal guard (`items.lock.json`) does not know
about progression — removing a currency that a track still references will
pass the items guard cleanly but break progression codegen loudly
(`SystemExit`: currency_def not found / not a currency). Keep
`content/progression.json` in sync when retiring a currency def; see the
advisory note in `items/README.md`'s def-removal section.

## Demo idle-income + autosave churn (И3b, cross-note — see also `resource_panel/README.md`)

The И3b demo binding (`src/ui/demo_hud.c`) feeds a small idle xp income
(`DEMO_XP_PER_SEC`, default 8/s) into `items` purse `tmpl.xp` every frame so
the demo `hero` auto-track visibly counts up and levels on its own — the
whole point of routing xp through `items_purse` (OQ6) is to make the L2->L1
include a REAL, exercised path, not a latent one. This income marks the save
dirty on every flush (autosave debounces at 2s, `main.c`
`GAME_SAVE_DEBOUNCE_MS`) — a template that keeps ticking in the background
is the INTENDED "idle game" demo, not a bug. A lead/game that wants a
perfectly silent template can zero `DEMO_XP_PER_SEC` in `demo_hud.c`; `hero`
then sits static at level 0/`cost[0]` and every §8 acceptance gate still
passes (they gate rendering/state correctness, never the exact number).
