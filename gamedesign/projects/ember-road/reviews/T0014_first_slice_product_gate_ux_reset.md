---
type: ProductReadGate
project: ember-road
task: T0014
surface: desktop
verdict: fail
timestamp: 2026-06-20T16:32:56.906Z
---

# Product Read Gate - ember-road / desktop

Verdict: **FAIL**

Screenshot: `build/captures/iterate_first_screen.png`

## Player Read

- Where am I? Old Gate town hub with hero, Gate Warden, quest rail, and route strip visible
- What should I do now? Accept the wolf quest, then follow the highlighted route to North Road
- What changed after input? The first-screen capture shows the corrected target grammar before input; DevAPI smoke proves the later quest, travel, battle, loot, equip, and claim flow
- What is the reward / why continue? The quest rail promises XP, gold, and ring reward; the route shows Old Mine as the next lock
- Why does this look like a game? It now reads more like a fantasy browser RPG hub than a separated debug dashboard, but art is still shape-composed placeholder

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
- first_screen: build/captures/iterate_first_screen.png
- hud_visible: build/captures/iterate_uizoom.png
- primary_action_ready: build/captures/iterate_first_screen.png
- locked_or_disabled_state: build/captures/iterate_first_screen.png

Not covered / debt:
- reward_active: not covered by first-screen capture
- transient_stress_state: not covered by first-screen capture
- modal_or_choice_open: not in this first-screen pass
- resume_or_reentry_state: not in this first-screen pass

## Review

Problem: UX composition improved from detached town/map panels to one town hub with quest rail and route strip, but the screen still uses debug shape art instead of product-grade fantasy location, character, item, icon, and UI assets.

Next: Generate or import accepted project-local fantasy location, NPC/hero/wolf, route, reward, and UI frame assets; pack them through the asset pipeline; then recapture first/reward/locked/transient states.

## Visual Critique

Strict: yes
Pass threshold: 4

Scores:
- composition: 3
- readability: 4
- ui_controls: 3
- action_direction: 4
- art_quality: 1
- audience_fit: 2

Issues:
- major / art_quality: shape-composed placeholder art still blocks product visual acceptance
- major / audience_fit: fantasy browser RPG UX is closer, but asset style still does not match the desired game
- minor / ui_controls: quest rail is more integrated, but controls still look like prototype buttons
