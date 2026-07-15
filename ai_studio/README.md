# AI Studio

The root command is a routing-only verification facade; domain behavior stays
in the owning Canvas, Taskboard, asset, workspace, feature, and template tools:

```powershell
node ai_studio/studio.mjs describe --json
node ai_studio/studio.mjs verify --changed
node ai_studio/studio.mjs verify --domain assets
node ai_studio/studio.mjs verify --full
```

`verify --changed` reads NUL-delimited Git porcelain and selects only shared
owner domains. Unknown shared paths fail ownership instead of silently running
everything. It never discovers or executes `games/<id>`; the root
`games/new_game.mjs` workflow is the exact exception. `verify --domain <id>`
runs one owner explicitly. `verify --full` runs up to three owner domains in
parallel; each domain batches Node tests with concurrency four and includes its
native, web/package, and platform release proof. Checks inside one domain remain
ordered. Blocked/setup exits `2`, executed failures exit `1`, and passes exit
`0`.

`ai_studio/` is the target home for reviewed AI game-studio pipeline modules.
Do not use it as a dump: move a module here only after it has an owner, contract,
public surface, internals, and validation path.

## Current Shape

- `config.mjs`: neutral loader for committed Studio defaults plus the ignored
  local override; key, environment, default, and path interpretation stays in
  the owning module.
- `tree.json`: declarative source for the working architecture tree.
- `studio_shell/`: one browser entry point, shared collapsible left navigation,
  theme, and unified local server for AI Studio surfaces.
- `architecture_map/`: live architecture map renderer and validation report for
  unmapped files.
- `assets/`: reviewed asset-facing modules, starting with Asset Viewer.
- `workspace/`: local/private workspace mounts and privacy preflight for nested
  game repositories under `games/<id>`.
- `runtime_automation/`: reviewed local DevAPI, capture, screenshot health, and
  UI readability proof helpers.
- `core_harness/`: reviewed core routing and agent harness docs.
- `core_harness/workflow/orchestration/`: reviewed bounded orchestration rule.
- `core_harness/agent_surfaces/`: generated Codex/Claude compatibility surfaces.
- `core_harness/profiling/`: passive hook diagnostics, canonical Codex session
  status, and chat-session reflection ownership.
- `dev_environment/`: local developer-environment generators such as VS Code
  task and launch file generation.
- `quality/`: reviewed small quality rules for selecting evidence and checks.
- `taskboard/`: reviewed durable task state module.

Create domain folders such as `assets/` or `tech/` only when that module is
reviewed and ready to move.

## Task Routing

Load only the route that matches the current task:

- Repository contract and hard invariants: `AGENTS.md`.
- Current game context: `games/<game-id>/`.
- New game folder creation:
  `node games/new_game.mjs --id <game-id> --visibility public|private --require-visibility`.
- VS Code task/launch regeneration:
  `node ai_studio/dev_environment/vscode_projects.mjs`.
- Private/local game mounts and leak preflight:
  `ai_studio/workspace/README.md` and
  `node ai_studio/workspace/games.mjs preflight --json`.
- Agent shell hooks are generated from
  `ai_studio/core_harness/agent_surfaces/hooks_sync.mjs`; private game creation
  installs the parent repository's pre-commit privacy preflight.
- Agent workflow, context policy, Markdown shape, or multi-agent use:
  `ai_studio/core_harness/workflow/README.md`.
- Independent bounded work where delegation reduces latency, context, or risk:
  `ai_studio/core_harness/workflow/orchestration/README.md`.
- Core entrypoint/doc references:
  `node ai_studio/core_harness/validation/doc_reference_check.mjs`.
- Generated Codex/Claude skills and hook surfaces:
  `node ai_studio/core_harness/agent_surfaces/sync.mjs --check`.
- Quality rule selection, player-facing evidence, clarity checks, and repeated failure
  stops: `ai_studio/quality/README.md`.
- Profiling, prototype closeout, or visual/asset routing:
  `ai_studio/core_harness/profiling/README.md`.
- Durable task state and task commands: `ai_studio/taskboard/README.md` and
  `node ai_studio/taskboard/cli.mjs context --json`.
- Runtime evidence, DevAPI client, screenshots, pixel health, and UI
  readability proof: `ai_studio/runtime_automation/README.md`.
- Reusable schema-first game-state feature:
  `features/game-state/README.md`.
- AI Studio architecture and refactor tree: `ai_studio/tree.json` and
  `ai_studio/architecture_map/README.md`.
- AI Studio browser entry point:
  `powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open`.
- Asset browsing and review:
  `http://127.0.0.1:8765/asset_viewer/`.
- Multi-image canvas, region detect/slice, and composite export:
  `http://127.0.0.1:8765/canvas`.
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

`ai_studio/tree.json` is the single architecture source. It lists durable module
owners and boundaries, not their implementation files or tests. The map page is
a renderer: open it through the local server so it can fetch the same JSON data.

```powershell
node ai_studio/architecture_map/validate_map.mjs
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ai_studio/studio_shell/start_site_windows.ps1 -Restart -Open
```

Open `http://127.0.0.1:8765/`. The map page reads the tree from
`/api/architecture-tree` and a live report from `/api/architecture-validation`;
the report is generated on demand and is not committed. Scanning is validation
only: new files or shallow workspace folders appear in the report until a human
maps, ignores, moves, or deletes them.

## Module Intake Rule

Move a module here only with: owner, README/contract, public surface, internal
helpers, and validation or an explicit no-validator reason.

## Review Loop

1. Pick one module from the map.
2. Decide: move to `ai_studio/`, keep external, or delete.
3. Create/update the module README after the decision.
4. Move reviewed source files and update callers.
5. Refresh the map validation report and run focused validators.
