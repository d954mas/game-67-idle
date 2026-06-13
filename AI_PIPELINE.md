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
- **Art iterations are packetized.** For generated game art, start from an
  accepted visual target, write an art request packet, slice from a manifest,
  run an explicit pack/material build, and validate in the primary runtime. Do
  not keep crop coordinates or slice9 decisions only in chat. Reusable guidance:
  `gamedesign/knowledge/ai_art_iteration_pipeline.md`. For new multi-asset
  passes, scaffold the contract with `node tools/assets/new_art_job.mjs` so the
  agent can move directly from candidate generation to crop/slice/pack/native
  evidence.
- **References are a build gate, not inspiration notes.** When a user names a
  reference game/style, produce an auditable deconstruction before coding
  gameplay, UI, economy, balance, or art. Use
  `gamedesign/knowledge/reference_deconstruction.md`: gather a source packet
  with checked dates, write a visible-action ledger and player transcript
  before system conclusions, extract screen grammar/mechanics/economy/visual
  composition, then translate into borrow/avoid/copy-risk plus a current-build
  mismatch audit. A central gameplay ref needs official/store/trailer visuals,
  gameplay footage/walkthrough or a long screenshot sequence, and the current
  native capture. The study is only implementation-ready when it names the next
  native screenshot/scenario proof. If the doc cannot answer a later "did you
  study the ref?" challenge, improve the doc before implementation.
- **Reference study has modes.** Before researching, state the mode:
  `quick check`, `central deconstruction`, or `deep deconstruction`. Quick
  checks are for small UI, wording, or visual details and require links/paths
  plus concrete visible observations, but they do not unlock core gameplay,
  economy, balance, or art-direction changes. Central deconstruction is the
  default for a named gameplay/art driver. Deep deconstruction is required when
  first-session pacing, balance, retention pressure, or release-critical UX
  depends on the reference. Central and deep studies must end with a current
  native mismatch audit and an exact screenshot/scenario proof for the next
  implementation pass.
- **"Studied" means source-backed.** An agent may only claim a gameplay
  reference was studied when the durable artifact records the actual source
  packet: links or local paths, checked date, gameplay video/walkthrough or a
  long screenshot sequence, official/store/trailer visuals when available,
  supporting guide/review/deconstruction sources for balance or friction
  claims, current native capture, and timestamped/framed observations. If this
  packet is missing, the honest status is "not studied enough to implement."
- **Reference source order is explicit.** Study refs through a Source Ladder:
  user-provided material -> official/store/trailer visuals -> raw gameplay
  video/walkthrough or long screenshot sequence -> guides/reviews/lectures/
  deconstructions/wikis/community notes. Central/deep refs must record the
  ladder in the durable doc with source role, checked date, watched/read scope,
  and whether the source is observation evidence or secondary interpretation.
  Secondary summaries can explain balance or friction, but cannot replace
  observed gameplay frames for loop, first-screen, control, reward, or UI
  claims.
- **Reference study needs an evidence board.** For central/deep refs, gather
  and cite at least six concrete frames/screenshots before drawing conclusions:
  first screen, first input, visible response, reward feedback,
  upgrade/progression UI, and a friction or blocked/full-board state. Pair
  those stills with raw gameplay/walkthrough evidence for timing, and add a
  guide/review/lecture/wiki/deconstruction only as supporting interpretation
  when making balance, pacing, or player-friction claims. Store links, local
  paths, timestamps/frame ids, checked date, and "what this proves" in the
  deconstruction. No frames/timestamps means "not studied enough to implement."
- **Reference Lock comes before implementation.** Before coding gameplay/UI,
  tuning balance, or generating final art from a named reference, the agent
  writes a short lock in the task log or deconstruction doc: study mode, ref
  question, doc path, source packet to collect, current native capture path or
  capture plan, no-coding/no-final-art boundary, and expected native proof.
  Implementation unlocks only after the deconstruction names at least three
  observed source facts, one current-build mismatch, borrow/avoid/copy-risk,
  and the exact next screenshot/scenario. If sources are missing, the lock
  stays closed and the next action is source gathering or a narrow exception.
- **Reference Intake comes before opinions.** When a user names a ref or says
  the result does not match it, the next agent move is not to defend the build
  or make another implementation pass. State the exact question the ref must
  answer, choose `quick check`, `central deconstruction`, or `deep
  deconstruction`, name the durable doc path, list the source packet to gather,
  capture or plan the current native screenshot, and name the first proof
  screenshot/scenario. Label every later design claim as observed, inferred,
  user-provided, or unknown; unknown/inferred claims do not unlock code,
  economy, balance, UI, or final art.
- **Reference study has a Definition of Ready.** A named reference may drive
  code, economy, balance, UI, or final art only after the durable doc contains:
  mode, Reference Lock, source matrix, gameplay footage/walkthrough or a long
  screenshot sequence for interaction claims, current native capture,
  observation ledger, borrow/avoid/copy-risk, current-build mismatch, and the
  next native proof. If any required item is missing, the agent must say the
  reference study is not ready for implementation and keep working on sources
  or ask for user material instead of coding from memory.
- **Reference study must be inspectable by the lead.** Before implementation
  resumes from a named reference, give a short Reference Digest in chat or the
  task log: study mode, sources checked, 3-5 observed facts, current-build
  mismatch, borrow/avoid/copy-risk, and the next native screenshot/scenario
  proof. If the digest would be vague, the study is still not ready.
- **Parallel reference work is research-only until unlocked.** A reference
  deconstruction may run beside unrelated setup or tooling, but not beside the
  gameplay/UI/economy/balance/final-art implementation it is supposed to guide.
  The implementation lane opens only after the durable doc has observed facts,
  current-build mismatch, borrow/avoid/copy-risk, Reference Digest, and the
  exact native proof. Do not retrofit research conclusions to code or art that
  was already made in parallel.
- **Record process pain sparingly.** Use `AI_PIPELINE_ITERATION_LOG.md` for
  occasional notes about time sinks, agent mistakes, and pipeline improvement
  ideas. Use `chat-session-reflection` for deeper retrospectives of long
  multi-turn sessions. Do not use the iteration log as a task board or project
  status file.
- **Profile long AI sessions without polluting git.** Use
  `AI_PIPELINE_SESSION_PROFILING.md` and `tools/ai_profile/` when work is long,
  repeated, release-critical, or explicitly about AI workflow. Reusable
  profiling rules/tools are committed; raw session JSONL, generated summaries,
  recovered thread dumps, and one-off telemetry extracts stay in
  `tmp/session_profiles/` unless the lead explicitly asks to promote them.
  `.gitignore` is the safety net for scratch paths; the workflow rule is what
  prevents agents from creating noisy session artifacts in non-ignored
  locations. Use `tools/ai_profile/run.mjs` for substantial commands and
  `tools/ai_profile/event.mjs` for sparse checkpoints so profiling is captured
  during work instead of reconstructed after the fact. Use
  `tools/ai_profile/closeout.mjs` and `tools/ai_profile/review.mjs` before
  reflection so the agent starts from measured waste/rework/context findings.
- **Runtime base is protected in this repository.** `state/`,
  `tools/state_codegen/`, `src/devapi/`, `tools/devapi/`,
  `src/game_storage.*`, and `external/cjson/` are reusable AI/runtime
  infrastructure for the clean seed. Do not delete them during game cleanup.
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

For reusable skill/process changes, also run `node tools/skills_eval.mjs`. The
eval is intentionally small and static: it checks that key skill activation
phrases and required output/process anchors have not been lost.

For the full reusable-base gate, run `node tools/pipeline_validate.mjs`. It
validates this repo (including runtime seed checks: state codegen and CMake
configure when those files are present), exports a fresh portable base, and
validates the exported project from inside the exported folder. The runtime
seed checks skip automatically in workflow-only exports.

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
- `tools/skills_sync.mjs`, `tools/skills_eval.mjs`,
  `tools/pipeline_validate.mjs`, `tools/ai_profile/`,
  `tools/assets/new_art_job.mjs`, `tools/taskboard/` - skill mirroring, skill
  regression checks, full reusable-base validation, AI session profile
  summarization, generated-art job scaffolding, and the task store (board UI +
  CLI).
- `gamedesign/knowledge/` - accumulated design lessons.
- `AI_PIPELINE.md`, `AI_PIPELINE_SESSION_PROFILING.md`, `tasks/README.md`,
  starter `tasks/STATUS.md`, starter `AGENTS.md` / `CLAUDE.md`.

Stays behind in workflow-only exports: runtime `src/`, `state/` schemas,
`tools/devapi/` scripts, build presets, and `gamedesign/<concept>/` docs/data.
In this repository those runtime files are still protected reusable seed
infrastructure, not old-game debris. The DevAPI pattern also travels via
`.codex/skills/game-runtime-automation/references/devapi-pattern.md`; each
future engine setup can either reuse this seed or implement the same bridge
locally.

After exporting: fill the `## Project` and `## Direction` sections of the new
`AGENTS.md`, then start at stage 1 with the first ideas as tasks.
