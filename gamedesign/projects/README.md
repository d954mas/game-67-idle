---
type: Project Wiki Guide
title: Project Game Wikis
description: Rules for game-specific design docs, GDDs, decisions, and research.
tags: [project-wiki, gdd, gamedesign]
timestamp: 2026-06-13T00:00:00Z
---

# Project Game Wikis

Create one folder per game concept:

```text
gamedesign/projects/<game-id>/
```

Use that folder for the living wiki of one game:

- concept and pitch;
- GDD and feature specs;
- decisions and open questions;
- balance, economy, content, UI flow, and progression data;
- reference studies that drive this game;
- project-specific source notes, screenshots, playtest notes, and evidence.

Do not put project facts into `gamedesign/knowledge/`. That folder is only for
reusable cross-project knowledge.

When a project lesson becomes broadly reusable, summarize the reusable rule in
`gamedesign/knowledge/` and link back to the project page if the example still
matters.
