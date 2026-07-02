---
id: T0222
title: "Canvas: text elements - Figma-like text node type (font, stroke, shadow)"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Lead request (2026-07-02): "в фигме удобно текстом вне групп писать
комментарии, или заголовки. текст это ещё один тип ноды как картинка или
группа. с размерами, обводкой, тенью и тд. и выбором шрифта. изучи аналоги
хочу текст как там". A new element type `text` in the flat model (lives in
elements[] beside images; groupId optional so it works loose on canvas for
comments/headings AND inside groups/screens). RESEARCH FIRST: study Figma's
text feature set (font family/weight/size, line height, letter spacing,
alignment, auto-width/auto-height/fixed resize modes, fill, stroke, effects/
drop shadow) plus analogs (Recraft, Canva) and distill what we adopt vs skip.

Design constraints known up front:
- Render parity is the hard part: the page draws via canvas 2D, renderGroup/
  export via PIL ImageFont - BOTH must use the SAME font files or exports
  won't match the canvas. Font files = assets: OFL/free fonts only, with
  license/provenance/origin per the studio invariant (echoes the engine law:
  real fonts, no handmade text rendering).
- Model: text payload on the element (content + style block), journaled ops
  (addText/patchElement extension), one gesture = one entry, undo exact.
- Editing UX: double-click a text element = inline edit (text has no regions,
  so no collision with region-edit; drill/deep-select semantics from T0219
  apply to text like any element).
- Tool parity: CLI text-add / element-set for style fields; agent can compose
  annotated screens.
- Export: text bakes into PNG (renders); text elements excluded from
  per-element image export rows or exported as rendered rasters - decide in
  design.

## Done when

- [ ] research note: Figma text model distilled (adopt/skip table), font sourcing/licensing decision (which OFL fonts ship, where they live)
- [ ] text element type: create (toolbar/context menu + CLI), inline edit on double-click, move/select/group like any element
- [ ] style: font family (picker from bundled fonts), size, weight, fill color, alignment; stroke + drop shadow; auto-size vs fixed box behavior decided in design
- [ ] canvas draw and renderGroup/export produce matching text (same font files, PIL parity test with pixel tolerance)
- [ ] journaled ops with undo; CLI parity; tests + gates green

## Open questions

## Log
- 2026-07-02: Created from lead request. Research phase launched same night (Opus): Figma/analog text-model survey -> design doc feeds this task.
