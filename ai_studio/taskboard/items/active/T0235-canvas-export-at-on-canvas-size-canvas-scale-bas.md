---
id: T0235
title: "Canvas: export at on-canvas size - 'canvas' scale base alongside source pixels"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead (2026-07-03, export verify): "нужна возможность выгрузить как оригинал
так и размер на канвасе". Today every export scale resolves against SOURCE
pixels (element.source_w/h); the element's on-canvas w/h is ignored. Add an
export base "canvas": the row exports at the element's CURRENT on-canvas size,
resolved at export time (tracks later resizes — not frozen into a w-token).

SCOPE GREW after two independent Opus UX reviews rated the first Size control
bad (lead: "UI UX плохой"). This task now = the synthesized Size-control
redesign + the canvas base, one packet.

Design (synthesized from both reviews):
- TOKEN MODEL KEPT: scale stays "2x"/"0.75x"/"512w"/"512h" strings. Base is a
  NEW additive per-row field base: "source"|"canvas" (default "source" when
  absent — old rows unchanged). Do NOT encode base into the token.
- ops.mjs: cleanExportRows must WHITELIST + validate `base` (current allowlist
  would silently drop it); exportElements picks base dims per row (canvas ->
  round(element.w/h), loud error when zero/missing); resolveExportScale itself
  stays token-only. Multi-row naming: canvas-base rows append "-canvas" to the
  marker ("name@2x-canvas.png"); single row stays clean "name.png".
- CLI parity: export-set and export gain --base source|canvas.
- SITE Size control REBUILT (inspector.js scaleInput): ONE composite line
  [number input][unit select x|W|H] — no "Custom..." morphing, typing is
  primary (0.75 = just type it); number input may carry a datalist of presets
  0.5/1/2/3/4 as ADDITIVE suggestions only. Unit switch is UI-ONLY local
  state: converts the shown number to the equivalent (output px unchanged),
  commits NOTHING; commit fires once on Enter/change/focusout-of-control when
  the resolved token differs from stored. Result readout moves to its OWN
  full-width muted line under the field: "= 1536 x 1152 px" (never inline,
  never clipped; reflects base). Base = card-level segmented [Source|Canvas]
  above the rows (one control, writes base to all rows via one setExportRows;
  dim/hide when no multiplier row exists).
- CSS: .insp-size-out own-line readout replaces inline .insp-size-hint;
  segmented control styles; keep .insp-export-head clearance for the x button.
- Reviews' full arguments: see the two Opus review returns logged in the
  orchestrator session (240px width math, Custom-trap, modal-mode critique).

## Done when

- [ ] base field round-trips ops/CLI/API; canvas-base export of a RESIZED
      element produces on-canvas pixels (test proves it).
- [ ] Size control: type 0.75 = one field; unit switch commits nothing until
      a value settles; readout on own line; Base segmented at card level.
- [ ] Naming: "@2x-canvas" style markers on multi-row; loud zero-dim errors.
- [ ] Full canvas suite green (baseline 254).

## Open questions

## Log

- 2026-07-03: created from lead feedback during export verify (his answer to
  the source-vs-canvas semantics question: BOTH must be exportable).
- 2026-07-03: scope grew to full Size-control redesign (2 Opus UX reviews synthesized); delegated to fast-worker (Sonnet)
- 2026-07-03: landed: composite Size control + canvas base, suite 254->259, :8780 restarted; awaiting lead live check
- 2026-07-03: lead round-2 feedback: order = [unit mode][value] (mode first), value field gets a dropdown of per-mode presets - scale: 0.5/1/1.5/2/4; W/H px: base size first then 32/64/128/256/512/1024/2048. Implement as custom preset menu button (NOT datalist - Chrome prefix-filter trap); queued behind T0237 worker (inspector.js one-writer law), orchestrator applies inline after
- 2026-07-03: round-2+3 landed (mode-first + preset menu w/ current-canvas-size first) + BASE DEFAULT FLIPPED to canvas (lead: scale must be from on-canvas size); markers now @1x-source; suite 268; :8780 restarted
