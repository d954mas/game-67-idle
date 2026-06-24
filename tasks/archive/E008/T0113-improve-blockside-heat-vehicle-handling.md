---
id: T0113
title: Improve Blockside Heat vehicle handling
status: done
epic: E008
priority: P1
tags: [prototype, blockside-heat, vehicle, physics]
created: 2026-06-23
updated: 2026-06-23
---

## What

Improve the first drivable car feel for Blockside Heat now that the first
native package-job product gate passed.

## Done when

- [x] Car movement has acceleration, braking, turning, and a visible difference
      from walking.
- [x] Player can enter/exit car and complete the package route using the
      improved vehicle loop.
- [x] Native capture/probe evidence covers first screen, in-car movement,
      pickup stress, and reward.
- [x] Product/readability gate remains pass or records the next smallest fix.

## Open questions

## Log

- 2026-06-23: Created after T0112 strict product gate pass. Next scope is
  vehicle feel, not a second district or broad open-world expansion.
- 2026-06-23: Added vehicle acceleration/braking/steering in
  `src/blockside_vehicle.c`, HUD vehicle controls in `src/blockside_hud.c`, and
  DevAPI probes `game.action.drive_probe` plus
  `game.action.drive_package_route`. Evidence:
  `tmp/blockside-heat/capture-states-report.json`,
  `tmp/blockside-heat/in-car-movement-latest.png`, and
  `tmp/blockside-heat/vehicle-route-complete-latest.png`. Product gate remains
  pass in `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`.
- 2026-06-23: close-slice PASS gate (desktop); gate: gamedesign/projects/blockside-heat/reviews/product_read_gate_2026-06-23T14-06-23-006Z_desktop.md; screenshot: tmp/blockside-heat/first-native-screenshot-latest.png; evidence: Vehicle smoke and capture PASS: tmp/blockside-heat/capture-states-report.json includes in_car_movement with nonzero car_speed and vehicle_route_complete with package_delivered=true/cash=75; gate PASS: gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json; build PASS: cmake --build --preset native-debug.; next: Next narrow slice can add a second street job or richer NPC/pursuit behavior.
