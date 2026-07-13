# resource_panel — feature reference

`src/features/resource_panel/` — L2 widget feature (`resource_panel.h` first
line: `// feature-layer: L2`). Depends ONLY on the L0 shell (`game_format.h`
for the abbrev formatter) and engine public APIs (`atlas/nt_atlas.h`,
`ui/nt_ui.h`, `clay.h`, ...) — it includes **ZERO** other `src/features/*`
headers (grep-gated). It has no idea items or progression exist.

## Purpose

Provide this template-owned, copy-then-own counter/bar HUD widget without
coupling it to items or progression.

## Public surface

`resource_panel.h` is public. The game supplies entries, getters, optional
icon handles, and composition calls.

## Validation

Build the template UI tests and run `node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. Patch preserves the public contract,
minor adds backward-compatible surface, and major permits breaking changes.
A copied game owns its revision after creation.

## Extension points

Customize entries and visual constants in the game-owned copy. Promote only a
proven reusable improvement back to this template reference.

## What it is

A generic counter/bar HUD widget. The GAME supplies a list of
`resource_panel_entry_t` — id, label, kind, an optional icon handle, and up
to three getters (`value` required, `max`/`level` optional) — and calls
`resource_panel_ui(ctx, entries, count)` once a frame. The panel never reads
items/progression/anything else directly; every value comes back through
`entry->value(entry->ud)` and friends. This is what makes the widget
reusable in any game: point it at any `int64_t(void*)` function and it works.

## Two visual forms

- **`RESOURCE_PANEL_COUNTER`** — icon (or a flat rect placeholder when
  `icon == NULL`) + an abbreviated number (`game_format_i64_abbrev`). Clay id
  `resource_panel/<entry.id>`.
- **`RESOURCE_PANEL_BAR`** — icon + a track/fill bar (`displayed/max` ratio)
  + a caption (`"Lv %lld  %s/%s"`, or `"Lv %lld  MAX"` past the cap; drops the
  `"Lv N  "` prefix entirely when `entry->level == NULL`). Clay id
  `resource_panel/<entry.id>/bar`. When `entry->max == NULL` a bar entry
  degrades to a label-only counter (no fill, no denominator) — see the NULL
  contract on `resource_panel_entry_t` in the header.

## Count-up + accent (v1 behavior — nothing beyond this)

- **`displayed` != `logical`.** Every frame reads `entry->value(ud)` once
  (poll) and eases the DISPLAYED number toward it (`TAU≈0.12s` exponential
  ease-out, retarget-not-restart — the ease target is always the current
  logical value, it never restarts from zero on a mid-flight change). A big
  jump (>~25% of the bar's denominator, or of a floored-at-1000 absolute
  base for a denominator-less counter) SNAPS instead of animating — this is
  what stops a fresh load's starting gold from visibly "counting up" from
  zero, and stops a `resource_panel_anchor`-tracked entry from crawling
  across the whole HUD on a session/slot change.
- **Accent (gain/spend).** A logical change tints the value/fill toward
  green (gain) or red (spend) for `ACCENT_SECS≈0.35s`, decaying back to
  normal. Skipped on an entry's first-ever frame (load is a snap, not a
  player-caused change).
- **Punch, without reflow.** The value glyph scales up slightly
  (`1.0 + 0.12*accent`) on a change — via a render-time-only transform
  (`nt_ui_transform_t` / `NT_UI_DATA_XFORM`) inside a FIXED-size cell, never
  by bumping the font size (that would re-measure Clay's FIT layout and shove
  every sibling on the row for the duration of the punch).
- **Poll+diff only.** The panel reads `entry->value` once per frame and
  diffs against last frame's value — it does NOT subscribe to the event log
  or hold a cursor. Many small changes within a frame (idle-income
  micro-ticks) auto-coalesce into one visible delta.

## `resource_panel_anchor` (coin_fx seam)

Best-effort, approximate screen position of the last-drawn entry by id
(`false` if that id wasn't drawn this frame). Immediate-mode UI only knows
the EXACT position after end-of-layout, and this widget doesn't do a
post-layout query — the returned position is the panel's known top-left
corner plus `row_index * row_height`, good enough for "roughly where the
coin counter is" but not pixel-exact. A future `coin_fx` feature that needs
the real position should query `Clay_GetElementData` by the entry's Clay id
instead of tightening this seam.

## Graceful no-art

`icon == NULL` renders a flat rect placeholder — the panel never requires an
atlas and never fails to render for lack of art. `art_needs` in
`feature.json` declares the (optional) icon slot; the demo binding
(`src/ui/demo_hud.c`) deliberately passes `icon=NULL` (И3 adds no new
assets).

## Demo idle-income + autosave churn (cross-note, see also `progression/README.md`)

The template's demo binding (`src/ui/demo_hud.c`) drives a small idle xp
income (`DEMO_XP_PER_SEC`, default 8/s) into `items` purse so the hero bar
this panel renders visibly counts up and levels on its own — otherwise a
fresh boot would show a perfectly static bar and nothing would prove the
panel's count-up/accent/levelup behavior without manual DevAPI pokes. This
income marks the save dirty on every flush (autosave debounces at 2s,
`main.c` `GAME_SAVE_DEBOUNCE_MS`) — a template that visibly "keeps ticking"
in the background is the INTENDED demo of an idle game, not a bug. A real
game (or a lead who wants a silent template) can zero `DEMO_XP_PER_SEC` in
`demo_hud.c`; the bar then sits static at `0/cost` and every acceptance gate
in §8 still passes (they gate rendering correctness, never the exact number).

## Customization (three steps)

1. **Config** — no code changes: point `entries[]` at different
   getters/icons/labels in your own composition (`demo_hud.c` is the
   template's example; a real game writes its own).
2. **Edit your copy** — this feature is copy-then-own like every other
   template feature; tweak layout constants (`RP_ROW_H`, `RP_BAR_W`, colors,
   `TAU`/`ACCENT_SECS`) directly in your game's copy.
3. **Promote** — a genuinely reusable improvement (a third visual form, a
   real post-layout anchor) goes back into `features/` before the arc closes
   on this game, not left stranded in one game's copy.

## Art

`art_needs` in `feature.json` declares ONE optional slot (`icon`,
`atlas-region`, per-resource). `build_packs.c` is never touched by this
feature — art flows in only as `nt_atlas_region_ref_t` handles the game
passes through `resource_panel_entry_t.icon` (declarative model, see the
root `src/features/README.md` "Ассеты").
