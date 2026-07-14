---
id: T0382
title: Build isolated deterministic Items and Balance Lua sandbox
status: doing
project: P001
epic: E016
priority: P0
tags: [items, balance, lua, sandbox]
created: 2026-07-10
updated: 2026-07-14
---

## What

Implement the deterministic module loader and fresh-process sandbox against the
ratified Items declaration contract and representative backend benchmark.

## Done when

- [ ] A small representative declaration workload selects and pins the simplest
      Lua backend/version that satisfies the sandbox hooks; no separate backend
      benchmark project is required.

- [ ] Approved modules resolve in deterministic order with cycle diagnostics;
      author code cannot use filesystem, network, shell, environment, time,
      random, dynamic loading, bytecode, debug, FFI, or unrestricted JIT.
- [ ] Unordered iteration and mutable declaration state are unavailable; inputs
      are frozen/deep-copied and repeated full runs match exactly.
- [ ] V1 always evaluates in a fresh isolated process with CPU/memory/instruction/
      recursion/output limits; timeout/OOM cannot corrupt the host.
- [ ] Exported formulas use the approved deterministic math surface; raw libm
      exponentiation is excluded from the first proof.
- [ ] Errors carry stable code, game-relative file, line/column, and field path.
- [ ] Currency/fixed-sword/levelled-sword fixtures match on Windows/Linux and
      across repeated runs with the selected backend/version fingerprint.

## Open questions

- Admit incremental evaluation only through a later purity/full-parity proof.

## Log

- 2026-07-14: Absorbed the decision-relevant part of T0363. Large/full-pipeline
  measurement remains the final T0380 gate.

- 2026-07-10: Tightened after red-team findings on `pairs`, mutable closures,
  libm differences, LuaJIT hooks, and stale incremental results.
- 2026-07-14: Selected as the next risk-first task after full active-card grooming; it is the first dependency of the E016 authoring vertical.
- 2026-07-14: Started backend selection and deterministic sandbox implementation. Scope: fresh-process evaluator/module loader plus representative Items fixtures; Snapshot/runtime export remain T0383/T0365.
