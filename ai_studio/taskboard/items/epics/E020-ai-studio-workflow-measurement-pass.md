---
id: E020
title: AI Studio workflow measurement pass
status: done
project: P001
priority: P2
tags: [workflow, profiling, performance]
created: 2026-07-15
updated: 2026-07-15
---

## Goal

Reduce agent time, output, and coordination waste from measured workflow
evidence without weakening verification, Quality, privacy, provenance, or
release guarantees.

## In scope

- Core Harness profiling accuracy and checkpoint advice.
- Taskboard/context, verification, review, CI-wait, and handoff measurements.
- Small test-first fixes whose benefit is visible in canonical transcripts.

## Out of scope

- New verification tiers, generic runners, caches, or external observability.
- Changes to game code or the read-only engine checkout.
- Push or publication without explicit lead instruction.

## Log

- 2026-07-15: Started from clean `0fd58bd89`, ahead of `origin/master` by 7. Canonical root transcript showed 233 waits for 30 spawns, multi-hour `gh run watch`, and misleading internal guardian entries in Codex agent rollups.
- 2026-07-15: Completed measured workflow pass: kept two evidence fixes, reverted unprofitable external-wait heuristic, review ACCEPT, full 10-domain proof passed in 38.7s, no push.
