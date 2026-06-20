---
id: T0040
title: Mech animation and battle-feel rejection fix
status: doing
priority: P0
tags: [visual, animation, controls, battle, native, lead-rejection]
created: 2026-06-20
updated: 2026-06-20
---

# T0040 - Mech Animation and Battle-Feel Rejection Fix

## Why

Lead feedback: the robots exist, but they have almost no animation and read like
plastic figurines. The battle also ends too quickly to walk around and judge the
mech, the arena feels too small, and movement/facing feels incorrect.

## What

- In scope: make the current hero mech feel alive in the native PC harness,
  slow the test battle enough for movement review, enlarge the arena, and make
  WASD/facing behavior predictable.
- Out of scope: skeletal animation import, new economy, new asset sourcing,
  web/mobile export, or more static props.

## Done when

- [ ] Hero mech has visible idle/move/turn/recoil motion in battle and hangar.
- [ ] First battle gives enough time and space to walk before victory/reward.
- [ ] WASD movement remains direct while mech facing is readable.
- [ ] Native build and DevAPI smoke capture a screenshot.
- [ ] Evidence records the remaining animation/material debt.

## Activity Log

- 2026-06-20: Created from lead rejection. Stopped expanding static asset intake;
  the next proof must improve animation, arena scale, battle pacing, and control
  feel.
