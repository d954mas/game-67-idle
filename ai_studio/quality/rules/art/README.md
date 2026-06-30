# Art Rules

Use this group when changed work affects art direction, composition, generated
art, visual polish, screenshots as final output, or a visual target.

## Not For

- player-facing clarity, layout, or responsive behavior: use
  [Player Clarity](../player_clarity/README.md);
- asset license, provenance, publishability, or runtime format: use
  [Assets](../assets/README.md);
- material data, UVs, textures, material maps, assignment, preservation, or
  conversion: use [Assets](../assets/README.md);
- runtime/build behavior: use [Technical](../technical/README.md).

## Checks

### [QART_001 - Closest Practical Visual](checks/QART_001_closest_practical_visual.md)

Checks: player-facing visuals use the closest practical version of the intended
final direction instead of defaulting to debug shapes, placeholder art, or
temporary overlays.

Use when: adding, replacing, generating, adapting, or sourcing visual candidates
for player-facing visuals, sprites, UI art, screenshots, or visual targets.

Record applied checks in the task log using the outcome format from the Quality
README.
