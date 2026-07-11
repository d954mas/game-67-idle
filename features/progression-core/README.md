# progression-core

In-place L2 module (see `features/README.md` — decisive rule: same-`.c`-across-games
+ data-only customization = module, not a copy-then-own feature). Precedent:
`features/game-state`, `features/items-core` — one copy of the source lives
here, each consuming game/template compiles it in-place against ITS OWN
generated headers and content (`../../features/progression-core/` from any
`templates/<x>` or `games/<id>`, depth-2 invariant). Extracted in T0337 out of
`templates/template/src/features/progression/` (the ENTIRE folder — items
kept a game-side corner, progression did not, see "No game-owned C hooks"
below).

## What it is

Named "tracks" (level + xp progress meters) declared as DATA in a game's
`content/progression.json`, compiled to a const table by
`scripts/generate_progression_tracks.py` (`progression_tracks.gen.{h,c}`),
mirroring items' content-catalog codegen. Runtime state (level, internal xp)
is one flat save fragment (`state/progression.schema.json`, `--fragment
progression`, game-owned) — a `tracks: map<string, TrackState>` keyed by
`track_id`, exactly like items' `owned` map. No UI, no DevAPI commands of its
own — the fragment is reachable through the universal `game.state.*` surface
the instant it is registered.

## Contents

```text
features/progression-core/
  include/features/progression/progression.h   public API L2, spelling preserved (see "Include spelling" below)
  src/
    progression.c                 modes/T5-caps/lazy allocation/tick
  scripts/
    generate_progression_tracks.py   content codegen (content/progression.json -> const int64 curve tables)
  feature.json
  README.md   (this file)
  INSTALL.md
```

## Layer

L2 — depends on `features/items-core` (L1) for purse reads/spends
(`progression.h` includes `features/items/items.h`, the ONE allowed feature
edge — `manual`/`auto` modes read and spend player currency through items'
public API: `items_purse`/`items_add`/`items_remove`). The reverse edge does
not exist — items code never mentions progression (grep-gated, G-rev). See
`features/items-core/README.md` for the L1 module this depends on.

## Three modes (one axis: `mode` in the catalog)

- **`manual`** — xp lives in purse (`currency_def`); `progression_level_up(track,
  reason)` spends `cost(level)` from purse on call. Does NOT tick. Successful
  calls emit `progression.levelup` with `mode: "manual"` and resource before/after
  context so analytics does not infer levelups from item transactions.
- **`auto`** — xp lives in purse; `progression_update()` (the frame tick)
  auto-buys levels while purse can afford it.
- **`threshold`** — xp is an internal accumulator
  (`progression_add_xp(track, n, reason)`); `progression_update()`
  auto-levels while the accumulator covers `cost(level)`.

Successful `manual`, `auto`, and `threshold` level changes emit
`progression.levelup` with `track`, `mode`, `cause`, `reason`, `old_level`,
`new_level`, cost/resource before-after fields, and `cascade_depth`. Additional
fact events cover non-levelup mutations: `progression.xp_added`,
`progression.level_set`, and `progression.reset`.

## Curve = baked int64 table (zero float in C)

A consuming game's `content/progression.json` authors ONE curve preset per
track — this module's codegen supports ONLY `curve.type: "exp"`
(`{base, growth_num, growth_den}`); any other type is a loud generator
`SystemExit` (`table`/`linear`/`poly` are a deliberate LEAN cut, not silently
ignored — add them with their own identity test when a real game needs
them). `scripts/generate_progression_tracks.py` bakes `cost[L] = floor(base
* (growth_num/growth_den)**L)` via pure integer arithmetic (`(base *
growth_num**L) // growth_den**L` — FLOOR by construction, no float-rounding
risk) into a `static const int64_t COST_<TRACK>[]` table at build time. The
runtime (`progression.c`) only ever reads `def->cost[level]` — there is no
formula interpreter in C. The template's `tests/test_progression_curve.c`
golden-asserts its demo track's baked values (`50, 75, 112, 168, ...`).

`on_level_up` (authored per-level currency/xp-cascade rewards) is a real
RUNTIME feature (`progression_emit_t`, `apply_on_level_up`, cascade
resolution with a depth cap) but the codegen does NOT bake it (LEAN cut) —
every generated track carries `.on_level_up = NULL, .on_level_up_count = 0`;
declaring `on_level_up` in `content/progression.json` is a generator
`SystemExit`. The runtime path is exercised ONLY by a hand-written test
catalog (`tests/test_progression_catalog.c` in the template) until a real
game needs authored cascades — bring the generator branch back with its own
test at that point.

## HARD caps on the tick (T5 — not optional)

`progression_update()` resolves every auto/threshold track through
`resolve_track()`. Two ways a single frame could hang without a cap: (1) a
self-refunding `auto` track whose `on_level_up` gives back >= its own cost
(infinite `while`); (2) a track-to-track xp cascade (`on_level_up.to_track`)
that feeds back on itself (`A -> B -> A -> ...`). Both are closed by hard
caps (`PROGRESSION_MAX_LEVELUPS_PER_TRACK = 64` per track per frame,
`PROGRESSION_MAX_CASCADE_DEPTH = 8` recursion depth) — hitting either logs
`nt_log_warn` and drops the rest of that track's levels for the frame; the
frame always returns.

## Lazy allocation

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
game-owned and items-internal, not something progression should reach into).
A spend into purse forwards `reason` straight to `items_remove`/`items_add`,
where the FULL items verb-check already runs — the verb vocabulary lives in
exactly one place.

## `set_level` vs `reset` — different primitives

- `progression_set_level(track, level, reason)` — clamps to `[0,
  max_level]`, leaves xp untouched. For a prologue ("start this hero at
  level 5").
- `progression_reset(track, reason)` — level=0 AND internal xp=0 (only
  `threshold` meaningfully has xp; `manual`/`auto` xp is the ignored
  default). Does NOT touch purse — a full currency prestige is game
  composition (`progression_reset` + a separate `items_remove`), not
  something progression does on its own (progression never owns purse
  balances, only reads/spends them).

There is no `level_down` — cut deliberately; `set_level`/`reset` cover every
ratified use case.

## State fragment (`state/progression.schema.json`, `--fragment progression`, game-owned)

`tracks: map<string, TrackState { level: int, xp: i64 }>` — the SAME flat-map
shape as items' `owned`, no per-mode branching in the schema. `level` is
capped at schema max 9999 (`content/progression.json`'s `max_level` must
stay `<= 9999`, enforced by the generator — a higher cap would silently
clamp on save instead of failing the build). `xp` is meaningful ONLY for
`threshold` tracks; `manual`/`auto` tracks carry the ignored default 0 (xp
lives in purse for those two modes — an L2->L1 read, not schema state).

**No hooks** (`on_new_game`/`reconcile` both absent) — see "No game-owned C
hooks" below. An orphaned track record (its `track_id` removed from
`content/progression.json`) is harmless by construction —
`progression_update()` only ever iterates the CATALOG's tracks, never scans
`tracks` for orphans — so no reconcile/quarantine pass is needed (contrast
with items, where an orphaned owned-record represents lost player value and
must be quarantined, not ignored).

**`created`/lock-file: deliberately absent for `content/progression.json`**
(unlike `items.json`/`items.lock.json`). An orphaned items def_id means a
LOST count in the save (destructive, needs a guard); an orphaned track_id is
inert (ignored by `progression_update()`, per the paragraph above) — so
there is no destructive-removal case to guard against. Cost of this
decision: removing a track from the catalog silently forgets the player's
earned levels for it, with no red gate. Accepted for the template fixture; a
real game with prestige/account-transfer across a shipped track roster
should add its own lock+migration workflow before removing a track that has
shipped.

## No game-owned C hooks (asymmetry vs items)

Unlike items, progression has **no game-side C corner** — items keeps
`reason_tags.h` + a bootstrap seed function in the consuming game's
`src/features/items/`; progression has neither a closed-verb header nor a
seed function, so `src/features/progression/` (which used to hold
`progression.h` + `.c` + this README) has been **deleted entirely** from
every consuming game by this extraction — nothing
is left for a game to own for progression. `reset()` alone is the correct
"fresh game" state (empty tracks = level 0 everywhere via the lazy-allocation
default above); a game that wants a strong starting hero calls
`progression_set_level` from its own composition code, not from an
`on_new_game` hook here (there is no hook here — `state/progression.schema.json`
declares `hooks: {}`).

## Include spelling (single physical root; no game-side shadow)

The public header keeps its historical spelling, `features/progression/progression.h`,
even though it physically lives under `features/progression-core/include/`.
Unlike items (which still resolves `features/items/reason_tags.h` from a
game-owned corner sharing the same logical prefix), progression's game-side
corner was deleted entirely (see above) — the WHOLE `features/progression/`
logical prefix now resolves from this one physical root
(`PROGRESSION_CORE_INC`), with nothing left in a game's own `src/` that
could shadow it. See INSTALL.md for the include-spelling contract shared with
items-core (a spelling rename would
have touched every consumer's include lines and broken byte-identical
relocation).

## Tools (`scripts/`)

- `generate_progression_tracks.py --catalog <progression.json> --items <items.json> --out-dir <dir>` —
  emits `progression_tracks.gen.{h,c}` (compile-time const curve tables).
  `--items` is the cross-check: every `manual`/`auto` track's `currency_def`
  must name an existing items def with a `currency` block.

## Consumer authoring (content workflow)

1. Edit `content/progression.json` (`namespace` + `tracks[]`; form mirrors
   `items.json`). `id` is a bare slug (`"hero"`, not `"tmpl.hero"`) — track
   ids are progression-internal, not items-namespaced.
2. Build codegen: `node ai_studio/dev_environment/python_run.mjs features/progression-core/scripts/generate_progression_tracks.py
   --catalog content/progression.json --items content/items.json --out-dir <dir>` —
   emits `progression_tracks.gen.{h,c}`. `--items` is the cross-check:
   `currency_def` (manual/auto tracks) must name an existing items def with
   a `currency` block.
3. `curve.type` must be `"exp"`; `mode` one of `manual|auto|threshold`;
   `max_level` in `[1, 9999]`; `on_level_up` must be ABSENT from the JSON
   (see above) — the generator rejects it loudly, not silently.

## Cross-dependency note (see also `features/items-core/README.md`)

`currency_def` in `content/progression.json` names a live items def_id.
items' own destructive-removal guard (`items.lock.json`) does not know
about progression — removing a currency that a track still references will
pass the items guard cleanly but break progression codegen loudly
(`SystemExit`: currency_def not found / not a currency). Keep
`content/progression.json` in sync when retiring a currency def; see the
advisory note in the game's own items-corner README def-removal section.

## Demo idle-income + autosave churn (template-specific, informational only)

The template's demo binding (`src/ui/demo_hud.c`) feeds a small idle xp
income (`DEMO_XP_PER_SEC`, default 8/s) into items purse `tmpl.xp` every
frame so the demo `hero` auto-track visibly counts up and levels on its
own — the whole point of routing xp through `items_purse` is to make the
L2->L1 include a REAL, exercised path, not a latent one. This is TEMPLATE
game composition, not part of this module — a lead/game that wants a
perfectly silent template can zero `DEMO_XP_PER_SEC` in `demo_hud.c`; `hero`
then sits static at level 0/`cost[0]` and this module's own tests still
pass (they gate curve/tick correctness, never the exact idle number).

## Backdoor (documented, not built)

A game with fundamentally different leveling semantics (e.g. a formula-driven
curve instead of a baked table, or a fourth `mode`) is not expected to fork
this module by adding a switch here — LEAN forbids speculative
generalization for a single consumer. Instead it copies `src/`+`include/`
out of this module into its own tree and owns that copy going forward
(copy-then-own, same escape hatch `features/items-core`/`settings`/
`resource_panel` already use). No code in this module supports that fork;
it is a documented possibility, not a feature.
