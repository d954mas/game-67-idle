---
id: T0018
title: Mechanics and meta matrix for Mech Builder Battler
status: review
epic: ""
priority: P1
tags: [gamedesign, mechanics, meta, progression, mobile, web, mechs]
created: 2026-06-19
updated: 2026-06-19
---

## What

Create a project-specific mechanics/meta synthesis for `mech-builder-battler`
that decomposes combat, mech assembly, grind, upgrades, resources, enemy roles,
build archetypes, MVP scope, and anti-scope from the current references.

Scope boundaries:

- In scope: design synthesis, reference-backed mechanics/meta matrix, casual
  translation rules, native PC proof expectations, evidence gaps.
- Out of scope: runtime implementation, pipeline/tools/engine changes, final
  art generation, exact economy tuning, and accepted balance numbers.

## Done when

- [x] `gamedesign/projects/mech-builder-battler/design/mechanics_meta_matrix_2026-06-19.md`
      exists with frontmatter and links to current project references/specs.
- [x] The matrix covers core loop, moment loop, session loop, meta loop, mech
      slots, control budget, enemy roles, resources, grind guardrails,
      archetypes, MVP scope, anti-scope, and native PC proof expectations.
- [x] The doc labels refreshed store-page evidence and states why it is not
      implementation-ready for exact UI/economy/balance.
- [x] `node tools/taskboard/cli.mjs validate` passes or any failure is logged.

## Open questions

- Should the matrix be copied into undated `GDD.md` later, or remain linked as
  a supporting dated design artifact?

## Log

- 2026-06-19: Added mechanics/meta matrix at
  `gamedesign/projects/mech-builder-battler/design/mechanics_meta_matrix_2026-06-19.md`
  after refreshing Google Play store-page evidence for Mech Arena, CATS, and
  War Robots. Kept status in `review` because it is design synthesis for lead
  acceptance, not implementation authorization.
