# Prototype MVP Spec

Статус: source of truth v0.2.
Дата: 2026-06-09.

Этот документ - главный источник истины для разработки первого прототипа.
`concept.md`, `market_research.md`, `lore.md` и визуальный сайт помогают понять
направление, но реализация должна сверяться с этим файлом, `data/balance.json`
и `prototype_build_handoff.md`.

## Implementation Handoff

Для разработки использовать:

- `prototype_build_handoff.md` - модель состояния, reducers, screen flow,
  save/offline, analytics;
- `playtest_acceptance_gates.md` - Go/No-Go перед внешним playtest;
- `playtest_script.md` - сценарий первого supervised playtest;
- `tools/validate_gdd.mjs` - автоматическая проверка GDD и баланса.

## Цель прототипа

Сделать mobile/web прототип на 30 минут игры для теста на реальных игроках:

- ЦА: дети 5-10 лет.
- Формат: mobile portrait first, web playable.
- Цель игрока: с `Сила 1/67` дойти до `15/67`.
- Главный крючок: число `X/67` над спрайтом и `67-жест`.
- Жанр: life-sim-lite incremental про становление.

## Платформы

### Primary

- Mobile portrait: 360x640 baseline.
- Web: браузерная версия с тем же portrait layout.

### Secondary

- PC/native build только как dev harness для скриншотов, input-emulation и QA.

## P0 Scope

### Экранная структура

1. Intro: `67/67 -> 1/67`, 5-10 секунд.
2. Главный экран.
3. Вкладка `Город`.
4. Вкладка `Дела`.
5. Вкладка `Улучшения`.
6. Вкладка `Дом`.
7. Event modal.
8. Mini-final `15/67`.

### Системы

- Сила `X/67`.
- Мем-коины.
- Крутость.
- Доход/сек.
- Сила клика.
- 67-жест.
- Город из 4 районов.
- Дела/задания как таймеры. Внутренне это `jobs`, но player-facing текст не использует `смена`.
- 4 навыка.
- Дом как апгрейды комфорта.
- Транспорт как апгрейды доступа/скорости.
- События с 2 хорошими выборами.
- Save/load.
- Offline progress с лимитом.
- Privacy-safe analytics.

## P1 / Future Scope

Не делать в первом playtest-прототипе:

- полноценный open world;
- сложную экономику с расходами;
- долги, штрафы, кризисы;
- биржу;
- настоящую монетизацию;
- rewarded ads;
- PvP;
- сложную систему отношений;
- тяжелый сюжет про бедность/унижение.

## Core Loop

```text
Сделать 67 / дело / обучение
  -> мем-коины + навык/крутость
  -> купить апгрейд
  -> сила X/67 или доход/сек или новый район
  -> событие/анимация/новая цель
  -> следующий milestone
```

## Public Resource Names

Использовать в UI:

- `Сила X/67`
- `Мем-коины`
- `Крутость`
- `Доход/сек`
- `Клик`

Не использовать в P0:

- деньги;
- репутация;
- энергия;
- долги;
- расходы;
- выживание.

## 30-Minute Milestones

| Время | Цель | Что игрок видит |
| ---: | --- | --- |
| 0:00 | старт | `67/67 -> 1/67`, кнопка `Сделать 67` |
| 0:30 | первая покупка | подсветка апгрейда за 5 мем-коинов |
| 2:00 | первый рост | `2/67`, вспышка, 67-жест |
| 5:00 | idle открыт | `+1/сек`, монетки сами летят |
| 8:00 | первое событие | Банан спрятал табличку |
| 12:00 | `6/67` | Банан ниже уровнем |
| 20:00 | `10/67` | открыт Мини-бизнес |
| 27:00 | финальное событие | Банан снова мутит схему |
| 30:00 | `15/67` | мини-финал, мечта `67/67` |

## Minimal Game State

```text
memeCoins
status
powerLabel
clickPower
coolness
incomePerSecond
skills: hands, mind, style, business
comfort
ownedUpgrades[]
ownedHousing
ownedTransport[]
unlockedDistricts[]
seenEvents[]
activeJob
lastSaveTime
sessionStartTime
```

## Screen Requirements

### Main Screen

Always visible:

- `X/67` над спрайтом.
- Герой и текущие визуальные апгрейды.
- Мем-коины.
- Доход/сек.
- Большая кнопка `Сделать 67`.
- Ближайшая покупка или цель.
- Bottom tabs: `Город`, `Дела`, `Улучшения`, `Дом`.

`Мемы` не являются отдельной P0-вкладкой. Мемы живут в реакциях, событиях,
стикерах, анимациях и финальном экране.

### City Tab

Shows:

- 4 района as cards.
- Locked states with one-line requirement.
- shortcut to the next available deal/training.

### Deals Tab

Shows:

- available `jobs` as player-facing `Дела`;
- active deal timer at the top while the big `Сделать 67` button stays usable;
- city district cards can deep-link to the relevant deal card;
- training cards needed for locked deals;
- active timer with `Забрать награду`;
- locked deal shows one next step and a shortcut button, e.g. `Прокачать руки`.

### Upgrades Tab

Shows:

- Affordable first.
- Locked below with requirement.
- Clear effect text: `+1 клик`, `+1 сила`, `+3/сек`.
- Purchase animation.

### Home Tab

Shows:

- current home visual;
- current transport visual;
- buyable housing cards;
- buyable transport cards;
- post-final lock for `67-тачка`.

Meme rewards are shown through event stickers, reactions, animations and the
mini-final, not through a separate required tab in P0.

## UI States

Every button/card needs:

- normal;
- affordable;
- locked;
- active;
- completed;
- newly unlocked;
- disabled during animation/job timer.

## Success Criteria

Prototype is ready for external playtest only if:

- `balance.json` has complete data for 15 upgrades and 4 events.
- Balance sim reaches `15/67` by 30 minutes for active play.
- Mobile layout works at 360x640.
- Web build saves/loads.
- Analytics events are implemented or stubbed.
- No forbidden words appear in player-facing P0 text.
- Playtest script exists.
- `node gamedesing/tools/validate_gdd.mjs` exits with code `0`.
- `node gamedesing/tools/simulate_balance.mjs` exits with code `0` for active and low-engagement scenarios.
