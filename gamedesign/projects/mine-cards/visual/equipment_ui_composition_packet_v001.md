# Equipment UI Composition Packet v001

Date: 2026-06-18
Task: `T0011`
Status: gated prep; do not start runtime integration until T0001 is accepted
or the lead explicitly prioritizes equipment UI work.

## Visual Session Contract

- Goal: define the source, atlas, composition, and native-proof requirements
  for turning accepted T0008 equipment sprites into a reusable Mine Cards
  equipment/inventory UI surface.
- Non-goal: add equipment mechanics, integrate into T0001 before acceptance, or
  replace T0008 item sprites unless the lead rejects them.
- Proof: accepted source families, atlas metadata/audit, composition proof, and
  a native screenshot/product gate for the equipment UI surface.
- Stop condition: if item art or UI composition fails visual/product-read
  review, freeze equipment integration and fix only the rejected visual axis.
- Likely files: future art job under `art_requests/`, runtime UI manifests,
  `assets/runtime/`, review proofs, and native captures.

## Current Inputs

Accepted for review, not final composition:

- Item source/runtime set:
  `assets/runtime/mine-cards-equipment-source-v001/`
- Item contact sheet:
  `gamedesign/projects/mine-cards/reviews/mine-cards-equipment-source-v001-contact.png`
- T0008 acceptance audit:
  `gamedesign/projects/mine-cards/reviews/t0008_equipment_art_acceptance_audit_2026-06-18.md`
- Item art job:
  `gamedesign/projects/mine-cards/art_requests/mine-cards-equipment-source-v001.json`

T0008 does not prove:

- equipment or inventory screen layout;
- atlas pack for composed equipment UI;
- source-family coverage for slot frames/panels/states;
- final-art composition proof.

## Target Surface Options

Default first surface:

```text
Compact item compare panel -> Equipment tab later
```

Machine-readable contract:

`gamedesign/projects/mine-cards/data/equipment_ui_composition_contract_v001.json`

Contract validation:

```powershell
py -3.12 tools/assets/validate_equipment_ui_composition_contract.py --contract gamedesign/projects/mine-cards/data/equipment_ui_composition_contract_v001.json
```

Surface options considered:

1. Equipment tab preview.
   Shows miner gear slots, item list, and selected item details.
2. Inventory modal.
   Shows item grid, selected item, equip/compare button states.
3. Compact item compare panel.
   Smallest first proof: selected item, current equipped item, stat delta, and
   equip action.

Reason: it proves item sprites, slot frames, state overlays, runtime text, and
one action without creating full equipment mechanics.

## Required Source Families

Generate or author separately:

- `equipment_slot_frames`: blank slot bases in empty/default/selected/locked
  states;
- `equipment_panel_frame`: resizable panel/frame pieces or slice9-ready panel;
- `equipment_state_overlays`: affordable/equippable, locked, selected, new item
  badge, compare up/down arrows;
- `equipment_item_shadow_layer`: optional separate soft shadow plate, not baked
  into item sprites;
- `equipment_action_buttons`: compact equip/compare button states if not
  covered by the existing UI kit.

Do not generate a full fixed screenshot as the runtime source. A full mockup
may be used only as visual target/reference.

## Runtime Composition Rules

- Item sprites stay separate from UI slot frames.
- Labels, stats, rarity, item names, costs, and state values are runtime text.
- Slot state is an overlay or separate frame state, not baked into item art.
- Item shadows are separate optional sprites/layers.
- Atlas metadata must record safe content rects, pivots, preview sizes, and
  allowed uses.
- Portrait and landscape must have authored layouts if the surface appears in
  both modes.

## First Composition Proof

Minimum proof:

- 3 equipped/available items: worn pickaxe, copper pickaxe, mining helmet;
- 1 selected item detail row;
- current/equipped state;
- locked/unavailable slot or future-slot hint;
- runtime text for item name and stat/change;
- one disabled/available equip action state;
- no baked text in source art.

Item scope:

- composition proof uses the three mining-first items above;
- atlas/composition contract still references all 12 accepted T0008 sprites so
  the review atlas can inspect the whole item set;
- item shadow layer is omitted in the first pass and generated only if the
  composition proof shows the item silhouettes need a separate reusable shadow.

Recommended first screenshot question:

```text
Can a new player see what item is selected, whether it is equipped, and what
the next equipment action would do?
```

## Validation Checklist

Before claiming composition readiness:

- source derivation record exists for every new source family;
- source-family coverage audit passes;
- crop manifests and runtime manifests exist;
- atlas metadata audit passes;
- atlas pack and atlas pack audit pass;
- generated UI asset pixel/edge audit passes;
- composition proof is captured as an image;
- native screenshot/product gate passes for the equipment UI surface;
- `external/neotolis-engine` has no diff.

Contract validation must pass before source generation starts:

```powershell
py -3.12 tools/assets/validate_equipment_ui_composition_contract.py --contract gamedesign/projects/mine-cards/data/equipment_ui_composition_contract_v001.json
```

## Rejection Axes

If rejected, record one concrete axis:

- item art too dark/noisy at UI size;
- slot frames compete with item silhouettes;
- text/stat hierarchy unclear;
- selected/equipped/locked states not distinguishable;
- composition feels like a debug grid rather than game UI;
- portrait layout cramped or unreadable.
