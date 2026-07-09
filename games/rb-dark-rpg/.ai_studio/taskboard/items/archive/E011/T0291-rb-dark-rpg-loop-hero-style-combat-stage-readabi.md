---
id: T0291
title: "rb-dark-rpg: Loop Hero style combat stage readability pass"
status: done
project: P003
epic: E011
priority: P2
tags: [rb-dark-rpg, combat, ui, visual]
created: 2026-07-05
updated: 2026-07-05
---

## What

Revise the running combat stage after user review:

- remove the central divider/card that visually separates hero and enemy;
- make hero/enemy the dominant center of the combat panel;
- move HP bars under each actor;
- replace the five-pip attack progress with a subtler next-attack cue;
- remove the running stats summary from the battle view unless later review
  proves it is needed.

## Done when

- [x] The running stage no longer reserves a central divider element between
      actors.
- [x] HP and attack readiness are attached under each actor.
- [x] Actors are larger than the first generated-art pass without breaking
      phone layouts.
- [x] Running combat keeps log separate and removes the stat chip row.
- [x] Native build, scenario tests, and gate/mill responsive captures pass.

## Open questions

## Log

- 2026-07-05: Compared against Loop Hero combat screenshots and applied the
  user direction: character-first field, HP near units, detailed stats outside
  the combat focus. Implemented the stable first size step; a larger second
  size step caused a phone portrait runtime reset and was reverted.
- 2026-07-05: Evidence: `cmake --build games\rb-dark-rpg\build\native-debug --target game`, `py -3.12 games\rb-dark-rpg\devapi\scenarios_test.py`, QCLR_002 gate retry matrix, and QCLR_002 mill matrix passed.
- 2026-07-05: Closed after Loop Hero style readability pass: removed center divider, attached HP/attack cues under actors, hid running stat chips, and verified gate/mill responsive captures. A more aggressive size step was tested and reverted after phone portrait reset.
