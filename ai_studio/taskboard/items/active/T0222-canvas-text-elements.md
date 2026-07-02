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

- [x] research note: Figma text model distilled (adopt/skip table), font sourcing/licensing decision (which OFL fonts ship, where they live) - see log 2026-07-02
- [ ] text element type: create (toolbar/context menu + CLI), inline edit on double-click, move/select/group like any element
- [ ] style: font family (picker from bundled fonts), size, weight, fill color, alignment; stroke + drop shadow; auto-size vs fixed box behavior decided in design
- [ ] canvas draw and renderGroup/export produce matching text (same font files, PIL parity test with pixel tolerance)
- [ ] journaled ops with undo; CLI parity; tests + gates green

## Open questions

## Log
- 2026-07-02: Created from lead request. Research phase launched same night (Opus): Figma/analog text-model survey -> design doc feeds this task.
- 2026-07-02: RESEARCH DONE (Opus, same night). Key decisions distilled (full report in session transcript):
  - MODEL: type:"text" element in flat elements[] - z-order/grouping/nesting/undo/marquee all type-agnostic, inherited for FREE. New op addText (mirrors addImage + frontOrder hook); content+style extend patchElement/patchElements (batched law intact). style block: fontFamily/fontWeight/fontSize/lineHeight(unitless)/align/color/stroke{width,color}/shadow{dx,dy,blur,color}/autoResize:"width".
  - V1 SCOPE (kills every parity trap): AUTO-WIDTH ONLY, explicit \n newlines, NO auto-wrap (the #1 divergence source between canvas2D and PIL); solid fill; OUTLINE + HARD offset shadow (blur=0) = the two primitives behind every Canva game-text preset (Hollow/Outline/Splice); letter-spacing/blur/vertical-align/fixed-box+wrap = v1.1+; rich-text spans/gradient fill/curve = skip.
  - PARITY STANCE: PIL = single source of rendered truth; canvas page = faithful same-font approximation (~1-2px glyph drift acceptable, line breaks identical by construction). Both renderers RE-MEASURE from content+style every paint - stored w/h never load-bearing. STROKE TRAP: canvas strokeText centers, PIL grows outward -> canvas draws stroke UNDER fill with lineWidth = 2x style width, lineJoin round. Baseline pin: canvas textBaseline=top / PIL anchor='la'.
  - FONTS: static TTF instances (NOT variable fonts - PIL set_variation_by_name is build-sensitive), module-local site/fonts/<Family>/ + OFL.txt per family + fonts.json manifest = the parity contract (page builds @font-face from it, ops.mjs resolves same file to abs path for render_group.py). Curated Cyrillic-safe OFL set: Inter (400/600/700), Fredoka (500/700 - chunky display; Lilita One is Latin-only, AVOID unless reusing engine's LilitaOne-RussianChineseKo.ttf after license check), Bitter (400/700), JetBrains Mono (400/700). Page preloads via FontFace API + document.fonts.ready gate (no FOUT mismatch). server.mjs MIME map needs ".ttf".
  - KNOWN ASYMMETRY (accepted): CLI addText stores nominal box, next page-open re-measures precisely; renderer re-measures so exported pixels always correct. Optional measure_text.py if exact off-page boxes ever needed.
  - INCREMENTS: 1) fonts bundle + model + addText/patch + T tool + inline dblclick edit (textarea overlay) + inspector Text section + paint_text in render_group.py + CLI = shippable core (headings in screens EXPORT in v1); 2) standalone text-PNG export, letter spacing, shadow blur, italic, v-align; 3) wrap/fixed box, presets.
  - WATCH: validate stroke 2x mapping visually on a chunky heading before calling parity done.
