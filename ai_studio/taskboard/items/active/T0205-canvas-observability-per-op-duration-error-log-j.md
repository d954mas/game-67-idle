---
id: T0205
title: "Canvas observability: per-op duration + error log (jsonl), timings in API responses"
status: backlog
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Operational observability for the canvas (lead: needed for future optimization and bug hunts): per-op duration_ms recorded on each journal entry, failed ops appended to a per-project errors.jsonl (op, params summary, error, duration), op timings returned in API responses so the page/toasts can surface slow ops, and a small CLI report command (ops-stats <id>) summarizing durations by op type from the journal.

## Done when

- [ ] every journaled entry carries duration_ms; failures land in errors.jsonl with enough context to reproduce
- [ ] API responses include the op duration; toasts show it for long ops
- [ ] ops-stats prints per-op count/median/p95 from a project's journal
- [ ] doc: README section on where logs live and how to read them

## Open questions

## Log
- 2026-07-02: Scoped from measured Canvas operation and error-reporting needs;
  acceptance criteria in this task are the current contract.
