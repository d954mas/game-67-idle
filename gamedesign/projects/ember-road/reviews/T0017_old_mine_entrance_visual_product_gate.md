---
type: ProductReadGate
project: ember-road
task: T0017
surface: desktop
verdict: pass
timestamp: 2026-06-20T18:42:31.197Z
---

# Product Read Gate - ember-road / desktop

Verdict: **PASS**

Screenshot: `build/captures/ember-road/state_modal_or_choice_open.png`

## Player Read

- Where am I? Old Mine Entrance after level 2; the route now shows a dedicated cave-and-timber mine backdrop instead of the North Road scene.
- What should I do now? Use the right-side choice panel: Scout is intentionally next-slice locked, Back returns to Old Gate, and the mine route plaque remains open.
- What changed after input? Entering Old Mine switches to old_mine, shows the dedicated mine entrance bitmap, keeps the scene visible, and exposes ember.mine.choice / ember.mine.back in DevAPI.
- What is the reward / why continue? The player now receives a clearer next-destination fantasy: the unlocked route has its own cave entrance, torchlight, and danger threshold before any dungeon systems are added.
- Why does this look like a game? The screen reads as a fantasy browser RPG: ornate HUD and rail, painted mine location art, visible hero at the entrance, route plaques, parchment choice panel, and distinct locked/active choices.

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
- resume_or_reentry_state

Covered states:
- modal_or_choice_open: build/captures/ember-road/state_modal_or_choice_open.png
- first_screen: build/captures/ember-road/state_first_screen.png
- hud_visible: build/captures/ember-road/state_hud_visible.png
- primary_action_ready: build/captures/ember-road/state_primary_action_ready.png
- locked_or_disabled_state: build/captures/ember-road/state_locked_or_disabled_state.png
- primary_action_feedback: build/captures/ember-road/state_primary_action_feedback.png
- reward_active: build/captures/ember-road/state_reward_active.png
- transient_stress_state: build/captures/ember-road/state_transient_stress_state.png
- progression_panel_open: build/captures/ember-road/state_progression_panel_open.png

Not covered / debt:
- resume_or_reentry_state: Resume/re-entry behavior is out of scope for this first native slice.

## Review

Problem: (none)

Next: (none)

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
- minor / ui_controls: Scout remains a next-slice locked choice rather than playable mine content.
- minor / readability: The right rail is dense, but the active choice and mine location are now readable without overlap.
