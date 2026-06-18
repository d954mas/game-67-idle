---
type: ProductReadGate
project: backrooms-liminal
task: T0010
surface: desktop
verdict: fail
timestamp: 2026-06-18T18:09:11.423Z
---

# Product Read Gate - backrooms-liminal / desktop

Verdict: **FAIL**

Screenshot: `build/captures/backrooms_t0010_impossible_geometry.png`

## Player Read

- Where am I? A stained yellow Backrooms room where a thick wall cut exposes a deeper room that cannot fit inside the corridor
- What should I do now? Use the copied mark as evidence, find the missing handle, and test the locked door
- What changed after input? The mark appears on the far wall of the impossible room, the journal updates, the locked door rejects missing-handle use, and fitting the handle reveals the exit
- What is the reward / why continue? The player learns that space can copy evidence and can open the real path by proving the room relationship
- Why does this look like a game? Native 3D liminal horror scene with a non-Euclidean room cut, stronger rim/contact shadow, stained wallpaper, darker corners, fluorescent depth cues, copied wall mark, locked-door rule, and minimal journal prompt

## State Coverage

Required states:
- (none)

Covered states:
- (none)

Not covered / debt:
- (none)

## Review

Problem: T0010 now has better physical framing and contact shadow around the impossible room, but the screenshot still does not meet the requested high-quality scary realistic Backrooms bar.

Next: Move beyond one-file shader tricks: introduce or emulate authored wall thickness, better carpet/wall material layers, stronger fluorescent shadowing, and a cleaner prompt treatment.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 4
- readability: 4
- ui_controls: 3
- action_direction: 4
- art_quality: 3
- audience_fit: 3

Issues:
- major / art_quality: The wall cut has stronger rim and contact shadow, but still lacks real authored geometry and high-fidelity material response
- minor / ui_controls: The journal and prompt are readable and softer, but prompt plates still feel temporary
- minor / art_quality: The carpet/wall breakup is improved but still shader-authored rather than realistic Backrooms material work
