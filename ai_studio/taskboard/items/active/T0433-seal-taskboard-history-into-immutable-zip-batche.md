---
id: T0433
title: Seal Taskboard history into immutable ZIP batches
status: doing
project: P001
epic: E001
priority: P1
tags: []
created: 2026-07-14
updated: 2026-07-14
---

## What

Keep completed Taskboard history readable by humans without leaving full stale
Markdown in normal repository search. Periodically seal pending closed tasks
into immutable deterministic ZIP batches; do not add a growing global index.

## Done when

- [ ] Closing remains guarded and writes readable Markdown to an ignored pending
      area until an explicit seal operation.
- [ ] Sealing creates one deterministic immutable ZIP with original task files,
      verifies it before deleting sources, and refuses overwrite or partial loss.
- [ ] Default Taskboard reads and ordinary `rg` do not discover sealed task
      bodies; explicit archive lookup extracts only the requested task.
- [ ] Existing archived tasks are migrated into a verified sealed batch with no
      loose historical Markdown left behind.
- [ ] Focused tests, Taskboard validation, full Windows verification, independent
      review, commit, push, and CI pass.

## Open questions

- None. Lead approved periodic immutable ZIP batches with direct human access
  and no global `index.jsonl`.

## Log

- 2026-07-14: Started from approved archive simplification. Scope is Taskboard
  history storage and lookup only; active tasks/projects/epics stay Markdown.
