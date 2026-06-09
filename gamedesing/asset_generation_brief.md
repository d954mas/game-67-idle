# Asset Generation Brief

Статус: source of truth v0.1.  
Дата: 2026-06-09.

Назначение: что генерировать/рисовать для P0 и как не потерять стиль. Финальные
ассеты идут в `gamedesing/assets/` или `gamedesing/art/`. Временная генерация,
черновики, маски и тестовые outputs идут только в root `tmp/`.

Machine-readable очередь генерации: `data/asset_generation_queue.json`.

## Style Prompt

Базовое направление:

```text
bright child-safe meme life-sim mobile game, toy-like 3D cartoon, readable
silhouettes, gold 67 accent, colorful city, no realistic hardship, no adult
crime, no violence, no romance, no tiny text
```

Драма:

```text
The hero was 67/67 at the top, got tricked by funny Banana and Strawberry,
starts again from 1/67, and climbs back through city life, deals, skills,
home and transport.
```

## Asset Folders

```text
gamedesing/assets/ui/
gamedesing/assets/icons/
gamedesing/assets/characters/
gamedesing/assets/backgrounds/
gamedesing/assets/fx/
gamedesing/art/mockups/
```

Create folders only when exporting final assets. Do not put temp generation
output there.

## UI Slice9 Assets

Priority P0:

| id | Size | Slice | Notes |
| --- | ---: | ---: | --- |
| `ui_button_primary_9s` | 192x96 | 24 | gold, main action |
| `ui_button_secondary_9s` | 192x96 | 24 | blue/neutral |
| `ui_panel_dark_9s` | 256x256 | 32 | generic screen panel |
| `ui_card_default_9s` | 256x160 | 28 | upgrade/deal/card |
| `ui_card_locked_9s` | 256x160 | 28 | locked state |
| `ui_modal_9s` | 320x240 | 36 | event/offline/final |
| `ui_badge_power_9s` | 160x88 | 24 | `X/67` badge |
| `ui_pill_resource_9s` | 160x72 | 22 | top resource pill |
| `ui_tab_9s` | 128x80 | 22 | bottom tabs |
| `ui_progress_frame_9s` | 256x40 | 16 | timer/skill bars |

Prompt template:

```text
Create a single clean mobile game UI <asset type> on a flat transparent-looking
neutral background for later slicing. Toy-like 3D cartoon, gold 67 accent,
rounded rectangle, thick readable border, simple center fill that can stretch,
corners designed for 9-slice. No text, no icon, no watermark, no complex detail
in the center.
```

## Character Assets

| id | Need |
| --- | --- |
| `hero_base_1` | start hero, simple, determined |
| `hero_cap_2` | cap visible |
| `hero_kiosk_3` | bag/sticker prop |
| `hero_helper_6` | helper companion |
| `hero_team_10` | team silhouettes |
| `hero_final_15` | badge flash/team pose |
| `banana_normal` | funny rival |
| `banana_confused` | reaction/shake |
| `strawberry_secret` | knows secret, wink but child-safe |

Prompt rules:

- same hero identity across stages;
- no realistic adult body;
- no adult luxury suit;
- no weapons;
- no tiny text on clothes except `67`.

## Background Assets

| id | Need |
| --- | --- |
| `bg_safe_sleep` | room after podstava, `1/67` mood |
| `bg_poster_room` | upgraded room with poster 67 |
| `bg_mini_hq` | brighter base/team stand |
| `district_yard_1` | yard / first district |
| `district_meme_kiosk` | kiosk activity |
| `district_school_yard` | schoolyard-style safe plaza |
| `district_mini_business` | mini business stand |

## Icon Assets

Use simple icon silhouettes, no text:

- meme coin;
- status 67;
- click power hand;
- income stream;
- coolness;
- hands skill;
- mind skill;
- style skill;
- business skill;
- comfort;
- lock;
- new unlock spark.

## FX Assets

- `fx_hands_67_loop`: 2-8 frames.
- `fx_badge_flash_67`: radial flash.
- `fx_coin_fly`: coin particles.
- `fx_map_ping`: district pulse.
- `fx_deal_stamp`: stamp, but text can be runtime-rendered.
- `fx_banana_twitch`: NPC reaction frames.

## Generated Image QA

Reject an asset if:

- it contains unreadable fake text;
- it introduces adult crime/romance/violence;
- it shows physical harm;
- it changes the hero identity between stages too much;
- it cannot be sliced because corners/center are too detailed;
- it breaks the `67/67 -> 1/67 -> comeback` story.

## Current Generated GDD Art

- `art/generated-67-comeback-keyart.png` - главный generated key art: вершина `67/67`, предательство, старт `1/67`.
- `art/generated-67-gameplay-fakeshot.png` - generated in-game fake shot: герой, `1/67`, большая кнопка `67`, работы, апгрейды, дом.
- `art/generated-67-life-sim-progression.png` - generated life-sim progression board: комната, киоск, дом, транспорт, бизнес.
- `art/generated-67-asset-sheet.png` - generated visual development sheet: стадии героя, Банан, Клубника, монета, жест, props.
- `art/gdd-hero-kids.png` - mood hero.
- `art/gdd-life-sim-progression.png` - life-sim progression board.
- `art/gdd-game-fakeshot-board.png` - 4 game-like UI fake shots.

## Ready Technical Pack

- `assets/generated/runtime_asset_manifest.json` - game-ready generated runtime art pack.
- `assets/generated/runtime_composed_screen.png` - proof screen composed from separate generated assets.
- `assets/generated/runtime_asset_pack_preview.png` - visual preview of generated characters/UI/backgrounds.
- `art_bible.html` - visual runtime art bible for embedding assets into the game.
- `assets/asset_pack_manifest.json` - machine-readable list of decomposed PNG categories.
- `assets/ui/slice9.json` - slice9 borders and source sizes for UI components.
- `assets/asset_pack_preview.png` - technical preview of decomposed UI/icons/characters/backgrounds/fx.

The runtime generated pack defines game-ready art. The older technical pack defines slice9/contracts and can be replaced by runtime-ready exports as implementation grows.

Rejected/scratch generations stay in `tmp/`.

Связи: `data/asset_manifest.json`, `data/asset_generation_queue.json`,
`ui_bible.md`, `screen_mockups_spec.md`.
