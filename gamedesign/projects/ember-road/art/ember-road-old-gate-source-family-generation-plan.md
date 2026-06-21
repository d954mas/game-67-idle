---
type: SourceFamilyGenerationPlan
job_id: ember-road-old-gate-fakeshot-v001
status: draft-ready
---

# Old Gate Source Family Generation Plan

This plan follows the accepted direction fake shot:
`gamedesign/projects/ember-road/art/ember-road-old-gate-fakeshot-v001.png`.

It is not permission to use the fake shot as one fused runtime image. Generate,
inspect, crop, pack, and compose separate project-local source families.

## Shared Constraints

- Preserve game/UI layout as Y-up in all implementation notes and runtime
  layout translation.
- Use project-local copies only.
- No readable text, fake letters, numbers, logos, watermarks, or copied
  reference-game ornament shapes.
- Runtime text, counters, labels, quest names, and state values must be drawn by
  the engine text renderer.
- Use flat chroma or true transparency for isolated sprites/icons.
- Keep gutters large enough for alpha trim and slice9 audits.

## Source Families

### old gate background layer sheet

Generate a cuttable background/layer sheet, not a composed UI screen:

- wide Old Gate backdrop plate without UI;
- road/forest continuation plate;
- route strip base;
- destination plaque bases for Old Gate, North Road, Old Mine;
- locked marker overlay and quest-highlight overlay;
- small torches/ember light overlays if separable.

Reject if the sheet includes text, labels, fused HUD, exact copied reference
architecture, or a complete screenshot composition.

### hero npc enemy sprite sheet

Generate isolated sprites on flat chroma/alpha:

- hero back/three-quarter standing pose, sword visible, cloak silhouette clear;
- Gate Warden portrait bust;
- Gate Warden scene standing marker;
- Road Wolf side pose;
- Road Wolf small combat card pose;
- optional hit/ember slash effect.

Every sprite needs enough padding and a planned pivot/anchor. Reject weak
silhouettes at gameplay size, cropped weapons, merged shadows, or text.

### quest reward route icon sheet

Generate semantic icons with gutters:

- quest marker;
- route arrow;
- locked mine/lock;
- wolf encounter marker;
- ring reward;
- XP spark/star;
- gold coin;
- sword/auto-battle;
- completed check/claim marker.

Icons must not include labels, frames fused into icons, numbers, or fake text.

### fantasy browser rpg ui frame sheet

Generate slice9-safe and overlay-ready UI parts:

- top status frame strip;
- right quest rail panel base;
- bottom log/action belt base;
- reward slot base;
- primary button states: default, pressed, disabled, selected;
- route plaque frame;
- corner/cap overlays;
- small non-stretch medallion/gem overlays.

Centers and long edges must stay repeatable. Reject busy stretch centers,
ornaments across content safe areas, or panels with baked text.

## Next Proof

1. Generate candidate sheets for the four missing families.
2. Inspect candidates and keep only accepted source art under
   `gamedesign/projects/ember-road/art/`.
3. Add generation records for each accepted source family.
4. Run source-family coverage again; it should pass before runtime slicing.
5. Crop/pack the accepted assets, integrate into native runtime, capture
   `build/captures/iterate_first_screen.png` again, then rerun strict product
   gate.
