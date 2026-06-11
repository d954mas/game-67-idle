# Iteration Roadmap

## Goal

Build the game in playable iterations. Each iteration must prove one part of the core loop and leave the project in a testable state.

## Principle

Do not build large RPG systems before the first route is fun.

Each iteration must answer:

- What does the player do?
- What changes on screen?
- Why is the next action tempting?
- What screenshot/input proof shows it works?

## Iteration 0: Command Discovery

Goal: know how to build, run, and capture the game.

Must do:

- inspect existing build/run scripts;
- identify native desktop run path;
- identify web path only if needed;
- record commands in `game_implementation_plan.md`.

Done when:

- exact build command exists;
- exact run command exists;
- exact validation command exists;
- engine submodule is not edited.

## Iteration 1: Clickable Expedition Skeleton

Goal: player can click through the loop without combat math.

Scope:

- province map;
- old road route;
- ruins encounter;
- result toast;
- camp unlock;
- return to map.

Done when:

- player reaches camp from map;
- at least one resource changes;
- next route appears locked/unlocked;
- screenshot shows map, encounter, camp.

## Iteration 2: First Combat Contract

Goal: `Ruin Wolf` combat from `data/combat.json` works.

Scope:

- enemy HP;
- player actions: Attack, Defend, Use Draught, Calm The Beast;
- victory;
- forced retreat;
- action log.

Done when:

- normal 3-attack win matches `normal_attack_win`;
- low Health retreat returns to camp;
- camp rest visibly heals.

## Iteration 3: Camp Preparation

Goal: camp feels useful, but not like base management.

Scope:

- rest;
- craft draught;
- Trail Herbalist I;
- companion talk;
- relic inspection stub.

Done when:

- player can spend herbs on upgrade;
- potion heal number changes;
- companion reveals next route;
- camp has no rooms/workers/timers.

## Iteration 4: First Region Mini-Loop

Goal: turn P0 into a small repeatable region.

Scope:

- 3-5 nodes;
- 2 enemy types;
- 1 hazard/search encounter;
- 1 faction hint;
- 1 dragon omen gate.

Done when:

- player can complete 10-20 minute region loop;
- at least two routes are optional;
- region ends with a clear next goal.

## Iteration 5: Retention Hook Test

Goal: prove the core is sticky before adding lots of content.

Test questions:

- Does the player want to open the next route?
- Does camp feel like relief and preparation?
- Does Dragon Omen create curiosity?
- Are resources pressure, not punishment?

Done when:

- playtest notes answer all four questions;
- at least one confusing UI state is fixed;
- content plan for next region uses `content_model.md`.

## Later Milestones

### Vertical Slice

- first complete region;
- final-ish UI style;
- 2 companions;
- 1 mini-boss;
- 1 dragon milestone.

### Content Expansion

- second region;
- faction consequence;
- gear/crafting expansion;
- companion personal quest.

### Production Direction

- runtime asset pack;
- save/load;
- mobile portrait polish;
- analytics/playtest telemetry;
- localization/copy pass.

## Stop Rules

- If the first route is boring, do not add regions.
- If combat feels unclear, do not add more enemies.
- If camp becomes the most complex screen, cut camp scope.
- If players do not understand Dragon Omen, rewrite the first relic beat.
