---
id: T0361
title: Tighten feature versions, game-owned packaging, and portal evidence contracts
status: backlog
project: P001
epic: E015
priority: P1
tags: [features, packaging, platform]
created: 2026-07-10
updated: 2026-07-10
---

## What

Make reusable feature contracts short and versioned, copy packaging ownership
into each game, and reuse the existing target/SDK adapter architecture without
claiming local mocks are real portal certification.

## Done when

- [ ] Each reusable feature has one concise human/agent router with mandatory
      SemVer and synchronized skill guidance; duplicate long-form contracts are
      removed or routed.
- [ ] A new game receives its packaging scripts/config from the template, then
      owns and evolves them independently; web packaging uses Node.
- [ ] The reference template includes a copied, game-owned `game verify`
      scaffold, and Studio CI proves that scaffold works without discovering
      arbitrary workspace games.
- [ ] Platform target and SDK remain separate, only the chosen adapter is built,
      and capabilities such as `externalLinksAllowed` are exposed at runtime.
- [ ] Existing platform-sdk/mock surfaces are reused rather than replaced.
- [ ] Local contract tests and mock validation are reported separately from real
      portal smoke/inspector evidence; placeholder ZIP config blocks release.
- [ ] Packaging creates the final ZIP, reopens and inspects that ZIP, rejects
      DevAPI/debug payloads and target/adapter mismatch, verifies required files,
      and emits an artifact manifest with hashes.
- [ ] Studio CI validates features/reference template only; each game owns its
      doctor, tests, playable proof, package validation, and CI.

## Open questions

- Real portal evidence remains environment/credential dependent and must not be
  fabricated from local tests.

## Log

- 2026-07-10: Old feature-version archives and a Studio game-CI matrix were
  explicitly rejected.
