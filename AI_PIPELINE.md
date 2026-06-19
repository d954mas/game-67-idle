# AI Pipeline

Reusable process for human-led, AI-assisted game development. `AGENTS.md` holds
repo-specific rules; this file holds portable workflow policy.

## Roles

- Lead: sets direction, taste, priority, and acceptance. The lead does not need
  to write detailed specs.
- Agent: turns intent into scoped work, asks only necessary questions, researches
  when references matter, implements small slices, and proves results with
  evidence.

## Agent-Facing Docs

Keep hot Markdown short and stable.

- `AGENTS.md`: project map, hard rules, validation defaults.
- `AI_PIPELINE.md`: reusable workflow and context policy.
- `tasks/README.md`: task store commands and lifecycle.
- `.codex/skills/*/SKILL.md`: one focused procedure per task type.
- Skill `references/` and `gamedesign/knowledge/`: detailed method, examples,
  research, and historical lessons loaded only when needed.

Do not duplicate the same rule in every file. Put each rule where the agent
first needs it, then link to the deeper source. Delete stale anecdotes once the
lesson is encoded as a tool, validator, task rule, or skill.

Write agent Markdown as:

- decision rule first;
- shortest required command;
- source-of-truth path;
- stop condition;
- no long chat history, no broad checklists in hot context.

## Context Policy

Default context for substantial work:

1. `AGENTS.md`
2. `node tools/taskboard/cli.mjs context`
3. one relevant task or evidence file
4. one matching skill

Prefer scoped search and compact command output over whole-file dumps. Use
archives, old task logs, generated artifacts, and broad design folders only when
the current task links to them or the user asks for review/history.

Keep stable context byte-stable inside a session when possible. Put volatile
session facts in tasks, status, evidence files, or final reports rather than
rewriting hot instruction files repeatedly.

## Work Loop

1. Interpret the user request into one working scope.
2. Select or create a task only when durable tracking is useful.
3. For non-trivial work, start/passively record profiling scope or state why it
   is unavailable.
4. Read only files needed for the selected scope.
5. Make the smallest coherent change.
6. Run the narrowest validation that proves the change.
7. Record evidence in the task/status/final response when the work changes
   project state.

Use multiple agents only for independent side work with clear ownership,
expected artifact, evidence, and out-of-scope boundaries. The main integrator
keeps final responsibility for merge, validation, and status.

## Quality Gates

Gates are separate verdicts:

- Product/readability: can a new player understand and operate the screen?
- Game-loop/fun: is there a hook, repeatable loop, reward, and next-5-minutes
  reason to continue?
- Art-source/assets: are runtime assets real, traceable, and appropriate for
  the target?
- Technical/build: does the changed runtime/tooling actually work?

Do not call a slice done from one green gate. Builds, probes, crop audits, and
manifests support the verdict; they do not replace player-facing judgment.

When a strict/product gate fails twice for the same major reason, stop the local
polish loop. Create or link the different path: architecture, tooling, source
asset, reference, or explicit lead acceptance. This is enforced by:

```powershell
node tools/product_gate/repeated_failure_guard.mjs
```

`node tools/pipeline_validate.mjs` runs that guard in quick mode.

## Validation Defaults

- Task/status docs: `node tools/taskboard/cli.mjs validate`
- Skill/process changes: `node tools/skills_eval.mjs`
- Product gate changes: `node --test tools/product_gate/test.mjs`
- Taskboard changes: `node --test tools/taskboard/test.mjs`
- AI facade/profile changes: `node --test tools/ai.test.mjs` and focused
  `tools/ai_profile` tests
- Reusable pipeline: `node tools/pipeline_validate.mjs`
- Portable/export/runtime gates: `node tools/pipeline_validate.mjs --full`

Escalate validation only when the changed behavior or export path requires it.
Full portable validation is a final/broad gate, not the default after every
small edit.

## Profiling

Profiling is passive telemetry for finding repeated failures, slow commands,
large context reads, and long gaps. It should not become another project.

Use `node tools/ai.mjs` for normal work: `start`, `run`, `context`,
`checkpoint`, `status`, and `reflect`.

Deep import/status/retrospective work is opt-in for AI workflow review or
profiler fixes. Do not commit raw telemetry from `tmp/session_profiles/`; commit
only durable lessons, rules, tools, or task changes.

## Assets And Visual Work

Use skills instead of copying asset procedure into hot docs. The common route is
`primary-gdd-pipeline` -> `game-visual-art-direction` ->
`generated-game-ui-assets` / `game-asset-pipeline` ->
`game-feature-iteration` / `game-runtime-automation`, loading only the skill
that matches the current task.

Generated/free art is allowed only as runtime-ready art that reaches the visual
target. Debug/procedural placeholders prove geometry; they do not satisfy a
final-art claim unless explicitly recorded as debug debt.

## Prototype Pause Or Close

When the lead says a prototype/game is done, stopped, or only a test, stop game
implementation. Then follow the latest explicit instruction for task/status
disposition. Do not silently archive, drop, or rewrite active work unless that
is part of the requested pipeline cleanup.

Preserve evidence historically. Promote only reusable lessons into pipeline
docs/skills/tools.

## Reuse In A New Project

The portable AI workflow is exported with:

```powershell
node tools/bootstrap/export_base.mjs --target C:\projects\new-game
```

Portable by default: agent skills, taskboard, `tools/ai.mjs`,
`tools/pipeline_validate.mjs`, product gate tools, game-context tools, generated
art job scaffolding, reusable design knowledge, and starter agent/task files.
The exact allowlist lives in `tools/bootstrap/export_base.mjs`.

Runtime seed files (`src/`, `state/`, DevAPI, CMake presets) move only when the
exporter/runtime template explicitly supports that target.
