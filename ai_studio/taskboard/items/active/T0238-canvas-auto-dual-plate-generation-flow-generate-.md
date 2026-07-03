---
id: T0238
title: "Canvas: auto dual-plate generation flow - generate pair, gate, cut, plates in meta"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead (2026-07-03): the dual-plate path must be AUTOMATIC — "Генерировать
пару, проверять, и делать". One flow: generate the white plate → black plate
as an EDIT of the white (subject-lock — reuse
.codex/skills/nt-asset-image-generation/scripts/gen_dual_plate.sh chain) →
pair gate → dual_plate extraction (T0237 op as the engine) → ONE NEW element
on the canvas with BOTH plates stored in meta.

DECISIONS (lead, discussed one-by-one 2026-07-03):
1. Placement = HYBRID: from a prompt -> viewport center; from an existing
   element (edit/variation) -> beside the source element.
2. Gate failure: ONE auto-retry of the black plate, then loud error; the
   white plate + failed pair are preserved so the lead can retry from the
   white plate or run the manual T0237 op on the pair himself.
3. Plates stored ALWAYS in project files/ ("именно для этого папка и
   задумана") — content-addressed refs in meta.alpha.plates
   [{src, role: light|dark}] + prompt + gate verdict.
4. Generation with alpha cleanup ALWAYS mints a NEW element (never replaces).

Inspector surface: element's Alpha/Provenance section shows both plate
thumbnails (files served already); click = preview, "Add to canvas" mints a
plain image element from that plate src (one journaled op).

REFRAMED per lead (2026-07-03, T0239 discussion): generation placeholders
produce RAW art with NO alpha — this flow is therefore an ACTION ON AN
EXISTING ART ELEMENT ("сделай дуал-плейт альфу этому арту"): the element's
current pixels ARE the light plate (requires flat light bg — loud refusal
otherwise), the dark plate is generated as an EDIT of it (subject-lock),
then gate (1 retry) -> T0237 op -> NEW cut element BESIDE the source.
Trigger today = agent skill command; a UI action on the element can come
later. Placement decision 1 (hybrid) collapses to "beside the source".

## Done when

- [ ] One command generates pair -> gate (1 retry) -> cut -> new element with
      plates+prompt+verdict in meta; hybrid placement per decision 1.
- [ ] Gate failure is loud, preserves white plate + pair, names the retry
      paths.
- [ ] Inspector shows plate thumbnails + "Add to canvas"; added plates are
      normal journaled elements.
- [ ] Full canvas suite green; skill doc updated (generation-with-alpha
      command).

## Open questions

## Log

- 2026-07-03: created; design settled with lead in one-by-one discussion.
  Depends on T0237 (pair op = engine); inspector part waits for inspector.js
  to free up.
- 2026-07-03: Fast-worker launched: auto dual-plate flow (element = light plate, generated dark via codex edit seam, gate w/ 1 retry incl. T0243 align, plates in meta, cut beside source).
- 2026-07-03: Landed + committed 0bd5d944: alphaDualPlateGenerate (flat-bg check, codex seam, 1 retry, plates in meta), addImageFromFile, CLI/API, inspector plate thumbnails. 310/310 at landing. Awaiting lead live verify on wings.
