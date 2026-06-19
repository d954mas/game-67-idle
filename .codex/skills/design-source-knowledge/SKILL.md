---
name: design-source-knowledge
description: Use when adding, reviewing, reorganizing, or promoting game-design research sources and reusable knowledge in this repository. Triggers include requests to add source notes, parse articles/videos/reference packets, turn sources into `gamedesign/knowledge/` rules, decide whether material belongs in global knowledge or a project wiki, audit source/knowledge hygiene, update knowledge indexes/logs, or make AI agents handle design sources correctly.
---

# Design Source Knowledge

Operate the design knowledge base as an evidence trail:
source material -> source note -> reusable or project-specific conclusion ->
index/log update -> cited claim in design work.

## Load Only What Applies

- `references/source-routing.md`: required docs to read first, folder routing
  between `gamedesign/sources/`, `gamedesign/knowledge/`, project wiki,
  `tasks/`, and `tmp/`.
- `references/source-intake-promotion.md`: Source Intake requirements, source
  quality, evidence labels (`observed`, `secondary`, `inferred`, `unknown`),
  and Promotion Workflow.
- `references/reference-work-review.md`: Reference-Driven Work,
  `reference_deconstruction.md`, Source Ladder, current-build mismatch,
  "not ready for implementation", Quality Review, index/log updates, and Report.
- `references/templates.md`: load only when creating or substantially rewriting
  source notes, knowledge pages, project reference notes, or source-review
  reports.

## Default Workflow

1. Read `AGENTS.md` and the relevant `gamedesign/` README/index files.
2. Decide route before editing: reusable source shelf, reusable knowledge,
   project wiki, `tasks/`, or `tmp/`.
3. Add or update source notes before extracting conclusions when external
   evidence matters.
4. Promote only reusable principles to `gamedesign/knowledge/`; keep current
   game facts in the active project wiki.
5. Link important conclusions back to source notes, update index/logs when
   discoverability changes, and report evidence gaps plainly.

## Non-Negotiables

- Do not put current-game facts, balance numbers, screenshots, implementation
  status, accepted GDD facts, or playtest results in reusable knowledge.
- Do not use search snippets, thumbnails, memory, or store copy as mechanics
  proof.
- Do not claim "grounded in refs" unless the durable deconstruction supports
  at least three labeled facts and one current-build mismatch.
- If evidence is incomplete, say "not ready for implementation" and name the
  missing evidence.
- Do not bury work status in design data files; use `tasks/`.
