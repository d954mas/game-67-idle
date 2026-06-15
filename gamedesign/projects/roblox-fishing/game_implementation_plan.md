---
type: Implementation Handoff
title: Splash Rods Native Prototype Plan
status: blocked-on-fake-shot-review
timestamp: 2026-06-15T00:00:00Z
---

# Splash Rods Native Prototype Plan

## Status

This is a first implementation handoff draft. The lead accepted the broad
direction on 2026-06-15: casual audience, simple gameplay, strong progression,
juicy non-realistic visuals. The reference gate now has enough screenshot
walkthrough/evidence-board material for the first native prototype. It is not
ready to execute until the first fake shot is reviewed or explicitly accepted
as the visual target.

## Source Order

1. `AGENTS.md`
2. `gamedesign/projects/roblox-fishing/concept.md`
3. `gamedesign/projects/roblox-fishing/references/fishing_reference_study.md`
4. `gamedesign/projects/roblox-fishing/gdd.md`
5. `gamedesign/projects/roblox-fishing/data/balance.json`
6. `gamedesign/projects/roblox-fishing/data/ui_flow.json`
7. `gamedesign/projects/roblox-fishing/data/game_asset_manifest.json`
8. `gamedesign/projects/roblox-fishing/art/visual_direction_brief.md`
9. `gamedesign/projects/roblox-fishing/art_requests/roblox-fishing-first-visual-v1.json`

## First Playable Slice Packet

- Player starts at: small tropical dock in Starter Cove.
- Screen shown first: third-person 3D dock/water scene with HUD.
- Available actions: cast/reel, sell all, open shop, open fish index.
- Currencies/stats: coins, XP/level, backpack count/capacity, index count.
- First activity: cast at Starter Cove.
- First reward: fish card + coins + XP + index progress.
- First upgrade: Better Line I for 30 coins.
- First visual/status change: wider reel target and upgraded rod/line chip.
- Save/load expectation: persist coins, XP, caught fish flags, backpack,
  upgrade ownership.
- Out of scope: multiplayer, web/mobile build, premium currency, ads/codes,
  realistic tackle, more than one area playable.

## Phase Plan

1. **State schema and data loading.**
   Add fishing state fields and load first-slice data from balance JSON or C
   constants generated from it.

2. **Core loop reducer.**
   Implement cast -> bite timer -> reel progress -> catch result -> backpack
   -> sell/upgrade state changes.

3. **Native 3D scene.**
   Build a small dock/water/island scene with avatar, rod line, bobber, fish
   marker, cast zone, water splash and coin burst. Use placeholder geometry
   only until generated/CC0 assets are integrated.

4. **HUD and panels.**
   Implement main HUD, catch result modal, shop panel, index panel, blocked
   toasts, and DevAPI UI nodes.

5. **Visual asset integration.**
   Integrate accepted fake-shot direction, CC0 candidates, and generated UI
   source families. Keep fake shot separate from runtime assets.

6. **Automation and proof.**
   Add DevAPI actions for reset, cast, force bite or wait, reel success, sell,
   buy upgrade, open index, capture screenshot. Run smoke/full probe or a new
   fishing scenario.

## Acceptance Gates

- Player can catch the first fish within 15 seconds.
- Reward changes visible coins, XP, backpack, and index count.
- Full backpack blocks casting and points to sell.
- Better Line I can be bought and changes reel control.
- UI explains next goal without reading docs.
- Native screenshot is nonblank and clearly shows avatar, water, rod/bobber,
  action UI, reward feedback, and upgrade affordance.
- Emulated input or DevAPI scenario proves the main action path.
- Product-read visual gate passes before claiming visual quality.

## Commands

- Build: `cmake --build --preset native-debug`
- Existing DevAPI smoke: `py -3.12 tools/devapi/smoke_test.py 9123`
- Existing full probe: `py -3.12 tools/devapi/full_probe.py 9123`
- New scenario to add: `py -3.12 tools/playtest/roblox_fishing_probe.py 9123`
- Screenshot proof path: `tmp/roblox_fishing/native_first_slice.png`

Command discovery still required before implementation because the runtime has
not yet been converted from Rune Marches to fishing.

## Do Not Edit

- `external/neotolis-engine/` unless explicitly requested.
- Web/mobile build files unless the lead explicitly approves web work.
- Existing Rune Marches project docs except when archiving or marking stale is
  explicitly requested.
- `tmp/` artifacts as durable source of truth.

## Next Implementation Prompt

Use the project rules in `AGENTS.md`. Implement the first native PC playable
slice for Splash Rods from the source files listed above.

Must implement:

- fishing state, cast/reel/catch/sell/upgrade loop;
- one dock/water 3D scene;
- first HUD/result/shop/index UI;
- DevAPI automation and screenshot capture.

Out of scope:

- web/mobile, multiplayer, monetization, extra islands, final UI kit.

Validation:

- build native;
- run DevAPI scenario;
- capture native screenshot;
- run product-read visual gate.
