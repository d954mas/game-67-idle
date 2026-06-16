---
id: T0076
title: Optimize skills and generation workflow
status: backlog
epic: E004
priority: P1
tags: [workflow, skills, generation, performance]
created: 2026-06-16
updated: 2026-06-16
---

## What

Optimize the next layer after the generated UI asset importer work: agent
skills, generation prompts/contracts, and validation workflow around generated
art. Do not re-open the T0075 importer/native worker unless a fresh benchmark
shows it is again the bottleneck.

Focus on making generation iterations faster, less error-prone, and easier to
validate: clearer skill instructions, fewer repeated manual steps, better
generation packets, and better evidence capture for accepted/rejected assets.

## Done when

- [ ] Current generated-art skills and art-job commands are audited for repeated
      friction, missing required context, and stale/duplicated instructions.
- [ ] The next generation workflow has one documented happy path from prompt
      packet to accepted source, runtime slice, audit evidence, and handoff.
- [ ] Any changed skill/tooling instructions are validated with the narrow
      project checks that cover them.
- [ ] T0075 importer/UI performance scope remains closed unless new benchmark
      evidence proves a regression or new bottleneck.

## Open questions

- Which generation provider/workflow should be treated as the default path for
  the next art iteration?
- Should the next optimization focus first on prompt packet quality, source
  intake rejection speed, or agent skill activation/context hygiene?

## Log

- 2026-06-16: Created after closing T0075. The generated UI/importer tool path
  is now noticeably faster; next work should target skills and generation
  workflow rather than adding a Node wrapper or more importer micro-optimization.
