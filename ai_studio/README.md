# AI Studio

`ai_studio/` is the target home for reviewed AI game-studio pipeline modules.
Do not use it as a dump: move a module here only after it has an owner, contract,
public surface, internals, and validation path.

## Current Shape

- `tree.json`: declarative source for the working architecture tree.
- `studio_shell/`: one browser entry point, shared collapsible left navigation,
  theme, and unified local server for AI Studio surfaces.
- `architecture_map/`: live architecture map renderer and validation report for
  unmapped files.
- `assets/`: reviewed asset-facing modules, starting with Asset Viewer.
- `game_project/`: reviewed active-game routing and prototype kickoff/context
  scaffolding.
- `core_harness/`: reviewed core routing and agent harness docs.
- `core_harness/workflow/orchestration/`: reviewed early split rule for broad
  read-heavy subagent work.
- `core_harness/agent_surfaces/`: generated Codex/Claude compatibility surfaces.
- `core_harness/profiling/`: passive profiling, Codex recovery, session status,
  and chat-session reflection ownership.
- `quality/`: reviewed small quality rules for selecting evidence and checks.
- `taskboard/`: reviewed durable task state module.
- `Not Refactored`: map node for everything not reviewed yet.

Create domain folders such as `assets/` or `tech/` only when that module is
reviewed and ready to move.

## Task Routing

Load only the route that matches the current task:

- Repository contract and hard invariants: `AGENTS.md`.
- Current active game context and prototype kickoff:
  `ai_studio/game_project/README.md` and `GAME_PROJECT.md`.
- Agent workflow, context policy, Markdown shape, or multi-agent use:
  `ai_studio/core_harness/workflow/README.md`.
- Broad read-heavy work that should be split before loading too much context:
  `ai_studio/core_harness/workflow/orchestration/README.md`.
- Core entrypoint/doc references and retired Core-era command routes:
  `node ai_studio/core_harness/validation/doc_reference_check.mjs`.
- Generated Codex/Claude skills and hook surfaces:
  `node ai_studio/core_harness/agent_surfaces/sync.mjs --check`.
- Quality rule selection, player-facing evidence, clarity checks, and repeated failure
  stops: `ai_studio/quality/README.md`.
- Profiling, prototype closeout, visual/asset routing, or portable export:
  `ai_studio/core_harness/profiling/README.md`.
- Durable task state and task commands: `ai_studio/taskboard/README.md` and
  `node ai_studio/taskboard/cli.mjs context --json`.
- AI Studio architecture and refactor tree: `ai_studio/tree.json` and
  `ai_studio/architecture_map/README.md`.
- AI Studio browser entry point:
  `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open`.
- Asset browsing and review:
  `http://127.0.0.1:8765/viewer/`.
- Architecture map validation report:
  `node ai_studio/architecture_map/validate_map.mjs`.

Detailed procedures belong in owned modules, docs, or skills, not here.

## Operating Rules

- Make one scoped change, then run the narrowest command that proves it.
- Do not call a slice done from one green gate; use the validation reference for
  acceptance rules.
- If the lead says a game/prototype is done, stopped, or only a test, stop game
  implementation and follow task/status instructions.

## Map Ownership

`ai_studio/tree.json` is the architecture source. The map page is a renderer:
open it through the local server so it can fetch JSON data.

```powershell
node ai_studio/architecture_map/validate_map.mjs
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

Open `http://127.0.0.1:8765/`. The map page reads `tree.json` and
`architecture_map/validation-report.json`.
Scanning is validation only: new files appear in the report until a human maps,
ignores, moves, or deletes them.

## Browser Surfaces

Use `kind: "surface"` for user-facing browser entries. Use `kind: "module"` for
domain ownership, data, APIs, and contracts. `studio_shell/` hosts surfaces; it
does not own Architecture Map or Taskboard domain logic.

## Migration Rule

Move a module here only with: owner, README/contract, public surface, internal
helpers, and validation or an explicit no-validator reason.

## Migration Loop

1. Pick one module from the map.
2. Decide: move to `ai_studio/`, keep external, or delete.
3. Create/update the module README after the decision.
4. Move reviewed source files and update callers.
5. Refresh the map validation report and run focused validators.
