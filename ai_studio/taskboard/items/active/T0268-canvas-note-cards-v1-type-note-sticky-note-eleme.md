---
id: T0268
title: "Canvas note cards v1: type:note sticky-note element - plain text w/ browser-side wrap, fixed clipped box + resize, background color presets, excluded from renderGroup/exportProject, site+CLI parity"
status: review
project: P001
epic: E010
priority: P1
tags: [canvas, notes]
created: 2026-07-04
updated: 2026-07-04
---

## What

Lead wants the canvas to work more like Miro — a board for everything. First
increment: NOTE cards ("заметки") to write lots of text in and keep oriented on
a big canvas. Lead decisions (2026-07-04): plain text (no markdown v1); box is
FULLY FIXED (w+h, user-resizable), overflow text CLIPS with an indicator ("I
don't have Miro-scale zoom; notes exist so I don't get lost on a big canvas");
notes are work annotations, NOT render content — renderGroup/exportProject skip
them (same spirit as group labels/chrome and recipe cards).

Design v1:
- New element `type: "note"` in flat `elements[]` (inherits z-order, groups,
  undo, marquee, copy/paste like `type:"text"` — the direct precedent).
- Fields: `content` (plain text, explicit \n allowed), `style` (font subset +
  text color), `background` (sticky presets + custom #rrggbb), fixed `w`/`h`.
- Word-wrap is a BROWSER DISPLAY concern only (ctx.measureText greedy wrap,
  cached per content+width+font): box is user-fixed and notes never render to
  PNG, so no PIL parity wrap, no nominal-box math.
- Overflow: clip at box (padding inset) + visible overflow indicator; full text
  via double-click editor (textarea overlay, wrap=soft) and inspector.
- Resize handles for note boxes (text elements have none today).
- Excluded from `renderGroup`/`exportProject` compositing (spec builder prunes
  notes); `exportElements` on a note refuses loudly like standalone text.
- STRICT tool parity: ops `addNote` (+ patch via `patchElement` content/style/
  background), HTTP route, CLI `add-note`/`element-set`, tests mirroring
  `tests/text.test.mjs`.

## Done when

- [x] `addNote` op + store support, journaled (undo/redo restore note incl.
      background), loud validation (bad style/background/content on non-note).
- [x] Page: N tool / context-menu create, wrapped clipped render + overflow
      indicator, background fill, double-click edit, resize handles, layers
      panel row w/ preview, inspector section (style + background presets).
- [x] CLI `add-note` + `element-set` patching content/style/background on a
      note; HTTP route; parity verified in tests.
- [x] `renderGroup`/`exportProject` skip notes (test proves a note inside a
      group changes nothing in the PNG); `exportElements` refuses a note.
- [x] `node --test ai_studio/assets/canvas/tests/*.test.mjs` green incl. new
      `note.test.mjs`; README Canvas section documents the note element.

## Open questions

## Log

- 2026-07-04 lead Q&A: plain text; fixed box + clip (not auto-height, not
  Miro font-shrink); notes excluded from renders (lead: "заметки это для
  работы, не для рендера"). Confirmed to lead: group labels also never reach
  PNG exports.
- 2026-07-04 implemented by deep-reasoner (Opus): 14 files + new
  tests/note.test.mjs (9 tests); suite 577/577 green. Orchestrator review
  caught RAW NUL BYTES in workspace.js noteWrapCache key (made the file
  binary for git/grep) — replaced with U+0000 escapes, semantics identical.
- 2026-07-04 live verify (isolated server :8791, scratch projects root):
  CLI add-note (preset + custom bg), element-set resize, loud errors (bad
  color, bg on text), undo/redo w/ --expect-head; headless-Chrome screenshot
  shows both cards wrap + clip w/ bottom-fade overflow, note rows in layers,
  note tool in rail; render-group of a group holding 2 notes -> manifest
  items:[] member_count:0, PNG = pure background. Follow-ups (agent notes):
  noteWrapCache never evicts deleted ids (bounded); rotate handle on a note
  renders inconsistently (consider gating off); no multi-select Note
  inspector section; note name = full first line (parity w/ text, but long).
- 2026-07-04: impl+verified 2026-07-04: 577/577 tests, live CLI+page smoke green, render exclusion proven; awaiting lead look
