---
type: ProductReadGate
project: backrooms-liminal
task: T0010
surface: desktop
verdict: fail
timestamp: 2026-06-18T19:29:48.813Z
---

# Product Read Gate - backrooms-liminal / desktop

Verdict: **FAIL**

Screenshot: `build/captures/backrooms_t0010_impossible_geometry.png`

## Player Read

- Where am I? Yellow Backrooms corridor facing a non-Euclidean room aperture with material-kind-aware native geometry, stronger center light spill, side shadow falloff, floor grout, wall seams, back-wall strips, ceiling grid, jambs, threshold, fixture, conduit, and landmark column
- What should I do now? Use the copied mark as evidence, find the missing handle, and test the locked door
- What changed after input? The mark appears inside the impossible room, and the runtime now shades 432 native nt_gfx overlay vertices with material-kind lighting and shadow response
- What is the reward / why continue? The player learns the space can be tested and later stabilized by fitting the found handle
- Why does this look like a game? Native 3D liminal horror view with deeper portal room, stronger solid-room lighting, shadowed aperture, stained wallpaper, carpet seams, floor/wall/ceiling detail, minimal journal UI, and texture-backed material geometry

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
- first_screen: covered by earlier first-screen gates, not the focus of T0010 portal proof
- modal_or_choice_open: T0010 has no modal/choice UI
- resume_or_reentry_state: not part of portal-memory spike
- transient_stress_state: covered by blackout/sprint tasks, not this portal proof

## Review

Problem: The portal proof now reads more solid because material kinds drive different alpha, light spill, side shadows, seam darkening, and depth falloff, but it still lacks true integrated 3D surfaces and render-target-backed lighting

Next: Move from overlay-lit material detail to proper authored 3D room surfaces or T0011 render-target portal rendering, then revisit product gate for art_quality and audience_fit

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
- major / art_quality: Material-kind lighting improves solidity and shadow response, but the room is still an overlay pass rather than a fully integrated physically lit 3D space
- major / audience_fit: The non-Euclidean hook is clearer and scarier, but the screenshot still falls short of the distinctive high-quality horror target needed to stand out among many Backrooms games
