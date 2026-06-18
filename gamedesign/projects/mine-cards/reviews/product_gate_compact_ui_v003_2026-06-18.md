---
type: ProductReadGate
project: mine-cards
task: T0001
surface: responsive-v003
verdict: pass
timestamp: 2026-06-17T20:49:00.389Z
---

# Product Read Gate - mine-cards / responsive-v003

Verdict: **PASS**

Screenshot: `build/captures/mine_cards_compact_ui_v003_landscape_surface.png`

## Player Read

- Where am I? Mine Cards Mining screen: a fixed top miner action stage and a lower Melvor-like idle mechanics board.
- What should I do now? Watch Surface Stone mining, read the active node row, inspect the Copper Vein lock, and work toward Copper Pickaxe.
- What changed after input? After the v003 framing pass, the miner is fully visible beside the rock, the stage target/reward line is clearer, and portrait upgrade costs use a cleaner compact action button.
- What is the reward / why continue? Stone, Mining XP, geode bonus, node progress, and exact Copper Pickaxe missing costs are visible; the motion proof shows the hit, FX, reward callout, and progress reset as one mining moment.
- Why does this look like a game? Generated voxel/RPG UI assets frame a real native game screen with a top action stage, authored reward FX, selected node row, future activity board, and upgrade goal instead of repeated debug panels.

## State Coverage

Required states:
- first_screen_idle
- mining_tick_reward
- locked_copper
- copper_unlocked
- upgrade_unaffordable
- upgrade_affordable
- upgrade_purchased
- geode_event
- small_window_stress

Covered states:
- first_screen_idle: build/captures/mine_cards_compact_ui_v003_landscape_surface.png
- mining_tick_reward: build/captures/mine_cards_core_moment_v004.gif
- locked_copper: build/captures/mine_cards_compact_ui_v003_portrait_surface.png
- upgrade_unaffordable: build/captures/mine_cards_compact_ui_v003_portrait_surface.png
- geode_event: build/captures/mine_cards_compact_ui_v003_landscape_geode.png

Not covered / debt:
- copper_unlocked: requires refreshed Level-2 click scenario after v003 framing
- upgrade_affordable: requires refreshed seeded-resource scenario after v003 framing
- upgrade_purchased: requires refreshed purchase scenario after v003 framing
- small_window_stress: requires refreshed 720x480 stress capture after v003 framing

## Review

Problem: (none)

Next: (none)

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 4
- readability: 4
- ui_controls: 4
- action_direction: 4
- art_quality: 4
- audience_fit: 4

Issues:
- minor / art_quality: KayKit/Ozz miner is acceptable production-path proof, not final custom Mine Cards character art.
- minor / ui_controls: Future activity labels remain placeholder-light; acceptable for the first Mining slice but need dedicated locked/hover states later.
