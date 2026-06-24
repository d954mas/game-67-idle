# Blockside Heat Implementation Plan

## First Slice

Build `Pickup Run` as the smallest native PC gameplay loop:

1. Replace the clean seed presentation with a 3D third-person city block scene.
2. Load project-local low-poly meshes from reused library assets where possible.
3. Add player movement, camera follow, and a nearby enterable compact car.
4. Add package pickup, drop zone, cash, wanted level, and one pursuer NPC.
5. Add one simple weapon/stun verb only if movement, vehicle, and pickup are
   already observable.
6. Add DevAPI/smoke endpoints for reset, state, enter car, pickup package,
   complete job, and capture-relevant state.

## Runtime Harness

- Primary target: native PC.
- Build command: `cmake --build --preset native-debug` after configure exists,
  or `cmake --preset native-debug` then build if needed.
- Run target: `build/_cmake/native-debug/game_seed.exe --devapi <port>
  --window-size 1280x720`.
- Product proof: native screenshot compared against
  `visual/targets/blockside-heat-first-slice-target.png`.

## Scope Control

Out of scope until first slice passes product-read gate:

- second district
- multiple mission contacts
- car collection/economy
- police system beyond `wanted_level` and one pursuer
- weapon inventory
- cinematic story scenes

## Risks

- Real mesh integration may take longer than debug geometry. Mitigation: source
  library assets first, then implement a single packed mesh path before broad
  asset variety.
- Current clean seed uses shape renderer debug visuals. Mitigation: log any
  temporary debug renderer usage as debt and replace focal visuals with real
  meshes before product-read pass.
- Text renderer integration may require pack setup. Mitigation: do not accept
  visible UI screenshots with handmade text.
