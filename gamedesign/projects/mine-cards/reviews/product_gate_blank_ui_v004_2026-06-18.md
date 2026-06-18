---
type: ProductReadGate
project: mine-cards
task: T0001
surface: responsive
verdict: fail
timestamp: 2026-06-17T20:00:41.893Z
---

# Product Read Gate - mine-cards / responsive

Verdict: **FAIL**

Screenshot: `build/captures/mine_cards_blank_ui_v004_landscape_surface.png`

## Player Read

- Where am I? Mine Cards Mining screen: fixed top miner action stage, lower idle mechanics board, and next Copper Pickaxe goal.
- What should I do now? Let Mining run, choose Surface Stone or Copper Vein, and work toward the Copper Pickaxe missing costs.
- What changed after input? Progress fills and rewards/FX fire on mining ticks; selected node and missing upgrade resources are visible.
- What is the reward / why continue? Stone, XP, occasional geode feedback, and a clear next upgrade target give the short loop.
- Why does this look like a game? Generated board and stage frames now push the screen toward a blocky RPG UI, with real icons, stage art, FX, and voxel miner.

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
- hud
- primary_action
- feedback
- blocked

Covered states:
- hud: build/captures/mine_cards_blank_ui_v004_landscape_surface.png
- primary_action: build/captures/mine_cards_blank_ui_v004_landscape_surface.png
- feedback: build/captures/mine_cards_blank_ui_v004_landscape_geode.png

Not covered / debt:
- blocked: compact_generated_controls_not_done

## Review

Problem: Partial blank UI kit integration is visible and cleaner, but T0001 visual gate still cannot pass while compact controls are procedural and the upgrade control is cramped.

Next: Generate or derive compact button/chip/card variants from the blank UI family, integrate them into the small lower-board controls, then recapture four-shot proof and rerun product gate.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 3
- readability: 3
- ui_controls: 2
- action_direction: 3
- art_quality: 3
- audience_fit: 3

Issues:
- major / ui_controls: small chips, node rows, nav buttons, and primary button still rely on compact procedural scaffold rather than safe generated compact variants
- major / readability: primary upgrade button/cost region remains visually cramped and needs a purpose-built compact generated control
- minor / composition: landscape lower board is improved but still dense; generated frame consumes space without compact variants
