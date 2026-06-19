---
id: T0001
title: close_slice fails when gate evidence file is missing on disk
status: done
epic: E001
priority: P1
tags: [pipeline, gates]
created: 2026-06-19
updated: 2026-06-19
---

## What

`close_slice.mjs` accepts free-text `--evidence` and prints `screenshot:
(missing)` without failing, so a green slice can reference artifacts that do not
exist (self-attestation). Resolve the gate's screenshot and path-like
`--evidence` tokens with `existsSync()`; fail (exit 1) on a pass / non-partial
close when a referenced file is missing, naming the path. Keep an explicit
partial-handoff escape (e.g. `--allow-fail`). Borrowed: external-validator-as-
arbiter (indie Stop-hook + 2026 field survey). Highest-leverage verification fix.

## Done when

- [x] close_slice exits non-zero on a pass close when the gate screenshot or a path-like `--evidence` file does not exist
- [x] an explicit partial / allow-fail escape still permits handoff with a stated reason
- [x] tools/product_gate/test.mjs covers both missing-file fail and present-file pass

## Open questions

## Log

- 2026-06-19: evidence-as-arbiter in close_slice.mjs — a non-partial close fails
  (exit 1) when the gate screenshot or a path-like `--evidence` file is missing;
  `--allow-fail` waives for partial handoff. Tests: product_gate 29/29 (added
  missing-file-fail + allow-fail), ai.test 11/11, `node tools/ai.mjs validate` green.
