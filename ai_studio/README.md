# AI Studio

`ai_studio/` is the target home for reviewed and refactored AI game-studio
pipeline modules.

This folder is not a dump of the current repo. Current files stay where they
are until a module is inspected, given an owner, cleaned up, and migrated with
compatibility shims where needed.

## Current Shape

- `tree.json` is the declarative source for the working architecture tree.
- `core_harness/` is the first physical migration target.
- Everything not reviewed yet stays visible through the `Not Refactored` node
  in the generated map.

Do not pre-create domain folders such as `assets/`, `tech/`, or `validation/`
before the corresponding module is reviewed. Create a folder only when the move
is deliberate and the module has a source contract.

## Task Routing

Load only the route that matches the current task:

- Repository contract and hard invariants: `AGENTS.md`.
- Current active game context: `GAME_PROJECT.md`.
- Agent workflow, context policy, Markdown shape, or multi-agent use:
  `docs/ai-pipeline/agent-workflow.md`.
- Done criteria, validation routing, product gates, or repeated failure stops:
  `docs/ai-pipeline/quality-validation.md`.
- Profiling, prototype closeout, visual/asset routing, or portable export:
  `docs/ai-pipeline/profiling-reuse.md`.
- Durable task state and task commands: `tasks/README.md` and
  `node tools/taskboard/cli.mjs context`.
- AI Studio architecture and refactor tree: `ai_studio/tree.json` and
  `docs/ai-pipeline/architecture-map.html`.
- Architecture map rebuild:
  `node tools/architecture_map/build_architecture_map.mjs`.

Detailed engine, validation, subagent, asset, release, and game-production
procedures belong in their own `ai_studio/` modules, docs, or skills. Do not add
that procedure here.

## Operating Rules

- Make one scoped change, then run the narrowest command that proves it.
- Do not call a slice done from one green gate; use the validation reference for
  acceptance rules.
- If the lead says a game/prototype is done, stopped, or only a test, stop game
  implementation and follow task/status instructions.

## Map Ownership

`docs/ai-pipeline/architecture-map.html` is generated output. Edit
`tree.json`, then rebuild:

```powershell
node tools/architecture_map/build_architecture_map.mjs
```

Visible node descriptions must be written in `tree.json`. The generator may use
`path` only to fill mechanical display data such as title and file link. It must
not invent the working tree's meaning from Markdown or the tool inventory.

## Migration Rule

Only move something into `ai_studio/` when it has:

- a clear domain owner;
- a source-of-truth README or contract;
- a public surface agents should use;
- known internal helpers;
- a validation path or an explicit reason why no validator applies.

## Migration Loop

1. Pick one current module from the architecture map.
2. Decide whether it belongs in `ai_studio/`, remains external, or should be
   deleted.
3. Create or update the target module README only after that decision.
4. Move reviewed source files, keeping old public commands as shims until every
   caller is updated.
5. Rebuild the architecture maps and run focused validators.
