---
id: T0023
title: Polish progress upgrade HUD states
status: review
epic: ""
priority: P1
tags: [ui, hud, visual, native]
created: 2026-06-12
updated: 2026-06-12
---

## What

Make the top progress-upgrade HUD readable during the one-hour 67 World
progression. The player should understand whether the button buys speed, buys
the next Better Crate level, needs more coins, needs more discovery, or is maxed
without the agent explaining it.

## Done when

- [x] Native HUD labels distinguish speed, Better Crate level, ready, saving,
      locked-next, and max states.
- [x] Stuck-board cleanup uses a clear player-facing spawn CTA.
- [x] DevAPI state exposes the computed HUD labels for automation checks.
- [x] Native PC scenario proves the labels and captures screenshot evidence.

## Open questions

None.

## Log

- 2026-06-12: Refined and started after Better Crate screenshot review showed
  confusing `SAVE`/plus-icon progress-upgrade states in the top HUD.
- 2026-06-12: Added shared native label helpers and DevAPI fields:
  `progress_upgrade_title`, `progress_upgrade_value`, and
  `spawn_action_label`.
- 2026-06-12: Evidence passed: `cmake --build --preset native-debug`;
  `py -3.12 tools/devapi/scenarios/better_crate_progression.py 9207 build/captures/scenarios/better_crate_hud_v2.png`;
  `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/better_crate_hud_v2.png`;
  `py -3.12 tools/devapi/pixel_health.py build/captures/scenarios/better_crate_hud_v2_stuck.png`;
  `py -3.12 tools/devapi/scenarios/first_67_loop.py 9206 build/captures/scenarios/first_67_loop_hud_polish_v1.png`;
  `py -3.12 tools/balance/simulate_67_world.py`.
  Screenshots:
  `build/captures/scenarios/better_crate_hud_v2.png`,
  `build/captures/scenarios/better_crate_hud_v2_stuck.png`.
