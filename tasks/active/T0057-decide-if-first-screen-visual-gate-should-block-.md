---
id: T0057
title: Decide if first-screen visual gate should block not just advise
status: idea
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

- [ ] The lead picks an option (or explicitly accepts the advisory status quo) and it is recorded in AGENTS.md / the visual gate skill.
- [ ] If an enforcement option is chosen, a follow-up implementation task is created.

## Open questions (lead decision)

Options:
- A. Keep advisory (status quo): the lead reviews the first-screen gate manually; the gate stays self-attested. Lowest friction, matches the passive direction.
- B. Make `gate`/screenshot evidence BLOCKING for first-screen handoff (e.g. slice_hygiene treats a missing/fail product gate as a hard problem for first-slice work, not just under `--strict`). Re-introduces one hard gate, but only for the first playable screen.
- C. Add a cheap automated image-similarity check of the native screenshot against the named fake shot (e.g. perceptual hash / SSIM thresholds) as an advisory score that feeds the gate. Independent signal without full human judgment.
- D. B + C combined.

Recommendation to weigh: C (cheap independent signal) layered on the current advisory gate gives the most quality leverage without re-bloating gates; B only if self-attestation proves unreliable in practice.

## Log

- 2026-06-15: Captured from the post-implementation review as the one residual quality-loop gap. Needs a lead decision before any enforcement work.
