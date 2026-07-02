---
id: T0210
title: "Canvas: per-element alpha cutout op (method choice, regions optional) - wings acceptance"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-03
---

## What

Bring the image-tools alpha pipeline onto the canvas as a FIRST-CLASS
per-element op (lead 2026-07-02: "готовый арт сразу в альфу" - alpha is not
a region workflow; regions are optional refinement). This is the missing
bridge proven by the wings test: canvas slice crops WITHOUT alpha, and no
canvas op can remove a generated background today.

Scope (increment 1):

- **Op** `alphaCutout({projectId, elementId, method?, regions?})` in
  ops.mjs: runs the element's CURRENT pixels through the image-tools alpha
  path and swaps the element to a NEW content-addressed file (PNG w/ alpha)
  in ONE journaled entry; undo restores the previous src exactly
  (non-destructive law - the original file stays in files/).
- **Methods**: default = the auto route (soft_score router) used by the
  tools pipeline; explicit `alpha_matte` (key_matte - the prod default
  keyer since 12354465). `alpha_dualplate` needs a white+black plate PAIR -
  OUT of v1 scope on canvas (single-element op has one image; note in
  design where a pair source could come from later).
- **Regions optional**: with `regions` given, alpha applies only inside
  those region bounds (rest of the element untouched); default = whole
  element.
- **Transport**: through the image-tools `_bridge` (warm worker, T0202) -
  no new python entry duplication; reuse
  ai_studio/assets/tools/image/alpha_matte (+ route/soft_score) apis.
- **Page**: the inspector Regions section's "Coming soon: Alpha cutout"
  hint becomes the real control - an Alpha button/section on the selected
  image element (method dropdown: Auto / Key matte), long-op via the queue
  + progress toast (T0203 flow). Meta records the tool run (origin,
  method, params) like slice provenance.
- **CLI parity**: `alpha <id> --element <eid> [--method auto|matte]
  [--regions r1,r2]`.
- **Acceptance asset = the WINGS**: run the op on the magenta-keyed wings
  (projects demo-02e8b7 / benchmark-fixture-c7f9dc) and save before/after
  renders for the lead's morning look. The visual verdict is the LEAD's
  (fake-shot direction law) - the build proves mechanics: alpha channel
  present, background pixels transparent, subject opaque, undo exact.

## Done when

- [ ] alphaCutout op: new file + one journal entry; undo restores previous src byte-exact; meta records method/params
- [ ] method choice: auto (router) + explicit matte; loud error for unavailable methods (dualplate) or non-image elements
- [ ] regions-scoped alpha works (only inside given regions)
- [ ] inspector Alpha control replaces the "Coming soon" hint; long-op queue + toasts; thin page
- [ ] CLI parity + HTTP route; warm-worker transport (no cold spawns, no python duplication)
- [ ] wings before/after artifacts produced for lead review; tests + gates green

## Open questions

## Log
- 2026-07-02: Future phase; scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02)
- 2026-07-03: Spec written by orchestrator from PLAN + image-tools track (T0218 landed the per-tool alpha modules; T0202 landed the warm worker transport). Launched overnight (lead approval for the night queue).
