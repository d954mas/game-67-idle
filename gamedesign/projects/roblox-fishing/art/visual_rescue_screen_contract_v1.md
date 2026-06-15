---
type: VisualRescueScreenContract
project: roblox-fishing
title: Splash Rods Visual Rescue Screen Contract V1
status: active-for-next-native-screenshot
timestamp: 2026-06-15T09:25:00Z
task: T0012
target_fake_shot: gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1.png
failure_report: gamedesign/projects/roblox-fishing/reviews/visual_product_failure_report_2026-06-15.md
---

# Splash Rods Visual Rescue Screen Contract V1

## Goal

Produce one native PC screenshot that looks like a bright, juicy,
Roblox-like casual fishing game screen, not a technical prototype.

This contract replaces checklist-driven visual acceptance. The next pass must
prioritize player-read, composition, and art quality before adding new systems.

## Player Read

The screenshot must answer, in order:

1. I am on a small tropical toy dock.
2. The important thing is the glowing water target / fish bite.
3. I should press one large Cast/Reel button.
4. I just caught or am about to catch a colorful fish.
5. Coins, backpack/index progress, and Better Line/next island are the reason
   to keep playing.

## Composition

- Camera: elevated third-person/isometric, closer than the failed screenshot.
- Focal triangle:
  - avatar on dock left-center;
  - glowing water target center-right;
  - catch/reward card upper-right or mid-right.
- Bottom UI should not cover the focal water target.
- Top HUD should be compact, not a full-width decorative banner.
- Background props should be fewer but higher quality: one shop/sell stand, one
  locked boat/island tease, two or three foliage/rock accents.
- Do not scatter many small shapes just to create busyness.

## World Art Bar

The first focal area should use selected/generated model or bitmap-backed art
where possible:

- water: layered cyan material with readable target ring, foam/sparkles, and
  animated-looking highlights;
- dock: chunky stylized planks with believable scale and warm material;
- avatar: blocky character with clean silhouette, readable face, hat/hair, and
  one fishing pose;
- rod/line/bobber: visible, simple, and aligned to the water target;
- fish: one large trophy fish silhouette in the reward moment, not tiny scenery
  decoration;
- shop/boat/goal: visible aspiration, but secondary to fishing action.

Shape-renderer fallback is allowed only for hidden scaffolding, simple FX, or
temporary collision/debug. It is not acceptable as the main visible art answer
for avatar, fish, dock, or UI panels.

## UI Hierarchy

Persistent HUD:

- coins;
- level/xp;
- backpack count;
- index count.

Keep these compact in a top-left/top-center cluster. They should not compete
with the fishing target.

Context UI:

- one dominant primary action button at bottom center: `CAST` or `REEL`;
- small secondary buttons: `SELL`, `UPGRADE`, optional `INDEX`;
- one objective/next-goal chip near the primary action or reward card.

Reward UI:

- catch card must be a premium moment: fish icon/model, rarity color, value,
  and index/progress feedback;
- card should not look like a generic debug panel;
- card text must be runtime text, not baked into source art.

## UI Asset Requirements

Next UI generation/integration should be separated into source families:

- blank resizable bases: HUD chip, catch card, primary button, secondary button;
- isolated icons: coin, backpack, index/book, rod, fish, upgrade;
- bar system: reel/catch meter track, fill, caps, marker/glow;
- decor overlays: corner highlights, shell/rope caps, shine/locked state;
- FX sprites: splash burst, coin burst, rarity shine.

Do not use one mixed source sheet as final UI kit. A mixed sheet may remain a
prototype source only.

## Model / Asset Requirements

For the rescue slice, prefer a small number of better assets over many rough
ones:

- blocky avatar model or improved generated mesh source;
- one dock kit with consistent scale/material;
- one fish trophy model with strong silhouette;
- one bobber/rod target prop;
- optional shop sign / boat goal.

The GLTF/GLB pipeline must stay active, but counts are not an art gate. The
visual gate is the screenshot.

## Acceptance Gate

Required commands/evidence for the next pass:

```powershell
cmake --build --preset native-debug
py -3.12 tools\playtest\roblox_fishing_probe.py 9123
node tools\product_gate\review.mjs --project roblox-fishing --task T0012 --screenshot tmp\roblox_fishing\native_first_slice.png --surface desktop --verdict pass --strict ...
node tools\taskboard\cli.mjs validate
```

The product gate may pass only if all five player-read answers are visually
credible from the screenshot.

## Explicit Rejections

- Do not call the current screenshot accepted.
- Do not continue feature expansion before visual rescue.
- Do not make the UI larger to compensate for weak scene art.
- Do not accept rough generated GLTF shapes as final asset quality.
- Do not treat bright colors alone as "juicy" visual quality.
- Do not use web/browser prototype as a shortcut.
