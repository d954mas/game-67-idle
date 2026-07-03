---
id: T0207
title: "Clean art: post-gen cleanup op — bg solidify, palette quantize, denoise (non-destructive)"
status: review
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-03
---

## What

Deterministic post-generation cleanup op (Python, ours) for noisy AI art - lead: art must never be broken. Three composable passes: background-solidify (snap pixels within a color distance of the detected/chosen bg color exactly to it - also a keyer pre-pass), palette quantization (k-means/median-cut to N colors), and optional edge-preserving denoise (median/bilateral 3x3). NON-DESTRUCTIVE: result is a NEW immutable file + journaled element.src swap (Ctrl+Z restores), original stays in files/. Emits a before/after report (changed-pixel %, palette size). Per-element and per-region application. UI + CLI parity. DEPENDS ON: lead's unified matte work landing in raster2d/cutout (bg-solidify must share its color-distance math).

## Done when

- [ ] cleanup op produces a new file + journaled swap; undo restores the original pixel-for-pixel
- [ ] bg-solidify makes the background a single exact color on a real noisy generated sheet (matte cuts it cleanly with default thresholds)
- [ ] each pass has pixel tests on fixtures proving it only touches what it should (bg pass never alters foreground pixels beyond the distance threshold)
- [ ] before/after report saved with each run; UI shows it; CLI prints it

## Invocation design (proposed to lead 2026-07-02, pending his ок)

Explicit one-click, zero mandatory tuning. Inspector "Clean up" button on the
element (+ `cleanup` CLI). NOT automatic on everything: painterly grain can be
intentional; quantization can kill gradients. Generation-agent convention
(encode in the imagegen skill when this op lands): flat/UI generations MUST be
cleaned by the agent with a before/after in the report; painterly is left
alone unless asked. Auto-everything defaults: bg color detected from
corners/edges, snap threshold from the lead's matte color-distance math,
palette size auto. Three presets instead of knobs: Flat/UI (snap+quantize),
Painterly (mild denoise only), Keying prep (magenta solidify only); advanced
sliders (threshold, palette N) live collapsed. Safe by construction: new file
+ journaled swap, before/after toggle in UI, Ctrl+Z reverts.

Bg-solidify has TWO roles (lead clarified 2026-07-02): (a) explicit journaled
op that mutates the asset — ONLY on explicit Clean up; (b) invisible in-memory
pre-pass inside other ops (alpha keying snaps near-bg before cutting; region
detect just reads with the same distance math) — these NEVER write a modified
source file. The lead never has to remember pass ordering.

NOT one monolithic "Clean up" (lead corrected 2026-07-02): a small Cleanup
section of separate tools, each with the UX its decision needs.
Quantize = INTERACTIVE tool: color-count slider (+ optional dither), LIVE
before/after compare (split or hold-to-see-original), preview recomputed
debounced with nothing written to disk; Apply = journaled new file, Cancel =
no trace. Denoise = same pattern with a strength slider. CLI parity:
`quantize --colors N --preview <out>` to try, without --preview to apply.

Bg-solidify CUT as a standalone tool (lead challenged 2026-07-02, subtract-
not-add): keying auto-snaps internally, detect only reads with the distance
math, and art-with-background is quantize's job — no real friction case
remains. It stays an INTERNAL shared function of the alpha keyer only (no op,
no button, no CLI). Reinstate as a full op (button + CLI together, parity) if
a real case shows up. Cleanup section = Quantize + Denoise.

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
- 2026-07-02: Invocation design proposed (explicit one-click + presets, agent auto-clean for flat/UI with report).
- 2026-07-03: lead asked for cleanup research (AI watermarks + color artifacts + quantization recap); deep-reasoner (Opus, web) launched -> tmp/research_art_cleanup_2026-07-03.md; findings will extend this spec
- 2026-07-03: research landed: tmp/research_art_cleanup_2026-07-03.md - quantize IS the color-artifact fix (MEDIANCUT on split RGB + alpha reattached byte-exact, dither off, RGBA quantize is FASTOCTREE-only so always split; libimagequant absent from Pillow 12.2.0); denoise = median3 + YCbCr chroma only (no cv2 deps); ladder = key/alpha FIRST then quantize then optional denoise then export; watermark verdict: gpt-image path clean, C2PA metadata already stripped by any numpy re-encode, only exportElements 1x-png byte-copy preserves metadata; cut: opencv/skimage/ML restorers
- 2026-07-03: lead decision 2026-07-03: quantize/denoise apply IN-PLACE (like alpha, byte-exact undo) but ONLY with a preview-before-apply: tool section shows params + live preview of the result (canvas renders preview bytes for the element at real zoom, before/after toggle), Apply commits the already-computed bytes as ONE journal entry; warm worker makes param-tweak previews interactive. Preview is a hard requirement, not polish
- 2026-07-03: Backend worker launched (lead away, 'делай дальше'): quantize+denoise per-tool python folders, cleanupPreview (tmp, no store/journal) + cleanupApply (storeAddFile + one journaled src swap + meta.cleanup), api/cli parity, cleanup.test.mjs. Bg-solidify NOT built (cut per lead 2026-07-02). UI increment follows.
- 2026-07-03: Backend landed+committed (quantize/denoise per-tool folders, preview/apply ops, parity, suite 470, python 11 tests, :8780 restarted). UI worker launched (inspector Cleanup section, on-canvas live preview via view-state override, hold-to-compare, Apply journaled).
- 2026-07-03: UI landed+committed: Cleanup section (quantize slider+number+dither, denoise 1|2|3, debounced on-canvas preview via workspace view-state, hold-to-compare, shared Apply w/ tool label, stale-seq guard, transform-guard parity). Suites 470/51. No restart needed (site JS). Measured on 1254px: quantize ~1-2s, denoise <1s per preview. Awaiting lead verify - checklist in tmp/VERIFY_2026-07-03.md item 4.
