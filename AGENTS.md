# AGENTS

## Repository Role

This repository is an AI Studio for game development: a shared agent/tooling
pipeline for creating game concepts, GDDs, assets, prototypes, release builds,
and liveops support.

The repository root is not a game. Templates live under `templates/`; reusable
feature packs live under `features/`; games are created under `games/`. Closed
prototypes are git tags.

## Communication

Minimize optional commentary. During execution, send only required notices,
blocking questions, meaningful progress updates, risk/verification notes, and
the final result.

## AI Studio

`ai_studio/` is the target home for reviewed AI pipeline architecture: core
harness, agents, skills, tools, architecture maps, asset workflows, technical
workflows, and future production modules.

Reviewed AI pipeline modules live in `ai_studio/`. New AI-pipeline docs/tools
belong in the owning `ai_studio/` module; do not add new root-level compatibility
paths.

Mechanical gates and the advisory boundary are summarized in
`ai_studio/core_harness/workflow/README.md#enforcement-boundary`.

## Agent Roles

Reusable agent roles live in the active harness catalog. Codex custom agents
live in `.codex/agents/*.toml`; Claude agents live in `.claude/agents/*.md`.
Delegate only when an independent bounded packet materially reduces latency,
context load, or review risk. Keep coherent local work, including related
multi-file changes, with the lead when context transfer and reintegration would
cost more than direct work.

When delegating, use the closest existing role, a short
Task / Scope / Return / Stop packet, and integrate and verify the result. Reuse
an existing suitable agent instead of creating disposable roles. If host policy
requires explicit approval, ask once per chat/session and reuse that approval
while scope remains in this repository.

Review budget follows risk: mechanical documentation, moves, and obvious edits
need no independent reviewer; normal logic gets one; security, concurrency, and
release work gets two independent reviewers. Repeat review only after a
high-risk finding or contract change.

## Current Game

Current game context lives under `games/<game-id>/`.

When the task is about a specific game, use the explicit game id from the user or
infer it only when there is exactly one game folder. Do not store game lore,
balance, roadmap, GDD detail, or per-game task state in `AGENTS.md`.

Game-specific knowledge, GDDs, reference lessons, playtest findings, and accepted
design facts live under `games/<game-id>/design/`. Use
`games/<game-id>/design/knowledge/` as that game's private knowledge base.

If there is no active game, do not start game implementation unless the lead
explicitly asks for it.

Parallel feature agents for a private game use
`ai_studio/workspace/feature_workspaces.mjs`; see
`ai_studio/workspace/README.md#parallel-feature-workspaces`. Do not make manual
Studio or game copies. A feature workspace starts from committed Studio/game
heads; source working-tree changes are intentionally not transferred.

## Hard Invariants

- Engine boundary: use `external/neotolis-engine` public APIs before custom code.
- The engine working tree is read-only for agents. Suspected engine problem:
  first establish where the ROOT CAUSE lives — the existence of a game/template
  workaround does not settle it (a workaround can be a hack that leaves the
  real defect in the engine). If the root cause is in the engine, convince the
  lead the fix is needed; every engine change ships only through an issue and
  PR in the engine repo — never a direct edit.
- Game/world/UI logic is Y-up; convert Y-down input/platform data only at boundaries.
- All user-visible text uses the engine text renderer with real fonts; no handmade
  `draw_text`.
- Source assets before generating: shared library, then free CC0/OFL sources, then
  generation.
- SVG/vector/procedurally drawn art is a direction mockup only. It never becomes a
  game asset unless the lead explicitly asked for or approved it; real game art
  goes through the asset pipeline (source-first, then raster generation).
- Every committed asset must have license, provenance, integrity, and `origin`.
- Paid or non-redistributable binaries never enter git.
- Heavy authoring workfiles that are not themselves shipping game assets live
  under the synchronized `<YandexDisk>/gamedev/games/<game-id>/` workspace;
  game repositories keep their scripts, provenance, manifests and hashes.
- Only the lead creates Taskboard items. An agent does not run
  `cli.mjs new project|epic|task` and does not write a new item file by hand, in
  any store. What belongs on the board is the lead's decision about what the
  project is doing; an agent that adds its own cards is making that decision
  instead of asking. Propose the work in the reply, name it, and wait.
  Two things stay allowed: updating an item the lead owns — status, log, closure
  evidence, quality — which is how work gets reported, and creating items inside
  `/to-spec` or `/to-tickets`, where the lead invoking the skill IS the request.

## Game Test Policy

A tool is judged by its contract; a game is judged by how it plays, and its
numbers are design knobs that move on every iteration. Test the two differently.

In game code, cover what breaks silently: saves and migrations, state schema,
economy and progression math, item catalogs, packaging, platform SDK, analytics
contracts.

Never pin a design knob. Balance constants, spawn counts, world-layout digests,
render-target sizes, HUD offsets and player-facing copy are inputs to iteration;
asserting their exact value turns every design change into test repair. Assert
the invariant instead: a range, an ordering, monotonicity, non-emptiness, no NaN.

Where an exact value must still be watched for accidental drift, keep it as a
golden that one command re-records (`features/test-goldens`), never as a
hand-edited constant.

Gameplay feel, render output and UI layout are proven by a run and a screenshot,
not by a unit test.

Every test declares a tier, and the CTest label is the only record of it -- no
runner, lane or reader keeps a second list:

- `core` (the default): silent-failure logic. Runs on every edit and stays fast.
- `slow`: correct but expensive -- heavy fixtures, simulations, packaging.
- `taste`: pins player-facing output and moves with design; runs before release.

`node tools/game.mjs test` runs `core`; `--tier slow|taste` and `--all` widen it,
and a game's release lane runs everything.

Removing a knob-pinning test is maintenance, not lost coverage. Propose the list;
the lead approves it.

## Transcript Cost

Every tool result is re-read by the model on every later request, so output
volume, not command count, is what makes iteration slow.

Read the range you need instead of the whole file; search with a glob and a
limit instead of walking a tree; patch through the harness edit tool instead of
shell-escaped strings; keep binaries out of the transcript. When a result comes
back truncated, narrow the query rather than re-running it wider.

`profiling/iteration_report.mjs` reports what this costs per week.

## Source Comments

The engine's rule (`external/neotolis-engine/AGENTS.md`, "Code style") applies to
every line of code in this repository, not only to the engine.

**A comment explains WHY, and only what the reader cannot derive from the code.**
Identifiers say what the code does.

Never write in source: task ids (`T0058`), dates, commit SHAs, PR or issue
numbers, quotes from the lead or any conversation, `(review fix)` / `Phase N` /
`CHUNK` / `hygiene` tags, test-name pins, or a narrative of what the code used to
do before a change. All of that belongs to the commit message and the taskboard
card, which is where a reader who needs history should be sent.

Do write: an invariant, an ownership or phase-order constraint, a unit or range,
or a short note on a non-obvious choice.

Length: one line preferred, two or three normal. A longer block is allowed only
when it carries a real system invariant that cannot be compressed without losing
it — never to narrate a change. If a block needs more than that to be understood,
the knowledge belongs in `games/<game-id>/design/knowledge/` or the owning
`ai_studio/` module, and the comment should point there.

## Pi Skill Chain

Fixed order for feature work in pi. Each step consumes the previous step's
output; skipping a step means the next one invents what it was not told.

1. `/grill-me` — interview until shared understanding. Produces no file; it
   settles the design tree. Never jump to step 3 from an unfinished grilling.
2. `research` — model-invoked when a step needs facts. Writes findings as
   Markdown, every claim cited to a primary source.
3. `/to-spec` — synthesize the conversation into a spec and create it as an
   **epic** under the owning project. No interview here; the only question is
   confirming the test seams.
4. `/to-tickets` — split that epic into tracer-bullet tickets in the
   **Taskboard**, blockers first, each declaring its `Blocked by` edges.
   Never `.scratch/` or any other parallel store — the Taskboard is the single
   source of truth for current work.
5. `/implement` — take a ticket from the frontier (blockers all closed), build
   it through `tdd`, then `code-review`, then commit.

Run pi from the studio root, not from a game folder: `.pi/settings.json` is
read from the current directory only and does not search parent folders, so a
session started inside `games/<id>` silently loses these skills and the project
permission rules.

Taskboard commands take `--game <game-id>` for game work; private game stores
are invisible without it.

`to-spec` and `to-tickets` here are local overrides in `.pi/skills/`, retargeted
from the upstream `.scratch/` and GitHub-issue layouts to the Taskboard CLI.
Both upstream copies are excluded from the package filter so they never collide.

Skills are declared in `.pi/settings.json`. The clone under `.pi/git/` is
gitignored; `.pi/settings.json` is committed.

## Context Routing

- Workflow and commands: `ai_studio/README.md`.
- Reusable agent roles: the active harness catalog, such as `.codex/agents/`
  or `.claude/agents/`.
- Task/status state: `ai_studio/taskboard/README.md` and the taskboard.
- AI Studio architecture: `ai_studio/README.md`, `ai_studio/tree.json`, and
  `ai_studio/architecture_map/README.md`.
- Templates, games, and features: `templates/README.md`, `games/README.md`,
  and `features/README.md`.
- Shared reusable game-development knowledge: `ai_studio/game_design/knowledge_base/README.md`.
- Game-specific design knowledge and GDDs:
  `games/<game-id>/design/README.md`.
- Detailed engine, workflow, validation, subagent, asset, and release procedures:
  load the matching `ai_studio/` module, doc, or skill only when the task needs it.
