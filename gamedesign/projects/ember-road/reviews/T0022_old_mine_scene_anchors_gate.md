---
type: ProductReadGate
project: ember-road
task: T0022
surface: desktop
verdict: review
timestamp: 2026-06-20T19:53:30.109Z
---

# Product Read Gate - ember-road / desktop

Verdict: **REVIEW**

Screenshot: `build/captures/ember-road/state_old_mine_next_delve_choice.png`

## Player Read

- Where am I? Old Mine Entrance after Depth 1, with the mine cache and Depth 2 lock visible in the scene.
- What should I do now? Delve the highlighted ember cache or return to Old Gate.
- What changed after input? The old wolf/ring reward overlay is gone; cache, lock, and mine route state are visible on the mine backdrop.
- What is the reward / why continue? The cache promises +1 shard before input and becomes CACHE TAKEN after the delve reward.
- Why does this look like a game? The scene now carries part of the RPG meaning itself: cave threshold, locked deeper route, cache marker, route strip, rail action, and bottom log align.

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
- old_mine_next_delve_choice
- old_mine_delve_reward
- resume_or_reentry_state

Covered states:
- old_mine_next_delve_choice: build/captures/ember-road/state_old_mine_next_delve_choice.png
- old_mine_delve_reward: build/captures/ember-road/state_old_mine_delve_reward.png
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
- resume_or_reentry_state: Resume/re-entry behavior is out of scope for this first native slice.

## Review

Problem: Scene-integrated UX is improved, but final dedicated Old Mine object/UI art is still not done.

Next: If accepted, decide between a dedicated mine object art sheet and the next gameplay slice; if rejected, revise the mine target/fake shot before more systems.

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
- minor / readability: D2 LOCKED marker is readable but still visually dark against the cave mouth.
- minor / art_quality: Scene anchors reuse current atlas icons rather than dedicated Old Mine object art.
