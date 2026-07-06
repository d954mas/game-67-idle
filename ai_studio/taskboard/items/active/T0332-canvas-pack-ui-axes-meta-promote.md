---
id: T0332
title: "Canvas pack-карта: конфиги паков из UI + axes в meta при promote"
status: idea
project: P001
epic: ""
priority: P3
tags: [assets, imagegen, canvas, declarative-art]
created: 2026-07-06
updated: 2026-07-06
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

- [ ] (уточнить при взятии в работу; не начинать без реального следующего
      пака — по принципу «инструмент после трения»)

## Open questions

- Порядок: сначала axes→meta (мелкое), потом карта? Или карта целиком?

## Log

- 2026-07-07: заведено при закрытии T0330; пилот принят лидом
  («отлично работает»).
