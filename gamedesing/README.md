# Game Design Hub

Папка для референсов, концепта и GDD проекта `Game 67 Idle`.

## Рабочая Идея

Life simulator-lite / idle про становление: герой был легендарным `67/67`, но
Банан намутил мемную подставу, Клубника знает секрет, и сила героя упала до
`1/67`. Игрок возвращает силу через `67-жест`, город, работы, навыки, дом,
транспорт, команду и мем-события.

Первый прототип: `1/67 -> 15/67` за 30 минут активной игры.

## Документы

- [index.html](index.html) - визуальный GDD-сайт.
- [fakeshots.html](fakeshots.html) - отдельный board для скриншотов fake shots.
- [editor.html](editor.html) - локальный редактор GDD-файлов через сервер.
- [gdd.md](gdd.md) - основной краткий GDD.
- [prototype_mvp_spec.md](prototype_mvp_spec.md) - source of truth для MVP.
- [dev_design_handoff_plan.md](dev_design_handoff_plan.md) - общий план для разработчика и дизайнера.
- [prototype_build_handoff.md](prototype_build_handoff.md) - handoff для реализации прототипа.
- [prototype_technical_blueprint.md](prototype_technical_blueprint.md) - технический blueprint сборки P0.
- [game_implementation_plan.md](game_implementation_plan.md) - порядок разработки первого playable slice.
- [ui_bible.md](ui_bible.md) - UI bible, компоненты, states и slice9 правила.
- [screen_mockups_spec.md](screen_mockups_spec.md) - список мокапов экранов и их состав.
- [asset_generation_brief.md](asset_generation_brief.md) - brief генерации ассетов и export правила.
- [art/generated-67-comeback-keyart.png](art/generated-67-comeback-keyart.png) - generated key art: `67/67 -> предали -> 1/67`.
- [art/generated-67-gameplay-fakeshot.png](art/generated-67-gameplay-fakeshot.png) - generated gameplay fake shot.
- [art/generated-67-life-sim-progression.png](art/generated-67-life-sim-progression.png) - generated life-sim progression board.
- [art/generated-67-asset-sheet.png](art/generated-67-asset-sheet.png) - generated asset sheet для будущей нарезки.
- [art_bible.html](art_bible.html) - visual runtime art bible с готовыми generated ассетами.
- [assets/generated/runtime_asset_manifest.json](assets/generated/runtime_asset_manifest.json) - game-ready generated runtime asset manifest.
- [assets/generated/runtime_composed_screen.png](assets/generated/runtime_composed_screen.png) - экран, собранный из отдельных generated PNG.
- [assets/asset_pack_manifest.json](assets/asset_pack_manifest.json) - ready technical asset pack manifest.
- [assets/ui/slice9.json](assets/ui/slice9.json) - ready UI slice9 metadata.
- [handoff_status.md](handoff_status.md) - что доказано, что не доказано, следующий шаг реализации.
- [implementation_backlog.md](implementation_backlog.md) - порядок сборки P0 прототипа.
- [runtime_test_plan.md](runtime_test_plan.md) - runtime-проверки готового билда.
- [playtest_acceptance_gates.md](playtest_acceptance_gates.md) - Go/No-Go gates перед playtest.
- [playtest_script.md](playtest_script.md) - сценарий первого supervised playtest.
- [data/balance.json](data/balance.json) - баланс и контент-таблица.
- [data/reducer_test_vectors.json](data/reducer_test_vectors.json) - acceptance vectors для reducer/actions.
- [data/asset_manifest.json](data/asset_manifest.json) - machine-readable покрытие ассетов.
- [data/analytics_events.json](data/analytics_events.json) - machine-readable контракт аналитики.
- [data/ui_flow.json](data/ui_flow.json) - machine-readable карта экранов, действий и UI-состояний.
- [data/ui_components.json](data/ui_components.json) - machine-readable UI компоненты и slice9 параметры.
- [data/asset_generation_queue.json](data/asset_generation_queue.json) - очередь генерации P0 ассетов и мокапов.
- [data/release_readiness.json](data/release_readiness.json) - machine-readable checklist для web/mobile playtest release.
- [data/implementation_tasks.json](data/implementation_tasks.json) - machine-readable P0 task matrix.
- [data/risk_register.json](data/risk_register.json) - machine-readable P0 risk register.
- [data/playtest_observation_schema.json](data/playtest_observation_schema.json) - схема ручного observation log.
- [data/runtime_evidence_manifest.json](data/runtime_evidence_manifest.json) - список runtime evidence для снятия No-Go.
- [tools/simulate_balance.mjs](tools/simulate_balance.mjs) - проверка баланса.
- [tools/validate_gdd.mjs](tools/validate_gdd.mjs) - структурная проверка GDD и баланса.
- [market_research.md](market_research.md) - исследование рынка.
- [references.md](references.md) - референсы и что берем из жанра.
- [gameplay_spec.md](gameplay_spec.md) - подробные системы и UI/UX.
- [mobile_web_ux_spec.md](mobile_web_ux_spec.md) - mobile/web UX.
- [analytics_spec.md](analytics_spec.md) - события для playtest.
- [parent_playtest_note.md](parent_playtest_note.md) - текст и ограничения для родителя/опекуна.
- [content_matrix.md](content_matrix.md) - матрица контента MVP.
- [p0_ui_copy.md](p0_ui_copy.md) - финальный player-facing copy для P0 карточек.
- [asset_manifest.md](asset_manifest.md) - список нужных ассетов.
- [visual_contract.md](visual_contract.md) - связь баланса, визуальных стадий и fake shots.
- [compliance_checklist.md](compliance_checklist.md) - детская безопасность.
- [playtest_review_round_01.md](playtest_review_round_01.md) - выводы критика и симуляции игрока.
- [lore.md](lore.md) - лор и мемная легенда.
- [open_questions.md](open_questions.md) - открытые вопросы.

## Папки

- `common/` - Obsidian-style база знаний: короткие заметки, связи, решения и личные правила.
- `refs/` - ссылки, заметки и скриншоты референсов.
- `art/` - финальные визуальные материалы для GDD.
- `assets/` - будущие финальные игровые ассеты, разбитые по UI/icons/characters/backgrounds/fx.
- `data/` - редактируемые данные баланса.
- `tools/` - полезные проектные скрипты.

Временные генерации, скриншоты аудита и одноразовые скрипты держать в корневой
папке `tmp/`, которая находится под gitignore.

## Локальный Сервер

Запуск из корня проекта:

```powershell
node gamedesing/server.mjs
```

Сайт:

```text
http://127.0.0.1:8067/
```

Редактор файлов:

```text
http://127.0.0.1:8067/editor.html
```

Проверка баланса:

```powershell
node gamedesing/tools/simulate_balance.mjs
```

Полная проверка GDD:

```powershell
node gamedesing/tools/validate_gdd.mjs
```

Проверка визуального GDD-сайта:

```powershell
node gamedesing/tools/validate_site.mjs
```

Проверка готовности handoff к сборке:

```powershell
node gamedesing/tools/validate_build_readiness.mjs
```

Проверка player-facing copy:

```powershell
node gamedesing/tools/validate_copy.mjs
```

Проверка reducer/action vectors:

```powershell
node gamedesing/tools/validate_reducer_vectors.mjs
```

Проверка покрытия ассетов:

```powershell
node gamedesing/tools/validate_assets.mjs
```

Проверка контракта аналитики:

```powershell
node gamedesing/tools/validate_analytics.mjs
```

Проверка UI flow:

```powershell
node gamedesing/tools/validate_ui_flow.mjs
```

Проверка technical blueprint:

```powershell
node gamedesing/tools/validate_technical_blueprint.mjs
```

Проверка release readiness:

```powershell
node gamedesing/tools/validate_release_readiness.mjs
```

Проверка implementation task matrix:

```powershell
node gamedesing/tools/validate_implementation_tasks.mjs
```

Проверка risk register:

```powershell
node gamedesing/tools/validate_risk_register.mjs
```

Проверка playtest observation schema:

```powershell
node gamedesing/tools/validate_playtest_observation_schema.mjs
```

Проверка handoff status:

```powershell
node gamedesing/tools/validate_handoff_status.mjs
```

Проверка runtime evidence manifest:

```powershell
node gamedesing/tools/validate_runtime_evidence_manifest.mjs
```

Единая проверка всех GDD-контрактов:

```powershell
node gamedesing/tools/validate_all.mjs
```
