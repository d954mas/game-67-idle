---
type: ProductReadGate
project: backrooms-liminal
task: T0007
surface: desktop
verdict: pass
timestamp: 2026-06-18T16:43:53.455Z
---

# Product Read Gate - backrooms-liminal / desktop

Verdict: **PASS**

Screenshot: `build/captures/backrooms_t0007_sprint_escape.png`

## Player Read

- Where am I? Return path during a blackout ambush while the stalker is close
- What should I do now? Hold Shift while moving toward the exit to sprint away from the stalker
- What changed after input? Sprint now becomes a real held input: it moves farther during blackout, lowers stalker pressure compared with no sprint, and drains more battery
- What is the reward / why continue? The player gets a recoverable escape action for wrong turns, trading battery for distance and lower chase pressure
- Why does this look like a game? 3D liminal corridor with blackout lighting, flashlight cone, close stalker silhouette, red sprint warning, and readable horror HUD controls

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
- first_screen: build/captures/backrooms_t0007_first_screen.png
- hud_visible: build/captures/backrooms_t0007_sprint_escape_uizoom.png
- primary_action_ready: build/captures/backrooms_t0007_sprint_escape.png
- primary_action_feedback: build/captures/backrooms_t0007_sprint_status.json
- reward_active: build/captures/backrooms_t0007_sprint_status.json
- modal_or_choice_open: build/captures/backrooms_t0007_sprint_escape.png
- resume_or_reentry_state: build/captures/backrooms_t0007_first_screen.png
- transient_stress_state: build/captures/backrooms_t0007_sprint_escape.png

Not covered / debt:
- progression_panel_open: not_in_this_sprint_escape_slice
- locked_or_disabled_state: not_in_this_sprint_escape_slice

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
