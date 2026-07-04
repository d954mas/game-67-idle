# Bottom Nav V11 Art Brief

Date: 2026-07-04

Status: artlead approved for implementation.

## Change From V10

V10 made the lens readable by showing stone tiles under it. V11 removes that
extra meaning and makes `Место` a cleaner inspect button:

- one standalone magnifying glass;
- no tiles under the lens;
- no clue/document/lantern props;
- lens readability from tint, rim, and highlights.

## Keep

- Equal outer button size and equal runtime hitboxes.
- Empty lower trough reserved for runtime text.
- No baked text in the PNG.
- Muted dark fantasy palette.
- `Еще` as three physical brass dots.

## Review Notes

V11 is stronger than v10 if `Место` should mean "inspect current place" without
suggesting a specific object, clue, or tile target.

## Implementation Source

- Source sheet:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/bottom_nav_component_sheet_11.png`
- Atlas rects:
  `games/rb-dark-rpg/assets/ui/generated/garrison_nav_tokens_11/atlas_manifest.json`
- Canvas group: `grp_7c4025a4`
- Canvas image element: `el_7f7482f8`
