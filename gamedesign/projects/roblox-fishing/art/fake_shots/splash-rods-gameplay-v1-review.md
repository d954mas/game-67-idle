---
type: Visual Review Packet
title: Splash Rods Gameplay Fake Shot V1 Review
status: needs-lead-review
timestamp: 2026-06-15T00:00:00Z
---

# Splash Rods Gameplay Fake Shot V1 Review

## Image

`gamedesign/projects/roblox-fishing/art/fake_shots/splash-rods-gameplay-v1.png`

## Visual Tier

Fake shot. This is the visual target for the first native prototype. It is not
a runtime asset pack and not final UI source art.

## What Works

- Bright juicy non-realistic direction matches the accepted lead direction.
- Water, dock, avatar, rod, bobber, fish, reward card, reel meter, and upgrade
  goal are all visible in one gameplay screen.
- Casual progression/grind is readable through level, coins, backpack, index,
  locked island, and better rod goal.
- One dominant primary action is visible: `REEL`.
- The image feels like a game screen rather than poster art.

## Intentional Non-Final Parts

- Generated UI text is not final and must be replaced by runtime text.
- UI layout is a direction target, not a copied runtime layout.
- Fish name `Bubble Bass` is not accepted for the game because it creates
  copy-risk. Use original names from `data/balance.json`, such as
  `Bubble Guppy`, `Mango Minnow`, or `Crown Catfish`.
- The dense side menu is useful as a progression signal, but the first native
  screen should expose fewer secondary buttons.
- The fake shot does not prove that UI assets are reusable; source families
  must be generated separately.

## Required Runtime Translation

- Keep: saturated water, toy dock, blocky avatar, big fish reveal, coin burst,
  obvious reel/catch meter, big primary action, better rod goal, next island
  aspiration.
- Simplify: side buttons and top HUD density for first-time players.
- Replace: baked labels, generated fish names, exact fake-shot icon shapes.
- Prove in native screenshot: avatar + water + rod/bobber + cast/reel button +
  catch reward + coins/index/backpack + one upgrade goal.

## Review Question

Choose the direction:

- keep this visual direction and simplify UI for runtime;
- adjust specific elements such as avatar, fish style, UI density, or color;
- regenerate from a different emphasis, such as more cozy, more toy-like, or
  more upgrade/grind focused.
