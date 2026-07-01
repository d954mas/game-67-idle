---
type: Knowledge Guide
title: Design Knowledge Base
description: Lightweight OKF-style rules for reusable game design knowledge.
tags: [knowledge, gdd, okf-lite, ai]
timestamp: 2026-06-13T00:00:00Z
---

# Game Development Knowledge Base

Reusable game-development knowledge for game GDD and production work.

This folder is an OKF-lite knowledge bundle: plain Markdown files with small
YAML frontmatter, links between related pages, citations when external sources
matter, and a short change log. It is intentionally not a task tracker, GDD,
status board, database, SDK, or required pipeline.

## Folder Roles

- `gamedev_knowledge/knowledge/` - reusable principles, checklists, patterns, and
  reference-study methods that can apply across game projects.
- `gamedev_knowledge/sources/` - optional raw source notes, excerpts, links, and
  durable research packets. Keep these closer to the original material.
- Project-specific concept docs belong in the active project GDD folder, not in
  this reusable knowledge base.
- Work status and deferred tasks belong in `tasks/`, not here.

## Entry Points

- [Index](index.md) - topic map for agents and humans.
- [Log](log.md) - compact history of meaningful knowledge-base changes.
- [GDD Application](gdd_application.md) - how to turn reusable rules into a
  concrete project spec.
- [Reference Deconstruction](reference_deconstruction.md) - how to study a
  reference before implementation.
- [Agent Legibility](agent_legibility.md) - how to keep docs/tooling useful for
  AI-assisted work.

## OKF-Lite File Shape

New or substantially edited files should start with this frontmatter:

```yaml
---
type: Game Design Knowledge
title: Short Human Title
description: One sentence describing when to use this page.
tags: [design, example]
timestamp: 2026-06-13T00:00:00Z
---
```

Only `type` is required. The other fields are recommended because they help an
agent find the right page without reading every file. Existing pages can be
updated when they are edited; do not run a large format-only rewrite.

## Page Rules

- Keep pages short enough to scan before a design or implementation pass.
- Prefer checklists, decision prompts, anti-patterns, and validation criteria
  over essays.
- Link related knowledge files instead of duplicating sections.
- Put external sources under `## References` or `## Citations`.
- Use `gamedev_knowledge/sources/` for raw research; summarize only reusable lessons
  in `knowledge/`.
- Add a new page only when an existing page would become less readable.

## What Belongs Here

- FTUE and onboarding design rules.
- Core loop, reward, economy, content, and progression patterns.
- UI/UX, accessibility, playtest, and release-readiness checklists.
- Reference deconstruction methods and reusable research protocols.
- AI-agent legibility rules that make design work easier to inspect and verify.

## What Does Not Belong Here

- Current project status.
- Sprint tasks or implementation checklists.
- Game-specific lore, names, currencies, jokes, copy, or balance numbers.
- One-off playtest notes unless they become reusable across projects.
- Tool output dumps, generated prompt logs, or raw chat transcripts.

## Maintenance

When adding or changing durable knowledge:

1. Update the page itself.
2. Add or fix links in [Index](index.md) if discoverability changed.
3. Add one short entry to [Log](log.md) for meaningful changes.
4. Move raw source material to `gamedev_knowledge/sources/` if it should be preserved.
5. Keep tooling optional; do not add a validator or script unless repeated
   mistakes prove it would save time.
