# AI Studio

`ai_studio/` is the target home for reviewed AI game-studio pipeline modules.
Do not use it as a dump: move a module here only after it has an owner, contract,
public surface, internals, and validation path.

## Current Shape

- `tree.json`: declarative source for the working architecture tree.
- `core_harness/`: reviewed core routing and agent harness docs.
- `core_harness/orchestration/`: reviewed early split rule for broad read-heavy
  subagent work.
- `taskboard/`: reviewed durable task state module.
- `Not Refactored`: map node for everything not reviewed yet.

Create domain folders such as `assets/`, `tech/`, or `validation/` only when
that module is reviewed and ready to move.

## Task Routing

Load only the route that matches the current task:

- Repository contract and hard invariants: `AGENTS.md`.
- Current active game context: `GAME_PROJECT.md`.
- Agent workflow, context policy, Markdown shape, or multi-agent use:
  `ai_studio/core_harness/workflow/README.md`.
- Broad read-heavy work that should be split before loading too much context:
  `ai_studio/core_harness/orchestration/README.md` and
  `node ai_studio/core_harness/orchestration/cli.mjs --help`.
- Stale Markdown/tool references and retired command routes:
  `node ai_studio/core_harness/validation/doc_reference_check.mjs`.
- Done criteria, validation routing, product gates, or repeated failure stops:
  `docs/ai-pipeline/quality-validation.md`.
- Profiling, prototype closeout, visual/asset routing, or portable export:
  `docs/ai-pipeline/profiling-reuse.md`.
- Durable task state and task commands: `ai_studio/taskboard/README.md` and
  `node ai_studio/taskboard/cli.mjs context`.
- AI Studio architecture and refactor tree: `ai_studio/tree.json` and
  `docs/ai-pipeline/architecture-map.html`.
- Architecture map rebuild:
  `node tools/architecture_map/build_architecture_map.mjs`.

Detailed procedures belong in owned modules, docs, or skills, not here.

## Operating Rules

- Make one scoped change, then run the narrowest command that proves it.
- Do not call a slice done from one green gate; use the validation reference for
  acceptance rules.
- If the lead says a game/prototype is done, stopped, or only a test, stop game
  implementation and follow task/status instructions.

## Map Ownership

`docs/ai-pipeline/architecture-map.html` is generated. Edit `tree.json`, then:

```powershell
node tools/architecture_map/build_architecture_map.mjs
```

Node descriptions live in `tree.json`; the renderer only fills mechanical file
data such as title and link.

## Migration Rule

Move a module here only with: owner, README/contract, public surface, internal
helpers, and validation or an explicit no-validator reason.

## Migration Loop

1. Pick one module from the map.
2. Decide: move to `ai_studio/`, keep external, or delete.
3. Create/update the module README after the decision.
4. Move reviewed source files and update callers.
5. Rebuild maps and run focused validators.
