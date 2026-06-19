---
id: T0002
title: "review.mjs --verify: opt-in fresh-context adversarial re-check"
status: backlog
epic: E001
priority: P2
tags: [pipeline, gates]
created: 2026-06-19
updated: 2026-06-19
---

## What

Add an opt-in `--verify` convention/flag to `review.mjs`: for a contested gate
the lead can spawn one independent verifier in a clean context whose only job is
to re-run the named validator/screenshot check and confirm or refute. Default
off (solo/lean); the lead opts in. Borrowed: cross-confirmation stance (field
survey /ultrareview), not a roster.

## Done when

- [ ] review.mjs accepts `--verify` and documents the independent-recheck convention
- [ ] docs/ai-pipeline/quality-validation.md has one line describing the opt-in verifier
- [ ] default behavior is unchanged when `--verify` is absent

## Open questions

## Log
