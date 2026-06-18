---
type: ProductReadGate
project: backrooms-liminal
task: T0010
surface: desktop
verdict: fail
timestamp: 2026-06-18T19:55:10.513Z
---

# Product Read Gate - backrooms-liminal / desktop

Verdict: **FAIL**

Screenshot: `build/captures/backrooms_t0010_impossible_geometry.png`

## Player Read

- Where am I? Yellow Backrooms corridor facing a deeper non-Euclidean room aperture with a cleaner opaque-looking portal cut, denser native room surfaces, reduced ghost frame artifacts, floor/wall/ceiling panels, fluorescent spill, copied mark, conduit, threshold, and landmark column
- What should I do now? Use the copied mark as evidence, find the missing handle, and test the locked door
- What changed after input? The mark appears inside the impossible room; the runtime now uses a fully opaque fullscreen portal-room composite inside the cut while keeping the 450-vertex native overlay as weaker authored surface/detail geometry
- What is the reward / why continue? The player learns the space can be tested and later stabilized by fitting the found handle
- Why does this look like a game? Native 3D liminal horror view with a larger-inside portal room, stained wallpaper, carpet seams, ceiling grid, shadowed aperture, minimal journal UI, cleaner portal rim, and texture-backed material geometry

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
- hud_visible: build/captures/backrooms_t0010_impossible_geometry.png
- primary_action_ready: build/captures/backrooms_t0010_drawn_mark.png
- primary_action_feedback: build/captures/backrooms_t0010_impossible_geometry.png
- reward_active: build/captures/backrooms_t0010_revealed_exit.png
- progression_panel_open: build/captures/backrooms_t0010_impossible_geometry.png
- locked_or_disabled_state: build/captures/backrooms_t0010_locked_door.png

Not covered / debt:
- first_screen: covered by earlier first-screen gates, not this portal-proof screenshot
- modal_or_choice_open: T0010 has no modal/choice UI
- resume_or_reentry_state: not part of portal-memory spike
- transient_stress_state: covered by blackout/sprint tasks, not this portal proof

## Review

Problem: The portal cut is cleaner and more opaque, but the visual bar still needs real integrated 3D surface lighting/render-target depth

Next: Use this cleaner aperture as the stopgap, then move to opaque authored interior geometry or T0011 render-target rendering; focus next on physically convincing light/material response instead of more blended frame decoration

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 4
- readability: 4
- ui_controls: 4
- action_direction: 4
- art_quality: 3
- audience_fit: 3

Issues:
- major / art_quality: The opaque portal cut and weaker external frame reduce the shader-overlay artifact, but the room still lacks true physically integrated authored 3D/render-target lighting
- major / audience_fit: The non-Euclidean hook reads cleaner, but the screenshot still needs a more distinctive high-quality horror look to stand out among many Backrooms games
