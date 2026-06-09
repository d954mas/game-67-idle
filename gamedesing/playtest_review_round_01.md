# Playtest Review Round 01

Дата: 2026-06-09.

## Кто ревьюил

- Сабагент-критик: проверял GDD как production-документ для mobile/web playtest.
- Сабагент-игрок: симулировал 30 минут игры глазами ребенка 5-10 лет и родителя.

## Главные проблемы

1. GDD расползался между несколькими играми: чистый idle, life-sim, PC-native,
   mobile/web playtest.
2. Не было одного источника истины для разработки.
3. `balance.json` был неполным и не доказывал путь `1/67 -> 15/67`.
4. ЦА 5-10 требовала compliance, безопасного тона и короткого UX, но старые
   формулировки местами оставались взрослыми.
5. Не хватало аналитики, asset list, mobile/web UX, текстов и playtest gates.

## Решения

- `prototype_mvp_spec.md` становится главным документом для прототипа.
- Primary target: mobile portrait + web. PC/native остается dev harness.
- P0: life-sim-lite, а не большой life-sim.
- P0 включает город, работу, навыки, дом и транспорт, но в упрощенной форме:
  районы как вкладки/карточки, работы как таймеры, дом/транспорт как апгрейды.
- `X/67` называется публично `Сила`.
- `67-жест` - главный input/анимация.
- Тяжелые темы запрещены в P0: бедность, долги, унижение, ночевка, мусор,
  романтическая измена, месть.
- Для playtest нужны privacy-safe analytics и воронка.

## Что добавлено после ревью

- [prototype_mvp_spec.md](prototype_mvp_spec.md)
- [analytics_spec.md](analytics_spec.md)
- [mobile_web_ux_spec.md](mobile_web_ux_spec.md)
- [content_matrix.md](content_matrix.md)
- [asset_manifest.md](asset_manifest.md)
- [compliance_checklist.md](compliance_checklist.md)

