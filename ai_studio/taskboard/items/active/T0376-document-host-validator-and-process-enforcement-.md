---
id: T0376
title: Document host validator and process enforcement boundaries
status: backlog
project: P001
epic: E015
priority: P1
tags: [agents, contracts]
created: 2026-07-10
updated: 2026-07-10
---

## What

Make agent/tool rules honest about where enforcement actually lives so logs do
not imply that a process convention was technically checked.

## Done when

- [ ] Canonical agent/workflow contracts label each material rule as
      host-enforced, repository-validator-enforced, or process convention.
- [ ] Each host/validator-enforced claim links to the actual configuration,
      command, or test that proves it.
- [ ] Logs and quality reports distinguish observed enforcement from advisory
      instructions and do not claim a model/router mechanism the repo lacks.
- [ ] After harness restart, a smoke procedure verifies requested role and
      actual model; generic fallback is reported as failure.
- [ ] Wording stays short and routes to owning docs instead of duplicating all
      role/provider formats.

## Open questions

## Log

- 2026-07-10: The lead chose not to refactor different Codex/Claude role
  catalogs. This task documents/enforces the real boundary only.
