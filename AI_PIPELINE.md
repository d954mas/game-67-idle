# AI Pipeline

How the human lead and AI agents work together in this repository, and which
parts move to the next game project. `AGENTS.md` holds project-specific rules;
this file holds the reusable process.

## Roles

- **Lead (user):** sets high-level direction, gives taste and feedback,
  accepts or rejects gates. Does not write detailed specs.
- **Agents:** ask clarifying questions, research before acting, refine and
  decompose requests, implement in small playable slices, and prove results
  with evidence (commands, screenshots, scenario runs).

## Agent entry points

| Agent CLI | Rules file | Skills |
|---|---|---|
| Codex | `AGENTS.md` | `.codex/skills/` (canonical) |
| Claude Code | `CLAUDE.md` (imports `AGENTS.md`) | `.claude/skills/` (generated pointers) |

`.claude/skills/` is generated — never edit it by hand. After adding or
renaming a skill in `.codex/skills/`, run:

```powershell
node tools/skills_sync.mjs
```

## Flow: idea to shipped

| Stage | What happens | Skill / tool |
|---|---|---|
| 1. Capture | Every stated idea becomes a task; deferred work is never lost | `task-manager`, `tasks/` store, `tools/taskboard/` |
| 2. Refine | Questions to the lead + research; `idea` -> `backlog` with checkable done-when | `task-manager` |
| 3. Design | Concept, GDD, refs, visual proof, data contracts | `primary-gdd-pipeline`, `game-design-steward`, `gamedesing/` |
| 4. Implement | Smallest playable slice; schema-first state; explicit asset paths | `game-feature-iteration`, `game-state-management`, `game-asset-pipeline` |
| 5. Validate | Agent drives the running game and captures evidence | `game-runtime-automation` (DevAPI), `game-visual-qa` |
| 6. Release | Explicit build/serve/package tasks | `game-build-release` |
| 7. Learn | On failure, name the missing component (instruction, source of truth, tool, validator, eval, recovery path) and encode the fix there — not only in prompts; lessons -> `gamedesing/knowledge/` | `agents-best-practices`, all skills |

## Conventions that make this fast

- **One source of truth per thing.** Skills: `.codex/skills/`. Task/status
  conventions: `tasks/README.md`. State shape: `state/*.schema.json`.
  Generated files are regenerated, not edited.
- **Evidence or it did not happen.** A task is `done` only with ticked
  `## Done when` boxes and an evidence line in `## Log`.
- **Small slices.** Prefer one playable iteration over broad speculative work.
- Scratch-vs-durable paths and platform validation order are project rules in
  `AGENTS.md`, not repeated here.

## Multi-agent work packets

Use multiple agents only when the user asks for parallel/delegated work or when
the current environment explicitly supports it. Prefer one linear agent loop for
small or tightly coupled work.

Delegate only bounded sidecar work that can run in parallel without blocking the
main integration path. Do not delegate the immediate critical-path task when the
next local action depends on its result.

Each work packet must state:

- role and objective
- owned files, subsystem, or responsibility
- expected artifact or answer
- validation/evidence to return
- out-of-scope boundaries
- warning not to revert or overwrite other active work

Split implementation packets by disjoint write scope. Split verification packets
by independent risk: gameplay, visual/readability, build/release, data/state, or
tooling. A verifier should report findings and evidence, not silently rewrite
the owner packet.

The lead/integrator keeps responsibility for:

- selecting packet boundaries
- reviewing returned changes or findings
- reconciling conflicts
- running the final evidence gate
- updating task logs and `STATUS.md` when the gate or next priorities change

## Tool and validation discipline

Use tools to reduce uncertainty, not to collect context indiscriminately.

Default order for substantial work:

1. Load the minimal current context from `tasks/README.md`.
2. Inspect only the files needed for the selected scope.
3. Prefer scoped search before repo-wide search.
4. Make the smallest coherent change.
5. Run the narrowest validation that proves the change.
6. Escalate to broader validation only when the scope or risk requires it.

For pipeline/tooling changes, prove both the current repository and the portable
export path when the change affects future projects. For game/runtime changes,
prove the specific playable or visual behavior, not only that the build
compiles.

Do not use old task logs, generated files, build outputs, or archived design
handoffs as current truth unless they are linked from `STATUS.md`, an active
task, or fresh validation evidence.

## Reuse in a new project

Export the portable base into a fresh repository:

```powershell
node tools/bootstrap/export_base.mjs --target C:\projects\new-game
```

Portable (copied by the exporter):

- `.codex/skills/` — all skills are written engine-agnostic: they discover
  local conventions instead of assuming this repo's layout.
- `tools/skills_sync.mjs`, `tools/taskboard/` — skill mirroring and the task
  store (board UI + CLI).
- `gamedesing/knowledge/` — accumulated design lessons.
- `AI_PIPELINE.md`, `tasks/README.md`, starter `tasks/STATUS.md`, starter
  `AGENTS.md` / `CLAUDE.md`.

Stays behind (game-specific): `src/`, `state/` schemas, `gamedesing/<concept>/`
docs and data, `tools/devapi/` scenario scripts, build presets. The DevAPI
*pattern* travels via `.codex/skills/game-runtime-automation/references/devapi-pattern.md`;
each game re-implements the bridge against its own engine setup.

After exporting: fill the `## Project` and `## Direction` sections of the new
`AGENTS.md`, then start at stage 1 with the first ideas as tasks.
