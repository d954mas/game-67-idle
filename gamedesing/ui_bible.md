# UI Bible

Статус: source of truth v0.1.  
Дата: 2026-06-09.

Назначение: правила визуального интерфейса P0. Этот файл отвечает на вопрос:
как должны выглядеть и переиспользоваться UI-компоненты игры `67`.

## UI Direction

Ключевой образ: яркий meme life-sim, где интерфейс похож на мобильную игру:
крупные кнопки, читаемые карточки, сочные рамки, явный `X/67` над героем.

Generated art anchors:

- `art/generated-67-comeback-keyart.png` - story/key art quality bar.
- `art/generated-67-gameplay-fakeshot.png` - target gameplay UI look.
- `art/generated-67-life-sim-progression.png` - life-sim progression look.
- `art/generated-67-asset-sheet.png` - reusable character/prop direction.

Technical reusable exports:

- `art_bible.html` - visual runtime art bible.
- `assets/generated/runtime_asset_manifest.json` - game-ready generated PNGs.
- `assets/generated/runtime_composed_screen.png` - first screen assembled from separate generated assets.
- `assets/ui/slice9.json` - borders and source sizes.
- `assets/asset_pack_manifest.json` - decomposed PNG categories.

Драма в UI:

```text
Вершина 67/67 -> подстава -> 1/67 -> возвращение через город/дела/дом
```

UI не должен выглядеть как взрослая криминальная стратегия. Он должен быть
детским, смешным, но с понятным падением статуса.

## Layout Targets

- `360x640`: baseline.
- `390x844`: tall mobile.
- desktop portrait frame: для PC/dev harness screenshots.

Правила:

- critical text >= 13px;
- primary action min height 72px;
- bottom tabs min height 56px;
- tap targets >= 48px;
- max player-facing line length: 42 chars;
- no hover-only interactions.

## Color Tokens

| Token | Use | Hex |
| --- | --- | --- |
| `gold_67` | главная кнопка, badge, highlight | `#ffd36a` |
| `gold_dark` | pressed gold | `#c99024` |
| `blue_city` | город, hero jacket, active map | `#4b86a8` |
| `green_gain` | reward, progress, success | `#3fa26b` |
| `red_podstava` | warning, подстава, blocked drama | `#d94d4d` |
| `cream_text` | main text on dark UI | `#f4efe5` |
| `panel_dark` | dark panel fill | `#171d21` |
| `panel_deep` | deep background | `#0c1013` |

## Typography

- Font class: rounded sans, high x-height, no thin weights.
- Main numbers: 900 weight.
- `X/67` badge: largest UI number after hero.
- Buttons: 1 line preferred, 2 lines max.
- No negative letter spacing.

## Component Principles

- Every repeated UI element is a component.
- Every stretchable component uses slice9/nine-slice.
- Data renders components; do not make unique hand-built cards for each item.
- Locked state always explains exactly one reason.
- Affordable state is visually louder than locked state.
- New unlock pulse plays once, then returns to normal.

## Slice9 / 9-Slice Rules

Base export size:

- button source: `192x96`, slice border `24px`;
- panel source: `256x256`, slice border `32px`;
- card source: `256x160`, slice border `28px`;
- modal source: `320x240`, slice border `36px`;
- badge source: `160x88`, slice border `24px`;
- tab source: `128x80`, slice border `22px`.

Rules:

- corners must remain pixel-stable;
- center can stretch freely;
- borders cannot contain detailed drawings that break under scale;
- shadows must be outside or in a separate layer if engine supports it;
- all source slice9 assets need a `@1x` and optional `@2x` export.

## Component Inventory

### `PrimaryButton`

Use: `Сделать 67`, main choices, claim reward.  
Slice9: `ui_button_primary_9s`.  
Min size: `240x72`.

States:

- `normal`: gold fill;
- `pressed`: lower y by 2px, darker fill;
- `disabled`: 45% opacity, no glow;
- `new`: one pulse outline;
- `active`: sparkle loop for `Сделать 67`.

### `Panel`

Use: screen sections, phone UI containers.  
Slice9: `ui_panel_dark_9s`.  
Min size: `280x120`.

### `Card`

Use: upgrade, district, deal, housing, transport.  
Slice9: `ui_card_default_9s`.  
Min size: `156x96`.

States:

- `normal`;
- `affordable`;
- `locked`;
- `active`;
- `completed`;
- `newly_unlocked`.

### `PowerBadge`

Use: `X/67` above sprite and milestone cards.  
Slice9: `ui_badge_power_9s`.  
Must be readable at 80px width.

### `ResourcePill`

Use: meme coins, income/sec, click power.  
Slice9: `ui_pill_resource_9s`.

### `BottomTabs`

Tabs:

- `Город`
- `Дела`
- `Улучшения`
- `Дом`

Slice9: `ui_tab_9s`.  
No `Мемы` tab in P0.

### `Modal`

Use: event, offline, mini-final.  
Slice9: `ui_modal_9s`.  
Must dim/lock background.

### `ProgressBar`

Use: deal timer, skills, comfort.  
Slice9: `ui_progress_frame_9s` + fill sprite.

## Screen Hierarchy

Main screen priority:

1. `PowerBadge X/67` above hero.
2. Hero visual and room/city background.
3. Big `Сделать 67`.
4. Next goal card.
5. Resource bar.
6. Bottom tabs.

Tab screen priority:

1. Screen title.
2. Active/available item.
3. Locked items with one reason.
4. Shortcut action.

## Animation Bible

- `hands_seesaw_67`: two hand shapes move up/down around `67`.
- `badge_flash_67`: radial flash behind `X/67`, 0.35s.
- `coin_fly`: reward coins fly to top resource pill.
- `map_ping`: active district pulses once.
- `deal_stamp`: `Дело сделано` stamp appears for 0.5s.
- `banana_twitch`: NPC shifts left/right twice.
- `strawberry_wink`: blink/wink, no flirt framing.

## Export Naming

```text
ui_button_primary_9s.png
ui_button_secondary_9s.png
ui_panel_dark_9s.png
ui_card_default_9s.png
ui_card_locked_9s.png
ui_modal_9s.png
ui_badge_power_9s.png
ui_pill_resource_9s.png
ui_tab_9s.png
ui_progress_frame_9s.png
```

All final UI assets go to `gamedesing/assets/ui/` when created. Temporary
generation files go to root `tmp/`.

## Do Not

- Do not create one PNG per card size.
- Do not put tiny text inside generated images.
- Do not use adult crime/luxury UI language.
- Do not make `67-тачка` available before mini-final.
- Do not add a fifth P0 tab.

Связи: `data/ui_components.json`, `screen_mockups_spec.md`,
`asset_generation_brief.md`, `data/ui_flow.json`.
