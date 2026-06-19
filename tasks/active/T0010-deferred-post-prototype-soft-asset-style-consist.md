---
id: T0010
title: "DEFERRED post-prototype: soft asset style-consistency (reference-anchor + tone-outlier check)"
status: idea
epic: E001
priority: P3
tags: [assets, deferred]
created: 2026-06-19
updated: 2026-06-19
---

## What

DEFERRED. Do NOT start before a game is past the prototype gate — early-stage
palette locking actively hindered the lead (it freezes the look while it is
still being found). Goal: keep AI-generated assets visually consistent once a
real game produces content in batches.

Design (calibrated for rich "dirty-palette" AI art, NOT flat/pixel art — a hard
palette-snap audit would reject ~everything and is the wrong tool):

- (a) Reference-anchor at generation (primary lever): feed 1-3 accepted anchor
  images / the art-bible plate as a reference into each generation so new assets
  inherit the look. No LoRA. Adopt first, from the first accepted art batch.
- (b) Soft tone-outlier CHECK (not snap): after cutout, compute each asset's
  colour stats (dominant colours, mean hue/sat/value, colour temperature) and
  FLAG outliers vs the accepted set. ADVISORY only (false-positives on
  intentionally-varied assets, e.g. a fire icon vs a stone icon) — a "did this
  drift?" signal for the lead's eye, never blocking.
- (c) Optional colour-grade normalisation: nudge tone toward a reference
  (gray-world / reference colour-match / LUT) to fix mild drift instead of
  rejecting. Only if drift is real.
- `palette.json` is a "style anchor" (a few representative swatches + tone
  ranges + reference image paths) that builds prompts and feeds the soft check,
  NOT a strict allow-list. Skip hard palette-snap entirely.

Lands in: delegated-image-generation + generated-game-ui-assets (reference-anchor
convention), a new advisory tone-outlier audit under tools/assets/audit/ wired
into validate_art_job, palette.json next to the art bible.

## Done when

- [ ] PROMOTE idea -> backlog only when: a game is past the prototype gate AND art direction/art-bible/fake-shots are accepted AND content is produced in batches (drift visible)

## Open questions

## Log
