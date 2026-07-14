---
id: T0389
title: Explore value-only remote Items overrides
status: done
project: P001
epic: E016
priority: P3
tags: [items, balance, remote-config, post-v1]
created: 2026-07-10
updated: 2026-07-14
quality: {"notApplicable":{"reason":"planning and routing cleanup only"}}
---

## What

Preserve a post-v1 option for remote balance control without adding it to the
current Items implementation. A future service may publish a small validated
value-only override set over the immutable bundled Items catalog.

## Done when

- [ ] Decide whether a real shipped game needs remote balance control before
      implementation; keep this card at `idea` until that evidence exists.
- [ ] Specify a signed/versioned patch bound to schema ABI and base content
      fingerprints, with stable `def_id`/`field_id` diagnostics and resolved
      runtime indices.
- [ ] Restrict overrides to allowlisted existing typed values/levels; reject
      IDs, types, storage mode, containers, state schema, item add/remove,
      formulas/code, and structural level changes.
- [ ] Validate the complete patch and atomically replace the active immutable
      overlay; accessors return value copies and expose base/effective/source/
      patch-version diagnostics.
- [ ] Define cached/offline fallback, rollback, activation safe point, stale
      patch behavior, and performance/security evidence before shipping.

## Open questions

- Prefer next-session activation by default; require explicit product evidence
  before supporting mid-session hot application.
- Benchmark overlay lookup versus rebuilding a small immutable effective
  snapshot only if this idea is promoted.

## Log

- 2026-07-10: Captured at lead request as an idea only. Current V1 loads one
  bundled immutable Items catalog at startup and implements no remote override
  download, storage, activation, or runtime mutation.
- 2026-07-14: Closure: waived; reason: remote overrides remain explicitly out of v1; preserve contract without active work; evidence: lead-requested remote override contract retained in archive
- 2026-07-14: Quality: not-applicable; reason: planning and routing cleanup only
