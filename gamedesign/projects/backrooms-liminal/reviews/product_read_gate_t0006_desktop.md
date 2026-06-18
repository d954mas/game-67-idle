---
type: ProductReadGate
project: backrooms-liminal
task: T0006
surface: desktop
verdict: pass
timestamp: 2026-06-18T16:34:54.648Z
---

# Product Read Gate - backrooms-liminal / desktop

Verdict: **PASS**

Screenshot: `build/captures/backrooms_t0006_blackout_ambush.png`

## Player Read

- Where am I? Return path inside the Backrooms corridor during a wrong-turn blackout ambush
- What should I do now? Run away from the closer stalker and keep moving toward the exit
- What changed after input? Wrong route choice now cuts the lights, pulls the stalker closer, raises pressure, and displays LIGHTS OUT - RUN; safe choice produces a green relief pulse
- What is the reward / why continue? Correct lane choices keep pressure lower and preserve the run; wrong turns now create a scary but recoverable chase spike
- Why does this look like a game? 3D yellow liminal corridor with flashlight cone, blackout-darkened ceiling lights, red emergency warning, close stalker silhouette, shadows, and readable horror HUD

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

Covered states:
- first_screen: build/captures/backrooms_t0006_first_screen.png
- hud_visible: build/captures/backrooms_t0006_blackout_ambush_uizoom.png
- primary_action_ready: build/captures/backrooms_t0006_choice_ready.png
- primary_action_feedback: build/captures/backrooms_t0006_blackout_ambush.png
- reward_active: build/captures/backrooms_t0006_safe_turn_relief.png
- modal_or_choice_open: build/captures/backrooms_t0006_choice_ready.png
- resume_or_reentry_state: build/captures/backrooms_t0006_first_screen.png
- transient_stress_state: build/captures/backrooms_t0006_blackout_ambush.png

Not covered / debt:
- progression_panel_open: not_in_this_blackout_ambush_slice
- locked_or_disabled_state: not_in_this_blackout_ambush_slice

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
- (none)
