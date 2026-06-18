---
type: ProductReadGate
project: mine-cards
task: T0001
surface: responsive
verdict: fail
timestamp: 2026-06-17T18:46:48.551Z
---

# Product Read Gate - mine-cards / responsive

Verdict: **FAIL**

Screenshot: `build/captures/mine_cards_stage_art_v002_landscape_surface.png`

## Player Read

- Where am I? Mine Cards mining screen with native 3D miner, mined rock, generated mine floor/light/wall stage pieces, and lower idle board.
- What should I do now? Watch the miner mine Surface Stone, read progress/reward, then use the lower board to understand nodes and the Copper Pickaxe goal.
- What changed after input? Stage-art v002 proves the generated/cropped stage source works in native runtime: the miner now stands on a mine floor with light and wall/debris context, and geode reward appears beside the mined target.
- What is the reward / why continue? Stone, coins, XP, progress, node lock, and pickaxe cost are visible, but the reward moment still relies on a text callout and lacks satisfying impact FX.
- Why does this look like a game? It is closer to a game stage than v011 because the character is grounded in generated mine art, but the screen still reads as a dense UI prototype rather than a polished idle RPG screen.

## State Coverage

Required states:
- first_screen_idle
- geode_event
- portrait_idle
- portrait_geode

Covered states:
- first_screen_idle: build/captures/mine_cards_stage_art_v002_landscape_surface.png
- geode_event: build/captures/mine_cards_stage_art_v002_landscape_geode.png
- portrait_idle: build/captures/mine_cards_stage_art_v002_portrait_surface.png
- portrait_geode: build/captures/mine_cards_stage_art_v002_portrait_geode.png

Not covered / debt:
- (none)

## Review

Problem: Stage art source/crop/runtime integration now works, but the screen still fails the product bar because the stage composition, reward FX, and lower-board hierarchy are not yet polished.

Next: Keep feature expansion frozen. Generate or accept a sprite/FX sheet for stone hit/geode reward, reduce lantern/stage visual dominance, and redesign lower-board hierarchy before adding mechanics.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 3
- readability: 4
- ui_controls: 3
- action_direction: 3
- art_quality: 3
- audience_fit: 3

Issues:
- major / composition: Generated stage art grounds the miner, but landscape still has weak stage balance and the lower board remains same-weight rectangles.
- major / action_direction: The cause-effect chain is clearer, but the hit/reward moment still lacks generated impact FX and relies on a text callout.
- major / art_quality: The alpha-cleaned stage source passes asset audit, but the lantern is too bright and poster-like for final runtime composition.
- major / audience_fit: Portrait is functional but still not phone-first enough; future tabs and lower board dominate the first read after the stage.
