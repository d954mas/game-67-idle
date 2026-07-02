---
id: T0218
title: "Image tools decomposition: raster2d+cutout -> assets/tools/image/<tool>, two alpha modes"
status: doing
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
- 2026-07-02: Recon complete (plan tmp/t0218_image_tools_recon_2026-07-02.md): June system fully alive at HEAD in cutout/ (key_matte=prod default since 12354465, key_to_alpha DELETED; dual_plate Smith&Blinn projection + pair gate; route soft_score 0.11). WIP verdicts: KEEP scipy-fallback+tests, ADAPT bundled-python discovery into shared _bridge, DROP viewer preview endpoint. Locked: chroma_key_alpha owned by alpha_matte (source_sheets cross-imports, documented); public HTTP URLs /api/asset-tools/raster2d/* + tmp prefix STAY (frozen viewer keeps working). 6 increments: _bridge extract -> alpha move -> raster2d move -> api split -> arch-map/docs -> canvas seam (gated on T0217).
- 2026-07-02: LAW from lead: NO silent fallbacks in image tools ("если нет инструментов нужно ошибку. Не нужно легаси и фаллбеков"). WIP scipy-numpy-fallback verdict flipped KEEP->DROP: scipy stays a hard import, missing dep = loud error naming interpreter+fix; portability solved in _bridge python discovery (that WIP hunk stays). Also kill the QUIET simple_key_matte_cutout fallback in slice_regions.py:226-235 -> hard error instead of silent quality degradation.
- 2026-07-02: Python environment FIX-ON-PC (lead: "как будто ее просто нужно исправить на пк"): increment 1 gains a studio venv - image/requirements.txt (pinned numpy/scipy/Pillow), one-shot setup script creating repo-local .venv (gitignored), studio.config.json pythonPath -> that interpreter. _bridge uses ONLY pythonPath; missing venv/dep = loud error with the setup command. The WIP pythonCandidates chain is DROPPED too (candidate probing = fallback chain, violates the law). Kills mixed 3.12/3.14 pycache; T0202 warm worker starts from the same interpreter.
- 2026-07-02: Increments 1-3 SHIPPED fe7b7e8a (accepted: 30 py + 5 bridge + 77 canvas tests, map strict clean). Venv live at .venv (numpy 2.1.1/scipy 1.17.1/Pillow 12.2.0), pythonPath in studio.config. Left: 4 (api split + server wiring), 5 (umbrella removal + doc polish), 6 (canvas seam + bench.mjs:325 stale detect path - MUST repoint with the seam).
- 2026-07-02: Increments 4-5 SHIPPED (accepted: 6 bridge tests, 30 py, map strict 309, doc check, live HTTP smoke held the frozen URL contract). raster2d/ = seam shim only; cutout/ gone. Left: increment 6 (canvas ops.mjs imports -> image/{regions,sources}/api.mjs, bench.mjs:325 repoint, delete raster2d/ + its seam node) - orchestrator does it after T0209 lands. Server restart needed to pick up server.mjs wiring (bundle with T0209 restart).
