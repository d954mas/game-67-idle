---
type: ProductReadGate
project: mine-cards
task: T0001
surface: responsive
verdict: fail
timestamp: 2026-06-17T17:24:45.606Z
---

# Product Read Gate - mine-cards / responsive

Verdict: **FAIL**

Screenshot: `build/captures/mine_cards_stage_rescue_v007_landscape_surface.png`

## Player Read

- Where am I? Mine Cards mining screen with a top miner action stage and lower idle board.
- What should I do now? Watch Surface Stone mining and work toward Copper Pickaxe; this is more readable than v004 but still not clear enough for a first-time player.
- What changed after input? Progress and geode reward appear in the stage, with improved actor/target placement.
- What is the reward / why continue? Stone, coins, XP, and upgrade cost are visible, but the reward loop still relies on text and placeholder panels.
- Why does this look like a game? It is closer to a game screen than v004, but still reads as a procedural prototype rather than a polished voxel idle RPG.

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
- first_screen_idle: build/captures/mine_cards_stage_rescue_v007_landscape_surface.png
- geode_event: build/captures/mine_cards_stage_rescue_v007_landscape_geode.png
- small_window_stress: build/captures/mine_cards_stage_rescue_v007_portrait_surface.png

Not covered / debt:
- mining_tick_reward: No dedicated normal tick reward sequence reviewed in this pass.
- locked_copper: Visible but still weak due placeholder badge/chip hierarchy.
- copper_unlocked: Not recaptured in this pass.
- upgrade_unaffordable: Covered by prior affordance pass, not this stage rescue.
- upgrade_affordable: Covered by prior affordance pass, not this stage rescue.
- upgrade_purchased: Covered by prior affordance pass, not this stage rescue.

## Review

Problem: Stage layout tuning improved the actor/target relationship, but the screen still fails product read because it lacks real stage art/UI/icon family and portrait-first composition.

Next: Stop numeric layout tuning. Create the generated/artist UI and stage/icon source family, then integrate it into the native screen and rerun landscape/portrait product gate.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 3
- readability: 4
- ui_controls: 3
- action_direction: 3
- art_quality: 2
- audience_fit: 2

Issues:
- major / composition: Stage rescue improved actor scale and removed the disconnected status badge, but the top stage is still mostly empty procedural chrome rather than a strong game scene.
- major / action_direction: The miner/rock relationship is clearer than v004, but the hit moment still lacks a proper staged impact pose/effect and portrait crops the character into the header area.
- major / ui_controls: The lower board still has same-weight rectangular active activity, future chips, node cards, upgrade, and nav; hierarchy is serviceable but not product quality.
- major / art_quality: Runtime still depends on procedural placeholder UI and reused placeholder markers; a generated/artist UI and icon family is required before feature expansion.
- major / audience_fit: Portrait is captured and usable, but not phone-first or Capybara-like enough; the character/action zone does not yet dominate first read.
