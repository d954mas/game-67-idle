# Runtime Test Plan

Статус: v0.1.  
Цель: как проверять готовый web/mobile prototype build.

Техническая структура билда проверяется по `prototype_technical_blueprint.md`.

## Required Test Commands

Design/data checks:

```powershell
node gamedesing/tools/validate_gdd.mjs
node gamedesing/tools/validate_site.mjs
node gamedesing/tools/simulate_balance.mjs
node gamedesing/tools/validate_reducer_vectors.mjs
node gamedesing/tools/validate_assets.mjs
node gamedesing/tools/validate_analytics.mjs
node gamedesing/tools/validate_ui_flow.mjs
node gamedesing/tools/validate_technical_blueprint.mjs
node gamedesing/tools/validate_release_readiness.mjs
node gamedesing/tools/validate_implementation_tasks.mjs
node gamedesing/tools/validate_risk_register.mjs
node gamedesing/tools/validate_playtest_observation_schema.mjs
node gamedesing/tools/validate_handoff_status.mjs
node gamedesing/tools/validate_runtime_evidence_manifest.mjs
```

Shortcut:

```powershell
node gamedesing/tools/validate_all.mjs
```

Runtime app checks are added by the implementation project. Minimum expected
commands:

```powershell
npm run test
npm run build
```

## Smoke Test

1. Open web build.
2. Confirm `child_test_safe` mode.
3. Confirm analytics disabled.
4. Confirm `Сделать 67` is the largest action.
5. Click `Сделать 67`.
6. Buy `Пакет для мем-коинов`.
7. Reset progress.

Pass:

- no crash;
- first click changes coins;
- first purchase changes visible scene;
- reset returns to `1/67`.

## First 5 Minutes

Route:

1. Start at `1/67`.
2. Click until first purchase.
3. Buy `Пакет для мем-коинов`.
4. Buy `Кепка 67`.
5. Open `Город`.
6. Use `К делу`.
7. Start first `Дело`.
8. Keep using `Сделать 67` while timer runs.
9. Claim reward.

Pass:

- first purchase <= 30 seconds;
- first status-up <= 90 seconds;
- at least 2 micro-reactions appear;
- child-facing UI never says `Работа`, `Смена`, `Поднять силу`.

## Full 30-Minute Route

Test both event-choice routes:

- always first choice;
- always second choice.

Pass:

- route reaches `15/67`;
- all 4 events appear;
- both final choices show mini-final;
- final screen shows team 67 gesture and `67/67` dream.

## Save And Offline

Route:

1. Reach `3/67`.
2. Start a `Дело`.
3. Close/reload app.
4. Confirm save restores state.
5. Simulate offline return.
6. Claim offline reward.

Pass:

- active timer restores or resolves predictably;
- offline reward capped at 30 minutes;
- offline does not trigger final without player action;
- reset clears local save.

## Analytics Safe Mode

Default external child test:

- `analyticsEnabled=false`;
- no network analytics calls;
- no persistent ad/tracking id;
- no free text;
- no account.

Internal supervised mode, if enabled:

- required event names match `data/balance.json`;
- every event includes build/platform/session_seconds/analytics_enabled;
- payload excludes name, age, location, ad id, free text.

## Viewport QA

Required:

- `360x640`;
- `390x844`;
- desktop browser with centered portrait frame;
- `fakeshots.html` board.

Pass:

- no horizontal scroll;
- tap targets >= 44px;
- `X/67` readable above hero;
- text stays inside cards/buttons;
- bottom tabs visible and not overlapped.

## Parent/Guardian Test Readiness

Before sending a public link:

- release checklist passes `data/release_readiness.json`;
- parent note attached;
- retention/deletion note prepared;
- hosting logs reviewed;
- SDK list reviewed;
- no ads/IAP/external links in P0;
- adult tester knows how to reset progress.

## Evidence To Save

Temporary audit files go to `tmp/`:

Runtime evidence follows `data/runtime_evidence_manifest.json`.

- desktop screenshot;
- mobile screenshot;
- console/runtime log;
- validator output;
- 30-minute route notes.
- playtest observation rows following `data/playtest_observation_schema.json`.

Final learnings go to:

- `playtest_review_round_*.md`;
- `playtest_script.md` notes converted into findings;
- `data/balance.json`;
- `common/what_worked.md`.
