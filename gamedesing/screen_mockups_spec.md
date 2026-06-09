# Screen Mockups Spec

Статус: source of truth v0.1.  
Дата: 2026-06-09.

Назначение: список мокапов, которые дизайнер должен сделать, а разработчик
должен воспроизвести в runtime. Fake shots на сайте дают визуальный вайб, но
этот файл задает screen-by-screen состав.

## Global Requirements

- Все мокапы: mobile portrait `360x640`.
- Дополнительно проверить `390x844`.
- Экран должен помещаться без горизонтального overflow.
- `X/67` всегда читается крупно, если герой виден.
- Bottom tabs всегда 4: `Город`, `Дела`, `Улучшения`, `Дом`.
- Player-facing copy брать из `p0_ui_copy.md`.
- Структуру экранов брать из `data/ui_flow.json`.

## Required Mockups

### 01 `intro_fall_67_to_1`

Purpose: показать драму до gameplay.

Must show:

- `67/67` на вершине;
- Банан + Клубника как причина подставы;
- `06:00`;
- обнуление до `1/67`;
- кнопка/пульс `Сделать 67`.

Do not show:

- violence;
- adult romance;
- dark realistic crime;
- trash/hardship framing.

### 02 `main_first_screen`

Maps to screenshotId: `first_screen`.

Must show:

- room/start background;
- hero with `1/67` badge above sprite;
- top stats: meme coins, income/sec;
- big `Сделать 67` button;
- next goal: first 5 meme coins;
- bottom tabs.

### 03 `main_first_click`

Maps to screenshotId: `first_click`.

Must show:

- 67 hand gesture animation;
- `+1` reward pop;
- Banana reaction;
- no modal interruption.

### 04 `main_status_up`

Maps to screenshotId: `status_up`.

Must show:

- `2/67` badge;
- cap or visible hero change;
- flash around badge;
- next unlock hint.

### 05 `city_map`

Maps to screenshotId: `city_map`.

Must show:

- 4 district cards or route nodes;
- unlocked: Двор, Киоск when requirements met;
- locked: Школьный двор, Мини-бизнес;
- one lock reason per locked card;
- shortcut `К делу`.

### 06 `deals_timer`

Maps to screenshotId: `deals_timer`.

Must show:

- active deal timer pinned;
- progress bar;
- `Сделать 67` still usable;
- deal reward preview;
- skill/training shortcut if locked deal exists.

### 07 `upgrades_first_purchase`

Maps to screenshotId: `first_purchase`.

Must show:

- affordable card at top;
- cost;
- one-line effect text;
- locked cards below;
- newly affordable pulse.

### 08 `home_growth`

Maps to screenshotId: `home_growth`.

Must show:

- current home visual in top half;
- current transport visual;
- buyable housing cards;
- buyable transport cards;
- `67-тачка` locked as future dream.

### 09 `event_modal`

Maps to screenshotId: `event`.

Must show:

- title, 1-2 body lines;
- Банан/Клубника involvement;
- two good choice buttons;
- visible reward on each choice;
- dimmed background.

### 10 `mini_final`

Maps to screenshotId: `mini_final`.

Must show:

- hero `15/67`;
- team 67 shared gesture;
- Banana reaction;
- future goal `67/67`;
- buttons: play more, reset for test.

Note: `data/ui_flow.json` currently requires 9 screenshot ids. `event_modal`
and `mini_final` are both required as screens; if production wants exactly 9
static mockups, combine event + mini-final into one artboard sheet with two
states.

## Artboard Naming

```text
mock_01_intro_fall_67_to_1.png
mock_02_main_first_screen.png
mock_03_main_first_click.png
mock_04_main_status_up.png
mock_05_city_map.png
mock_06_deals_timer.png
mock_07_upgrades_first_purchase.png
mock_08_home_growth.png
mock_09_event_modal.png
mock_10_mini_final.png
```

Final mockups go to `gamedesing/art/mockups/`. Temporary generations and
screenshots go to root `tmp/`.

## Review Checklist

- [ ] Can a child identify the main button in 5 seconds?
- [ ] Does the drama read as `67/67 -> 1/67`?
- [ ] Does every locked card explain one next step?
- [ ] Is text readable at `360x640`?
- [ ] Are components visibly reused?
- [ ] Are stretchable frames slice9-compatible?
- [ ] Does the screen match `data/ui_flow.json` required components?

Связи: `ui_bible.md`, `asset_generation_brief.md`, `data/ui_flow.json`,
`fakeshots.html`.
