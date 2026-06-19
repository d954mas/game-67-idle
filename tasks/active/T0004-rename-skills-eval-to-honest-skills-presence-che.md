---
id: T0004
title: Rename skills_eval to honest skills_presence_check + pointer
status: backlog
epic: E001
priority: P2
tags: [pipeline, skills]
created: 2026-06-19
updated: 2026-06-19
---

## What

`skills_eval.mjs` is a substring-presence linter mislabeled "eval", creating
false confidence that skills are behaviourally checked. Rename it to
`skills_presence_check` (or at minimum relabel its output + docs) and add a
one-line pointer that it checks presence of stable anchors, not behaviour or
quality — the human rubric is the real eval. Update pipeline_validate label,
callers, and docs.

## Done when

- [ ] the tool, its output, and docs no longer call presence-checking an "eval"
- [ ] all callers (pipeline_validate, docs, skill references) updated; validate stays green

## Open questions

## Log
