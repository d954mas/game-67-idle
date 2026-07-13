---
id: T0382
title: Build isolated deterministic Items and Balance Lua sandbox
status: backlog
project: P001
epic: E016
priority: P0
tags: [items, balance, lua, sandbox]
created: 2026-07-10
updated: 2026-07-10
---

## What

Implement the deterministic module loader and fresh-process sandbox against the
ratified Items declaration contract and representative backend benchmark.

## Done when

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
      across repeated runs with the T0363 provisional backend/version fingerprint.

## Open questions

- Admit incremental evaluation only through a later purity/full-parity proof.

## Log

- 2026-07-10: Tightened after red-team findings on `pairs`, mutable closures,
  libm differences, LuaJIT hooks, and stale incremental results.
