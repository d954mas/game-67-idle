# AI Pipeline History

Dated history of AI-workflow iteration lessons and retrospectives. This is not
the task board, not live status, and not the canonical process doc (`AI_PIPELINE.md`).
Add a new entry only when there is a reusable process lesson; if a lesson creates
real work, put the work in `tasks/`. For deeper retrospectives of long multi-turn
sessions, use the `chat-session-reflection` skill.

## Iteration Log

Compact reusable lessons about AI-assisted development. Keep each entry under
about 10 lines.

### 2026-06-14 - Automation Green Did Not Mean Product Good

- Context: A long native RPG prototype run added systems, routes, state,
  DevAPI scenarios, and screenshots, but the lead rejected the visible game as
  unclear and visually unacceptable.
- Friction: The agent treated passing probes as progress while the first
  screen still failed product-read, FTUE, and art-direction quality.
- Lesson: For visual/gameplay-heavy game work, stop content expansion until a
  native screenshot passes the player-read gate: where am I, what do I do,
  what changed, what is the reward, and does it look like a game.
- Status: Added pipeline and skill stop-gates; T0006 is the visual rescue task.

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

## Retrospective - 2026-06-13

Scope: long 67 World AI-assisted development session across concept work,
reference study, generated art, native gameplay, balance, release packaging,
and AI workflow rules.

### Summary

The game made substantial native progress, but the AI workflow became too
heavy. Tooling for profiling, validation, reflection, task status, reference
study, and release evidence started creating process obligations that competed
with the actual game work.

Current product truth remains simple: automated release/package checks can pass,
but the game is not release-ready until real manual child-test/user acceptance
is returned and any resulting fixes are applied.

### Main Problems

- The agent sometimes optimized for faster-looking paths instead of the native
  PC harness.
- Reference work was initially too shallow, then overcorrected into a bulky
  gate duplicated across docs.
- Visual work sometimes polished placeholders instead of moving through the art
  asset pipeline.
- `tasks/STATUS.md` became an evidence log instead of a short current index.
- Many completed tasks stayed in `review`, making current work look noisy.
- Profiling/reflection tools grew into a maintenance workflow.
- Broad validation was too easy to run repeatedly.

### Current Fixes

- Tool defaults are passive and advisory.
- Deep AI workflow artifacts require explicit `--deep`, `--verbose`,
  `--review`, `--all`, or `--include-final`.
- `tasks/STATUS.md` is compact again.
- Taskboard hides review tasks from normal list/context output.
- Profiling records slow/failing/large-gap signals by default instead of every
  small step.
- Validation planner defers broad/final checks unless explicitly requested.

### Next Rule

If AI tooling creates work that does not directly help answer "what should we
build, change, or verify next in the game?", simplify the tool or move the
behavior behind an explicit deep mode.

## External AI Observability Decision Criteria

The project profiles AI sessions with the local `tools/ai_profile/` JSONL
profiler and stays local-first. Do not add an external tracing/eval platform
(LangSmith, Phoenix, Langfuse, Braintrust, OpenTelemetry export, etc.) just
because reflection needs more data. Run a bounded side-by-side external pilot
only when a concrete trigger exists: multiple humans/agents need a shared trace
dashboard; human review/labels/annotations are part of the workflow;
datasets/experiments/evals must compare agent or model changes over the same
inputs; production AI app telemetry/cost/latency or online evals are required;
OTLP export is needed for a wider stack; or local JSONL review cannot answer an
important repeated question without manual reconstruction. Reject a tool that
needs accounts, keys, servers, or SDK wiring before the first useful question is
known, that would capture prompts/screenshots/child-test notes off the machine
without an explicit privacy decision, or that only duplicates local
summary/review/follow-up outputs. A pilot earns adoption only after it shows
lower reflection/debug time, better cross-agent/human review than local
markdown/JSON, reusable datasets/evals that prevent regressions, or production
monitoring the project actually needs. Local JSONL in `tmp/session_profiles/`
stays the baseline evidence source unless the lead explicitly changes that rule.
