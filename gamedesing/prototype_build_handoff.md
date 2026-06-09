# Prototype Build Handoff

Статус: v0.1.
Дата: 2026-06-09.

Этот документ отвечает на вопрос: что именно нужно сделать разработчику, чтобы
получить первый mobile/web playtest-прототип без дополнительных дизайн-догадок.

## 1. Build Target

P0 билд:

- mobile portrait first: baseline `360x640`;
- web playable in browser with centered portrait layout;
- PC/native is only dev harness and screenshot/input automation target;
- session target: `1/67 -> 15/67` in 30 minutes active play.

## 2. Source Files

Use these files as authoritative:

- `gamedesing/prototype_mvp_spec.md` - scope and feature rules;
- `gamedesing/prototype_technical_blueprint.md` - technical build architecture;
- `gamedesing/data/balance.json` - content, costs, effects, unlocks;
- `gamedesing/data/reducer_test_vectors.json` - reducer/action acceptance vectors;
- `gamedesing/data/ui_flow.json` - screen flow, tabs, actions and UI acceptance;
- `gamedesing/mobile_web_ux_spec.md` - layout and touch rules;
- `gamedesing/analytics_spec.md` - playtest event taxonomy;
- `gamedesing/data/analytics_events.json` - machine-readable analytics contract;
- `gamedesing/content_matrix.md` - player-facing tone and text;
- `gamedesing/p0_ui_copy.md` - final player-facing card/button copy;
- `gamedesing/playtest_acceptance_gates.md` - release gates.
- `gamedesing/playtest_script.md` - first supervised playtest script.

If files disagree, resolve in this order:

1. `data/balance.json` for numerical/gameplay data.
2. `data/reducer_test_vectors.json` for reducer/action semantics.
3. `data/analytics_events.json` for analytics payload semantics.
4. `data/ui_flow.json` for screen/action semantics.
5. `prototype_mvp_spec.md` for feature scope.
6. `prototype_technical_blueprint.md` for implementation architecture.
7. `mobile_web_ux_spec.md` for layout behavior.
8. `p0_ui_copy.md` for exact player-facing wording.
9. `content_matrix.md` for tone/safety rules.

## 3. Minimal Runtime Model

```ts
type GameState = {
  memeCoins: number;
  status: number;
  powerLabel: string;
  clickPower: number;
  incomePerSecond: number;
  coolness: number;
  handsSkill: number;
  mindSkill: number;
  styleSkill: number;
  businessSkill: number;
  comfort: number;
  workSpeedBonus: number;
  gestureLevel: number;
  screenCompanion: null | string;
  visualStage: string;
  finalReady: boolean;
  ownedUpgrades: string[];
  ownedHousing: string[];
  ownedTransport: string[];
  unlockedActivities: string[];
  seenEvents: string[];
  activeJob: null | {
    jobId: string;
    startedAtMs: number;
    durationMs: number;
  };
  lastSaveTimeMs: number;
  sessionStartTimeMs: number;
};
```

Derived values:

- `powerLabel = status + "/67"`;
- `status` is capped at `15` in P0 before post-final content;
- affordable items are items with `cost <= memeCoins` and requirements met;
- locked item card shows one requirement only.
- `workSpeedBonus`, `gestureLevel`, `screenCompanion`, `visualStage` and
  `finalReady` are saved gameplay state because `balance.json` writes them.
- If an item has multiple requirements, use `requirementPriority` and
  `lockedHints` from `balance.json` to show the next single child-readable step.

## 4. Required Reducers

Implement these actions:

| Action | Required Result |
| --- | --- |
| `startSession` | load save, calculate offline reward, emit `session_start` |
| `do67` | add `clickPower` meme-coins, play 67 gesture, emit `activity_used` |
| `buyUpgrade(upgradeId)` | spend coins, apply effects, save, emit `upgrade_bought` |
| `buyHousing(housingId)` | spend coins, apply effects, change home visual |
| `buyTransport(transportId)` | spend coins, apply effects, change travel visual |
| `startTraining(trainingId)` | spend coins, start short timer or apply after duration |
| `startJob(jobId)` | start deal timer if requirements met |
| `claimJob(jobId)` | apply job reward, save, emit `job_completed` |
| `chooseEvent(eventId, choiceId)` | apply choice effects, mark seen, save |
| `tick(deltaSeconds)` | add idle income, update timers, unlock notifications |
| `saveGame` | persist current state |
| `resetForPlaytest` | clear local save and analytics session id |

Effects from `balance.json` must be generic:

- `add`;
- `set`;
- `setMax`;
- `addFromResource`;
- `chanceAdd`;
- `unlock`;
- `triggerEvent`.

## 5. Screen Flow

```text
Intro
  -> Main
      -> City
      -> Deals
      -> Upgrades
      -> Home
      -> Event Modal
      -> Offline Modal
      -> Mini Final
```

Bottom tabs for P0:

- `Город`;
- `Дела`;
- `Улучшения`;
- `Дом`.

Do not add a separate P0 `Мемы` tab unless the prototype has spare time.
Memes appear as reactions, events, stickers and final screen.

## 6. Screen Acceptance

### Intro

- shows `67/67 -> 1/67`;
- lasts 5-10 seconds or can be skipped after 2 seconds;
- no more than 2 short text lines;
- ends on main screen with pulsing `Сделать 67`.

### Main

- `X/67` is visually dominant above hero;
- primary button is at least 72px high on mobile;
- first click within 5 seconds is possible without reading;
- first affordable upgrade is highlighted at 5 coins.

### City

- shows 4 district cards;
- locked district shows one requirement;
- unlocked district links to deal/training content.

### Deals

- shows available internal `jobs` as player-facing `Дела`;
- city district cards can deep-link here through `К делу`;
- active deal timer remains visible while the main `Сделать 67` action stays usable;
- only one active deal at a time in P0;
- training cards are shown near locked deals that need skills;
- locked deal shows one next step using `lockedHints`;
- active deal has visible timer and `Забрать награду`;
- reward visibly flies into meme-coins.

### Upgrades

- affordable first, locked lower;
- every card has cost and one-line effect;
- buying status-up plays `X/67` animation.

### Home

- shows current home and transport visual;
- housing and transport are buyable cards;
- `67-тачка` is post-MVP: hidden or locked until after `15/67`, and must not
  increase `status` inside the P0 cap.

### Event Modal

- pauses background clicking;
- title, one short body, 2 choices;
- every choice gives visible reward;
- close only after choosing.

### Mini Final

- triggers when player reaches `15/67` through final event path;
- shows hero, team, Банан reaction and future goal `67/67`;
- offers `Играть дальше` and `Сбросить для теста`.

## 7. Save And Offline

Save:

- every 10 seconds;
- after every purchase;
- after every event choice;
- after final.

Offline:

- cap at 30 minutes;
- show one return modal;
- give meme-coins from `incomePerSecond`;
- never complete final while offline. Player must press a button after return.

## 8. Analytics Requirements

Implement analytics as local/stub-safe for first prototype:

- anonymized session id;
- no name, age, email, phone, geolocation, ad id;
- console/file/local endpoint is acceptable for internal web playtest;
- event names match `analytics_spec.md`.

Required events before external test:

- `session_start`;
- `first_click`;
- `upgrade_bought`;
- `status_changed`;
- `activity_used`;
- `job_started`;
- `job_completed`;
- `event_seen`;
- `event_choice`;
- `final_15_reached`;
- `session_end`.

## 9. Prototype Cut Line

P0 is done when:

- all required reducers exist;
- all 4 tabs work;
- all data from `balance.json` can be rendered;
- all 4 events can trigger;
- `15/67` can be reached;
- save/offline works;
- analytics stubs emit required events;
- acceptance gates pass.

Anything else is P1.
