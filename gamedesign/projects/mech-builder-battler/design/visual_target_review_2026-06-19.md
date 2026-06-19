---
type: Project Visual Target Review
title: Mech Builder Battler Visual Target Review
description: Accepted visual target and model-quality bar for the first playable slice.
tags: [project, visual-target, fake-shots, models, 3d, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Visual Target Review

Status: accepted visual direction for starting the first playable.

## Accepted Bar

The game must read as a bright, juicy 3D mech game. The visual proof cannot be
flat, debug-shaped, or visually dry.

Required from the first playable direction:

- real/model-like 3D mech assets, preferably GLB;
- chunky modular silhouette with visible shoulder rocket sockets/modules;
- material response: painted metal, dark joints, bevel highlights;
- lighting: key/fill/rim separation, warm hangar lights, cyan reactor accents;
- shadows: contact shadows under mech, drones, props, and arena obstacles;
- normal/detail readability on armor plates and mechanical surfaces;
- juicy effects: amber rocket trails, compact explosions, salvage glow,
  `Cooling` vent glow;
- phone-scale readability over dense realism.

Debug shape renderers are acceptable only for engineering experiments. They are
not acceptable as the product screenshot or visual target.

## Fake Shots

Generated and accepted as direction references:

- [Hangar first screen](../art/fake_shots/hangar_first_screen_2026-06-19.png)
- [Battle drone swarm](../art/fake_shots/battle_drone_swarm_2026-06-19.png)
- [Reward upgrade](../art/fake_shots/reward_upgrade_2026-06-19.png)

Verdict:

- Hangar: strong mech scale and material read; runtime should simplify top
  counters and service-game chrome.
- Battle: best current target; it clearly proves shoulder rockets against drone
  swarm with mobile-style controls.
- Reward/upgrade: strong resources-to-module visual; runtime should reduce the
  left panel to one salvage counter, one module card, and one attach action.

## Model Sourcing Decision

For the first playable, use ready/generated GLB-style assets rather than
hand-drawn shape approximations.

Preferred order:

1. Use permissively licensed ready GLB mech/drone/industrial props if available.
2. If no suitable model exists, generate or kitbash temporary GLB models for
   starter mech, shoulder rocket module, drone, and Foundry Warden.
3. Keep model count small: one starter mech, one shoulder rocket module, one
   drone enemy, one industrial mini-boss silhouette, a few hangar/arena props.

The first playable can use temporary models, but they must exercise the real
asset path: model loading, transforms, materials, lighting, shadows or shadow
proxy, effects, and screenshot readability.
