---
name: design-source-knowledge
description: Use when adding, reviewing, reorganizing, or promoting game-design research sources and reusable knowledge in this repository. Triggers include requests to add source notes, parse articles/videos/reference packets, turn sources into `gamedesign/knowledge/` rules, decide whether material belongs in global knowledge or a project wiki, audit source/knowledge hygiene, update knowledge indexes/logs, or make AI agents handle design sources correctly.
---

# Design Source Knowledge

Operate design knowledge as an evidence trail:
source -> source note -> conclusion -> index/log update -> cited claim.

## Load Only What Applies

- `references/source-routing.md`: required docs and routing between shared
  sources, reusable knowledge, project wiki, `tasks/`, and `tmp/`.
- `references/source-intake-promotion.md`: Source Intake, source quality,
  labels (`observed`, `secondary`, `inferred`, `unknown`), Promotion Workflow.
- `references/reference-work-review.md`: Reference-Driven Work,
  `reference_deconstruction.md`, Source Ladder, mismatch, review, and Report.
- `references/templates.md`: load only for creating/substantially rewriting
  source notes, knowledge pages, project reference notes, or review reports.

## Default Workflow

1. Read `AGENTS.md` and the relevant `gamedesign/` README/index.
2. Route before editing: source shelf, reusable knowledge, project wiki,
   `tasks/`, or `tmp/`.
3. Add source notes before conclusions when external evidence matters.
4. Keep reusable principles in `gamedesign/knowledge/`; current-game facts stay
   in the active project wiki.
5. Link conclusions to sources, update index/logs, and state evidence gaps.

## Non-Negotiables

- Keep current-game facts, balance, screenshots, implementation status, accepted
  GDD facts, and playtest results out of reusable knowledge.
- Do not use snippets, thumbnails, memory, or store copy as mechanics proof.
- Claim "grounded in refs" only with durable deconstruction supporting at least
  three labeled facts and one current-build mismatch.
- If evidence is incomplete, say "not ready for implementation" and name gaps.
- Do not bury work status in design data files; use `tasks/`.
