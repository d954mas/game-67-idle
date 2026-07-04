# Bottom Nav V11 Production Handoff

Date: 2026-07-04

Status: artlead approved for implementation.

## Source

- Canvas project: `canvas://rb-dark-rpg-9874a1`
- Canvas group: `grp_7c4025a4` /
  `UI direction - standalone glass inspect nav tokens 11`
- Canvas image element: `el_7f7482f8`
- Source PNG:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/bottom_nav_component_sheet_11.png`
- Atlas manifest:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/atlas_manifest.json`
- Provenance:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/provenance.md`

## Runtime Layout

Bottom nav order:

1. `Снаряж.`
2. `Дневник`
3. `Карта`
4. `Место`
5. `Еще`

`Карта` is centered by position only. It is not larger and does not get a
special hitbox.

All five buttons must share the same runtime width, height, baseline, and
hitbox. The source atlas has minor width variance from generation; normalize
that at runtime.

## Rendering Rules

- The nav sits over the bottom fade band, not inside a hard panel.
- Use the PNG as atlas frames or crop it into sprites from
  `atlas_manifest.json`.
- Runtime draws the text labels over the lower trough.
- No baked text from the PNG.
- Runtime owns lock states, notification badges, selected/pressed state, and
  counters.
- Icons are part of the approved button art for the first pass. If later split
  into base+icon layers, keep the same silhouettes and colors.
- Do not add a separate talk button. The first action remains tapping the guard.

## Button Semantics

- `Снаряж.`: character equipment/loadout.
- `Дневник`: quests, story, current task log.
- `Карта`: map and movement between known locations.
- `Место`: inspect/search the current place; this is the standalone magnifying
  glass icon.
- `Еще`: compact overlay for settings, help, and later systems.

## Visual Acceptance

- The bottom UI still reads as dark Roblox-like fantasy, not generic mobile UI.
- The five buttons are visually related but not identical.
- The `Место` icon must stay as one standalone magnifying glass, with no tiles,
  clue sheet, or lantern under it.
- The labels remain legible on both desktop and phone when drawn by runtime
  text with outline/shadow.
