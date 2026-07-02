---
id: T0210
title: "Canvas: per-element alpha cutout op (method choice, regions optional) - wings acceptance"
status: review
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

- [x] alphaCutout op: new file + one journal entry; undo restores previous src byte-exact; meta records method/params
- [x] method choice: auto (router) + explicit matte; loud error for unavailable methods (dualplate) or non-image elements
- [x] regions-scoped alpha works (only inside given regions)
- [x] inspector Alpha control replaces the "Coming soon" hint; long-op queue + toasts; thin page
- [x] CLI parity + HTTP route; warm-worker transport (no cold spawns, no python duplication)
- [x] wings before/after artifacts produced for lead review; tests + gates green

## Open questions

## Log
- 2026-07-02: Future phase; scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02)
- 2026-07-03: Spec written by orchestrator from PLAN + image-tools track (T0218 landed the per-tool alpha modules; T0202 landed the warm worker transport). Launched overnight (lead approval for the night queue).
- 2026-07-03: Increment 1 built (increment-1 scope complete). Design record:
  - NEW canvas python tool `tools/alpha_cutout.py` — REUSES the image-tools alpha modules
    verbatim: `route/route_cutout` (border key + soft-score routing) and
    `alpha_matte/key_matte` (prod keyer). No matte logic duplicated; loud import guard.
    Runs through the T0218 `_bridge` warm worker (no cold spawn, no second interpreter).
  - Region-mask composition happens IN python, one worker call: with regions, the base is
    the ORIGINAL opaque pixels and each region's keyed crop is pasted back over its mask
    (rect, or `ImageDraw.polygon` for polygonal regions) — the rest stays untouched. Output
    size always = source, so element geometry never changes on the src swap.
  - Router usage: `method:"auto"` runs `route_cutout` per scope; a wide soft/semi-transparent
    zone (soft_score >= 0.11 or deep gradient) routes to `dual_plate` → LOUD error (a single
    element can't provide a white+black plate pair; out of v1 scope). `method:"matte"` forces
    `key_matte`. dual_plate/generation methods rejected loudly in the op.
  - Non-destructive src swap: added store helper `addFile` (content-addressed write, no new
    element; addImage now delegates to it). `alphaCutout` swaps `element.src` to the new file
    + records `element.meta.alpha` (method, params, parentSrc, key, routing) + a tool_runs row,
    in ONE commitMutation. Undo restores the exact previous element (tested byte-exact,
    incl. meta) since the prior file stays immutable in `files/`.
  - Parity: op `alphaCutout` in ops.mjs; HTTP `POST /api/canvas/projects/<id>/alpha`; CLI
    `alpha <id> --element <eid> [--method auto|matte] [--regions r1,r2]`; page inspector
    Regions section Alpha control (method dropdown Auto/Key matte + run button, region-scoped
    when regions are selected) via `actions.alphaCutoutFor` (runLongOp queue + toast). Thin page.
  - Tests: `tests/alpha.test.mjs` (8) — validation (non-image, bad/dualplate method, unknown
    region) + pipeline (auto whole-element alpha channel/transparent-bg/opaque-subject, ONE
    journal entry + byte-exact undo/redo, regions-scoped in/out, API+CLI parity). Canvas suite
    240 green (232 baseline + 8). Map + doc gates green.
  - Wings acceptance (real project benchmark-fixture-c7f9dc): duplicated el_c2595167 →
    el_f3832532 (x=1400, beside the original, journaled/undoable; original untouched) and
    alpha-cut the DUP with matte (auto correctly routes these soft-edged wings to dual_plate).
    Artifacts in tmp/t0210_wings_before_after/. Mechanical: 4 corner alphas = 0 (bg
    transparent), subject sample (858,610) alpha 255 (opaque); 53.5% transparent / 35.1%
    opaque. Visual verdict is the lead's (fake-shot direction law).
  - Deferred: dual_plate on canvas (needs a "generate white+black plate pair" op — noted as
    the future pair source); standalone dual-plate/generation method.
