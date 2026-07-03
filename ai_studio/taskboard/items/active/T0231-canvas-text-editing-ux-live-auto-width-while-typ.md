---
id: T0231
title: "Canvas: text editing UX - live auto-width while typing + layers shows text content preview"
status: doing
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Two lead reports from live verification 2026-07-03:
1. "поле с текстом не расширяется автоматически по ширине, пока я пишу; после
   нажатия enter расширяется" — the in-place textarea only grows on
   Enter/commit; it must grow live on every input keystroke.
2. "в layers написано просто Текст; хотелось бы после названия в скобках сам
   текст показать, как-то отделить что это текст а не название, мб цветом" —
   layers rows for text elements show only the name; append a dimmed
   (secondary-color) content preview so name vs content is visually distinct.

## Done when

- [ ] While typing in the canvas text editor the box widens per keystroke
      (same measurement as commit; no reflow jump on Enter).
- [ ] Layers row for a text element: name + dimmed truncated content preview
      (e.g. `Text  "dsadaz dsd…"`), styled distinctly from the name; updates
      after edits; non-text rows unchanged.
- [ ] Full canvas suite green.

## Open questions

## Log

- 2026-07-03: created from lead reports during T0210 verification.
