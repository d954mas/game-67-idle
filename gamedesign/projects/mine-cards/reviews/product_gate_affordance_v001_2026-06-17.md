---
type: ProductReadGate
project: mine-cards
task: T0001
surface: landscape
verdict: fail
timestamp: 2026-06-17T17:05:55.824Z
---

# Product Read Gate - mine-cards / landscape

Verdict: **FAIL**

Screenshot: `build/captures/mine_cards_affordance_v001_landscape_surface.png`

## Player Read

- Where am I? Mine Cards mining screen with top miner action stage and lower idle mechanics board.
- What should I do now? Let Surface Stone run, watch resources, and work toward Copper Pickaxe in the next-goal panel.
- What changed after input? Progress, geode reward, and upgrade states are visible; lower board now separates active Mining from future systems.
- What is the reward / why continue? Stone, coins, and upgrade readiness become visible; geode pop gives a special reward moment.
- Why does this look like a game? It now reads as a block mining idle RPG screen, but UI art remains procedural and icon treatment is placeholder.

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
- first_screen_idle: build/captures/mine_cards_affordance_v001_landscape_surface.png
- geode_event: build/captures/mine_cards_affordance_v001_landscape_geode.png
- upgrade_unaffordable: build/captures/mine_cards_affordance_v001_upgrade_unaffordable.png
- upgrade_affordable: build/captures/mine_cards_affordance_v001_upgrade_affordable.png
- upgrade_purchased: build/captures/mine_cards_affordance_v001_upgrade_purchased.png

Not covered / debt:
- mining_tick_reward: Not recaptured as a dedicated normal tick reward sequence in this pass.
- locked_copper: Visible, but lock affordance still uses placeholder badge and needs stronger player-facing treatment.
- copper_unlocked: Not recaptured in this pass.
- small_window_stress: Not recaptured after affordability/icon pass.

## Review

Problem: Affordance/readability improved, but final product-read gate still fails on art quality and future activity silhouettes.

Next: Create/integrate a real generated icon/UI family or accept placeholder-art debt before expanding mechanics.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 4
- readability: 4
- ui_controls: 4
- action_direction: 4
- art_quality: 3
- audience_fit: 3

Issues:
- major / art_quality: Future activity and resource icons are reused placeholder atlas sprites, not a proper generated/artist icon family.
- minor / audience_fit: The screen reads more like an idle RPG now, but still lacks polished game UI material and stronger future-activity silhouettes.
