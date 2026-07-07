---
id: T0332
title: "Canvas pack-карта: конфиги паков из UI + axes в meta при promote"
status: review
project: P001
epic: ""
priority: P1
tags: [assets, imagegen, canvas, declarative-art]
created: 2026-07-06
updated: 2026-07-07
---

## What

Выросло из T0330 (пилот принят лидом 2026-07-07). Два связанных куска:

1. **Pack-карта на канвасе** — расширение recipe card (блоб аддитивный,
   patchRecipe валидируется): поля `axes`, `sheet`, `background`, `anchor_ref` +
   кнопка «Generate pack» поверх expand_jobs/gen_batch. `style_prefix` — по
   ссылке `style_ref` на style card вместо копипасты (трение доказано пилотом:
   3 конфига мечей с продублированным verbatim-стилем). Лид явно просил
   задавать/читать конфиги через канвас.
2. **Фаза 3 из спеки T0330**: axes из cells-манифеста доезжают до canvas meta
   вырезанных элементов при promote — отбор фильтруем по осям. Сейчас
   provenance пилота лежит note-заметкой + сайдкарами на диске, meta пустая.

Спека-родитель:
`.codex/skills/nt-asset-image-generation/references/build_spec_pack_expander_2026-07-07.md`

## Done when

Спека (v2 «слить в recipe», 4 ревью):
`ai_studio/assets/canvas/docs/build_spec_pack_card_2026-07-07.md`

- [x] recipe.pack (опциональный режим карты) + разморозка params; отдельный
      тип карты удалён (решение лида «Слить»).
- [x] recipe-pack-preview (бесплатное превью через реальный экспандер) /
      recipe-pack-generate (резюм --run, реген --sheet = REPLACE) /
      recipe-pack-slice (гейт по счёту, per-sheet контракт).
- [x] axes в meta: лист — полный манифест+снапшоты, кат — своя ячейка+оси.
- [x] Экспорт opt-in: group.screen (решение лида), migrate_screen_flags.mjs
      (dry-run; --apply за лидом).
- [x] Фаза C UI: pack-суб-блок инспектора со всеми UX-находками ревью;
      галочка Screen; рендер meta.pack; Regenerate на листе.
- [x] Доки: README Pack mode; «Disk vs Canvas pack» в скилл-референсе;
      указатель в canvas-скилле; sync.
- [x] Верификация: 687/687 тестов (реальный экспандер/регионы, не заглушки);
      живой цикл card→preview→generate→slice→regen на pack-smoke-t0332-b19615.
- [ ] Лид: прогнать migrate_screen_flags.mjs --apply (иначе экспорт старых
      проектов пуст) и принять фичу в UI.

## Open questions

- candidates>1: резюм дедуплицирует по осям без индекса кандидата — отдельный
  пакет при реальной надобности (записано в спеке).

## Log

- 2026-07-07: заведено при закрытии T0330; пилот принят лидом
  («отлично работает»).
- 2026-07-06: Лид поднял приоритет (2026-07-07): нужна ОДНА система на канвасе — стиль+промпт+пак сочетаются, конфиги видны и повторяемы; пишем build-spec
- 2026-07-06: Build-spec написан и прошёл dual-review (блокеры: видимость карты на 4 поверхностях, вызов экспандера, sliceRegions без меты; вычтены expanded_jobs и anchor_ref); исполнение фаз A/B пошло, фаза C ждёт UX-ответов лида
- 2026-07-07: Решение лида: СЛИТЬ в recipe card (не отдельный тип). Спека v2: recipe.pack опциональный суб-объект; bg_key/n_candidates/prompt/style_ref переиспользуются; ноль новых поверхностей видимости; фаза A пересаживается (~70%). Дельта на фокус-ревью
- 2026-07-07: UX/DX dual-review по запросу лида: удобно/понятно risky, гибко ok; 16 находок сведены в спеку (резюм --run/--sheet, failed[], per-sheet контракт slice, баннер expanded, screen-хинт, доки/скиллы как deliverables). Лид решил: params разморожен, экспорт opt-in по screen-флагу
- 2026-07-07: Пересадка готова: recipe.pack (slim) + params разморожен + packPreview с реальным экспандером; отдельный тип удалён подчистую; 641/641 тестов. Дальше фаза B: generate-ветка (резюм --run/--sheet, failed[]) -> export opt-in + slice
- 2026-07-07: Всё реализовано и верифицировано: фазы A/B/C + доки + фиксы deep-ревью (replace-реген подтверждён живьём); 687/687; smoke pack-smoke-t0332-b19615. На лиде: миграция screen-флагов --apply и приёмка UI
