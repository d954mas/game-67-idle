---
name: design-source-knowledge
description: Use when adding, reviewing, reorganizing, or promoting game-design research sources and reusable knowledge in this repository. Triggers include requests to add source notes, parse articles/videos/reference packets, turn sources into `gamedesign/knowledge/` rules, decide whether material belongs in global knowledge or a project wiki, audit source/knowledge hygiene, update knowledge indexes/logs, or make AI agents handle design sources correctly.
---

# Design Source Knowledge

Operate the design knowledge base as an evidence trail:
source material -> source note -> reusable or project-specific conclusion ->
index/log update -> cited claim in design work.

## Load First

1. Read `AGENTS.md`.
2. Read `gamedesign/README.md`, `gamedesign/knowledge/README.md`,
   `gamedesign/sources/README.md`, and `gamedesign/knowledge/index.md`.
3. If references guide gameplay, UI, economy, balance, or final art, also read
   `gamedesign/knowledge/reference_deconstruction.md`.
4. For templates, read `references/templates.md` only when creating or
   substantially rewriting source/knowledge files.

## Routing

- `gamedesign/sources/`: raw or near-raw source notes for reusable
  cross-project knowledge. Keep facts close to the original material.
- `gamedesign/knowledge/`: reusable principles, checklists, methods, and
  validation rules. Do not put current-game facts here.
- `gamedesign/projects/<game-id>/sources/` or `references/`: source material
  that supports one game, one GDD, one balance file, one art direction, or one
  current implementation decision.
- `gamedesign/projects/<game-id>/`: project-specific conclusions, decisions,
  examples, reference studies, evidence, screenshots, playtest notes, UI flow,
  balance, and content data.
- `tasks/`: work status and deferred tasks.
- `tmp/`: temporary extraction, screenshots, transcripts, rejected material,
  scratch scripts, and audit logs.

If a note names the active game, local balance numbers, screenshots, current
implementation status, accepted GDD facts, or playtest results, keep it in the
project wiki unless it has been rewritten as a reusable rule.

## Source Intake

For every non-trivial source, record:

- title, author/publisher if known, link or local path, and checked date;
- source quality: official/store/trailer, gameplay footage, walkthrough/guide,
  review/community, deconstruction/analysis, academic/industry talk,
  user-provided, or unknown;
- what the source proves and what it does not prove;
- evidence labels for claims: `observed`, `secondary`, `inferred`, or `unknown`;
- exact timestamps, frame numbers, screenshot paths, sections, or quotes when a
  claim depends on visual/gameplay evidence.

Do not use search snippets, thumbnails, memory, or store copy as mechanics
proof. If browsing or source access is missing, write the evidence gap plainly.

## Promotion Workflow

1. Decide scope before editing: reusable knowledge, project-specific knowledge,
   raw source shelf, or task/status.
2. Add or update the source note first when external evidence matters.
3. Extract only reusable conclusions into `gamedesign/knowledge/`, or
   project conclusions into the active project wiki.
4. Link the conclusion back to its source note when the source matters.
5. Update `gamedesign/knowledge/index.md` when discoverability changes.
6. Update `gamedesign/knowledge/log.md` for meaningful reusable knowledge-base
   changes. Keep the log short.
7. Do not create a new knowledge page if a focused edit to an existing page is
   clearer.

## Reference-Driven Work

When a named reference drives gameplay, UI, economy, balance, or final art:

- declare study mode: quick check, central deconstruction, or deep
  deconstruction;
- use the Source Ladder in `reference_deconstruction.md`;
- create or update the durable deconstruction in the project wiki;
- include source matrix, observation ledger, borrow/avoid/copy-risk,
  current-build mismatch, and next native screenshot/scenario proof;
- do not claim "grounded in refs" unless you can cite at least three labeled
  facts and one current-build mismatch from the durable doc.

If the evidence is incomplete, state: "not ready for implementation", name the
missing evidence, and continue source gathering or ask for material.

## Quality Review

Before finishing, check:

- frontmatter exists on new durable Markdown files;
- `knowledge/` pages are reusable and not disguised project docs;
- `sources/` notes stay close to source material and avoid polished
  conclusions unless clearly labeled as takeaways;
- each important conclusion has a source link/path or an explicit "inferred"
  label;
- index/log updates match the scope of the change;
- no work status, prompt dumps, temp artifacts, or generated-audit noise leaked
  into `knowledge/` or `sources/`;
- reference digests are backed by durable deconstruction docs when the
  reference controls implementation.

## Report

In the final response, state:

- files changed;
- where each source/conclusion was routed and why;
- evidence gaps or claims that remain only inferred;
- index/log updates;
- validation performed.
