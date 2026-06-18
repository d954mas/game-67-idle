---
type: ProductReadGate
project: mine-cards
task: T0001
surface: responsive
verdict: fail
timestamp: 2026-06-17T20:17:20.079Z
---

# Product Read Gate - mine-cards / responsive

Verdict: **FAIL**

Screenshot: `build/captures/mine_cards_compact_ui_v001_landscape_surface.png`

## Player Read

- Where am I? Mine Cards Mining screen: fixed top miner stage plus lower activity board.
- What should I do now? Watch Mining run, read active Surface Stone, inspect locked Copper Vein, then work toward Copper Pickaxe.
- What changed after input? Runtime now uses generated compact chip/card/button/nav slice9 assets instead of procedural rectangles; framebuffer proof shows the assets render and scale.
- What is the reward / why continue? Stone, XP, coins, geode feedback, progress bar, and pickaxe costs are visible.
- Why does this look like a game? Generated voxel/RPG UI art is now present across the lower board, not only in the stage and icons.

## State Coverage

Required states:
- (none)

Covered states:
- hud_visible: build/captures/mine_cards_compact_ui_v001_landscape_surface.png
- reward_active: build/captures/mine_cards_compact_ui_v001_landscape_geode.png
- progression_panel_open: build/captures/mine_cards_compact_ui_v001_portrait_surface.png

Not covered / debt:
- (none)

## Review

Problem: Extraction/cropping is fixed, but the compact generated art is too visually heavy when applied wholesale to the lower board; this is an art/application mismatch, not a chroma-key failure.

Next: Keep the saved/cut compact sheet as source evidence. Create a calmer compact runtime variant: use generated art for selected/primary states, derive simpler idle rows/tabs from the same source or generate a low-ornament compact sheet, then recapture landscape/portrait and rerun product gate.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 3
- readability: 3
- ui_controls: 3
- action_direction: 3
- art_quality: 3
- audience_fit: 3

Issues:
- major / ui_controls: Compact rows and nav tabs are source-derived and cleanly cut, but their ornate corners/green strips dominate the lower board at gameplay sizes.
- major / composition: The lower board now has too many competing generated frames; the active row, future chips, node rows, upgrade card, and nav tabs all fight for attention.
- minor / readability: Text remains readable by zoom audit, but dense decorative controls make scanning slower than the Melvor-like reference direction.
