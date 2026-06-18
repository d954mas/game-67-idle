---
type: ProductReadGate
project: backrooms-liminal
task: T0010
surface: desktop
verdict: fail
timestamp: 2026-06-18T20:47:57.375Z
---

# Product Read Gate - backrooms-liminal / desktop

Verdict: **FAIL**

Screenshot: `build/captures/backrooms_t0010_impossible_geometry.png`

## Player Read

- Where am I? Yellow Backrooms corridor facing a deeper non-Euclidean room aperture with an asset-backed material atlas, denser solid native wall/floor/ceiling surfaces, nested back-wall frame, copied mark, conduit, threshold, and landmark surfaces
- What should I do now? Use the copied mark as evidence, find the missing handle, and test the locked door
- What changed after input? The mark appears inside the impossible room; runtime now draws 294 solid-pass native nt_gfx vertices before 450 blended detail vertices, loads assets/backrooms-liminal/materials/portal_material_atlas.ppm, and reports material_atlas_loaded_from_asset=true
- What is the reward / why continue? The player learns the space can be tested and later stabilized by fitting the found handle
- Why does this look like a game? Native 3D liminal horror view with a larger-inside portal room, asset-backed stained wallpaper/carpet/ceiling/trim atlas, nested dark frame, fluorescent light spill, shadowed aperture, minimal journal UI, and stronger opaque native interior layer

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

Problem: The asset boundary now exists and the game loads the portal material atlas from a project source file, but the source is still procedural and lighting is not physically integrated enough for production-quality realistic Backrooms rendering

Next: Replace the procedural PPM source with generated or artist-authored material sources, or unblock T0011 render-target lighting; keep content expansion frozen while art_quality/audience_fit remain under 4

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
- major / art_quality: Material atlas is now an asset-loaded source file, but it is still procedural and lacks production-grade material detail.
- major / audience_fit: The impossible-room hook and asset path are clearer, but the screenshot still needs a distinctive high-quality horror look to stand out among many Backrooms games.
