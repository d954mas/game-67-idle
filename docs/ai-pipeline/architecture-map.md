# AI Pipeline Architecture Map

This is the current architecture and decomposition map for the shared AI game
development pipeline. It is a working source of truth for agents that need to
understand the repo before changing tools, skills, tasks, assets, or game
template code.

The root is not a game. The root is the studio pipeline: agent-facing policy,
workflow state, reusable skills, executable tools, design knowledge, asset
handling, validation, and the game starter template.

`ai_studio/` is the target home for the cleaned architecture. Current files do
not move there automatically. A module enters `ai_studio/` only after it is
reviewed, refactored, assigned to an owner, and given a compatibility plan for
old public paths.

Current source layout for the map:

- `ai_studio/tree.json` owns the visible tree: node ids, hierarchy, manual
  descriptions, roles, tags, and explicit child order.
- `tools/architecture_map/build_architecture_map.mjs` is the generator. It
  reads `tree.json`, scans selected Markdown and tools, then writes the HTML.
- `docs/ai-pipeline/architecture-map.html` is generated renderer output for the
  working refactor map. Do not edit it by hand.
- `docs/ai-pipeline/architecture-map-full.html` is generated renderer output for
  the full current inventory.

"Enrichment" is limited to mechanical display data. If a tree node has a
`path`, the generator can fill title and file link data from Markdown or the
tool inventory. It does not infer hierarchy or descriptions for the working
tree. Every visible node description must be written in `tree.json`; missing
descriptions fail the map build.

## Visual Graphs

There are two generated visual reports:

- `docs/ai-pipeline/architecture-map.html` is the working Core-first refactor
  map. Start there. Core means harness, agent-facing contract, `AGENTS.md`,
  short routing, and the smallest public entry points needed to start work.
- `docs/ai-pipeline/architecture-map-full.html` is the full inventory graph.
  Use it when you need every Markdown/tool node, not while making the first
  refactor decisions.

Both are generated, not hand-maintained. Rebuild them after changing
architecture docs, skills, or tools:

```powershell
node tools/architecture_map/build_architecture_map.mjs
```

The generator reads selected Markdown sources, `.codex/skills/*/SKILL.md`, and
current files under `tools/`. It extracts each Markdown node's title, first
plain-language paragraph, local Markdown links, and H2/H3 headings. The full
HTML groups those nodes by architecture module and adds a utility catalog for
the same modules. The Core map compresses that inventory into a Core-first
refactor view.

Core map has two layers:

- the `AI Studio Explorer` is the primary working view. It is a card-tree:
  clicking a card replaces the board with that card's children, and `Back`
  returns one level up;
- the card-tree starts at `ai_studio/`, then enters reviewed nodes such as
  `Core Harness`, `Architecture & Migration`, `Not Refactored`, and concrete
  files/tools/contracts in the same board;
- the graph below the explorer is secondary. Use it to inspect cross-module
  relationships, pan/zoom, and reposition modules.

Core map visual language:

- color means owner or group;
- icon means entity type, such as doc, agent contract, route, tool, guard,
  sync, group, or removal candidate;
- tag means refactor decision or status, such as `core`, `public-api`,
  `source-of-truth`, `move-out`, `needs-owner`, or `delete-candidate`;
- entering a card-tree node moves one level deeper: `ai_studio/` -> module or
  backlog node -> file/tool/contract details;
- graph expansion is secondary and only mirrors a compact subset inside the
  module card.

Core map interaction model:

- click an explorer card to replace the board with that card's child tree;
- use `Back` to return one level up;
- click a graph module to load the same module into the explorer;
- mouse wheel zooms around the cursor;
- dragging empty canvas pans the graph;
- dragging a module moves that module without selecting/opening it;
- graph expansion must still show concrete files/tools inside the graph module
  card, not as detached abstract groups.

Use the graph as the refactoring baseline:

1. Clean Core first: remove domain procedure from Core docs and replace it
   with links/routes to owned modules.
2. If a file or workflow is not visible in the full graph, decide whether to
   catalog it, move it under an existing owner, or remove it.
3. If a tool is used by agents, give it a clear public facade and a validation
   route.
4. If a tool is just support code, keep it behind a module owner instead of
   exposing it as agent procedure.
5. If two modules depend on each other, make that contract explicit in docs or
   a validator before refactoring.

## Research Takeaways

External references point to the same architecture shape:

- Agent systems should start simple and use composable patterns before adding
  complex multi-agent orchestration. Anthropic separates fixed workflows from
  open-ended agents and recommends adding complexity only when it measurably
  helps: https://www.anthropic.com/engineering/building-effective-agents
- Tools are an agent-computer interface. A good tool has clear names, narrow
  inputs, compact/paginated outputs, helpful errors, and test coverage:
  https://www.anthropic.com/engineering/writing-tools-for-agents
- Repo-local knowledge should be a map plus structured sources of truth, not a
  giant instruction blob. OpenAI's Codex harness writeup frames the engineer's
  job as making missing capabilities legible and enforceable, then encoding
  boundaries through docs, linters, tests, and tools:
  https://openai.com/index/harness-engineering/
- MCP's useful mental model is host/client/server plus three server-side
  primitives: resources, prompts, and tools. It also makes consent, boundaries,
  and tool safety explicit:
  https://modelcontextprotocol.io/specification/2025-11-25
- Raylib is a useful analogy for public surface discipline: small named modules,
  examples as learning material, a cheatsheet/public API, and tools beside the
  runtime instead of hidden inside it: https://github.com/raysan5/raylib
- LLVM is a useful analogy for a toolchain repo: many products under one
  umbrella, but each component stays modular and reusable, with tools,
  libraries, headers, tests, and docs kept visible:
  https://github.com/llvm/llvm-project

Local references point the same way:

- `external/neotolis-engine/README.md` shows a module set, an offline builder,
  examples, tests, scripts, and docs around a stable public API.
- `external/neotolis-engine/AGENTS.md` makes architecture enforceable with
  module boundaries, platform abstraction, builder validation, asserts, and
  pre-commit checks.
- `template/CONVENTIONS.md` applies the same idea to games: thin `main.c`,
  one system per file, world as source of truth, engine APIs first, no god-file.

The lesson for this repo: the pipeline should be a set of agent-usable tools and
skills with stable public entry points, not a pile of scripts and instructions.

## Vocabulary

- Lead agent: the current main agent. Owns scope, integration, validation,
  task/status changes, hot docs, commits, and the final report.
- Subagent or worker: a bounded delegated context. It receives a packet, works
  in an isolated scope, and returns a compact handoff. It does not own
  acceptance.
- Skill: reusable procedural knowledge loaded by trigger. Skills route an agent
  to the right references and commands.
- Tool: executable capability under `tools/`, usually a CLI or library used by
  a CLI. Tools should be narrow, testable, and return actionable output.
- Validator or gate: a tool that enforces a mechanical invariant or records
  acceptance evidence.
- Hot doc: a small always-nearby map, such as `AGENTS.md`, `ai_studio/README.md`, or
  `ai_studio/taskboard/README.md`.
- Durable state: task files, project wiki files, asset catalog records,
  generated manifests, gate records, and profiler logs. The prompt is not the
  database.

## Target Layering

```text
Lead/user request
  -> AGENTS.md and ai_studio/README.md route the work
  -> one matching skill loads focused procedure
  -> taskboard/project wiki supplies durable state when needed
  -> the owning domain CLI performs action/validation
  -> domain libraries and validators enforce contracts
  -> evidence is written to task/project/gate artifacts only when durable
```

Preferred implementation pattern for executable domains:

```text
tools/<domain>/
  README.md          optional route map when the domain is broad
  cli-or-tool.mjs    small command surface, stable for agents
  lib.mjs            pure or mostly pure domain logic
  *.test.mjs         contract tests beside the domain
  public artifacts   JSON/Markdown outputs with stable schema
```

Use Python where image/file formats or existing libraries make it the right
choice; keep the command boundary explicit and the output compact.

## Public Agent Surface

These are the main stable entry points an agent should know first.

| Surface | Role | Notes |
|---|---|---|
| `AGENTS.md` | Repo policy and hard invariants | Root says there is no active game concept; games are folders copied from `template/`. |
| `ai_studio/README.md` | Portable workflow map | Points to deeper docs; should stay short. |
| `ai_studio/taskboard/README.md` | Task store map | Commands, lifecycle, minimal context, done rules. |
| `.codex/skills/*/SKILL.md` | Workflow routing | One focused procedure per task type; details go to references. |
| `ai_studio/taskboard/cli.mjs` | Task and packet CLI | Owns task CRUD, context, subagent packet templates/checks, orchestration bootstrap/check. |
| `tools/pipeline_validate.mjs` | Validation orchestrator | Quick/review/full validation over tools, docs, skills, taskboard, gates, export. |
| tools/bootstrap/new_game.mjs | New game folder bootstrap | Copies `template/` into a game folder. |
| `tools/game_context/new_prototype.mjs` | New prototype kickoff | Creates project wiki/task/status skeleton for a selected concept. |
| `tools/assets/source/find_assets.mjs` | Asset source-first search | Shared library and free-source decision record. |
| `tools/asset_review/*` | Asset review/pull/promote | Review galleries, pull assets, promote selected records. |
| `tools/product_gate/*` | Product/readability gates | Visual, material, repeated failure, close-slice, layout checks. |

## Domain Map

| Domain | Source of truth | Main tools | Main skills | Durable outputs |
|---|---|---|---|---|
| AI Studio target structure | `ai_studio/README.md`, `ai_studio/tree.json`, `ai_studio/core_harness/README.md` | tools/architecture_map/build_architecture_map.mjs | `ai-pipeline-maintenance` | Reviewed modules under `ai_studio/`, generated map HTML |
| Pipeline policy and context | `AGENTS.md`, `ai_studio/README.md`, `docs/ai-pipeline/` | `tools/context_budget.mjs`, `tools/doc_reference_check.mjs`, `tools/pipeline_validate.mjs` | `ai-pipeline-maintenance` | Updated docs, validation output |
| Task state and orchestration | `ai_studio/taskboard/README.md`, `tasks/STATUS.md`, `tasks/active/`, `tasks/epics/` | `ai_studio/taskboard/cli.mjs`, `ai_studio/taskboard/server.mjs` | `task-manager`, `ai-pipeline-maintenance` | Task files, status index, packet handoffs |
| Passive profiling and feedback | `docs/ai-pipeline/profiling-reuse.md` | `tools/ai_profile/*`, tools/hooks_sync.mjs | `chat-session-reflection`, `ai-pipeline-maintenance` | `tmp/session_profiles/` raw logs, promoted lessons |
| Game concept and GDD | `gamedesign/projects/<game-id>/`, `gamedesign/knowledge/` | `tools/game_context/new_prototype.mjs`, `tools/game_context/iteration_context.mjs` | `primary-gdd-pipeline`, `design-source-knowledge` | GDD, project wiki, core loop, reference notes |
| Reusable design knowledge | `gamedesign/knowledge/`, `gamedesign/sources/` | none as a single facade yet | `design-source-knowledge`, `primary-gdd-pipeline` | Source notes, promoted reusable knowledge |
| Game starter template | `template/`, tools/bootstrap/TEMPLATE.md, `template/CONVENTIONS.md` | tools/bootstrap/new_game.mjs, `tools/bootstrap/export_base.mjs` | `game-feature-iteration`, `game-state-management` | New game folders, copied starter shell |
| Runtime implementation | Per-game folder copied from `template/` | Native build commands, `tools/devapi/*`, `tools/state_codegen/*` | `game-feature-iteration`, `game-runtime-automation`, `game-state-management` | Source modules, generated state, screenshots, save fixtures |
| Asset sourcing and library | Shared YandexDisk library, project asset folders | `tools/assets/source/*`, `tools/assets/intake/*`, `tools/asset_review/*`, `tools/assets/restricted.mjs` | `game-asset-pipeline`, `game-asset-prep`, `game-3d-models`, `game-texture-generation` | Catalog/license records, previews, project-local runtime copies |
| Generated raster/UI assets | Art job files under project wiki and runtime assets | `tools/assets/job/*`, `tools/assets/cutout/*`, `tools/assets/pack/*`, `tools/assets/crop/*`, `tools/assets/assemble/*` | `generated-game-ui-assets`, `delegated-image-generation`, `game-visual-art-direction` | Source sheets, prompt packets, crop manifests, atlases |
| Product and visual gates | `docs/ai-pipeline/quality-validation.md`, per-project reviews | `tools/product_gate/*`, tools/visual_invariant_guard.mjs | `game-visual-art-direction`, `game-runtime-automation`, `game-feature-iteration` | Product-read gates, critique JSON, rejection locks |
| Portable export and sync | `tools/bootstrap/export_base.mjs`, tools/sync.mjs | `tools/skills_sync.mjs`, tools/hooks_sync.mjs, `tools/bootstrap/export_base.mjs` | `ai-pipeline-maintenance` | Exported pipeline base, synced `.claude` surfaces |
| External engine and extensions | `external/neotolis-engine/`, `extensions/` | Engine build/test scripts, extension probes | `game-feature-iteration`, `game-runtime-automation` | Engine-facing runtime code, extension modules |

## Tool Directory Map

| Path | Responsibility | Current shape |
|---|---|---|
| `tools/lib/` | Shared small utilities | CLI failure helper, JSON, paths, licenses, MIME, hash, asset catalog, validation flags. |
| `ai_studio/taskboard/` | Markdown task store and orchestration packet support | CLI, server UI, lib, tests, public web UI. |
| `tools/ai_profile/` | Passive tool/session profiler | Hook recorders, status, Codex import, agent rollup, tests. |
| `tools/bootstrap/` | Pipeline/game/template export and copy model | `new_game`, `export_base`, template path ownership, tests. |
| `tools/game_context/` | Active game/prototype context gates | Kickoff skeleton, iteration context, workflow guard, tests. |
| `tools/product_gate/` | Product/readability/art/repeated-failure gates | Review, visual rejection, material floor, responsive layout, close slice, critic runner. |
| `tools/assets/source/` | Source-first asset lookup/import | Library search, Poly Pizza import, free-source metadata. |
| `tools/assets/intake/` | Asset intake, manual/downloaded records, preprocessing audits | Library bootstrap, download/manual intake, archive ingest, accept incoming, source sheet/tileable audits. |
| `tools/assets/job/` | Generated art job contracts | New art jobs, prompt packets, generation records, strict asset boundary audits. |
| `tools/assets/cutout/` | Source-sheet alpha/cutout preprocessing | Key matte, route cutout, dual-plate alpha gates. |
| `tools/assets/pack/` | UI atlas and labels | Build/audit UI atlas packs and review labels. |
| `tools/assets/crop/` | Crop planning | Runtime crop planning from intake. |
| `tools/assets/assemble/` | Runtime asset assembly | Build runtime assets from crop plans. |
| `tools/assets/audit/` | Asset policy guards | Restricted asset leak guard and allowlist. |
| `tools/asset_review/` | Human/agent asset browser workflow | Build gallery, serve/record gallery, pull, promote. |
| `tools/devapi/` | Runtime automation helpers | DevAPI client/CLI, screenshots, screen recording, pixel/readability/state capture. |
| `tools/state_codegen/` | Schema-first game state codegen | Python generator, C template, tests. |
| `tools/requirements/` | Python dependency sets | Full AI pipeline requirements. |
| `scripts/` | Small host scripts | Web serving helper. |

Current warning: `tools/blockside-heat/` appears to contain only ignored Python
cache output in this checkout. Treat it as candidate cleanup, not an active
domain, unless a closed prototype tag or task proves otherwise.

## Skill Map

| Skill | Ownership |
|---|---|
| `ai-pipeline-maintenance` | Reusable AI pipeline cleanup, context budgets, validators, taskboard/profiling/product-gate workflow, export, skill placement. |
| `task-manager` | Task capture, decomposition, taskboard lifecycle, epics, status, done evidence. |
| `chat-session-reflection` | Long session retrospectives, profiler interpretation, repeated friction, improvement backlog. |
| `primary-gdd-pipeline` | New/revised game concept, GDD, visual GDD, core loop, art bible, implementation handoff. |
| `design-source-knowledge` | Reusable design source notes and promotion into `gamedesign/knowledge/`. |
| `game-feature-iteration` | Playable feature implementation/debug/validation, native slices, release/package/CI. |
| `game-runtime-automation` | DevAPI, command bus, UI tree/click/drag, frame wait, screenshots, recordings, runtime visual QA. |
| `game-state-management` | State schema, generated C APIs, saves, migrations, fixtures, DevAPI state commands. |
| `game-visual-art-direction` | Visual direction, fake shots, art-quality judgment, replacing placeholder visuals. |
| `generated-game-ui-assets` | UI source sheets, icon sheets, slice9, crop manifests, atlases, composition proofs, responsive UI gates. |
| `delegated-image-generation` | Last-resort real raster generation after source-first search fails. |
| `game-asset-pipeline` | Runtime asset tracing, sourcing/library, manifests, provenance, pack/load failures. |
| `game-asset-prep` | Prepare sourced/made assets into engine-ready reusable library form. |
| `game-3d-models` | GLB/GLTF/OBJ/FBX sourcing, conversion, previews, real meshes instead of debug geometry. |
| `game-texture-generation` | Standalone material texture source/generation/review/integration. |
| `app-tunnel` | Expose local app/page/game over a public quick tunnel when the lead wants device testing. |

Boundary rule: do not create a new skill when the workflow belongs to one of
these owners. Add a reference under the owner first.

## Agent and Role Map

There is no durable local `.agents` roster in this checkout. Agent roles are
currently protocol-level, not installed worker definitions.

| Role | Current implementation | Authority |
|---|---|---|
| Lead/orchestrator | Main conversation plus `docs/ai-pipeline/subagent-protocol.md` | Owns scope, hot docs, task/status, integration, validation, commits, final report. |
| Read-only mapper | Packet preset `codebase-map` from `ai_studio/taskboard/lib.mjs` | Reads bounded scopes, returns entry points/data flow/risks. |
| Independent reviewer | Packet preset `review` | Reviews one artifact or axis, returns verdict/issues. Verdict is input, not acceptance. |
| Asset researcher | Packet preset `asset-research` | Finds candidates and licenses, does not import. |
| Texture/image worker | Packet preset `texture-gen` plus `delegated-image-generation` | Generates isolated raster artifact under `tmp/`, does not wire runtime. |
| Asset intake worker | Packet preset `asset-intake` | Sequential source/generate/verify/propose flow; lead integrates. |
| Verifier | Product gates, focused tests, or a clean review packet | Confirms/refutes output. Does not prove delegation happened. |

Non-goal: build an agent zoo early. Keep packet bodies harness-neutral. Add
durable worker definitions only after the same packet/workflow repeats enough to
justify a reusable wrapper.

## Lifecycle Map: From Idea To Live Game Work

```text
1. Concept intake
   user idea
   -> primary-gdd-pipeline
   -> gamedesign/projects/<game-id>/ wiki + GDD
   -> tasks/ epic/task

2. Reference and design grounding
   sources/references
   -> gamedesign/sources/
   -> design-source-knowledge
   -> gamedesign/knowledge/ only when reusable

3. New game shell
   node tools/bootstrap/new_game.mjs --id <id>
   -> <id>/{src,state,assets}/ copied from template
   -> game owns copied files

4. Asset flow
   find_assets/source search
   -> shared library candidate
   -> intake/catalog/license/provenance
   -> asset_review pull/promote
   -> project-local runtime copies
   -> restricted guard blocks non-publishable binary leaks

5. Implementation flow
   task + matching skill
   -> engine public API first
   -> small native slice in game folder
   -> state_codegen/devapi/runtime automation as needed
   -> focused build/test/screenshot proof

6. Product acceptance
   screenshot/state/art evidence
   -> product_gate review / visual critic / material floor
   -> lead accept/reject
   -> rejection freezes feature/content expansion until resolved or accepted

7. Feedback loop
   repeated friction or failure
   -> fix source of truth, tool, validator, skill, or task rule
   -> validate narrowest relevant command
```

## Ownership Rules

1. Hot maps stay thin.
   `AGENTS.md`, `ai_studio/README.md`, `ai_studio/taskboard/README.md`, and skill entrypoints route
   agents to deeper material. They should not become manuals.

2. Each domain needs one public facade.
   Good examples already exist: `tools/pipeline_validate.mjs`, `ai_studio/taskboard/cli.mjs`,
   `tools/assets/source/find_assets.mjs`, and `tools/product_gate/review.mjs`.
   Prefer improving those surfaces over teaching agents to call internal helper
   scripts directly.

3. Each mandatory rule should have a mechanical home.
   Policy belongs in docs, but enforcement belongs in a validator, gate, test,
   schema, or guard whenever practical.

4. Tool results should be agent-legible.
   Default output should be short and actionable. Broad output needs an explicit
   flag, pagination, filtering, JSON output, or an artifact path.

5. Risky actions split draft from commit.
   Examples: research vs import, generate vs wire runtime, product critique vs
   acceptance, stage export vs overwrite, metadata catalog vs binary copy.

6. Assets are a separate legal/product domain.
   Publishability is not the same as source. `tools/assets/restricted.mjs` is the
   current source of truth for whether a binary can enter open git.

7. Runtime game code follows the template's decomposition.
   Thin entry, world as source of truth, systems by responsibility, UI/theme
   separate, DevAPI outside `main.c`, engine APIs first.

## Current Strengths

- The old single AI facade has been removed; use the owning module CLI directly.
- The taskboard has a real Markdown store, CLI, server, orchestration packet
  templates, validation, and tests.
- The skills are domain-oriented and mostly have clear boundaries.
- The asset domain is unusually mature for a prototype: source-first rules,
  shared library, provenance, generation records, UI atlas flow, restricted
  asset policy, and guards.
- Product gates treat builds/probes as evidence rather than acceptance, which is
  the right separation for game work.
- The template explicitly teaches decomposition instead of relying on prompts.
- Sync tooling has a single source of truth for `.codex` -> `.claude` skills and
  hooks.

## Current Gaps And Risks

- There was no single architecture map before this file; agents had to infer
  ownership from many docs and scripts.
- `.agents` has no durable worker definitions; roles are implicit in protocol
  docs and packet presets.
- Some docs contain mojibake in rendered punctuation. This is not fatal, but it
  weakens agent readability and should be cleaned when touching those files.
- `tools/assets/` is powerful but broad. It needs stable facade discipline so
  agents do not learn to call internal helpers in the wrong order.
- `gamedesign/knowledge/` is rich but lacks a single executable/indexed query
  facade. For now, route through the skill and `gamedesign/knowledge/index.md`.
- Current context-budget review passes; keep hot docs and skill entrypoints under
  their limits as the map grows.
- `tools/blockside-heat/` looks like stale scratch/cache in this checkout and
  should be confirmed before any cleanup.

## Refactor Direction

Do not start by moving many files. Start by making ownership enforceable.

Recommended sequence:

1. Keep this map current and link it from the hot workflow map.
2. For each broad domain, write or update a short README file that says:
   public entry points, internal helpers, durable outputs, validation command,
   and owning skill.
3. Add a small tool inventory checker only after the map starts drifting.
4. For `tools/assets/`, pick a smaller public facade set and label the rest as
   internal helpers in the domain README.
5. Add JSON output and actionable error shapes to any tool that agents call
   repeatedly.
6. Add structural guards only for repeated failures. Do not turn orchestration
   telemetry into an acceptance gate.
7. Create durable worker definitions only after packet presets repeat enough to
   justify them.

The architectural north star is:

```text
short maps
  -> domain skills
  -> stable CLI facades
  -> small domain libraries
  -> mechanical validators
  -> durable evidence/state
```

That gives the agent a system it can navigate, use, and improve without growing
another monolithic instruction file.
