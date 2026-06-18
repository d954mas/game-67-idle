---
type: ProductReadGate
project: mine-cards
task: T0001
surface: responsive
verdict: fail
timestamp: 2026-06-17T17:17:43.691Z
---

# Product Read Gate - mine-cards / responsive

Verdict: **FAIL**

Screenshot: `build/captures/mine_cards_stage_review_v004_landscape_surface.png`

## Player Read

- Where am I? Mine Cards mining screen with a top action stage and lower idle board.
- What should I do now? The intended action is to watch Surface Stone mining and work toward the Copper Pickaxe, but the visual hierarchy does not make that obvious enough.
- What changed after input? Progress and geode reward can change, but the feedback moment is visually small and competes with status/chrome.
- What is the reward / why continue? Stone, coins, XP, and upgrade cost are present, but the reward loop is still too text/panel driven.
- Why does this look like a game? It resembles a debug/procedural idle layout more than a polished voxel idle RPG screen.

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
- first_screen_idle: build/captures/mine_cards_stage_review_v004_landscape_surface.png
- geode_event: build/captures/mine_cards_stage_review_v004_landscape_geode.png
- small_window_stress: build/captures/mine_cards_stage_review_v004_portrait_surface.png

Not covered / debt:
- mining_tick_reward: No dedicated normal tick reward sequence reviewed in this pass.
- locked_copper: Visible but still weak due placeholder badge/chip hierarchy.
- copper_unlocked: Not recaptured in this pass.
- upgrade_unaffordable: Covered by prior affordance pass, not this stage review.
- upgrade_affordable: Covered by prior affordance pass, not this stage review.
- upgrade_purchased: Covered by prior affordance pass, not this stage review.

## Review

Problem: Lead-visible screenshots still do not read as a game: top action stage is disconnected/empty, character-target contact is weak, and landscape/portrait are not strong responsive compositions.

Next: Freeze mechanic expansion. Produce a proper composition rescue: reference-backed stage layout, generated/artist UI/icon family, and native landscape+portrait screenshot proof before adding mechanics.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 2
- readability: 4
- ui_controls: 3
- action_direction: 2
- art_quality: 2
- audience_fit: 2

Issues:
- blocker / action_direction: The miner, pickaxe, and rock still do not read as one mining action; the miner is small/partial and the target contact is ambiguous.
- major / composition: The top stage is a large empty panel with a small action cluster and a disconnected status badge; it does not anchor the screen like the fake shot/reference direction.
- major / ui_controls: The lower mechanics board still uses similar rectangular weights for active activity, future tabs, node cards, upgrade, and nav, so priority is hard to scan.
- major / art_quality: The screen still relies on procedural/runtime placeholder UI and placeholder icon treatment instead of a coherent generated/artist Mine Cards UI family.
- major / audience_fit: Portrait exists, but it is a stacked version of the same dense panel language rather than a phone-first idle screen with a strong character/action hero.
