---
id: T0295
title: "rb-dark-rpg: tune release combat curve with simulator checks"
status: backlog
project: P003
epic: E012
priority: P1
tags: [rb-dark-rpg, release, combat, balance, qa]
created: 2026-07-05
updated: 2026-07-05
---

## What

Run deterministic simulator checks on the release combat curve after authored
content JSON is expanded.

## Done when

- [ ] Mandatory encounters are easy/fair with intended level and gear tier.
- [ ] Risky states point to a heal or upgrade action.
- [ ] No mandatory encounter requires a crit roll to win.
- [ ] Fight duration targets are checked against the release pacing contract.

## Open questions

- Which simulator output should become the stable balance report: JSON summary,
  markdown table, or both?

## Log

- 2026-07-05: Created as follow-up to keep combat math verification separate
  from the authored content expansion.
