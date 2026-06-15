---
type: VisualProductFailureReport
project: roblox-fishing
title: Splash Rods Native Visual Failure Report
status: fail-freeze-feature-expansion
timestamp: 2026-06-15T09:20:35Z
source_screenshot: tmp/roblox_fishing/native_first_slice.png
target_fake_shot: gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1.png
product_gate: gamedesign/projects/roblox-fishing/reviews/product_read_gate_latest.json
---

# Splash Rods Native Visual Failure Report

## Verdict

The current native screenshot is a useful technical prototype, but it fails the
visual/product bar for the requested game. It should not be treated as an
accepted visual slice.

Feature expansion is frozen until a rescue screenshot passes a new product-read
gate against the fake shot and the visual direction brief.

## What The Screenshot Proves

- Native PC gameplay loop exists: cast, reel, catch, sell, upgrade.
- Generated UI textures are wired into the runtime.
- GLTF/GLB mesh loading and rendering work through the builder/resource/material
  path.
- The screen contains the required gameplay objects: dock, water, avatar, rod,
  bobber, fish/reward card, coins, backpack, index, upgrade affordance.

These are engineering proofs, not proof of visual quality.

## Why It Fails

### World Art

- The world reads as shape-renderer/programmer geometry, not a polished
  Roblox-like toy island.
- Water is saturated but flat; it lacks attractive shader motion, depth, foam,
  readable fishing target polish, and material separation.
- The dock, island, foliage, shop, and boat props have inconsistent scale and
  weak silhouettes.
- The imported GLTF props prove the pipeline but are not art-directed enough to
  carry the screen.
- Lighting and camera do not create a pleasant first-viewport composition; the
  scene looks assembled from tests rather than staged for play.

### Character And Fish

- The avatar is blocky, but the silhouette, pose, face, clothes, and proportions
  are too crude for the target.
- The fishing action lacks a satisfying pose/arc/line/bobber sequence.
- Fish are visible, but they do not feel collectible or trophy-like enough.
- The catch moment is missing a premium reveal: fish card, splash, rarity shine,
  scale pop, and coin feedback do not act as one readable event.

### UI And UX

- The HUD is oversized and disconnected from the 3D world.
- The top bar is long and thin, while the bottom controls are huge; the screen
  feels squeezed between UI bands instead of composed around fishing.
- Buttons have generated texture color, but they still behave like debug
  controls because hierarchy, spacing, labels, and state styling are weak.
- The primary action is visible, but the player focus is split between the
  catch card, meter, dock, avatar, and bottom buttons.
- The UI kit is a partial mixed source-sheet crop, not a final reusable UI
  system with clean base/decor/icon/state families.

### Fake Shot Mismatch

- The fake shot sells a juicy toy fishing fantasy; the native screen sells a
  technical test of the fishing loop.
- The fake shot has a cleaner central action moment, stronger trophy/card
  energy, and better world/UI integration.
- The native screenshot has the right object checklist but not the target
  quality, composition, or feel.

## Rescue Direction

Do not polish this exact composition incrementally. Rebuild the visual slice
around a simpler, stronger first screen:

- One dock stage with a larger, cleaner water target and fewer background props.
- One strong blocky avatar pose, facing the action.
- One big readable fish/catch reveal moment.
- Smaller persistent HUD, stronger context panel, and one dominant primary
  action.
- UI generated as separate source families: blank bases, icons, decor overlays,
  progress bar parts, reward card pieces, and state overlays.
- World props from selected/final low-poly assets or generated model sources,
  with consistent scale, palette, and material language.

## Next Screenshot Gate

The next native screenshot must answer these without explanation:

- Where am I? A bright toy fishing dock/island.
- What do I do now? Cast or reel at the glowing water target.
- What changed after input? Bite/catch/reward feedback is visually obvious.
- What did I get? Fish, rarity/value, coins, index/backpack progress.
- Why continue? A visible upgrade/island/fish-index goal looks desirable.

## Minimum Acceptance For Rescue Pass

- No obvious programmer-art props in the first focal area.
- UI does not dominate more than necessary; primary action remains dominant.
- Fish/catch reveal is the brightest moment after the cast target.
- 3D world and UI share one palette and material language.
- Mesh/model path remains active, but art quality is judged by screenshot read,
  not by asset counts.
- Product gate must be rerun and must pass before visual task status can move
  beyond review.
