---
id: T0371
title: Remove legacy Architecture Map dependency graph
status: backlog
project: P001
epic: E015
priority: P1
tags: [architecture-map, legacy]
created: 2026-07-10
updated: 2026-07-10
---

## What

Remove the unreadable legacy dependency-graph representation and every dormant
consumer without changing the current human file/component tree.

## Done when

- [ ] Repository search identifies all graph, arrow, transition, `dependsOn`,
      and `usedBy` producers/consumers before deletion.
- [ ] Legacy graph data, generation, tests, and dead UI rendering are removed.
- [ ] The current tree page has identical structure and interaction after the
      deletion; no replacement graph or inferred dependency analysis is added.
- [ ] Architecture Map contracts describe files, components, ownership, and
      short descriptions only.

## Open questions

## Log

- 2026-07-10: Split from `T0354`; deletion and storage parity now have separate
  evidence and closure.
