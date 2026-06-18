---
type: ProductReadGate
project: mine-cards
task: T0001
surface: landscape
verdict: fail
timestamp: 2026-06-17T16:55:00.921Z
---

# Product Read Gate - mine-cards / landscape

Verdict: **FAIL**

Screenshot: `build/captures/mine_cards_board_hierarchy_v002_landscape_geode.png`

## Player Read

- Where am I? Mine Cards mining screen, with the miner working in the top action stage.
- What should I do now? Let Surface Stone run, then use the lower board to pick nodes and work toward Copper Pickaxe.
- What changed after input? Mining progress and geode reward are visible, but the lower board still needs stronger state proof.
- What is the reward / why continue? Stone and coins increase; geode pop shows the special reward moment.
- Why does this look like a game? The stage and board now read more like an idle RPG screen, but polish and state coverage are incomplete.

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
- first_screen_idle: build/captures/mine_cards_board_hierarchy_v002_landscape_surface.png
- geode_event: build/captures/mine_cards_board_hierarchy_v002_landscape_geode.png

Not covered / debt:
- mining_tick_reward: Not recaptured as dedicated tick state in this pass.
- locked_copper: Visible but lock affordance remains weak after compact board pass.
- copper_unlocked: Not recaptured in this pass.
- upgrade_unaffordable: Need dedicated unaffordable screenshot/state proof after hierarchy pass.
- upgrade_affordable: Not covered by this visual pass.
- upgrade_purchased: Not covered by this visual pass.
- small_window_stress: Not recaptured after board hierarchy pass.
- geode_event: not covered by this gate

## Review

Problem: Lower board hierarchy improved but does not yet pass as a complete playable game screen.

Next: Cover live-state matrix upgrade scenarios and improve lower-board affordance/readability before expanding mechanics.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 3
- readability: 4
- ui_controls: 3
- action_direction: 4
- art_quality: 2
- audience_fit: 3

Issues:
- major / ui_controls: Landscape lower board is still cramped and the upgrade affordability explanation is too small.
- major / art_quality: Future activity chips still lack icons or distinct silhouettes, so the board is improved hierarchy but not polished game UI.
