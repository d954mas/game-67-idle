---
type: ProductReadGate
project: mech-builder-battler
task: T0030
surface: desktop-mech-lighting-material-pop
verdict: pass
timestamp: 2026-06-19T20:08:46.983Z
---

# Product Read Gate - mech-builder-battler / desktop-mech-lighting-material-pop

Verdict: **PASS**

Screenshot: `build/captures/mech_t0030_lighting_material_battle_smoke.png`

## Player Read

- Where am I? Battle arena on a Roblox-like studs/baseplate world with the Assault Walker engaging drones.
- What should I do now? Move with WASD while the mech auto-fires at drones.
- What changed after input? The Assault Walker and enemies now show stronger rim light, specular highlights, fill light, and ground-bounce material pop.
- What is the reward / why continue? Drone count, cooling meter, and salvage counter show battle progress and reward context.
- Why does this look like a game? Bright toy colors, stronger plastic/metal highlights, sourced mech silhouette, combat beams, and arena dressing read as a juicy Roblox-like mech battler.

## State Coverage

Required states:
- (none)

Covered states:
- (none)

Not covered / debt:
- (none)

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
- minor / art_quality: Mech normals now catch stronger rim/specular/fill light, but the pass is still shader tuning rather than final authored PBR materials or texture detail.
- minor / composition: The action screenshot is busy with battle VFX and arena dressing; later camera/layout polish should keep the hero silhouette cleaner during heavy effects.
