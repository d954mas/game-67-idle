---
id: T0237
title: "Canvas: dual-plate alpha path - pair of plate elements (white+black) -> one cut element"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead (2026-07-03): "до сих пор нет дуал пути для альфы?" — the canvas has no
dual-plate path. The TOOLS exist and are hardened
(ai_studio/assets/tools/image/alpha_dualplate/: dual_plate_alpha.py =
Smith&Blinn from a white+black plate pair, dual_plate_pair_gate.py = loud
pair validation); the canvas alpha op only refuses soft art telling the lead
dual-plate is needed. Close the loop: TWO selected image elements (the same
art rendered on a WHITE plate and a BLACK plate) -> ONE op -> a NEW cut
element on the canvas.

Design (settled by orchestrator, mirrors alphaCutout patterns):
- ops.alphaDualPlate({projectId, elementIds: [a, b]}): exactly 2 image
  elements; plate roles auto-detected by the pair gate (which is which =
  gate's job, loud on a non-pair: misaligned art, same-color plates, size
  mismatch). Runs a new canvas tool tools/alpha_dualplate.py through the warm
  worker, REUSING the image-tools modules unmodified (no logic duplicated).
  Result = NEW element (content-addressed PNG) placed at the first plate's
  x/y, named "<plate-name> alpha"; BOTH plate elements stay untouched
  (non-destructive; lead deletes plates himself). One journal entry (create),
  one Ctrl+Z removes the new element. meta.alpha provenance records method
  dual_plate + both parent srcs + gate metrics; tool_runs row.
- Refusals are LOUD and specific: not exactly 2 ids, non-image, pair gate
  failures (each with the gate's own message).
- API POST /alpha-dual {elementIds}; CLI alpha-dual <id> --elements a,b.
- Inspector: when the multi-selection is EXACTLY 2 images, the existing
  multi Alpha section grows a second button "Dual-plate cutout" under the
  batch "Apply to 2 images" row (same runLongOp pattern).
- Tests: happy pair (fixture: same blob on white/black plates) -> new element
  + one entry + undo removes; gate refusal (mismatched pair) mutates nothing;
  arity/type validation; API+CLI parity. encodePng fixtures extended.

## Done when

- [ ] Pair of plate elements -> one new cut element, one journal entry,
      byte-exact undo (element removed, plates untouched).
- [ ] Loud pair-gate refusals reach the UI as clean messages (no tracebacks).
- [ ] API/CLI/inspector parity; full canvas suite green (baseline 259).

## Open questions

- Result placement: at first plate's x/y (default) vs beside it — lead may
  adjust after first use.

## Log

- 2026-07-03: created from lead question during live verify; design settled;
  delegated to fast-worker (Sonnet) after T0235 freed ops.mjs.
- 2026-07-03: landed: alphaDualPlate op + tool + API/CLI/inspector, suite 263->268, :8780 restarted; awaiting lead live check (needs a real plate pair)
