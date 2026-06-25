# Core Harness

`core_harness/` is the first reviewed migration target inside `ai_studio/`.
It owns the smallest always-loaded harness contract:

- agent entry policy;
- workflow for task execution order;
- root agent-facing compatibility;
- compatibility rules for root agent-facing surfaces;
- the minimal public commands that let an agent start work.

Files move here only after review. The tree lists candidates so each file can be
inspected, cleaned, and migrated deliberately.

## Current Compatibility Surface

These files currently stay at the repo root because external harnesses expect
them there:

- `AGENTS.md`
- `CLAUDE.md`

Root compatibility is not a design goal during this refactor. Keep only entry
files that are still structurally useful.

## Workflow Group

Workflow is part of Core Harness because it defines how an agent turns a request
into scoped context, work, verification, optional delegation, and closeout.

Current workflow candidates:

- `docs/ai-pipeline/agent-workflow.md`
- `docs/ai-pipeline/subagent-protocol.md`
- `docs/ai-pipeline/orchestration-playbook.md`

## Tree Shape

During the Core audit, grouping should be added only after ownership,
boundaries, and compatibility rules are verified.
