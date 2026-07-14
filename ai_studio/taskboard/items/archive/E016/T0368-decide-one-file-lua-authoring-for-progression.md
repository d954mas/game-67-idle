---
id: T0368
title: Decide one-file Lua authoring for progression
status: done
project: P001
epic: E016
priority: P0
tags: [progression, lua, decision]
created: 2026-07-10
updated: 2026-07-14
quality: {"notApplicable":{"reason":"planning and routing cleanup only"}}
---

## What

Decide whether Progression authoring moves fully into one modular Lua contract
such as `progression.track { ... }`, replacing `content/progression.json` and
the standalone generator while leaving the save/state schema separately owned.

Compare three options: keep JSON plus Balance references; make Lua canonical
for progression authoring; or migrate only derived formulas while literal
content remains structured data. Include migration cost and agent context.

## Done when

- [ ] Current producers/consumers, duplicated truth, migrations, test impact,
      and agent edit/read cost are measured for all three options.
- [ ] The lead chooses one option and the card is replaced by scoped backlog
      work, or explicitly keeps the current architecture.

## Open questions

- Does one Lua source remove duplication without mixing save schema and balance?
- Which current progression consumers and tests would be retired or adapted?
- How do designers edit literal level overrides if Lua becomes canonical?

## Log

- 2026-07-10: The assistant proposed full one-file authoring, but the lead did
  not accept it. It must not enter implementation through `T0364` or `T0365`.
- 2026-07-14: Closure: waived; reason: lead rejected this direction; preserve decision without active work; evidence: decision retained in archived card
- 2026-07-14: Quality: not-applicable; reason: planning and routing cleanup only
