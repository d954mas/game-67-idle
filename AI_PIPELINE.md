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

`.claude/skills/` is generated; never edit it by hand. After adding or
renaming a skill in `.codex/skills/`, run:

```powershell
node tools/skills_sync.mjs
```

## Flow: idea to shipped

| Stage | What happens | Skill / tool |
|---|---|---|
| 0. Startup gate | Before implementation, prove one active concept, one actionable task, one project wiki, a native/runtime harness, and a named visual/product proof gate | `tools/game_context/new_prototype.mjs`, `tools/game_context/iteration_context.mjs`, `task-manager`, `primary-gdd-pipeline` |
| 1. Capture | Every stated idea becomes a task; deferred work is never lost | `task-manager`, `tasks/` store, `tools/taskboard/` |
| 2. Refine | Questions to the lead + research; `idea` -> `backlog` with checkable done-when | `task-manager` |
| 3. Design | Concept, GDD, refs, visual proof, data contracts | `primary-gdd-pipeline` (incl. design stewardship), `gamedesign/` |
| 4. Implement | Smallest playable slice; schema-first state; explicit asset paths | `game-feature-iteration`, `game-state-management`, `game-asset-pipeline` |
| 5. Validate | Agent drives the running game and captures evidence | `game-runtime-automation` (DevAPI + visual QA) |
| 6. Release | Explicit build/serve/package tasks | `game-feature-iteration` (build/release section) |
| 7. Learn | On failure, name the missing component (instruction, source of truth, tool, validator, eval, recovery path) and encode the fix there, not only in prompts; lessons -> `gamedesign/knowledge/` | `gamedesign/knowledge/agent_legibility.md`, all skills |

## Conventions that make this fast

- **One source of truth per thing.** Skills: `.codex/skills/`. Task/status
  conventions: `tasks/README.md`. State shape: `state/*.schema.json`.
  Generated files are regenerated, not edited.
- **Tools are assistants, not gates.** Tool defaults must be quiet, bounded, and
  advisory. A script should summarize the useful next action by default and
  require an explicit `--verbose`, `--deep`, `--all`, `--review`, or
  `--include-final` style flag for exhaustive output, broad validation,
  generated handoff artifacts, or old queues. If a script creates process work
  during normal game implementation, simplify the script or move that behavior
  behind an explicit deep mode.
- **Tools have portability layers.** Before cleaning a project or exporting a
  new game base, use `tools/README.md` and `tools/tool_layers.json`.
  `portable_ai_pipeline` moves to new games, `reusable_game_infrastructure`
  moves only when the runtime matches, and project-specific tooling is
  deleted, archived, or intentionally adapted.
- **Art iterations are packetized.** For generated game art, start from an
  accepted visual target, write an art request packet, slice from a manifest,
  run an explicit pack/material build, and validate in the primary runtime. For
  generated UI, research/source notes come first when the pipeline is being
  changed; current notes live in
  `gamedesign/sources/generated_game_ui_asset_pipeline_research_2026-06-14.md`.
  Do not keep crop coordinates, content safe areas, or slice9 decisions only in
  chat. Start generated runtime UI work with
  `.codex/skills/generated-game-ui-assets/`; it coordinates art direction,
  asset pipeline, pixel audits, responsive layout audits, and runtime proof.
  Accepted generated source sheets must carry provenance: provider/model or
  workflow, workflow file/json, seed or no-seed reason, prompt, negative prompt, source family
  role, accepted source image, and rejected candidate notes. Record it with
  `node tools/assets/new_generation_record.mjs` and reference the created file
  from `expected_outputs.generation_records`. Generated/artist records need a
  real workflow path or non-empty workflow JSON; dummy `{}` provenance is
  draft-only and cannot pass the final-art gate. If a provider does not expose
  a stable seed, record a concrete no-seed reason rather than writing `unknown`
  as a pseudo-seed. Procedural or programmer-art
  fallbacks may prove geometry, but they do not satisfy final generated-art
  work unless an explicit debug exception is recorded in the generation record
  and the art job.
  Reusable guidance: `gamedesign/knowledge/ai_art_iteration_pipeline.md`.
  For new multi-asset passes, scaffold the contract with
  `node tools/assets/new_art_job.mjs`, run
  `node tools/assets/validate_art_job.mjs --job <job>` before generation, and
  run `node tools/assets/validate_art_job.mjs --job <job> --strict` after
  slicing/runtime manifests exist. Run
  `node tools/assets/validate_art_job.mjs --job <job> --final-art` before
  claiming a final generated/artist art pass; this must fail while any accepted
  source is procedural debug art or has partial/unknown generation provenance.
  Before slicing a generated UI source sheet, run
  `py -3.12 tools/assets/normalize_source_sheet_chroma.py --source <raw-sheet> --output <clean-sheet>`
  when the generator produced a non-flat chroma background, then run
  `py -3.12 tools/assets/audit_source_sheet_intake.py --source <source-sheet> --json-output <audit.json> --report <audit.md>`
  to catch non-flat chroma backgrounds, merged components, clipped components,
  and too-small gutters. For generated UI runtime PNGs, also run
  `py -3.12 tools/assets/audit_generated_ui_assets.py --crop-manifest <crop-manifest>`
  after slicing; this catches clipped icon alpha bounds and chroma-key edge
  fringe that a JSON validator cannot see.
- **Visual-first session contract.** For visual, UI, FTUE, feel, or
  audience-test work, write a 5-line contract before coding: `goal`,
  `non-goal`, `proof`, `stop condition`, and `likely files`. The proof must
  name a native screenshot/product gate/art audit, not only a build command.
  Before visual code changes, compare the current native screenshot or capture
  plan against the accepted fake shot/reference/art target and list the visible
  mismatches. After each meaningful render change, capture a new native
  screenshot, update the mismatch list, and run or record the product-read gate
  verdict before adding features or content.
- **Product-read gates stop content expansion.** For game work where visual,
  FTUE, gameplay feel, or audience testing matters, the first playable screen
  must pass a screenshot/player-read review before adding more content or
  systems. The review asks: where am I, what can I do now, what changed after
  input, what is the reward, and does this look like a game rather than a
  debug tool? If the answer is weak, freeze scope and fix the screen/loop
  first. Passing builds, scenarios, or probes is not enough. Use
  `node tools/product_gate/review.mjs` or `node tools/ai.mjs gate` to write the
  durable gate artifact. Before handing off a slice, use
  `node tools/ai.mjs close-slice` so the task log records the gate, validation
  evidence, and next action. If the gate fails, feature/content expansion stays
  frozen unless the lead explicitly accepts the debt for that slice.
  For beautiful, casual, generated-UI, fake-shot, or child-testable prototype
  work, add `--visual-strict` to the product gate and score composition,
  readability, UI controls, action direction, art quality, and audience fit.
  A pass requires all six scores at least 4/5 and no blocker/major visual issue.
  If an independent visual/UI critic is needed, create a packet first with
  `node tools/ai.mjs critic` and use its findings as the source for the strict
  gate verdict.
- **Responsive UI needs geometry evidence.** For desktop/mobile UI composition,
  pair screenshot gates with a UI-tree layout audit when the runtime exposes
  element bounds. Use `node tools/product_gate/responsive_layout_audit.mjs` to
  check required action nodes, minimum touch sizes, non-overlap, and portrait
  primary-action layout. This catches compressed clickable zones that a static
  screenshot review can miss.
- **References are a gate only when they drive the work.** If the user names a
  reference or says the build does not match one, use the smallest honest mode:
  quick check, central deconstruction, or deep deconstruction. Before
  reference-driven implementation, the durable doc must support a short
  Reference Digest: mode, sources checked, observed facts, borrow/avoid/copy
  risk, current-build mismatch, and next native proof. Details live in
  `gamedesign/knowledge/reference_deconstruction.md` and
  `.codex/skills/primary-gdd-pipeline/references/reference-research-playbook.md`.
- **Record process pain sparingly.** Use `AI_PIPELINE_ITERATION_LOG.md` for
  occasional notes about time sinks, agent mistakes, and pipeline improvement
  ideas. Use `chat-session-reflection` for deeper retrospectives of long
  multi-turn sessions. Do not use the iteration log as a task board or project
  status file.
- **Profiling is passive by default.** Its job is to reveal where an AI agent
  gets stuck, not to become another workflow. Normal game work may use
  `node tools/ai.mjs run -- <command>` and `node tools/ai.mjs checkpoint
  "<intent>"`; the facade records only slow commands, failed commands, large
  context reads, and long manual/research gaps. Do not fix stale profile
  bundles, packets, drafts, baselines, or follow-up artifacts during ordinary
  game development. Use `node tools/ai.mjs status` for the short passive
  diagnostic. Before long prototype work, reset the current scope with
  `node tools/ai.mjs start <task-id> <iteration>` after selecting the task.
  Treat `Review confidence: broken` as a review blocker and `partial` as a
  warning that conclusions must name the missing telemetry. Low wall-clock
  coverage means the profile is not complete evidence of where the session time
  went; review notes must cite the largest coverage gaps from
  `node tools/ai.mjs status` or mark those intervals as unknown. For AI
  workflow, profiler, or retrospective review slices, run
  `node tools/ai.mjs status --require-current-scope-usable` before claiming
  profiling evidence; if it fails, fix scope/checkpoints or explicitly record
  the missing telemetry. Use `node tools/ai.mjs reflect` for a short closeout only.
  Full reflection handoff (`packet`/`draft`/`review`/baseline comparison) is
  opt-in with `node tools/ai.mjs reflect --deep` or direct
  `tools/ai_profile/*` commands when the task is explicitly about AI workflow,
  a long retrospective, or debugging the profiler itself. Raw telemetry stays
  in `tmp/session_profiles/`.
- **External AI observability is gated.** Do not add LangSmith, Phoenix,
  Langfuse, Braintrust, OpenTelemetry export, or another tracing/eval platform
  just because reflection needs more data. First use
  `AI_PIPELINE_OBSERVABILITY_TOOLS.md` and
  `node tools/ai_profile/observability_gate.mjs` to decide whether local JSONL
  is enough, a bounded external pilot is justified, or pilot evidence makes an
  external system an adoption candidate. Local JSONL remains the baseline
  evidence source unless the lead explicitly changes that rule.
- **Runtime base is protected in this repository.** `state/`,
  `tools/state_codegen/`, `src/devapi/`, `tools/devapi/`,
  `src/game_storage.*`, and `external/cjson/` are reusable AI/runtime
  infrastructure for the clean seed. Do not delete them during game cleanup.
- **Evidence or it did not happen.** A task is `done` only with ticked
  `## Done when` boxes and an evidence line in `## Log`.
- **Prototype commit/review hygiene.** Before committing or handing off a
  prototype slice, run the hygiene audit and record the result:
  `node tools/product_gate/slice_hygiene.mjs --strict ...`. A normal slice over
  30 changed files should be split by phase unless the lead explicitly asked
  for an end-of-experiment snapshot; use `--snapshot` only for that case.
  Include build/probe evidence, product gate, screenshot evidence, profiler
  guard evidence from `node tools/ai.mjs status --require-current-scope-usable`,
  taskboard validation, known red gates, and diff size in review notes. Do not
  promise push until the command or `git status -sb`/remote checks show a usable
  push target. Changed fail/stale audit artifacts must be refreshed, archived as
  historical evidence, or called out with `--known-red-gate` and final notes.
- **Small slices.** Prefer one playable iteration over broad speculative work.
- **Startup gate before code.** At the start of a new prototype, prefer
  `node tools/game_context/new_prototype.mjs --game-id <id> --title "<name>" --brief "<one sentence>"`
  to create the first wiki/task/status skeleton and gate evidence. After a
  pivot or manual setup, run `node tools/game_context/iteration_context.mjs`.
  If `prototype_startup_gate.status` is `not_ready_for_implementation`, do not
  begin broad runtime implementation. First create/repair the active concept,
  task, project wiki, runtime harness, and visual/product proof gate.
- **Visual failure is a stop condition.** If the lead rejects a screenshot or
  runtime build as ugly, unclear, or unplayable, stop feature expansion. Create
  a rescue task, a visual/product failure report, and the smallest runtime
  screenshot proof needed for the next pass.
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

For playable game work in this repository, the platform gate is strict:

- Native PC is the default and primary development harness.
- A web prototype is not an acceptable shortcut for making the game look or feel
  better.
- Web/mobile/browser work requires explicit user permission in the current
  request, or a separate approval after the agent states why native PC cannot
  answer the task.
- If an agent is tempted to use web because PC rendering is rough, that is a
  signal to improve the native PC slice, not to change platforms.

Default order for substantial work:

1. Load the minimal current context from `tasks/README.md`, starting with
   `node tools/ai.mjs summary` or `node tools/taskboard/cli.mjs summary`. Use
   `node tools/ai.mjs context -- node tools/taskboard/cli.mjs context` only
   when the summary is not enough; never read a large `tasks/STATUS.md`
   wholesale for orientation.
2. Select or create one task scope, then run
   `node tools/ai.mjs start <task-id> <iteration>` for long implementation,
   visual, research, or tooling work.
3. Inspect only the files needed for the selected scope.
4. Prefer scoped search before repo-wide search.
5. Make the smallest coherent change.
6. Run the narrowest validation that proves the change.
7. Escalate to broader validation only when the scope or risk requires it.

For reusable/process/tooling work, run the quick reusable-pipeline validation
described below as the default gate, and only escalate to the full gate when the
change affects shared behavior or the portable export path. Re-running the full
gate after every small edit is waste unless the previous gate failed or new
shared behavior changed.

For pipeline/tooling changes, prove both the current repository and the portable
export path when the change affects future projects. For game/runtime changes,
prove the specific playable or visual behavior, not only that the build
compiles.

For reusable skill/process changes, also run `node tools/skills_eval.mjs`. The
eval is intentionally small and static: it checks that key skill activation
phrases and required output/process anchors have not been lost.

For quick reusable-pipeline validation, run `node tools/pipeline_validate.mjs`
(or its alias `node tools/ai.mjs validate`). It validates the core
workflow/tooling path without portable export, runtime configure, or deep
generated-asset tests. For the full reusable-base gate, run
`node tools/pipeline_validate.mjs --full` (alias: `node tools/ai.mjs validate
--full`). Full mode validates this repo
(including runtime seed checks: state codegen and CMake configure when those
files are present), exports a fresh portable base, and validates the exported
project from inside the exported folder. Treat full mode as a broad/final gate:
run it once after narrower checks pass, unless debugging a failure in the
exporter itself. The runtime seed checks skip automatically in workflow-only
exports.

Do not use old task logs, generated files, build outputs, or archived design
handoffs as current truth unless they are linked from `STATUS.md`, an active
task, or fresh validation evidence.

## Reuse in a new project

This repository contains both:

- a clean runtime seed: `src/`, `state/`, save/load, DevAPI, capture, and
  native/web build wiring;
- a portable AI workflow base copied by `tools/bootstrap/export_base.mjs`.

The current exporter is workflow-first. It intentionally copies agent skills,
taskboard, pipeline docs, and reusable design knowledge. It does not copy the C
runtime seed unless the exporter is explicitly extended for a runtime template.

Export the portable AI base into a fresh repository:

```powershell
node tools/bootstrap/export_base.mjs --target C:\projects\new-game
```

Portable (copied by the exporter):

- `.codex/skills/` - all skills are written engine-agnostic: they discover
  local conventions instead of assuming this repo's layout.
- `tools/ai.mjs`, `tools/ai.test.mjs`, `tools/skills_sync.mjs`,
  `tools/skills_eval.mjs`,
  `tools/pipeline_validate.mjs`, `tools/ai_profile/`,
  `tools/game_context/`, `tools/product_gate/`, `tools/assets/new_art_job.mjs`,
  `tools/assets/new_generation_record.mjs`, `tools/assets/validate_art_job.mjs`,
  `tools/assets/audit_generated_ui_assets.py`, `tools/taskboard/`, `tools/README.md`,
  `tools/tool_layers.json` - fast AI
  workflow facade, skill mirroring, skill regression checks, reusable-base
  validation, AI session profiling internals, game iteration context, generated
  art job scaffolding/validation, task store UI/CLI, and tool portability map.
- `gamedesign/knowledge/` - accumulated design lessons.
- `AI_PIPELINE.md`, `AI_PIPELINE_SESSION_PROFILING.md`,
  `AI_PIPELINE_OBSERVABILITY_TOOLS.md`, `tasks/README.md`, starter
  `tasks/STATUS.md`, starter `AGENTS.md` / `CLAUDE.md`.

Stays behind in workflow-only exports: runtime `src/`, `state/` schemas,
`tools/devapi/` scripts, build presets, and `gamedesign/<concept>/` docs/data.
In this repository those runtime files are still protected reusable seed
infrastructure, not old-game debris. The DevAPI pattern also travels via
`.codex/skills/game-runtime-automation/references/devapi-pattern.md`; each
future engine setup can either reuse this seed or implement the same bridge
locally.

After exporting: fill the `## Project` and `## Direction` sections of the new
`AGENTS.md`, then start at stage 1 with the first ideas as tasks.
