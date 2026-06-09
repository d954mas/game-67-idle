# Mobile And Web UX Spec

Статус: v0.2.
Дата: 2026-06-09.

## Targets

Machine-readable implementation contract: `data/ui_flow.json`.

Primary:

- mobile portrait `360x640` baseline;
- mobile portrait `390x844` common target;
- web browser with same portrait layout centered.

Secondary:

- PC/native dev harness for screenshots and input emulation.

## Layout

```text
Top stats bar
  X/67 | мем-коины | +/сек

Stage
  power label above sprite
  hero / rival / current visual upgrades
  reaction bubble

Primary action
  big button: Сделать 67

Context panel
  next goal card / active deal timer / event prompt

Bottom tabs
  Город | Дела | Улучшения | Дом
```

## P0 Tabs

Use exactly 4 bottom tabs:

- `Город`;
- `Дела`;
- `Улучшения`;
- `Дом`.

No separate `Мемы` tab in P0. Meme content appears through event modals,
reactions, stickers, animation states and the mini-final.

## Progressive Disclosure

First 5 minutes show only:

- `X/67`;
- meme-coins;
- `+/сек`;
- big `Сделать 67` button;
- one next goal.

Do not show all skills in the top UI. Skill names appear only on cards that
need them, such as `Нужно Руки 1`, with a shortcut to train that skill.

## Screen Wireframe Rules

Main:

- fixed top stats bar;
- stage takes the largest vertical area;
- primary button is above bottom tabs;
- next goal card sits between button and tabs.

City:

- 2x2 district grid on tall mobile, single-column if text wraps;
- current district has a clear background/scene thumbnail;
- Банан/Клубника can appear as small scene reactions, not as text walls.

Deals:

- active timer pinned at top of tab;
- available deals first;
- locked deals below;
- training shortcuts directly under the locked deal that needs the skill.

Upgrades:

- affordable first;
- newly affordable card pulses once;
- status-changing upgrades show a `67` badge.

Home:

- top half visualizes base/home/transport;
- bottom half lists housing and transport cards;
- post-MVP `67-тачка` is visibly locked until mini-final.

Modals:

- max width 92% of viewport;
- choices are full-width buttons;
- no modal text longer than 2 short lines before choices.

## Touch Rules

- Main button minimum height: 72px.
- Bottom tabs minimum height: 56px.
- Cards minimum tap target: 48px.
- No critical text below 13px.
- Max player-facing line length: 42 characters on mobile.
- No hover-only interactions.

## Onboarding: First 30 Seconds

1. Show `67/67 -> 1/67`.
2. Show Банан/Клубника as funny podstava symbols.
3. Pulse button `Сделать 67`.
4. On first click: big `+1`, hero does 67 gesture.
5. When 5 coins reached: highlight first upgrade.

## Locked States

Locked cards must say exactly one reason:

- `Нужно 3/67`;
- `Нужен Самокат`;
- `Нужно Руки 1`;
- `Купи Наклейку 67`.

No multi-condition text on one card for P0.

## Web Save

Web prototype uses localStorage:

- save every 10 seconds;
- save after purchase;
- save after event choice;
- save on final.

## Offline Progress

- cap at 30 minutes for P0;
- show one modal on return;
- never complete final `15/67` while offline without player action.

## Required Screenshots

Before external playtest capture:

- first screen: `1/67`, `Сделать 67`, `Собери 5 мем-коинов`;
- first click: hands 67 gesture, `+1`, Банан reaction;
- first purchase: `Пакет для мем-коинов`, effect visible;
- first status-up: `2/67`, `Кепка 67` visible on hero;
- city map: 4 districts, one lock reason per locked district;
- deals timer: one active `Дело`, collect reward state;
- event: Банан/Клубника, two choices, visible choice effects;
- home: `Дворовая база -> Уютный угол 67`;
- mini-final `15/67`: team 67 shared gesture, future `67/67`;
- mobile `360x640`;
- mobile `390x844`;
- desktop web portrait frame.
