# Core Harness

`core_harness/` is the first reviewed migration target inside `ai_studio/`.
It owns the smallest always-loaded harness contract:

- agent entry policy;
- workflow for task execution order;
- root agent-facing compatibility;
- compatibility rules for root agent-facing surfaces;
- passive profiling of agent/tool execution;
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

## Workflow Module

Workflow is part of Core Harness because it defines how an agent turns a request
into scoped context, work, verification, optional delegation, and closeout.

Reviewed workflow file:

- `ai_studio/core_harness/workflow/README.md`

## Workflow Orchestration

Orchestration is a child rule inside Workflow, not a sibling module. It exists
to define optional bounded delegation and independent review when their benefit
exceeds coordination cost, using a simple Task / Scope / Return / Stop packet.

Reviewed orchestration file:

- `ai_studio/core_harness/workflow/orchestration/README.md`

## Agent Surfaces Module

Agent Surfaces owns generated compatibility files for Codex and Claude. Canonical
sources stay in one place; generated files are checked or regenerated from them.

Reviewed agent surface files:

- `ai_studio/core_harness/agent_surfaces/README.md`
- `ai_studio/core_harness/agent_surfaces/sync.mjs`
- `ai_studio/core_harness/agent_surfaces/skills_sync.mjs`
- `ai_studio/core_harness/agent_surfaces/hooks_sync.mjs`

## Profiling Module

Profiling is part of Core Harness because it observes the harness work loop:
session starts, command start/result pairs, failures, repeated commands, slow
commands, coverage gaps, and subagent spawn diagnostics.

Agent Surfaces owns the generated hook config. Profiling owns the recorder and
review commands those hooks call.

Reviewed profiling files:

- `ai_studio/core_harness/profiling/README.md`
- `ai_studio/core_harness/profiling/status.mjs`
- `ai_studio/core_harness/profiling/hook_record.mjs`
- `ai_studio/core_harness/profiling/hook_record_fast.c`
- `ai_studio/core_harness/profiling/tests/profiling.test.mjs`

The reflection skill that consumes profiling output lives in
`.codex/skills/nt-chat-session-reflection/SKILL.md`.

## Validation Module

Core Harness validation owns only fast mechanical checks for agent-facing docs
and routes. It does not run module test suites or validate the whole pipeline.

Reviewed validation files:

- `ai_studio/core_harness/validation/README.md`
- `ai_studio/core_harness/validation/doc_reference_check.mjs`
- `ai_studio/core_harness/validation/enforcement_check.mjs`
- `ai_studio/core_harness/validation/tests/doc_reference_check.test.mjs`
- `ai_studio/core_harness/validation/tests/enforcement_check.test.mjs`

## Tree Shape

During the Core audit, grouping should be added only after ownership,
boundaries, and compatibility rules are verified.
