---
type: ProductReadGate
project: ember-road
task: T0019
surface: desktop
verdict: review
timestamp: 2026-06-20T19:30:50.991Z
---

# Product Read Gate - ember-road / desktop

Verdict: **REVIEW**

Screenshot: `build/captures/ember-road/state_old_mine_depth_encounter.png`

## Player Read

- Where am I? Old Mine Entrance after clearing the first Depth 1 encounter.
- What should I do now? The player sees Depth 1 Clear, reads the encounter reward, and can return to Old Gate.
- What changed after input? After scout, resolve_old_mine_depth defeats the Cave Bat, records damage, updates shards/gold/XP, changes the Mine route plaque to D1 DONE, and shows a framed depth result log.
- What is the reward / why continue? The result grants +2 ember shards, +4 gold, +6 XP, records 3 bat damage, and marks bat_defeated/depth_resolved in state.
- Why does this look like a game? The screen now reads as a fantasy RPG result state: dominant mine scene, right-side result rail, route-state plaques, framed bottom report log, and persistent hero HUD.

## State Coverage

Required states:
- first_screen
- hud_visible
- primary_action_ready
- primary_action_feedback
- reward_active
- locked_or_disabled_state
- transient_stress_state
- progression_panel_open
- modal_or_choice_open
- old_mine_scout_result
- old_mine_depth_encounter
- resume_or_reentry_state

Covered states:
- first_screen: build/captures/ember-road/state_first_screen.png
- hud_visible: build/captures/ember-road/state_hud_visible.png
- primary_action_ready: build/captures/ember-road/state_primary_action_ready.png
- locked_or_disabled_state: build/captures/ember-road/state_locked_or_disabled_state.png
- primary_action_feedback: build/captures/ember-road/state_primary_action_feedback.png
- reward_active: build/captures/ember-road/state_reward_active.png
- transient_stress_state: build/captures/ember-road/state_transient_stress_state.png
- progression_panel_open: build/captures/ember-road/state_progression_panel_open.png
- modal_or_choice_open: build/captures/ember-road/state_modal_or_choice_open.png
- old_mine_scout_result: build/captures/ember-road/state_old_mine_scout_result.png
- old_mine_depth_encounter: build/captures/ember-road/state_old_mine_depth_encounter.png

Not covered / debt:
- resume_or_reentry_state: Resume/re-entry remains out of scope for this slice.

## Review

Problem: This is still one deterministic depth result, not a repeatable dungeon loop or final sliced UI kit.

Next: If accepted, build the next narrow slice: either a repeatable depth loop or town/equipment polish; if rejected, update the exact visual/UX mismatch before more runtime work.

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
- minor / art_quality: Depth result still reuses current atlas/UI pieces instead of final cut components from a dedicated mine UI kit.
