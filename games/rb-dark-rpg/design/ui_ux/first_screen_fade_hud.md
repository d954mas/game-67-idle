---
type: Game UI/UX Direction
title: First Screen Fade HUD
description: Accepted first-pass UI/UX direction for the Last Post first screen.
tags: [ui, ux, hud, first-screen, last-post]
game_id: rb-dark-rpg
status: draft
---

# First Screen Fade HUD

Status: draft decision note.

This page captures the current direction for the first playable screen:
`Последний Пост` with the gate guard as the first interactive action.

## Core Decision

The first screen should not use heavy top and bottom panels. The hub background
remains the main surface. UI sits over two dark fade bands:

- top fade: character status, location context, resources;
- bottom fade: persistent navigation buttons.

The player does not press a `Talk` button. The first action is clicking/tapping
the guard in the scene.

## First Screen Hierarchy

1. Scene: living border city interior, gate area, guard as the active object.
2. Guard marker: subtle outline, quest mark, and tap target.
3. Top fade HUD: state readout, not a call to action.
4. Bottom fade nav: RPG section navigation, mostly locked on first arrival.

## Top Fade Layout

Left side is reserved for the Poki button and should not contain important game
state.

Recommended composition:

```text
[ Poki reserve / quiet fade ][       Последний Пост       ][ player cluster ]
                                                          [ resources/util ]
```

Center:

- location plaque: `Последний Пост`;
- optional small state: `Хаб` or `Ночь`.

Right player cluster:

- character portrait;
- class/name: `Наемник`;
- level: `Ур. 1`;
- HP bar: large enough to read quickly;
- XP bar: thinner, directly under HP or under the name.

Right resource row:

- gold;
- supplies/food only if used as an actual early resource;
- settings/audio as small utility icons.

Do not show premium currencies, mail, social systems, clan buttons, or MMO
noise in the first slice.

## Bottom Fade Layout

The bottom area is navigation, not the primary action.

Initial buttons:

- `Снаряжение`;
- `Дневник`;
- `Карта`;
- `Место`;
- `Еще`.

Accepted order:

```text
[ Снаряжение ][ Дневник ][ Карта ][ Место ][ Еще ]
```

Rationale:

- `Карта` sits in the center because it becomes the frequent travel action;
- `Дневник` sits beside `Карта`: what to do next is adjacent to where to go;
- `Место` sits beside `Карта`: current-location actions are adjacent to travel;
- `Снаряжение` is important, but less frequent than map/journal checks;
- `Еще` is rare and late-game/system overflow.

Definitions:

- `Карта`: visual region map and travel between unlocked locations;
- `Дневник`: quests, step history, notes, and later clue records;
- `Снаряжение`;
- `Место`: contextual list of objects/actions in the current scene;
- `Еще`.

First-arrival state:

- `Дневник` can show a small active marker for `Допуск за ворота`;
- `Карта` is locked until the seeker token;
- `Снаряжение` is locked until starter gear is received;
- `Место` is available and can list `Страж у ворот` as the active row;
- locked buttons must give one short reason if tapped.

The bottom navigation itself should not scroll. If all important sections do not
fit, reduce the primary set to four buttons plus `Еще`; do not make the nav strip
horizontal-scrollable. Scrolling belongs inside screens such as inventory,
journal, and clue lists.

## More Bottom Sheet

`Еще` opens a compact bottom sheet above the bottom navigation. It slides upward
from the nav area and does not cover the primary scene action.

First-pass contents:

- `Справка`;
- quick audio/help options if needed;
- late or rare systems as locked rows.

Behavior:

- tap `Еще` to open;
- tap `Еще` again, tap outside, or press close to dismiss;
- dim the scene lightly, never as a full modal takeover;
- keep the sheet below half the screen height;
- do not put core-loop systems inside `Еще`.

Settings live as a small utility button in the top-right HUD/resource cluster,
not inside `Еще`.

## Place Bottom Sheet

`Место` opens a contextual bottom sheet for the current scene. This is a helper
for touch devices and accessibility; it does not replace clicking objects in the
scene.

In `Последний Пост`, first-arrival contents:

- `Страж у ворот`: active, first action;
- `Кузница`: locked until the guard sends the player;
- `Лазарет`: locked or quiet until healing matters;
- `Доска контрактов`: locked until the seeker token;
- later: Dragon memorial / council scribe if they become relevant.

In quest locations, `Место` changes to local actions such as inspect, enter,
return, or start encounter.

## Guard Interaction

The guard remains the first click target:

- separate character layer;
- hit target larger than the visible character;
- subtle outline or warm rim light;
- small quest mark above or near the guard;
- optional tutorial finger pointing at the guard;
- hover/tap nameplate: `Страж у ворот`.

No bottom CTA button is used for talking to the guard.

## Open Details

- Exact right cluster width for 960x540: likely 360-460 px.
- Whether the location plaque lives inside the top fade or slightly overlaps
  the scene.
- Final icon set for the bottom navigation.
- Exact locked-state treatment: dim, small lock, or both.
