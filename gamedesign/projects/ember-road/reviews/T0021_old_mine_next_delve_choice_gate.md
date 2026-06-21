---
type: ProductReadGate
project: ember-road
task: T0021
surface: desktop
verdict: review
timestamp: 2026-06-20T19:47:35.263Z
---

# Product Read Gate - ember-road / desktop

Verdict: **REVIEW**

Screenshot: `build/captures/ember-road/state_old_mine_next_delve_choice.png`

## Player Read

- Where am I? Old Mine Entrance after Depth 1 is cleared.
- What should I do now? Delve the visible ember cache or return to Old Gate.
- What changed after input? The screen replaced the dead CLEARED state with an active DELVE/CACHE choice and route/log feedback.
- What is the reward / why continue? Delving grants a small cache reward: +1 shard, +2 gold, +3 XP; the next depth remains locked.
- Why does this look like a game? The mine scene stays dominant while route, right rail, and bottom log now describe a real next RPG choice.

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

Problem: The next-action UX mismatch is improved, but this is not final visual art or a full accepted mine loop.

Next: If accepted, decide whether to push Depth 2 or return to town/equipment polish; if rejected, revise the fake shot/reference target before more runtime systems.

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
- minor / art_quality: Still uses the current runtime atlas/UI pieces rather than a final accepted Old Mine UI kit.
