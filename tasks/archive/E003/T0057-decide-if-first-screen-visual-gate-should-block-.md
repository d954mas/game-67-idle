---
id: T0057
title: Decide if first-screen visual gate should block not just advise
status: done
epic: E003
priority: P2
tags: [quality, gate, visual, decision]
created: 2026-06-15
updated: 2026-06-15
---

## What

The post-implementation review found the one residual way a future prototype
could still ship bad/unlike-ref: the new binding visual definition of done
(T0045) + core-moment feel check (T0046) are **human-judgment, self-attested,
and advisory in the automated chain**. `ai.mjs gate` records whatever
`pass|fail` verdict the agent supplies; `slice_hygiene` only warns on a
missing/failed product gate unless `--strict` is passed; the profiler guard is
now advisory (T0044). So an agent could self-score a screenshot `pass` against
the fake shot, or skip `--strict`, and hand off a slice that still does not
match the reference. Nothing independently verifies screenshot-vs-fake-shot
similarity.

This is an acceptable trade for a solo-lead workflow (the lead is the backstop)
and is consistent with the milestone's "passive/advisory, subtract not add"
direction. But it is the named gap. This task is a LEAD DECISION, not an
implementation: choose how (or whether) to close it.

## Done when

- [x] The lead's decision is recorded in AGENTS.md + the visual gate skill.
- [x] No enforcement/auto-similarity tooling remains (option C was tried and removed as the wrong mechanism).

## Resolution (lead decision, 2026-06-15)

Option C (automated image-similarity vs the fake shot) was implemented, then
REJECTED by the lead: **the fake shot is aspirational inspiration, not a pixel
target — the real game will never look like it.** Pixel/hash/SSIM/histogram
similarity is therefore the wrong tool; it would score a genuinely good screen
as "not similar." Options B/D (blocking) are also declined — they contradict the
passive/advisory direction (the lead is the backstop).

Decision = keep the gate QUALITATIVE and advisory, with one clarification baked
in: the visual gate judges whether the screen reaches the DIRECTION the fake
shot points to (mood, palette, composition, readability, art-quality bar, "looks
like a game"), never pixel similarity. AGENTS.md and `game-feature-iteration`
now state this explicitly. The `visual_similarity.py` tool + `--reference`
wiring were removed.

## Log

- 2026-06-15: Captured from the post-implementation review as the one residual quality-loop gap.
- 2026-06-15: Tried option C (visual_similarity.py: dHash + histogram + SSIM, advisory, wired into the gate). Lead feedback: fake shot is inspiration, not a pixel target, so similarity is the wrong mechanism. Removed the tool + wiring; reworded AGENTS.md definition-of-done + continuous-gate bullets and the game-feature-iteration step to "reaches the direction/quality the fake shot points to, judged qualitatively, never by image similarity." Net: gate stays qualitative + advisory; no new tooling. skills_eval 9/9, product_gate 20/20, taskboard ok.
