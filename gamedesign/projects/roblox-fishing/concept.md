---
type: Concept Draft
title: Roblox-Like Casual 3D Fishing Prototype
status: direction-accepted-for-first-fake-shot
timestamp: 2026-06-15T00:00:00Z
---

# Roblox-Like Casual 3D Fishing Prototype

## Definition Of Done

Current phase is assumption-based concept and visual proof, not gameplay
implementation.

Must exist before moving to implementation:

- reference packet with 3-7 refs, borrow/avoid/copy-risk, and readiness status;
- concept/GDD draft with first-slice loop, economy, UI flow, and assets;
- visual direction target and first gameplay fake shot;
- no more than three lead questions or explicit assumptions;
- profiling notes for skill/tool friction.

Out of scope for this phase:

- code changes;
- final generated art or runtime UI kit;
- web/mobile prototype or local web server;
- engine/submodule edits.

Accepted proof:

- durable project files under `gamedesign/projects/roblox-fishing/`;
- taskboard entries under epic `E002`;
- final response with reference digest and questions.

## Working Title

`Splash Rods` is the temporary internal title. It is not final and should be
renamed after taste review.

## Working Assumptions

Lead answers captured on 2026-06-15:

- Audience: casual players.
- Progression/grind: good and important.
- Gameplay complexity: bad; mechanics must stay simple and instantly readable.
- First proof priority: feel and gameplay fake shot are both important; the
  fake shot is the base target for the build.
- Gameplay clarity: player must understand the goal and progression path.
- Forbidden: realism. The game must be bright, juicy, visually pleasant, and
  noticeable.

Design interpretation:

- Primary gameplay/progression feel: accessible `Fisch`-like fishing and gear
  progression, but simpler and more readable.
- World progression: `Fishing Simulator`-like islands, boats, quests, and fish
  index as long-term grind goals.
- Tone: cozy, colorful, toy-like hangout energy without requiring multiplayer
  in the prototype.
- First proof priority: a beautiful gameplay fake shot that shows the action,
  catch reward, goal, and upgrade path in one glance.
- Avoid: realistic sim pacing, dense menus, subtle/muted palettes, and exact
  Roblox/Fisch visual copying.

## Known Direction

- Genre: casual 3D fishing, collection, light upgrade progression.
- Platform for prototype: native PC first.
- Look: bright, toy-like, Roblox-adjacent blocky 3D, saturated water, readable
  silhouettes, juicy catch feedback.
- Player fantasy: "I am a cheerful island angler who catches weird fish, shows
  them off, upgrades gear, and unlocks new fishing spots."
- Session shape: short 2-5 minute loops with visible progress every catch.

## Three Pillars

1. Instant readable fishing: the player must understand where to stand, where
   to cast, and what changed after the catch within 5 seconds.
2. Juicy collectible surprises: every catch has a visible fish, weight/rarity,
   coin burst, and collection progress.
3. Sunny toy-world progression: rods, bait, boats, islands, and fish index are
   colorful status objects, not spreadsheet-only upgrades.

## Accepted Direction Decision

## 2026-06-15 - Casual juicy fishing direction

- Status: accepted for first fake shot and GDD draft.
- Decision: build for a casual audience with strong progression/grind, simple
  fishing interaction, and bright juicy non-realistic 3D visuals.
- Why: lead explicitly prioritized feel, fake shot, progression clarity, and
  pleasant noticeable visuals; realism and complex gameplay are rejected.
- Applies to: GDD, fake shot, UI hierarchy, first playable slice, generated
  art prompts.
- Revisit when: first fake shot fails to communicate action, reward, or
  progression in one screenshot.

## No-Go List

- Realistic, slow, simulation-first fishing.
- Dark, muddy, horror, or survival tone.
- Dense adult MMO UI in the first prototype.
- Monetization pressure as a core loop.
- Copying Fisch, Roblox, or Fishing Simulator names/assets/screens.

## First Slice Assumption

One small tropical dock scene:

1. Player stands by water with a rod.
2. Primary action casts a bobber into a highlighted water zone.
3. Bite triggers a simple hold/click timing reel bar.
4. Catch reveal shows fish name, rarity, weight, value, and collection progress.
5. Player sells or keeps the fish, then buys one rod or bait upgrade.

## Lead Questions

1. How toy-like/childlike can the avatar and fish be before it feels too young?
2. Should the first fake shot show a successful catch reveal or the active reel
   minigame moment?
3. Should the grind fantasy emphasize rods/boats/islands or fish collection
   first?
