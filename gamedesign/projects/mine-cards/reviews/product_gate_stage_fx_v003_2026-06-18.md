---
type: ProductReadGate
project: mine-cards
task: T0001
surface: responsive
verdict: fail
timestamp: 2026-06-17T19:18:22.507Z
---

# Product Read Gate - mine-cards / responsive

Verdict: **FAIL**

Screenshot: `build/captures/mine_cards_stage_fx_v003_landscape_geode.png`

## Player Read

- Where am I? Mine Cards Mining screen with a fixed top miner action stage, 3D KayKit/Ozz miner, generated mine floor/wall pieces, and lower idle mechanics board.
- What should I do now? Watch Surface Stone mining, read the geode reward pop, then follow NEXT GOAL toward Copper Pickaxe.
- What changed after input? The v003 pass reduces oversized FX/stage stamps, keeps the geode reward in the action stage, and rewrites the upgrade cost rows as direct Need N resource goals.
- What is the reward / why continue? Stone, XP, coins, geode bonus, node unlock progress, and Copper Pickaxe costs are visible; the geode moment now has generated voxel FX instead of only a text callout.
- Why does this look like a game? The stage now reads closer to a game action strip with real sprite/3D assets, but the lower board is still a draft repeated-card UI and not yet a polished idle RPG product screen.

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
- portrait_idle
- portrait_geode

Covered states:
- first_screen_idle: build/captures/mine_cards_stage_fx_v003_landscape_surface.png
- geode_event: build/captures/mine_cards_stage_fx_v003_landscape_geode.png
- portrait_idle: build/captures/mine_cards_stage_fx_v003_portrait_surface.png
- portrait_geode: build/captures/mine_cards_stage_fx_v003_portrait_geode.png

Not covered / debt:
- (none)

## Review

Problem: Stage/FX integration improved and four-state proof is current, but the first screen still fails the product bar because lower-board hierarchy and final UI art are not polished enough.

Next: Keep feature expansion frozen. Generate or accept blank UI kit/decor overlay source families for board/card/button treatment, then recapture four-shot proof and rerun product gate.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 3
- readability: 4
- ui_controls: 3
- action_direction: 4
- art_quality: 3
- audience_fit: 3

Issues:
- major / composition: Landscape still has unused stage space and same-weight lower-board rectangles.
- major / ui_controls: NEXT GOAL is clearer, but cost rows and button still need a stronger authored upgrade card treatment.
- major / art_quality: FX/stage generated crops are clean, but the board/slice9 family remains procedural/draft-level rather than final generated UI.
- minor / action_direction: Actor, rock, reward and progress now read in order, but the reward still sits like a UI badge more than a physical mined payoff.
