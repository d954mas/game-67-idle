# Mine Cards Runtime UI Asset Manifest

Status: procedural shell with generated stage nodes, generated icons/FX, generated large blank UI slice9 bases, and generated compact chip/card/button/nav slice9 bases.

Pack: `assets/mine_cards_ui.ntpack`

This pack exists to move the native Mining screen off debug-only shape panels and onto the engine atlas/sprite path while the final Mine Cards UI kit is still pending. The visual language is based on the old Mine Cards PSD previews: dark RPG shell, purple selected states, lime active accent, and chunky pixel panels.

Generated runtime sprites now included:

- `mine-cards/ui/rock_stone` from `gamedesign/projects/mine-cards/visual/runtime_ui/source/mine_node_stone_v001.png`
- `mine-cards/ui/rock_copper` from `gamedesign/projects/mine-cards/visual/runtime_ui/source/mine_node_copper_v001.png`

Icon runtime sprites now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-icons-v001-candidate-e-alpha.png` with crop/contact/pixel-audit evidence in `gamedesign/projects/mine-cards/reviews/`.

Stage runtime sprites now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-stage-bg-v001-candidate-a-alpha.png` with contact/pixel-audit evidence in `gamedesign/projects/mine-cards/reviews/`.

FX runtime sprites now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-fx-v004-candidate-d-alpha.png` with contact/pixel-audit evidence in `gamedesign/projects/mine-cards/reviews/`.

Blank UI generated slice9 bases now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-blank-ui-kit-v002-candidate-b-clean.png`; this runtime pass uses only the generated board panel and stage action card where the current layout is large enough for their slice9 margins.

Compact UI generated slice9 bases now included from `gamedesign/projects/mine-cards/art/candidates/mine-cards-compact-ui-kit-v001-candidate-a-cyan-clean.png`; these replace the compact activity chips, node rows, upgrade button, and bottom nav tabs that were previously procedural atlas patches.

Generation provenance: `gamedesign/projects/mine-cards/art/generation_records/mine-nodes-source-sheet-v001.json` and `gamedesign/projects/mine-cards/art/generation_records/mine-cards-icons-v001-candidate-e.json`.

Next art step: inspect the native desktop/portrait screenshots and tune compact layout usage before expanding mechanics.
