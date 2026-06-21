---
type: ProductReadGate
project: ember-road
task: T0024
surface: town-forge-native-v2
verdict: review
timestamp: 2026-06-21T03:50:05.223Z
---

# Product Read Gate - ember-road / town-forge-native-v2

Verdict: **REVIEW**

Screenshot: `build/captures/ember-road/state_town_lantern_upgrade.png`

## Player Read

- Where am I? Old Gate Town Forge, with the Mine Lantern upgrade named in the scene title.
- What should I do now? Forge the Mine Lantern from the source-derived forge plaque/workbench or the compact rail action.
- What changed after input? The scene title, scene badge, route plaque, and rail switch to Lantern Ready / equipped state after forging.
- What is the reward / why continue? The Mine Lantern equips and visibly opens the Depth 2 route for the next Old Mine expedition.
- Why does this look like a game? Painted RPG location, source-derived forge/worktable/lantern/action plaque/result strip, state-specific scene title, route plaques, item rail, and bottom log carry the event as a game screen.

## State Coverage

Required states:
- first_screen
- hud_visible
- primary_action_ready
- primary_action_feedback
- reward_active
- progression_panel_open
- modal_or_choice_open
- locked_or_disabled_state
- resume_or_reentry_state
- transient_stress_state
- town_lantern_upgrade

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
- old_mine_next_delve_choice: build/captures/ember-road/state_old_mine_next_delve_choice.png
- old_mine_delve_reward: build/captures/ember-road/state_old_mine_delve_reward.png
- town_lantern_upgrade: build/captures/ember-road/state_town_lantern_upgrade.png
- town_lantern_forged: build/captures/ember-road/state_town_lantern_forged.png

Not covered / debt:
- resume_or_reentry_state: Resume/re-entry behavior is out of scope for this first native slice.

## Review

Problem: The state-specific scene title improves the first read, but T0024 still needs lead acceptance of the strengthened direction before Depth 2 expansion resumes.

Next: Ask for lead acceptance on the refreshed native forge screenshots or continue with another visual-only correction if rejected.

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
- minor / art_quality: Some support pieces still come from the older Old Gate set, although the forge, worktable, lantern, action plaque, result strip, and badge are source-derived town-forge v2 assets.
- minor / audience_fit: Lead acceptance is still pending after the earlier visual/UX rejection, so feature/content expansion remains frozen until this direction is accepted or redirected.
