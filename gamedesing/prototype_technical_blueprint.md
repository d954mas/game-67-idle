# Prototype Technical Blueprint

Статус: v0.1.  
Цель: технический план сборки P0 web/mobile прототипа без архитектурных догадок.

Этот документ не заменяет GDD. Он говорит, как собрать первый билд из уже
зафиксированных контрактов.

## 1. Recommended Stack

P0 web:

- TypeScript;
- Vite или аналогичный легкий web bundler;
- framework optional: React/Solid/Svelte допустимы, если state/reducer остается чистым;
- CSS modules/plain CSS допустимы;
- localStorage для save;
- no backend for P0 gameplay;
- no ad SDK;
- no external analytics SDK before separate privacy review.

Mobile store test:

- сначала тот же web build как mobile portrait;
- wrapper/native export допустим только после прохождения web P0 gates;
- gameplay logic must stay shared with web build.

## 2. Suggested File Map

```text
src/
  data/
    balance.json
    reducer_test_vectors.json
    analytics_events.json
    ui_flow.json
    asset_manifest.json
  game/
    state.ts
    reducer.ts
    effects.ts
    selectors.ts
    requirements.ts
    simulation.ts
  save/
    storage.ts
    offline.ts
  analytics/
    analytics.ts
    schema.ts
  ui/
    App.tsx
    screens/
      Intro.tsx
      Main.tsx
      City.tsx
      Deals.tsx
      Upgrades.tsx
      Home.tsx
      EventModal.tsx
      OfflineModal.tsx
      MiniFinal.tsx
    components/
      HeroStage.tsx
      TopStats.tsx
      BottomTabs.tsx
      NextGoal.tsx
      Card.tsx
  tests/
    reducer_vectors.test.ts
    balance_route.test.ts
    ui_flow.test.ts
    analytics_schema.test.ts
```

Names can change, but responsibilities should not.

## 3. Data Import Rule

Runtime must load or copy these contracts without changing IDs:

1. `data/balance.json`;
2. `data/reducer_test_vectors.json`;
3. `data/ui_flow.json`;
4. `data/analytics_events.json`;
5. `data/asset_manifest.json`.

IDs from these files are public contract. Do not rename them in implementation
without updating validators and docs.

## 4. Core Runtime Modules

State:

- starts from `balance.initialState`;
- derives `powerLabel`;
- caps P0 status at `15/67`;
- stores `visualStage`, `screenCompanion`, `gestureLevel`, `finalReady`;
- stores active job timer and seen events.

Reducer:

- actions listed in `prototype_build_handoff.md`;
- all effects are applied through generic `effects.ts`;
- unknown effect/target throws in dev;
- reducer passes `data/reducer_test_vectors.json`.

Requirements:

- supports scalar requirements;
- supports `ownedTransport`;
- supports `upgrade`;
- multi-requirement cards show only one next step using `requirementPriority`
  and `lockedHints`.

Selectors:

- affordable first;
- locked below;
- next goal from `nextGoalTemplates`;
- current visible stage from state and asset manifest.

## 5. UI Runtime

Screen source:

- implement screens and states from `data/ui_flow.json`;
- bottom tabs are exactly `Город`, `Дела`, `Улучшения`, `Дом`;
- main `Сделать 67` remains usable during deal timer;
- every card renders `visibleResult` after action;
- every locked card shows one next step.

Responsive:

- `360x640` baseline;
- `390x844` common target;
- desktop uses centered portrait frame;
- no horizontal scroll;
- tap targets follow `data/ui_flow.json.globalRules`.

Visual fallback:

- use `data/asset_manifest.json` first;
- if asset is missing, use simple CSS/shape fallback with same `assetId`;
- every `visualStage`, `screenCompanion`, animation and fake shot must resolve.

## 6. Save And Offline

Storage:

- key: `game67:p0:save`;
- save after purchase, event choice, final, and every 10 seconds;
- reset clears save and analytics session id.

Offline:

- max 30 minutes;
- reward only meme-coins from `incomePerSecond`;
- one return modal;
- never completes final while offline.

## 7. Analytics Stub

Default:

- `analyticsEnabled=false` for external child tests;
- no network calls unless explicitly enabled after guardian notice/consent.

Implementation:

- validate event payload against `data/analytics_events.json`;
- include common params on every event;
- reject forbidden payload fields in dev;
- local console/file endpoint is enough for internal supervised testing.

## 8. Required Tests

Minimum implementation tests:

```powershell
npm run test
npm run build
```

Test coverage:

- reducer vectors pass;
- active first-choice route reaches `15/67`;
- active second-choice route reaches `15/67`;
- low-engagement route reaches at least `10/67`;
- analytics payloads match `data/analytics_events.json`;
- UI screens/tabs/actions match `data/ui_flow.json`;
- save/load restores active timer and bought items;
- offline cap does not trigger final.

## 9. Build Cut Line

Do build:

- full P0 from `1/67` to `15/67`;
- web playable portrait;
- local reset;
- save/offline;
- analytics stub;
- fake-shot screenshot states.

Do not build in P0:

- backend economy;
- ads/IAP;
- account/login;
- chat/free text;
- prestige;
- full open world;
- `67-тачка` as status progress before mini-final.

## 10. First Implementation Order

1. Load data contracts.
2. Implement state/reducer/effects.
3. Pass reducer vectors.
4. Build main screen and `Сделать 67`.
5. Build tabs from `data/ui_flow.json`.
6. Render all balance collections.
7. Add events and mini-final.
8. Add save/offline.
9. Add analytics stub.
10. Capture required screenshots and run gates.

Связи: `prototype_build_handoff.md`, `implementation_backlog.md`,
`runtime_test_plan.md`, `data/ui_flow.json`, `data/analytics_events.json`.
