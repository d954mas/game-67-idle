---
name: design-source-knowledge
description: Use when adding, reviewing, reorganizing, or promoting game-design research sources and reusable knowledge in this repository. Triggers include requests to add source notes, parse articles/videos/reference packets, turn sources into `gamedesign/knowledge/` rules, decide whether material belongs in global knowledge or a project wiki, audit source/knowledge hygiene, update knowledge indexes/logs, or make AI agents handle design sources correctly.
---

# Design Source Knowledge

Operate design knowledge as: source -> note -> conclusion -> index/log -> cited claim.

## Load Only What Applies

- `references/source-routing.md`: required docs and routing between shared
  sources, reusable knowledge, project wiki, `tasks/`, and `tmp/`.
- `references/source-intake-promotion.md`: Source Intake, source quality, labels
  (`observed`, `secondary`, `inferred`, `unknown`), Promotion Workflow.
- `references/reference-work-review.md`: Reference-Driven Work,
  `reference_deconstruction.md`, Source Ladder, mismatch, review, Report.
- `references/templates.md`: load only for creating/substantially rewriting
  source notes, knowledge pages, project reference notes, or review reports.

## Default Workflow

1. Use the already-loaded root rules and read the relevant `gamedesign/`
   README/index.
2. Route before editing: source shelf, reusable knowledge, project wiki,
   `tasks/`, or `tmp/`.
3. Add source notes before conclusions when external evidence matters.
4. Keep reusable principles in `gamedesign/knowledge/`; current-game facts stay
   in project wiki.
5. Link conclusions to sources, update index/logs, and state evidence gaps.

## Non-Negotiables

- Keep current-game facts, screenshots, status, GDD facts, and playtest results
  out of reusable knowledge.
- Do not use snippets, thumbnails, memory, or store copy as mechanics proof.
- Claim "grounded in refs" only with durable deconstruction: 3 labeled facts and
  1 current-build mismatch.
- If evidence is incomplete, say "not ready for implementation" and name gaps.
- Do not bury work status in design data files; use `tasks/`.
