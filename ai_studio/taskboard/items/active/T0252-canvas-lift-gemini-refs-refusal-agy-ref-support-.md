---
id: T0252
title: "Canvas: lift gemini+refs refusal - agy ref support verified (T0251), plumb refs + seen.txt proof"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

## Done when

- [ ]

## Open questions

## Log
- 2026-07-03: Landed 4f8fbf8e: refusal lifted, refs plumbed to agy (--add-dir per unique ref dir, open-and-view + .seen.txt proof clause), verifyAgyRefProof silent-divergence guard unit-tested directly. both+refs now runs both engines. No-refs agy template byte-identical (test-pinned). Suite 392->399. :8780 restarted, smoke 200. Note: attempt.skip branch in results loop now dead code, left per surgical rule - candidate for cleanup. Awaiting lead verify (gemini gen with refs on a real card).
