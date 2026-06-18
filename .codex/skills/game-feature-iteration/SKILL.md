---
name: game-feature-iteration
description: Use when implementing, changing, debugging, or validating a playable game feature in an existing game repository, or when discovering, adding, fixing, or running build, launch, debug, release, package, or CI tasks. Triggers include gameplay mechanics, controls, cameras, UI flows, game state, progression, balance-affecting code, engine integration, requests to make a small playable prototype or vertical slice, VS Code tasks, CMake presets, build scripts, launch configurations, release outputs, serving web builds, packaging, and explaining how to run or distribute the game. Works across engines by discovering local build/run/test conventions first.
---

# Game Feature Iteration

Use this skill to make small, playable game changes without losing project context.

## Workflow

1. Read local project rules first: `AGENTS.md`, project state/runbooks if present, then relevant design docs, build presets, and nearby code.
   For playable implementation work, first run
   `node tools/ai.mjs context` when available. This prints the compact game
   context and profiles the pre-code context cost. If the facade is absent,
   fall back to `node tools/game_context/iteration_context.mjs`.
2. Select one task scope. For non-trivial playable, visual, pipeline, or
   tooling work, set profiling scope with
   `node tools/ai.mjs start <task-id> <iteration>` or explicitly state that
   profiling is unavailable/off. Collection stays passive and should not turn
   into profiler maintenance during normal game work. For sessions longer than
   60-90 minutes, write a short checkpoint: objective, proof, blocker, next.
   If you rely on `node tools/ai.mjs status` evidence, name any low/broken
   coverage.
3. State the selected runtime harness before implementation, including why it is allowed by local rules.
4. If the feature is based on a named reference, verify a reference deconstruction
   exists in the active project wiki before coding, and keep the
   reference-driven gameplay/UI/economy/balance lane locked until it is ready.
   In brief, the durable doc must: declare a study mode; gather evidence through
   the Source Ladder and Reference Evidence Board (observed frames, not secondary
   summaries) covering first screen, first-60-seconds and the 1-5 minute loop,
   control, response, reward, progression UI, and friction; record screen
   grammar, mechanics/balance notes, reward/UI hierarchy, borrow/avoid/copy-risk,
   and a mismatch audit against the current build; reach its Definition of Ready
   as a pre-code gate; and end in a Reference Digest (mode, sources checked,
   observed facts, current-build mismatch, borrow/avoid/copy-risk, next native
   screenshot/scenario proof). Run Reference Intake when the user names a ref or
   says the build is not like it. If any part is missing, state that the
   reference study is not ready for implementation and gather sources instead of
   coding. Parallel reference work is research-only; the implementation lane
   stays locked until the digest, mismatch audit, and next native proof exist.
   Full reference-deconstruction method and gates: `gamedesign/knowledge/reference_deconstruction.md`.
5. Identify the smallest playable slice that satisfies the request.
6. If the request names visual quality, FTUE, feel, audience testing, or a
   "casual" product target, capture or inspect the first playable screen before
   broadening scope. Do not add more locations, quests, systems, or content
   until the screen answers: where the player is, what they can do now, what
   changed after input, what reward they got, and why it looks like a game
   rather than a debug tool. Write the gate with
   `node tools/product_gate/review.mjs` or `node tools/ai.mjs gate` when that
   tool exists. For UI/visual/player-read work, first define live-state coverage
   from `gamedesign/knowledge/live_state_acceptance_matrix.md` and pass it to
   the gate with `--state-matrix`, `--require-state`, `--covered-state`, and
   `--not-covered-state`; a pass only proves those covered states. Use
   `node tools/ai.mjs close-slice` before handoff when the slice depends on
   product-read evidence.
   For visual-first work, write a 5-line session contract first: goal,
   non-goal, proof, stop condition, and likely files. Before coding, compare
   the current native screenshot or capture plan against the accepted fake
   shot/reference/art target and list mismatches. After meaningful render
   changes, capture a new screenshot and update the mismatch list before
   expanding content.
   Native PC scale/focus proof is part of the first playable UI gate, not a
   polish task. Before calling a native UI slice reviewable, prove it in a real
   desktop window using the project's reference-resolution scale layer
   (`nt_ui_scale` or equivalent logical viewport), not raw framebuffer-pixel
   layout. The review must explicitly answer: where am I, what is active, what
   can I click now, what is locked, and whether input/DevAPI enabled state
   matches the visual state. Future systems may be visible as roadmap tabs or
   locked/disabled affordances when that helps communicate breadth, but they
   must not look or behave like implemented active controls.
   Apply the AGENTS.md definition of done for playable/visual work: the slice
   is done when the native screen reaches the direction and quality bar the fake
   shot points to and the core moment feels right, NOT when probes are green.
   The fake shot is aspirational inspiration, not a pixel target -- the real game
   will never pixel-match it, so judge it qualitatively (composition,
   readability, art quality, mood/palette, "looks like a game"), never by image
   similarity. Judge the screenshot against that direction every visual
   iteration (continuous gate), not once at the end, and treat a screen that
   does not reach the bar as a failing build. Hold the visual-first freeze:
   while the first screen fails the gate, fix the screen to the reference bar
   before expanding systems, state, routes, content, or automation.
   Core-moment feel check (cheap, mandatory for the first playable slice; this
   skill owns "is it fun / does it play like the reference"): capture a short
   sequence or recording of the single core action (the cast, the catch, the
   hit, the upgrade). Confirm three felt criteria: (1) the action produces an
   immediate, readable response; (2) the payoff reads as one satisfying moment,
   not a silent state change; (3) in motion it resembles the named reference's
   feel, not just its static layout. If any fails, the core moment - not more
   content - is the next work. This is observation against a short capture, not
   a new manifest or provenance artifact.
7. For non-trivial work, use the iteration cycle in `references/iteration-cycle-playbook.md`.
8. Keep implementation close to existing engine and game patterns.
9. Avoid broad refactors unless the feature cannot be implemented safely without them.
10. Validate the primary runtime target first; validate secondary targets only when relevant or requested.
11. Capture evidence and report what changed, where to run it, and what was verified.
12. Before committing or handing off a prototype slice, run
    `node tools/product_gate/slice_hygiene.mjs --strict` with build/probe
    evidence, product gate, screenshot evidence, known red gates, and current
    profiler scope/status or an explicit profiling-unavailable note.
    Split a normal slice over 30 changed files unless the lead explicitly
    requested an end-of-experiment snapshot. Do not promise push until
    push/upstream state is checked.

## Discovery

Prefer local source-of-truth files over assumptions:

- Project rules: `AGENTS.md`
- Design docs: `gamedesign/`, `docs/design/`, `GDD.md`, or equivalent local folder
- Build/run: `CMakePresets.json`, `.vscode/tasks.json`, package scripts, engine docs
- Existing examples: `examples/`, `samples/`, nearby features, engine submodules

If naming differs, infer the equivalent directories from the repository.

## Implementation Rules

- Treat the local primary runtime as a hard platform gate, not a preference.
- Do not create, serve, validate, or pivot to a web prototype for playable work unless the current user request explicitly asks for web/mobile/browser output or the user approves a clearly stated exception.
- Before starting a web server, opening localhost for a web build, creating HTML/CSS/JS prototype files, or installing frontend/browser tooling, restate the explicit permission that allows it. If no such permission exists, continue on the primary native/game runtime path.
- Make one coherent gameplay increment at a time.
- Before writing code (game or tooling), climb the build-less ladder and stop at
  the first rung that works: (1) Does this need to exist at all? Cut speculative
  systems/scope first. (2) Can kept infra already do it -- an existing skill, the
  engine, DevAPI, schema-first state (`tools/state_codegen`), the taskboard, or
  an installed dependency? Reuse before building. (3) Is there a stdlib/native
  way (C stdlib, node/python builtins)? (4) Can it be one small edit in an
  existing file instead of a new module/tool/skill? (5) Only then write minimal
  new code. Never cut corners on input/state validation, the visual + core-moment
  bar, security, or anything the lead explicitly asked for -- those get full care
  regardless of how lazy the rest of the solution is.
- Write or infer a short task packet for work that spans design, code, visuals, or validation.
- Keep code agent-readable: clear names, small functions, limited comments.
- Preserve engine boundaries; do not edit submodules or vendored engine code unless explicitly requested.
- Do not silently wire asset-pack generation into every normal game build.
- When adding state, input, or rendering, include a simple way for the user to observe the behavior.
- Treat a failed screenshot/player-read review as a stop condition. Fix the
  core screen or loop before adding more data, balance, quests, routes, or
  technical systems.
- Build the core moment as a felt reveal, not a state transition. The payoff of
  the core action must read as one satisfying event (animation, feedback,
  juice), not appear silently in state. (Fishing failure: the catch had the
  right object checklist but no premium reveal, so it did not read as one event.)
- Product gate fail blocks feature/content expansion unless the lead explicitly
  accepts the debt for the current slice.
- Keep product/readability, game-loop/fun, art-source/assets, and
  technical/build gates separate. A build pass, clean asset audit, or scenario
  pass does not imply that the first screen is readable or the loop is fun.
- Do not implement a "like reference X" feature from a feature list alone.
  Translate the reference into player-facing screen grammar and visible
  runtime evidence first.
- Do not implement a reference-driven feature from memory unless sources are
  unavailable and the user accepts a clearly scoped memory-only exception.
- Keep reusable workflow in skills; keep project-specific facts and run commands in project docs.
- Do not commit stale fail audits as current proof. Refresh them, archive them
  as historical evidence, or call them out in the slice hygiene report and
  final/review notes.
- When the lead says a prototype was only a test run or is no longer active,
  stop game implementation and follow the latest explicit instruction for
  task/status disposition. Do not automatically drop tasks, close epics, or
  rewrite live status just because the word "test" was used. Preserve evidence
  historically, and move only reusable lessons into pipeline docs/skills.

## Build, Launch, And Release Tasks

When the work is about build/launch/release configuration itself:

1. Discover local build sources before inventing commands: `CMakePresets.json`,
   `.vscode/tasks.json`, `.vscode/launch.json`, package manager scripts,
   engine docs or examples.
2. Separate configure, build, run, release, serve, and package tasks.
3. Give important tasks clear names that show up in the user's IDE, e.g.
   `Build: native debug`, `Release: web`, `Pack: build game pack`. Make launch
   entries visible in the run/debug picker if the user expects to click them.
4. Keep asset-pack generation explicit unless the project intentionally
   requires automatic packs.
5. After editing build config: parse the JSON/YAML/TOML files, list available
   presets or tasks if the tool supports it, run the smallest affected build,
   and state output paths for executables, web artifacts, and packages.

## Validation

Use the project primary target from `AGENTS.md` or local docs. If none is defined, prefer the fastest native desktop/dev target.

For visual or interaction changes, run or inspect the game when possible and capture evidence in the project scratch area if one exists.

If the project exposes an agent playtest harness or runtime runbook, use it before ad hoc checks.

## References

- `references/iteration-cycle-playbook.md`: director/developer/designer/tester iteration loop, task packet, evidence, review, state update, and report format.
