---
id: T0008
title: Bootstrap exporter for new game projects
status: done
epic: E002
priority: P1
tags: [ai-pipeline, tooling]
created: 2026-06-11
updated: 2026-06-11
---

## What

One-command export of the portable AI base (skills, taskboard, skills_sync,
knowledge, pipeline doc, starter AGENTS.md/CLAUDE.md) into a new project.

## Done when

- [x] node tools/bootstrap/export_base.mjs --target <dir> copies the base
- [x] exported taskboard CLI works standalone in the target
- [x] skills_sync runs automatically in the target
- [x] existing target files preserved unless --force

## Open questions

## Log

- 2026-06-11: Done. Evidence: exported to %TEMP%/ai-base-export-test; in target: `cli.mjs new` created T0001, `validate` -> "ok: no problems found"; 9 skills generated; test dir cleaned up.
