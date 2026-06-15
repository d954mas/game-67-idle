---
type: Visual Direction Brief
title: Splash Rods Visual Direction Brief
status: direction-accepted-for-first-fake-shot
timestamp: 2026-06-15T00:00:00Z
---

# Visual Direction Brief

## Visual Target

Bright, juicy, blocky 3D fishing toybox for a casual audience: saturated
tropical water, chunky wooden docks, rounded low-poly props, expressive
oversized fish, and reusable game UI with big icon-first controls. Realism is
forbidden; the image should feel pleasant, noticeable, and immediately
understandable.

## Palette

- Water: cyan, turquoise, deep blue accents, white sparkle highlights.
- Land: fresh grass green, warm sand, coral/pink flowers, honey wood docks.
- Rewards: coin gold, rarity green/blue/purple/orange.
- UI: white/seafoam panels with saturated trim, not dark fantasy panels.

## Runtime Asset Families

- World: water plane/shader or animated surface, dock, island ground, rocks,
  grass/foliage, shop sign, quest marker.
- Avatar: blocky original character with simple idle/cast/reel poses.
- Fishing gear: rod, line, bobber, bait bucket, upgrade props.
- Fish: at least 3 readable silhouettes for prototype; can begin as generated
  2D cards/icons plus simple 3D shapes if true 3D fish assets are not ready.
- UI: blank resizable panels/buttons, currency icons, fish rarity badges,
  action icons, catch card frame, upgrade card frame.
- FX: water splash, bite pulse, coin burst, rarity shine.

## First Fake Shot Requirement

One gameplay shot must show:

- avatar at dock;
- visible rod/line/bobber in bright water;
- fish bite/reel meter or catch reveal;
- reward feedback with fish rarity/weight/value;
- coins, backpack/fish count, objective, and one upgrade/shop affordance;
- visible next grind goal such as better rod, boat, island, or fish index;
- no baked UI text in reusable source assets.

## First Fake Shot V1

- Path: `gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1.png`
- Prompt:
  `gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1-prompt.md`
- Review:
  `gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1-review.md`
- Status: needs lead review.
- Accepted as: visual target only.
- Not accepted as: final runtime UI, reusable source art, final fish names, or
  final layout.

## Runtime UI Source Prompt Packets

- Blank UI kit:
  `gamedesign/projects/roblox-fishing/art/prompts/splash-rods-blank-ui-kit-v1-prompt.md`
- Isolated icon sheet:
  `gamedesign/projects/roblox-fishing/art/prompts/splash-rods-icons-v1-prompt.md`
- UI decor overlay sheet:
  `gamedesign/projects/roblox-fishing/art/prompts/splash-rods-ui-decor-v1-prompt.md`

## Asset Options

- Kenney Nature Kit: CC0 3D nature pieces for island/foliage/rocks.
- Kenney Watercraft Kit: CC0 3D boats/watercraft.
- Kenney Blocky Characters: CC0 animated 3D blocky characters.
- Kenney Fish Pack: CC0 2D fish assets for UI cards/icons if needed.
- Generated assets: use for fish variants, UI kit, catch cards, icons, and
  fake shot style proof after reference direction is accepted.

## Visual Risks

- Too close to Roblox/Fisch screenshots: use original world/fish names,
  custom palette, and different UI layout.
- Too much programmer art: first screenshot must read as a game scene without
  explanation.
- Flattened mockup trap: generated UI must be separated into reusable source
  families before runtime integration.
- 3D asset mismatch: mix external CC0 kits only after scale/material/palette
  normalization.
