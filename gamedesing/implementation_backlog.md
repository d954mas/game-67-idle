# Implementation Backlog

Статус: v0.1.  
Цель: порядок сборки P0 web/mobile прототипа без дизайн-догадок.

Machine-readable task matrix: `data/implementation_tasks.json`.

## Build Rule

Разработчик берет source-of-truth в таком порядке:

1. `data/balance.json` - контент, экономика, эффекты, visible results.
2. `data/reducer_test_vectors.json` - expected reducer/action semantics.
3. `data/asset_manifest.json` - machine-readable asset coverage.
4. `data/analytics_events.json` - analytics payload contract.
5. `data/ui_flow.json` - screen/action/UI state contract.
6. `prototype_technical_blueprint.md` - architecture/file map/test commands.
7. `prototype_build_handoff.md` - state/reducer/actions.
8. `mobile_web_ux_spec.md` - layout/touch.
9. `visual_contract.md` - visual stages/fake shots/fallbacks.
10. `p0_ui_copy.md` - финальный player-facing текст.
11. `playtest_acceptance_gates.md` - приемка.
12. `playtest_script.md` - ручной сценарий первого теста.

Если документы конфликтуют, `data/balance.json` побеждает для чисел и id, а
`p0_ui_copy.md` побеждает для player-facing текста, а `content_matrix.md` побеждает для tone/safety rules.

## Phase 0. Project Shell

Deliverables:

- web app opens in browser;
- mobile portrait frame centered on desktop;
- project modules follow `prototype_technical_blueprint.md`;
- reset button exists for playtest;
- local save key is namespaced, e.g. `game67:p0:save`;
- `child_test_safe` mode is default.

Acceptance:

- load to first clickable UI under 5 seconds;
- no ads/IAP/account/chat/free text;
- analytics disabled by default.

## Phase 1. Core State And Reducer

Implement:

- `GameState` from `prototype_build_handoff.md`;
- effect types: `add`, `set`, `setMax`, `addFromResource`, `chanceAdd`,
  `unlock`, `triggerEvent`;
- requirements: scalar fields, `ownedTransport`, `upgrade`;
- status cap at `15/67` in P0;
- both final choices set `finalReady=true`.
- implementation passes `data/reducer_test_vectors.json`.

Acceptance:

- buying every item from `balance.json` does not crash;
- unknown effect/target fails loudly in dev;
- no repeat purchase unless item type explicitly allows it.

## Phase 2. Main Screen

Implement:

- hero stage;
- `X/67` above hero;
- meme-coins and `+/сек`;
- one `nextGoalTemplates` card;
- big `Сделать 67` button;
- micro-reactions from `microReactions`;
- active deal timer, if any.

Acceptance:

- first click within 5 seconds is obvious;
- `Сделать 67` remains usable while a deal timer runs;
- first purchase can happen within 30 seconds.
- screen components/actions follow `data/ui_flow.json`.

## Phase 3. Tabs

Implement P0 tabs only:

- `Город`;
- `Дела`;
- `Улучшения`;
- `Дом`.

City:

- 4 district cards;
- one lock reason;
- unlocked district has `К делу` deep-link.

Deals:

- one active deal;
- training shortcuts near locked deals;
- `Забрать награду` state.

Upgrades:

- affordable first;
- each purchase shows `visibleResult`.

Home:

- current `visualStage`;
- housing/transport visual props;
- post-MVP `67-тачка` shown only as future dream after mini-final.

## Phase 4. Events And Mini-Final

Implement:

- `banana_sign`;
- `alarm_0600_event`;
- `strawberry_secret`;
- `final_banana_scheme`;
- two choices per event;
- visible choice reward;
- mini-final when status reaches `15/67` through final event path.

Acceptance:

- first-choice route reaches `15/67`;
- second-choice route reaches `15/67`;
- mini-final shows team 67 shared gesture and dream `67/67`.

## Phase 5. Save, Offline, Analytics

Save:

- coins, status, bought items, active timers, seen events, finalReady;
- restore active deal timer;
- reset clears save.

Offline:

- cap at 30 minutes;
- show one return modal;
- never auto-complete final `15/67` without player action.

Analytics:

- default `analyticsEnabled=false`;
- emit local/stub events only when enabled;
- required event list from `balance.json.requiredAnalyticsEvents`.

## Phase 6. Visual QA

Build must match these shots:

- `fakeshots.html` first screen;
- first click;
- first purchase;
- status-up;
- city map;
- deal timer;
- event;
- home;
- mini-final.

Acceptance:

- no horizontal scroll at `360x640` and `390x844`;
- tap targets >= 44px;
- text readable, no overlap;
- motion short and non-flashing.
- asset coverage passes `data/asset_manifest.json`.

## Done For External P0 Test

Done means:

- `runtime_test_plan.md` passes;
- `playtest_acceptance_gates.md` Gates 1-10 pass;
- parent note is visible or included in invite;
- no P0 blocker remains in `open_questions.md`;
- test can be run with 5-8 children and guardians.
- moderator can run the first session from `playtest_script.md`.
