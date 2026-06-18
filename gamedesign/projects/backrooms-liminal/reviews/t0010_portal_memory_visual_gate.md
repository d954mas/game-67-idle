---
type: ProductReadGate
project: backrooms-liminal
task: T0010
surface: desktop
verdict: fail
timestamp: 2026-06-18T20:08:59.471Z
---

# Product Read Gate - backrooms-liminal / desktop

Verdict: **FAIL**

Screenshot: `build/captures/backrooms_t0010_impossible_geometry.png`

## Player Read

- Where am I? Yellow Backrooms corridor facing a deeper non-Euclidean room aperture with denser native authored shell planes, stronger ceiling soffit/back wall mass, darker side returns, copied mark, conduit, threshold, and landmark column
- What should I do now? Use the copied mark as evidence, find the missing handle, and test the locked door
- What changed after input? The mark appears inside the impossible room; the runtime now combines the opaque fullscreen portal composite with a 492-vertex native nt_gfx room mesh, including 42 solid-shell vertices for floor, side walls, back wall, ceiling, soffit, and center rib surfaces
- What is the reward / why continue? The player learns the space can be tested and later stabilized by fitting the found handle
- Why does this look like a game? Native 3D liminal horror view with a larger-inside portal room, stained wallpaper, carpet seams, ceiling grid, shadowed aperture, minimal journal UI, and denser authored interior room planes

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

Problem: The authored shell layer improves room construction evidence, but the visual bar still needs true opaque authored 3D geometry or render-target depth instead of a blended room proxy

Next: Use the solid-shell overlay as the bridge, then either build the portal interior as real opaque geometry in the native pass or unblock T0011 render-target portal lighting; avoid adding more cosmetic shell plates

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
- major / art_quality: The authored shell layer makes the room read more constructed, but it is still blended over a fullscreen composite rather than true opaque geometry or render-target portal lighting
- major / audience_fit: The impossible-room hook is clearer, but the image still needs a more distinctive high-quality horror look to stand out among many Backrooms games
