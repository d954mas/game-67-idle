---
id: T0371
title: Remove legacy Architecture Map dependency graph
status: done
project: P001
epic: E015
priority: P1
tags: [architecture-map, legacy]
created: 2026-07-10
updated: 2026-07-11
---

## What

Remove the unreadable legacy dependency-graph representation and every dormant
consumer without changing the current human file/component tree.

## Done when

- [x] Repository search identifies all graph, arrow, transition, `dependsOn`,
      and `usedBy` producers/consumers before deletion.
- [x] Legacy graph data, generation, tests, and dead UI rendering are removed.
- [x] The current tree page has identical structure and interaction after the
      deletion; no replacement graph or inferred dependency analysis is added.
- [x] Architecture Map contracts describe files, components, ownership, and
      short descriptions only.

## Open questions

None.

## Log

- 2026-07-10: Split from `T0354`; deletion and storage parity now have separate
  evidence and closure.
- 2026-07-11: Checkpoint: T0351 closed at c2263d492. Starting read-only inventory of legacy Architecture Map graph producers and consumers before deletion; current human tree structure and interaction remain invariant, while T0354 storage repair, T0393/E016, games/web-dressup, and external engine stay out of scope.
- 2026-07-11: Inventory evidence: repository-wide graph/arrow/transition/dependsOn/usedBy search found the dependency model only in the hidden Architecture Map surface; tree JSON, loader, and API contained no dependency data. The remaining dev-environment dependsOn fields are unrelated VS Code task sequencing, and current drill layout names describe the ownership hierarchy rather than dependency analysis.
- 2026-07-11: TDD evidence: the new hierarchy-only surface contract failed RED on `moduleEdges`, then passed after legacy controls, synthetic module/edge data, old rendering/state/events/detail/queue code, arrow SVG remnants, and producerless residue were deleted. The test syntax-compiles the inline app and pins current click, keyboard, Back, type-filter, drag persistence, wheel zoom, copy-path, shared CSS, UTF-8, and semantic Tree contracts.
- 2026-07-11: Implementation: Architecture Map now renders only the explicit ownership tree and validation report. Studio Shell labels it Tree; no replacement graph, inferred relationship model, or T0354-owned tree storage/API change was introduced. Net surface reduction is about 1765 lines.
- 2026-07-11: Verification: Architecture Map 23/23, strict map 353 mapped / 786 scanned with 0 issues, malformed Studio Shell 1/1, Taskboard validation 0 problems, syntax/residue/scoped diff checks pass. Isolated Playwright matched the baseline root 12 labels, Architecture Map 6 children, two Back controls and root return, Module filter 12 to 3, 13x13 icons, baseline viewport computed styles, no legacy DOM/arrows, and 0 console errors/warnings; final screenshot is visually intact.
- 2026-07-11: Review convergence: cycle 1 restored active shared hierarchy CSS and removed dead CSS/JS/handlers while strengthening interaction contracts; cycle 2 removed the final fake search/zoom/drill-switch/center-role scaffold. Final architecture/correctness and tests/process rechecks both report 0 HIGH and 0 actionable MEDIUM/LOW.
- 2026-07-11: Quality: QTECH_001=pass; evidence: RED-GREEN hierarchy-only contract, 23/23 Architecture Map tests, strict map zero issues, isolated browser parity and clean console, semantic residue scan, and two independent closure reviews at 0 HIGH and 0 actionable
- 2026-07-11: Closed after two review/fix cycles converged at 0 HIGH and 0 actionable; all four acceptance criteria and QTECH_001 evidence are present.
