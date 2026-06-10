# Iteration 4 First 30s Visual Pass

Статус: designer handoff candidate.  
Scope: только `gamedesing/assets/generated/ui/**` и этот note. Runtime ids не менялись.

## Goal

Первый экран должен читаться как toy-like mobile idle за 1-2 секунды: кто герой, какой у него статус, куда нажать, что растет дальше. Для Poki/casual kids это важнее детального UI: крупные силуэты, понятные иконки, минимум текста, один главный tap target.

## Player Behavior

1. 0-5 секунд: ребенок видит героя, большой пустой статусный badge под runtime-текст `1/67`, понимает "я маленький, надо расти".
2. 5-15 секунд: ребенок нажимает на самую большую золотую action plate с runtime-текстом и hand/tap icon.
3. 15-30 секунд: после награды взгляд уходит в coin icon и next-goal arrow, чтобы без чтения длинного текста понять "коплю и открываю следующее".

## New Candidate Assets

Все ассеты ниже без baked text, без цифр внутри изображения, с прозрачным фоном. Runtime должен сам рисовать `1/67`, цену, copy и локализацию поверх.

| File | Size | Use | Why it helps first 30s |
| --- | ---: | --- | --- |
| `gamedesing/assets/generated/ui/ui_first_status_badge_shell_9s.png` | 320x144 | large `PowerBadge` shell above hero | делает статус главным объектом экрана; ребенок видит большой collectible-like знак до чтения текста |
| `gamedesing/assets/generated/ui/ui_first_action_plate_9s.png` | 384x156 | main tap button background | заменяет плоскую shape-кнопку на игрушечную золотую кнопку; держать min rendered height 88px on 360x640 |
| `gamedesing/assets/generated/ui/icon_tap_hand_67.png` | 256x256 | icon on/near main button | жест "нажми" понятен до чтения; можно pulse/scale 0.94-1.04 при idle |
| `gamedesing/assets/generated/ui/icon_meme_coin_67.png` | 192x192 | resource pill icon and reward fly source | монета отделяет currency от обычного текста; good for small top bar at 32-44px |
| `gamedesing/assets/generated/ui/icon_next_goal_arrow_67.png` | 192x192 | next goal / first upgrade card icon | стрелка объясняет climb-back loop без длинного tutorial text |

## System Behavior

- Do not replace existing runtime pack ids yet: keep `bg_starter_room_yard`, `hero_1_67_body`, `button_67_gesture` unchanged.
- Treat these five PNGs as candidate exports for the next pack pass. Add ids only when the implementation agent intentionally updates the pack builder/manifest.
- Keep all player-facing words as engine-rendered text. No text baked into future generated PNGs except deliberate non-localized symbol art approved in `ui_bible.md`.
- On 360x640, first screen hierarchy should be:
  1. `ui_first_status_badge_shell_9s` rendered near top center, text `1/67` in runtime.
  2. `hero_1_67_body` center, not hidden by panels.
  3. `ui_first_action_plate_9s` bottom-middle with `icon_tap_hand_67` at left or above.
  4. compact resource row using `icon_meme_coin_67`.
  5. next goal card using `icon_next_goal_arrow_67`.

## Feedback

- Main button idle: hand icon small bounce every 1.2s, no layout shift.
- Tap feedback: button plate moves down 2px, coin icon emits 3-5 tiny coin particles toward resource pill.
- First affordable upgrade: next-goal arrow gets one green pulse, then settles.

## Tuning Knobs

- Status badge rendered width: 190-240px mobile portrait, 240-300px desktop harness.
- Action plate rendered width: 280-330px mobile portrait, height 88-104px.
- Hand icon rendered size: 48-72px when inside button, up to 96px if floating above.
- Coin icon rendered size: 32-44px in top resource pill, 72px in first reward callout.
- Arrow icon rendered size: 44-64px in next goal card.

## Validation

- 360x640 screenshot: main action must be the largest tap target and not overlap hero feet.
- 390x844 screenshot: badge, hero, action plate, and next goal should all fit without scrolling.
- Text check: no baked unreadable text in PNGs; runtime text remains readable at 13px+.
- Web risk check: PNGs are static transparent RGBA; no atlas/slice shader assumptions until pack integration.

## Visual Risks

- Current generated assets mix painted 3D art with procedural icon candidates. If they feel too flat, repaint only these five files, keeping filenames and sizes.
- `ui_first_action_plate_9s.png` is a candidate 9-slice shell, but slice borders are not declared in a manifest yet. Use as full sprite until the pack builder has explicit borders.
- Overusing gold can make the screen one-note. Keep background blue/green and reserve gold for status, action, and reward.

## Next Assets

1. `fx_first_tap_coin_trail_67.png` - 256x256 transparent, 6-8 coin/spark frames, no text.
2. `ui_next_goal_card_9s.png` - 320x180 transparent card shell, border 28px, no icon/text baked.
3. `icon_first_upgrade_cap_67.png` - 192x192 transparent cap/upgrade symbol, no text.
4. `fx_status_badge_flash_67.png` - 256x256 transparent radial flash behind runtime `X/67`.
