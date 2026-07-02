---
id: T0218
title: "Image tools decomposition: raster2d+cutout -> assets/tools/image/<tool>, two alpha modes"
status: todo
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Lead handed over his in-progress matte refactor (2026-07-02: "raster2d/api.mjs
можешь полностью отрефакторить. Твоя структура сейчас лучше... Важно чтобы
каждая такая фича была декомпозирована"). Decompose the raster2d monolith +
cutout into per-tool folders under the renamed umbrella
`ai_studio/assets/tools/image/<tool>/` — each tool = own thin `api.mjs` bridge
+ own Python + own tests + README, and OWN NODE in the architecture map
(assets -> tools -> image -> crop / bg_fix / regions / alpha_...) so agents
see a toolbox, not a monolith. Tool list refined by reconnaissance; expected:
crop, bg_fix (snap-to-bg, internal pre-pass shared with keying), regions
(detect+slice), alpha_matte (key-matte), alpha_dualplate (alpha from a
generation pair — the June two-path system: commits 1cfb83d5 two-path cutout,
10a27b89 optimize, 2dffd0e4 bench evidence, 12354465 drop legacy keyer).
DECISIONS (lead 2026-07-02): (1) his uncommitted WIP in raster2d/cutout/viewer
= starting point, REVIEW and keep the useful parts, then I own those files;
(2) canvas is THE editor — the old asset viewer is FROZEN (don't maintain
parity, don't break gratuitously; it goes away eventually); (3) umbrella stays
but renamed `image` ("по сути это же работа с картинками") — the umbrella is a
MEDIA-TYPE tier: future siblings `assets/tools/model3d/`, `assets/tools/audio/`
etc. (lead: "потом там могут быть 3д модели, звуки"). Both alpha modes
are first-class steps. Sequencing: reconnaissance read-only now; execution
AFTER T0217 lands (canvas ops.mjs seam). This unblocks T0210 (canvas alpha op)
and T0207 (cleanup tools).

## Done when

- [ ] ai_studio/assets/tools/image/<tool>/ folders exist, each with api.mjs + python + tests + README; old raster2d/api.mjs monolith gone
- [ ] architecture map shows one node per tool under assets tools image; validate_map --strict green
- [ ] both alpha modes work as separate tools (key-matte; dual-plate from a generation pair) with tests ported/green from the June system
- [ ] all callers migrated (canvas ops bridge, studio shell, skills/docs references); full test suite + doc_reference_check green
- [ ] lead's WIP reviewed: useful parts kept (documented which), the rest consciously dropped; viewer left frozen-but-working or explicitly marked frozen
- [ ] canvas T0210 (alpha op) unblocked: image/alpha_* callable from canvas ops

## Open questions

## Log
- 2026-07-02: Created from lead's live handover; decisions on WIP/viewer/layout recorded. Reconnaissance dispatched (read-only).
