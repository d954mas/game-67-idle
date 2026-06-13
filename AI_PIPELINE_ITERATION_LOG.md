# AI Pipeline Iteration Log

Purpose: preserve only compact reusable lessons about AI-assisted development.
This is not the task board, not live status, and not a full retrospective.

## Current Lessons

### 2026-06-13 - Tooling Must Stay Passive

- Context: Profiling, validation, reflection, and task scripts grew into their
  own workflow.
- Friction: Agents started servicing stale diagnostics, review queues, broad
  validation, and generated handoff artifacts instead of working on the game.
- Lesson: AI tools should default to quiet, bounded, advisory output. Exhaustive
  output and generated artifacts need explicit `--verbose`, `--deep`,
  `--review`, `--all`, or `--include-final`.
- Status: Implemented for profiling/status/taskboard/validation planner; keep
  applying this rule to new scripts.

### 2026-06-13 - Live Status Must Be Short

- Context: `tasks/STATUS.md` grew into a long evidence log.
- Friction: Every orientation risked loading old history instead of current
  blockers and next actions.
- Lesson: `STATUS.md` is only a live index. Detailed evidence belongs in task
  logs, reports, screenshots, and git history.
- Status: `tasks/STATUS.md` was reset to a compact current index.

### 2026-06-13 - Review Queues Are Not Current Work

- Context: Many finished tasks remained in `review`.
- Friction: List/context output treated review backlog like active work.
- Lesson: Default taskboard commands should show `doing/todo/backlog` only.
  Use `list --review` explicitly for review cleanup.
- Status: Implemented in taskboard CLI and task-manager skill.

### 2026-06-13 - Reference Gates Need Short Front Doors

- Context: Reference-study rules were duplicated in `AGENTS.md`, `AI_PIPELINE.md`,
  skills, and playbooks.
- Friction: The correct rule became hard to scan and looked heavier than the
  implementation task.
- Lesson: Main agent files should contain the short gate and point to the
  detailed playbook only when a named reference actually drives the work.
- Status: Main docs shortened; detailed reference docs remain opt-in.

## Entry Rule

Add a new entry only when there is a reusable process lesson. Keep it under
about 10 lines. If the lesson creates real work, put the work in `tasks/`.
