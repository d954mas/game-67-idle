# Bottom Nav V3 Production Handoff

Date: 2026-07-04

Canvas project: `canvas://rb-dark-rpg-9874a1`

Production canvas group: `grp_358a3fa5` / `UI production elements - bottom nav v3 alpha slices`

Local export folder:
`games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_03/slices/`

## Accepted Direction

Use the rugged garrison-token bottom navigation direction from UI pass v3.
The five navigation buttons are the same visual size and hitbox. `Карта` is
central by position only, not larger.

The scene still uses top and bottom fade bands, not hard UI panels. Bottom
buttons sit over the lower fade. Runtime text, labels, counters, and badges are
drawn by the engine text/UI layer, not baked into the PNG frames.

Bottom nav order:

1. `Снаряж.`
2. `Дневник`
3. `Карта`
4. `Место`
5. `Еще`

## Runtime Assembly

Use the empty button bases as the 9-slice frame sources. All five bases have
canvas `slice9` metadata set to source insets:

`left: 58, top: 58, right: 58, bottom: 58`

Recommended runtime composition per button:

1. Draw lower fade band behind the nav.
2. Draw same-size 9-slice button base.
3. Draw one icon centered above the label area.
4. Draw runtime text label.
5. Draw optional overlay: lock or red notification badge.
6. Apply hover/pressed/selected states through tint, brightness, or a separate
   selected base, not by changing button dimensions.

Do not mask icons to button shape by default. Icons are already alpha-cut
separate elements. Clip only if a future icon intentionally overflows the
button safe area.

## Canvas Element Map

Runtime 9-slice bases:

| Role | Canvas element | Local PNG |
| --- | --- | --- |
| Equipment base | `el_4b27252b` | `nav_base_equipment_empty.png` |
| Journal selected base | `el_5b4eecf9` | `nav_base_journal_active_empty.png` |
| Map base | `el_0ad730b5` | `nav_base_map_empty.png` |
| Place base | `el_8dd98fbd` | `nav_base_place_empty.png` |
| More base | `el_f8a7ddae` | `nav_base_more_empty.png` |

Runtime icons:

| Role | Canvas element | Local PNG |
| --- | --- | --- |
| Equipment chest | `el_4c39767b` | `nav_icon_equipment_chest.png` |
| Journal book | `el_e9ece77f` | `nav_icon_journal_book.png` |
| Map | `el_333106cc` | `nav_icon_map_folded.png` |
| Place lantern | `el_8809029b` | `nav_icon_place_lantern.png` |
| More dot 1 | `el_89f545ac` | `nav_icon_more_dot_1.png` |
| More dot 2 | `el_5b407a67` | `nav_icon_more_dot_2.png` |
| More dot 3 | `el_a40dc51c` | `nav_icon_more_dot_3.png` |

Runtime overlays:

| Role | Canvas element | Local PNG |
| --- | --- | --- |
| Lock overlay | `el_6cbd27aa` | `nav_overlay_lock.png` |
| Red badge overlay | `el_6510e94f` | `nav_overlay_red_badge.png` |

Preview-only full buttons:

| Role | Canvas element | Local PNG |
| --- | --- | --- |
| Equipment preview | `el_28af3adb` | `nav_button_equipment_full.png` |
| Journal active preview | `el_9fe7bdfc` | `nav_button_journal_active_full.png` |
| Map preview | `el_adb2e6ee` | `nav_button_map_full.png` |
| Place preview | `el_0863ee08` | `nav_button_place_full.png` |
| More preview | `el_81bbe2e7` | `nav_button_more_full.png` |

Status fragments kept for reference:

| Role | Canvas element | Local PNG |
| --- | --- | --- |
| Equipment lock status | `el_4809948d` | `nav_status_lock_equipment.png` |
| Active badge status | `el_00c0aa16` | `nav_status_badge_active.png` |
| Map lock status | `el_233ef4eb` | `nav_status_lock_map.png` |

Canvas note: `el_79934c24`.

## Layout Rules

Use a fixed bottom nav safe area. The five buttons should share width, height,
baseline, and vertical center. On narrow screens, scale the whole row or reduce
horizontal gaps before changing individual button size.

`Еще` opens a compact bottom sheet above the bottom nav. It should contain
secondary systems such as settings, mail if needed later, help, and late-game
systems. Settings may also have a small top-right quick button, but the bottom
nav remains the primary persistent navigation.

`Место` is the current location/town screen entry. It is not the same as the
world map. `Карта` is the frequent travel/map action and stays in the center.

## Acceptance Criteria

- No heavy top or bottom panel blocks; use fade bands.
- No baked Russian labels in button art.
- All five bottom buttons have identical runtime dimensions.
- `Карта` is centered but not enlarged.
- Icons remain readable at 960x540.
- Button art does not cover the guard tap target.
- Locked/badge states are layered overlays, not separate layout variants.
- 9-slice scaling preserves corners and bevels without stretching icon art.
- Local PNGs and canvas ids stay traceable through `slices/manifest.json`.
