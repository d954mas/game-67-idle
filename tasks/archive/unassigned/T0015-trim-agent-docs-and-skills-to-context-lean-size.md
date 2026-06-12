---
id: T0015
title: Trim agent docs and skills to context-lean size
status: done
epic: ""
priority: P1
tags: [ai-pipeline, docs]
created: 2026-06-11
updated: 2026-06-11
---

## What

Cut redundant text from agent rules and skills; context size is a first-class
cost. Always-loaded files (AGENTS.md, CLAUDE.md imports) matter most; skill
bodies load on trigger; reference playbooks load on demand.

## Done when

- [x] CLAUDE.md no longer imports AI_PIPELINE.md into every session
- [x] AGENTS.md compressed without losing any rule
- [x] primary-gdd-pipeline SKILL.md deduplicated against its references
- [x] task-manager SKILL.md deduplicated against tasks/README.md
- [x] skills_sync re-run, tests pass, store validates

## Open questions

## Log

- 2026-06-11: Done. Word counts: CLAUDE.md 58->8 (and -492 of imported
  AI_PIPELINE per Claude session); AGENTS.md 359->305;
  primary-gdd-pipeline/SKILL.md 2876->1053 (-63%, every cut detail verified
  present in a reference playbook); task-manager/SKILL.md 453->290.
  Evidence: skills_sync 9/9, node --test 8 pass, cli validate ok.
  Follow-up behavioral eval: T0016.
