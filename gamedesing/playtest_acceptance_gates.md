# Playtest Acceptance Gates

Статус: v0.1.
Дата: 2026-06-09.

Цель: до выпуска mobile/web playtest-билда на реальных игроков проверить, что
прототип не просто запускается, а тестирует именно гипотезы `Game 67 Idle`.

## Gate 1. Scope Lock

Pass criteria:

- основной источник правды: `prototype_mvp_spec.md`;
- реализация читает или вручную повторяет `data/balance.json` без расхождений;
- реализация следует фазам из `implementation_backlog.md`;
- runtime evidence собирается по `runtime_test_plan.md`;
- first session follows `playtest_script.md`;
- player-facing текст использует `p0_ui_copy.md`;
- P0 содержит 4 вкладки: `Город`, `Дела`, `Улучшения`, `Дом`;
- нет P0-систем из future scope: долги, штрафы, биржа, rewarded ads, PvP,
  сложные отношения, prestige.

Fail if:

- игрок может пройти P0 без `67-жеста`;
- игрок может дойти до `15/67` только через offline;
- `67-тачка` открывается до `15/67`.

## Gate 2. Balance

Required command:

```powershell
node gamedesing/tools/simulate_balance.mjs
node gamedesing/tools/validate_site.mjs
node gamedesing/tools/validate_build_readiness.mjs
node gamedesing/tools/validate_copy.mjs
node gamedesing/tools/validate_reducer_vectors.mjs
node gamedesing/tools/validate_assets.mjs
node gamedesing/tools/validate_analytics.mjs
node gamedesing/tools/validate_ui_flow.mjs
node gamedesing/tools/validate_technical_blueprint.mjs
node gamedesing/tools/validate_release_readiness.mjs
node gamedesing/tools/validate_risk_register.mjs
node gamedesing/tools/validate_playtest_observation_schema.mjs
node gamedesing/tools/validate_handoff_status.mjs
node gamedesing/tools/validate_runtime_evidence_manifest.mjs
```

Pass criteria:

- exit code `0`;
- reaches `15/67`;
- first purchase target <= 30 seconds;
- first status-up target <= 90 seconds;
- all 4 events appear in simulated route.
- active first-choice, active second-choice and low-engagement scenarios have
  no simulation errors.
- public GDD site contains all 9 fake shots required by `visual_contract.md`.
- screen flow and UI actions pass `data/ui_flow.json`.
- technical blueprint is complete enough for implementation handoff.
- release checklist passes `data/release_readiness.json`.
- P0 risks are tracked in `data/risk_register.json`.
- manual observation rows follow `data/playtest_observation_schema.json`.
- handoff status is explicit in `handoff_status.md`.
- runtime evidence requirements are explicit in `data/runtime_evidence_manifest.json`.

## Gate 3. Mobile UX

Viewports:

- `360x640`;
- `390x844`;
- desktop browser with centered portrait frame.

Pass criteria:

- no horizontal scroll;
- screen components and transitions follow `data/ui_flow.json`;
- primary button remains visible on main screen;
- bottom tabs do not overlap content;
- `X/67` above hero is readable without zoom;
- no button smaller than 48px tap target;
- no player-facing text below 13px;
- all locked cards explain exactly one requirement.
- multi-requirement cards use `requirementPriority` and `lockedHints`.

## Gate 4. First 5 Minutes

Manual playtest route:

1. Reset save.
2. Start session.
3. Do not read instructions; press the obvious button.
4. Buy first upgrade when highlighted.
5. Reach first status-up.
6. Open `Город`.
7. Start first job or training.
8. See first idle income.

Pass criteria:

- first click happens without confusion;
- first upgrade is visible before 30 seconds;
- `2/67` appears before 2 minutes;
- player sees a clear next goal after every purchase;
- no stuck state with no affordable action for more than 90 seconds.
- child confusion is counted if the child taps locked/irrelevant UI 5+ times,
  asks "что делать?", or stops interacting for 20 seconds.

## Gate 5. Full 30-Minute Route

Manual route:

1. Play actively until `15/67`.
2. Use work timers when available.
3. Buy upgrades in natural affordable order.
4. Trigger and choose every event.
5. Confirm mini-final appears.

Pass criteria:

- `15/67` is reachable within 30 minutes active play;
- final event `Банан снова мутит схему` appears before mini-final;
- mini-final references future goal `67/67`;
- player can continue after mini-final;
- reset button exists for repeated playtests.

## Gate 6. Save And Offline

Manual route:

1. Start game, buy at least 2 upgrades.
2. Reload page.
3. Confirm state persists.
4. Wait or simulate offline time.
5. Return and claim offline reward.

Pass criteria:

- save restores coins, status, bought items and active timers;
- offline reward is capped at 30 minutes;
- offline modal appears once;
- offline cannot trigger final completion without player action.

## Gate 7. Analytics

Pass criteria:

- required event names from `prototype_build_handoff.md` are emitted;
- payload contract follows `data/analytics_events.json`;
- every event includes `session_id`, `build`, `platform`, `session_seconds`;
- no personal data is collected;
- analytics can be disabled for a child/parent test if needed.
- default external child test mode has analytics disabled until guardian notice
  and consent are handled.
- persistent identifiers are treated as privacy-sensitive unless used only for
  permitted internal operations.

## Gate 8. Child-Safe Content

Player-facing P0 text must not contain:

- `измена`;
- `бомж`;
- `бедность`;
- `ночевка`;
- `мусор`;
- `долги`;
- `штраф`;
- `унижение`;
- `месть`;
- `кризис`.

Allowed substitutes:

- `подстава`;
- `обнулили мем`;
- `Банан мутит`;
- `Клубника знает секрет`;
- `сила растет`;
- `до 67 ближе`.

## Gate 9. Store/Web Readiness

Before wider playtest:

- web build has a clear reset/save behavior;
- release mode follows `data/release_readiness.json`;
- parent-facing privacy text exists and states: no ads, no account, no free
  text input, what local/session data is stored, retention, deletion/reset, and
  how analytics can be disabled;
- third-party analytics/ad SDKs are absent in P0 unless separately reviewed;
- hosting/server logs are reviewed for IP/user-agent retention;
- no ads or monetization in P0;
- no account creation;
- no free text input from children;
- screenshots exist for all required shots from `mobile_web_ux_spec.md` and
  `visual_contract.md`.
- baseline load to first clickable UI is under 5 seconds on target devices;
- no crash/hard error in a 30-minute manual route;
- motion can be reduced or animations remain short/non-flashing.

## Gate 9.1. Playtest Matrix

Run at least:

- 3 children age 5-7;
- 3 children age 8-10;
- guardian present for every session;
- one repeat session after reload/save;
- one web session and one mobile portrait session if both targets are tested.

Observation log must include:

- first clicked element;
- first confusion moment;
- first understood goal;
- parent content concern, if any;
- whether child can explain `67` as a funny power rank.
- fields from `data/playtest_observation_schema.json`.

## Gate 10. Go / No-Go

Go if:

- Gates 1-9 pass;
- `handoff_status.md` has no Current No-Go item left for runtime build evidence;
- evidence listed in `data/runtime_evidence_manifest.json` exists for runtime build;
- no P0 blocker remains in `open_questions.md`;
- no critical unresolved risk remains in `data/risk_register.json`;
- implementation backlog P0 phases are complete;
- runtime test plan has saved evidence in `tmp/` or final playtest notes;
- first qualitative test can be run with 5-8 children and guardians.
- moderator script exists and covers first click, first purchase, confusion and parent concern.

No-go if:

- balance sim fails;
- mobile layout fails baseline viewport;
- analytics/privacy gate fails;
- child-safe text gate fails;
- playtest route cannot reach `15/67`.
