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

Named "tracks" (level + xp progress meters) authored in a game's Lua beside its
items and compiled to const tables by `scripts/generate_progression_tracks.py`
(`progression_tracks.gen.{h,c}`) out of the Items Snapshot's `tracks` section. Runtime state (level, internal xp)
is one flat save fragment (`state/progression.schema.json`, `--fragment
progression`, game-owned) — a `tracks: map<string, TrackState>` keyed by
`track_id`, exactly like items' `owned` map. The content generator requires
the game-owned schema through `--state-schema`, validates its progression
identity/shape, and derives the maximum track-id length from `string_max - 1`
for the generated NUL-terminated state key. No UI or DevAPI commands of its
own — the fragment is reachable through the universal `game.state.*` surface
the instant it is registered.

## Contents

```text
features/progression-core/
  include/features/progression/progression.h   public API L2, spelling preserved (see "Include spelling" below)
  src/
    progression.c                 modes/T5-caps/lazy allocation/tick
  scripts/
    generate_progression_tracks.py   codegen (Snapshot tracks section -> const baked tables)
  feature.json
  README.md   (this file)
  INSTALL.md
```

## Layer

L2 — depends on `features/items-core` (L1) for the payment scope it spends and
grants through (`progression.h` includes `features/items/items.h`, the ONE
allowed feature edge — `manual`/`auto` modes price a level in items and move
them through items' public API: `items_can_pay_stacks`/`items_try_pay_stacks`/
`items_try_stack_add`/`items_stack_count`). The reverse edge does
not exist — items code never mentions progression (grep-gated, G-rev). See
`features/items-core/README.md` for the L1 module this depends on.

## Three modes (one axis: `mode` in the catalog)

- **`manual`** — priced in items; `progression_level_up(track, reason)` pays the
  step from the bound scope on call. Does NOT tick.
- **`auto`** — priced in items; `progression_update()` (the frame tick) buys
  levels while the scope can afford them.
- **`threshold`** — priced in the track's own accumulator
  (`progression_add_xp(track, n, reason)`); `progression_update()` auto-levels
  while the accumulator covers the step.

Every payment is atomic and ordered: check, then allocate the track's save
record, then pay. Paying first would burn the resources whenever the tracks map
is full — a level nobody received, for currency nobody can get back.

Successful `manual`, `auto`, and `threshold` level changes emit
`progression.levelup` with `track`, `mode`, `reason`, `old_level`, and
`new_level`, plus the price in the form the mode actually pays in: `manual`/`auto`
fill `cost[]` (`def_id`, `amount`, `before`) and leave `xp_cost`/`xp_before` zero,
`threshold` does the reverse — its accumulator is not an item, and there is only
ever one of it. Additional fact events cover non-levelup mutations:
`progression.xp_added`, `progression.level_set`, and `progression.reset`.

## Where the numbers come from

Tracks are authored in the same Lua as items, in the neighbouring `studio.tracks`
space (`features/items-core/README.md`), and reach this module through the
`tracks` section of the Items Snapshot. `scripts/generate_progression_tracks.py`
bakes each track into const tables: `progression_step_t steps[]` (one per level
the track can reach), an exact column table, a fractional one, or neither.

Two level bases meet here and are converted once, in the generator. Authoring is
1-based: row 1 is the un-upgraded state and carries the track's zero
contribution. The runtime is 0-based: `steps[L]` is the step that leaves level
L, so `max_level == len(rows) - 1`.

Each step carries exactly one price, decided by the track's mode. `manual` and
`auto` fill `cost[]` -- a list of `{def_id, amount}`, so a level priced in coins
and wood is one step, not a special case. `threshold` fills `xp_cost`, a single
number, because a track's own accumulator is not an item and there is only ever
one of it.

A step may also grant items on being reached. The runtime pays and grants
through the bound payment scope; grants land in its first container, so a level
hands items back into the containers it charged.

## The tick gives the frame back

`progression_update()` resolves every auto/threshold track. An auto track buys
levels on its own, so a level that hands back at least what it charged of the
same resource would never stop buying -- that shape is rejected at authoring
time, in the evaluator and again at the Snapshot boundary. The runtime keeps
`PROGRESSION_MAX_LEVELUPS_PER_TRACK = 64` as the backstop against a
nearly-self-paying curve: hitting it logs `nt_log_warn` and drops the rest of
that track's levels for the frame. The frame always returns.

## Lazy allocation

A track with no save record reads as level 0 / xp 0
(`progression_level`/`progression_xp_current` on an absent record are 0,
never a crash). A record is allocated ONLY right before the first real
mutation (`level_up`/`add_xp`/`set_level`, or the first level-up inside
`progression_update()`) — a tick over an `auto` track nobody can afford or an
empty `threshold` track does NOT create a record. This keeps a fresh save's
`tracks` map empty, matching items' "no gratuitous ownership record"
discipline.

## Three laws worth keeping

- **A track stores only its own contribution; the base lives where the thing it
  affects lives.** Population in a live game is touched by six nodes; a base per
  node would be the same number written six times, wrong five of them.
- **Row 1 carries the zero contribution.** The authoring form makes you write
  that row, so say what it means: it is the un-upgraded state, not level one of
  the effect. Left unsaid, authors put the base there and every reader adds it
  twice.
- **`progression_set_level` does not run grants.** A jump from 0 to 5 skips five
  levels' worth of them. DevAPI and tests move levels through exactly this call,
  so a grant is not something a save can be nudged into.

## reason contract (lighter than items)

Every mutation takes `reason` (`verb:subject`), asserted in debug builds
(`progression_reason_check`, no-op in release) — but progression does NOT
pull in items' closed verb list (`features/items/reason_tags.h` is
game-owned and items-internal, not something progression should reach into).
A spend forwards `reason` straight into items, where the FULL items verb-check
already runs — the verb vocabulary lives in exactly one place. The two reasons
this module writes itself are part of its contract: the tick charges
`level_cost:auto` / `level_cost:threshold`, and a level's grant lands under
`loot:levelup`, so every consumer must have both verbs in its `reason_tags.h`
(see INSTALL.md).

## `set_level` vs `reset` — different primitives

- `progression_set_level(track, level, reason)` — clamps to `[0,
  max_level]`, leaves xp untouched. For a prologue ("start this hero at
  level 5").
- `progression_reset(track, reason)` — level=0 AND internal xp=0 (only
  `threshold` meaningfully has xp; `manual`/`auto` xp is the ignored
  default). Does NOT touch the payment scope — a full currency prestige is game
  composition (`progression_reset` + the game's own items call), not
  something progression does on its own (progression never owns the containers,
  only reads and spends them).

There is no `level_down` — cut deliberately; `set_level`/`reset` cover every
ratified use case.

## State fragment (`state/progression.schema.json`, `--fragment progression`, game-owned)

`tracks: map<string, TrackState { level: int, xp: i64 }>` — the SAME flat-map
shape as items' `owned`, no per-mode branching in the schema. `level` is
capped at schema max 9999 (a track's authored row count must stay within it,
enforced by the generator — a higher cap would silently
clamp on save instead of failing the build). `xp` is meaningful ONLY for
`threshold` tracks; `manual`/`auto` tracks carry the ignored default 0 (what
those two spend is an item, counted in the payment scope — an L2->L1 read, not
schema state).

**No hooks** (`on_new_game`/`reconcile` both absent) — see "No game-owned C
hooks" below. An orphaned track record (its `track_id` no longer declared in the
game's Lua) is harmless by construction —
`progression_update()` only ever iterates the CATALOG's tracks, never scans
`tracks` for orphans — so no reconcile/quarantine pass is needed (contrast
with items, where an orphaned owned-record represents lost player value and
must be quarantined, not ignored).

**`created`/lock-file: deliberately absent for track declarations** (unlike
`items.json`/`items.lock.json`). An orphaned items def_id means a
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

- `generate_progression_tracks.py --snapshot <items.snapshot.json> --state-schema <progression.schema.json> --out-dir <dir>` —
  emits `progression_tracks.gen.{h,c}`. `--snapshot` is the catalog: its `tracks`
  section is what the game authored in Lua. `--state-schema` validates the
  authoritative game-owned `game_seed.progression` fragment and derives the
  maximum track-id length from `string_max - 1`.

## Consumer authoring

1. Declare tracks in the game's Lua through `studio.tracks`, in a module listed
   by `items.lua.json`. A track id is the key of its save record: renaming one
   silently forgets every player's earned levels.
2. Build the Items catalog; the Snapshot carries the tracks section with it.
3. Run the generator above (the consuming build already wires this as a codegen
   step).

## Cross-dependency note (see also `features/items-core/README.md`)

A track's price and grants name items by reference, and both spaces are
evaluated together: a step that names a retired def_id fails in the evaluator,
before any snapshot exists, so there is no way to ship a track priced in
something nobody can hold. Retiring a def is therefore a two-space edit — see
the advisory note in the game's own items-corner README def-removal section.

## Demo idle-income + autosave churn (template-specific, informational only)

The template's demo binding (`src/ui/demo_hud.c`) feeds a small idle income
(`DEMO_XP_PER_SEC`, default 8/s) of the `tmpl.xp` item into the wallet every
frame so the demo `hero` auto-track visibly counts up and levels on its
own — the whole point of pricing it in a real item is to make the L2->L1 edge
an exercised path, not a latent one. This is TEMPLATE game composition, not
part of this module — a lead/game that wants a perfectly silent template can
zero `DEMO_XP_PER_SEC` in `demo_hud.c`; `hero` then sits static at level 0 and
this module's own tests still pass (they gate curve/tick correctness, never the
exact idle number).

## Backdoor (documented, not built)

A game with fundamentally different leveling semantics (e.g. a formula-driven
curve instead of a baked table, or a fourth `mode`) is not expected to fork
this module by adding a switch here — LEAN forbids speculative
generalization for a single consumer. Instead it copies `src/`+`include/`
out of this module into its own tree and owns that copy going forward
(copy-then-own, same escape hatch `features/items-core`/`settings`/
`resource_panel` already use). No code in this module supports that fork;
it is a documented possibility, not a feature.

## Purpose

Provide reusable progression tracks, bounded tick behavior, and curves baked at
build time over the items-core payment boundary.

## Public surface

`include/features/progression/progression.h`, generated outputs, and the
generator command in `feature.json` are public. Game content and UI are not.

## Validation

Run the progression tests named in `feature.json.registers.ctest_targets`,
then `node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. Patch preserves the public contract,
minor adds backward-compatible surface, and major permits breaking changes.
Consumers pin both this version and an exact repository revision.
Version `2.0.0` makes `--state-schema` mandatory; `1.x` generator invocations
must add the owning game schema explicitly.
Version `3.0.0` replaces the legacy `--items <items.json>` input with the
canonical `--items-snapshot <items.snapshot.json>` build output.
Version `3.1.0` adds `curve.type: "table"` (verbatim hand-authored per-level
costs) alongside the existing `"exp"` formula curve — backward-compatible,
existing `"exp"` catalogs generate byte-identical output.
Version `4.0.0` reshapes `progression.levelup`: the price rides `cost[]`
(`def_id`/`amount`/`before`) for item-paid modes and `xp_cost`/`xp_before` for
`threshold`. The five scalars it replaces — `cost_def_id`, `cost_amount`,
`resource_before`, `resource_after`, `cascade_depth` — are gone. A consumer
reading the old fields must move to the accessors.
Version `5.0.0` moves authoring into the shared Lua evaluator and the API onto
handles. Tracks are declared with `studio.tracks` and read from the Snapshot's
`tracks` section; `content/progression.json` and the `--catalog` flag are gone.
Every call takes a `progression_track_ref_t` from `progression_track(id)` instead
of a string. A step's price is a list of items or a single xp threshold, a step
may grant items, and a track may carry exact and fractional columns read through
`progression_valuei`/`progression_valuef`. `progression_xp_needed` and
`progression_can_level_up` are removed: the first answered with one number a
list-priced level cannot have, and the second was built on it — affordability is
now `items_can_pay_stacks` over the step's own list. Cascades
(`progression_emit_t.to_track`, the recursion, its depth cap, and the
`cascade_depth` event field) are removed with them. The two reason verbs the
module writes — `level_cost:<track_id>` and `loot:levelup` — are part of the
contract a consumer's `reason_tags.h` must satisfy.

Version `6.0.0` makes two soft answers hard. A column read of the wrong type used to
promise an assert and a zero; `NT_ASSERT` traps in every build, so the zero was
unreachable and the promise was false -- the reads now say plainly that a mismatch is
a caller bug. And the generator, which read `string_max` from the owning save schema
and hardcoded the rest, now takes the track count and the level cap from that same
document, refuses an empty catalog, refuses a column that would redefine
`PROGRESSION_VALUE_COUNT`, refuses a column only some track kinds own, and refuses a
price longer than one payment. A catalog that built under `5.x` can be rejected here;
every rejection names what to change.

## Extension points

Extend through game-owned catalogs, generated curves, state fragments, and
composition; fork explicitly for different core semantics.
