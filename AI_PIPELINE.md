# AI Pipeline

Portable map for human-led AI game work. `AGENTS.md` owns repo-specific rules;
this file routes agents to the right workflow source.

## Load Only What Applies

- Agent behavior, context policy, Markdown shape, or multi-agent use:
  `docs/ai-pipeline/agent-workflow.md`
- Done criteria, validation routing, product gates, or repeated failure stops:
  `docs/ai-pipeline/quality-validation.md`
- Profiling, prototype closeout, visual/asset routing, or portable export:
  `docs/ai-pipeline/profiling-reuse.md`

Default context: `AGENTS.md`, `node tools/taskboard/cli.mjs context`, one
task/evidence file, one matching skill, and at most one deep reference above.

## Operating Rules

- Keep hot Markdown short; move procedure/history behind references, skills,
  tasks, or validators.
- Profiling is automatic (PostToolUse hook); read a session with `node tools/ai.mjs status`.
- Make one scoped change, then run the narrowest command that proves it.
- Do not call a playable/visual slice done from one green gate; product,
  game-loop, art-source, and technical gates stay separate.
- If strict/product fails twice for the same major reason, stop local polish and
  change path: architecture, tooling, source asset, reference, or lead
  acceptance.
- If the lead says a game/prototype is done, stopped, or only a test, stop game
  implementation and follow task/status instructions.

## Common Commands

```powershell
node tools/taskboard/cli.mjs context
node tools/ai.mjs validate
node tools/ai.mjs validate --review
node tools/ai.mjs validate --full
```

Use quick validation after normal pipeline edits; `--review` when intentionally
reviewing context/caps; `--full` for export, runtime, or deep asset gates.
