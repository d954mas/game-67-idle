---
id: T0245
title: "Canvas: align/distribute icons redesign - pictographic SVGs + captions + mode badge (two-Opus review synthesis)"
status: doing
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead verdict on T0232-1 buttons: "по иконкам и буквам вообще ничего не понятно".
Two independent Opus reviews (competitor survey + UX critique, 2026-07-03) agree;
this task applies the synthesis. Scope: ONLY `renderAlignSection` +
`ALIGN_BUTTONS`/`DISTRIBUTE_BUTTONS` in site/inspector.js and `.insp-align-*`
in site/canvas.css. No op/API/CLI changes (alignSelection/distributeSelection
untouched).

### 1. Icons (the core fix)

Consensus grammar of Figma/Illustrator/Affinity/Sketch/PowerPoint/Penpot:
align = thin ANCHOR LINE at the target edge/center + TWO rounded bars of
UNEQUAL length flush to it; distribute = THREE equal bars with equal gaps, no
line. Never letters, never arrows. All shapes `fill="currentColor"` rects
(theming free), viewBox 0 0 16 16, `aria-hidden="true"` on the svg,
`aria-label = title` on the button. Paste verbatim:

- align-left: `<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="1.5" y="2" width="1.4" height="12" rx=".7"/><rect x="3.6" y="4" width="10" height="3" rx="1.5"/><rect x="3.6" y="9" width="6.5" height="3" rx="1.5"/></svg>`
- align-hcenter: `<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="7.3" y="2" width="1.4" height="12" rx=".7"/><rect x="3" y="4" width="10" height="3" rx="1.5"/><rect x="4.75" y="9" width="6.5" height="3" rx="1.5"/></svg>`
- align-right: `<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="13.1" y="2" width="1.4" height="12" rx=".7"/><rect x="2.4" y="4" width="10" height="3" rx="1.5"/><rect x="5.9" y="9" width="6.5" height="3" rx="1.5"/></svg>`
- align-top: `<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="2" y="1.5" width="12" height="1.4" rx=".7"/><rect x="4" y="3.6" width="3" height="10" rx="1.5"/><rect x="9" y="3.6" width="3" height="6.5" rx="1.5"/></svg>`
- align-vcenter: `<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="2" y="7.3" width="12" height="1.4" rx=".7"/><rect x="4" y="3" width="3" height="10" rx="1.5"/><rect x="9" y="4.75" width="3" height="6.5" rx="1.5"/></svg>`
- align-bottom: `<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="2" y="13.1" width="12" height="1.4" rx=".7"/><rect x="4" y="2.4" width="3" height="10" rx="1.5"/><rect x="9" y="5.9" width="3" height="6.5" rx="1.5"/></svg>`
- distribute-h: `<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="2" y="3" width="2.5" height="10" rx="1.25"/><rect x="6.75" y="3" width="2.5" height="10" rx="1.25"/><rect x="11.5" y="3" width="2.5" height="10" rx="1.25"/></svg>`
- distribute-v: `<svg viewBox="0 0 16 16" fill="currentColor" aria-hidden="true"><rect x="3" y="2" width="10" height="2.5" rx="1.25"/><rect x="3" y="6.75" width="10" height="2.5" rx="1.25"/><rect x="3" y="11.5" width="10" height="2.5" rx="1.25"/></svg>`

Keep tuple shape `[key, svg, title]`; render `btn.innerHTML = svg` (trusted
static strings) + keep `btn.title`.

### 2. Layout / grouping (UX critique fixes)

- Keep two rows, keep order (L C R T M B / dist-h dist-v) — matches Illustrator.
- Muted 11px caption above each row: "Align" / "Distribute" (new
  `.insp-align-caption`, `var(--muted)`) — kills the "broken half-row" read.
- Mode badge in the collapsible header (collapsible() already takes a badge
  arg): `"to frame"` when nodeIds.length === 1, `"to selection"` when >= 2 —
  surfaces the invisible reference switch (2+ -> selection bbox, 1 -> parent
  frame).
- When nodeIds.length < 3: always-visible muted hint under the distribute row
  "Select 3+ objects to distribute." (keep buttons disabled; tooltip alone is
  not discoverable on a disabled button).

### 3. CSS

`.insp-align-btn { flex:0 0 auto; width:28px; height:28px; display:flex;
align-items:center; justify-content:center; padding:0; }` (drop font-size/
weight — no text left), `.insp-align-btn svg { width:16px; height:16px;
display:block; }`, `.insp-align-caption` (11px muted, 2px bottom margin),
`.insp-align-btn:focus-visible { outline:1px solid var(--cyan);
outline-offset:1px; }`.

## Done when

- [ ] All 8 buttons pictographic SVG per spec above; no letters/arrows left.
- [ ] Row captions + "to frame"/"to selection" badge + 3+ hint in place.
- [ ] aria-labels set; suite green; lead verifies live on :8780.

## Open questions

## Log

- 2026-07-03: created from two-Opus review synthesis (competitor survey +
  UX critique). BLOCKED on inspector.js until T0238 worker lands (one writer
  per file).
- 2026-07-03: Fast-worker launched now that T0238 freed inspector.js/canvas.css. Includes bonus cleanup: move T0238 plate-row inline styles into canvas.css classes.
