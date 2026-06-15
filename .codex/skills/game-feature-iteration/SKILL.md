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
2. Select one task scope. Profiling is optional and passive: for long work you
   may run `node tools/ai.mjs start <task-id> <iteration>`, but it is never
   required to begin work and never blocks a handoff. Use `node tools/ai.mjs
   status` only when the task is explicitly about AI workflow or a requested
   retrospective; if you rely on its evidence, name any low/broken coverage.
3. State the selected runtime harness before implementation, including why it is allowed by local rules.
4. If the feature is based on a named reference, verify a reference deconstruction
   exists before coding. It must include source evidence,
   first-10-seconds and first-60-seconds actions, 1-5 minute loop,
   screen grammar, mechanics/balance notes, reward/UI hierarchy,
   borrow/avoid/copy-risk, and mismatch audit against the current build.
   For a central gameplay reference, the deconstruction must be
   observation-first and include official/store/trailer visuals, gameplay
   video/walkthrough or a long screenshot sequence, and a current build capture.
   It must also follow the four-pass method: source packet, player transcript,
   systems extraction, and translation gate. If the translation gate does not
   name the next screenshot/scenario proof, do not code yet. If the user asks
   whether the ref was studied or rejects the current gameplay as unlike the
   ref, answer from the deconstruction doc; if the doc cannot answer, improve
   the study before coding. Do not claim a gameplay ref was studied unless the
   doc records source links/paths, checked dates, timestamps/frames or
   screenshot ids for observed beats, supporting guide/review/deconstruction
   sources used for balance claims, and the current native capture. Treat the
   Reference Study Definition of Ready as a pre-code gate: if mode, doc path,
   Reference Lock, source matrix, current native capture, observation ledger,
   borrow/avoid/copy-risk, current-build mismatch, or next native proof are
   missing, state that the reference study is not ready for implementation and
   gather sources or ask for user material instead of coding.
   The deconstruction must also record the Source Ladder before conclusions:
   user-provided material, official/store/trailer visuals, raw gameplay
   video/walkthrough or long screenshot sequence, then supporting guides,
   reviews, lectures, deconstructions, wikis, or community notes. Secondary
   summaries cannot replace raw gameplay evidence for first-screen, control,
   loop, reward, or UI hierarchy claims.
   The deconstruction must include a Reference Evidence Board for central/deep
   refs: at least six cited frames/screenshots for first screen, first input,
   visible response, reward feedback, upgrade/progression UI, and
   friction/blocked state, plus raw gameplay/walkthrough timing evidence. If
   those frames/timestamps cannot be named, the reference is not studied enough
   to implement.
   If the user says the current build is not like the reference, run Reference Intake
   before defending or making another pass: state the reference question, mode,
   durable doc path, source packet, current native capture plan/path,
   no-coding/no-final-art boundary, and first proof. Treat observed and
   user-provided claims as usable; inferred/unknown claims must stay out of
   implementation unless accepted as a narrow exception.
   Before coding resumes, provide a Reference Digest with the mode, sources
   checked, 3-5 observed facts, current-build mismatch, borrow/avoid/copy-risk,
   and next native screenshot/scenario proof. If the digest cannot be written
   from the durable doc, the reference is not implementation-ready.
   Parallel reference work is research-only: source collection, frame capture,
   visible transcript, and mismatch capture may run beside unrelated setup, but
   the reference-driven gameplay/UI/economy/balance implementation lane stays
   locked until the digest, mismatch audit, and next native proof exist.
5. Identify the smallest playable slice that satisfies the request.
6. If the request names visual quality, FTUE, feel, audience testing, or a
   "casual" product target, capture or inspect the first playable screen before
   broadening scope. Do not add more locations, quests, systems, or content
   until the screen answers: where the player is, what they can do now, what
   changed after input, what reward they got, and why it looks like a game
   rather than a debug tool. Write the gate with
   `node tools/product_gate/review.mjs` or `node tools/ai.mjs gate` when that
   tool exists. Use `node tools/ai.mjs close-slice` before handoff when the
   slice depends on product-read evidence.
   For visual-first work, write a 5-line session contract first: goal,
   non-goal, proof, stop condition, and likely files. Before coding, compare
   the current native screenshot or capture plan against the accepted fake
   shot/reference/art target and list mismatches. After meaningful render
   changes, capture a new screenshot and update the mismatch list before
   expanding content.
   Apply the AGENTS.md definition of done for playable/visual work: the slice
   is done when the native screenshot matches the named fake shot/reference and
   the core moment feels right, NOT when probes are green. Score the screenshot
   against the fake shot every visual iteration (continuous gate), not once at
   the end, and treat divergence as a failing build. Hold the visual-first
   freeze: while the first screen fails the gate, fix the screen to the
   reference bar before expanding systems, state, routes, content, or
   automation.
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
    evidence, product gate, screenshot evidence, and known red gates. A profiler
    guard (`--profile-guard`) is optional/advisory and never blocks the slice.
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
- Do not implement a "like reference X" feature from a feature list alone.
  Translate the reference into player-facing screen grammar and visible
  runtime evidence first.
- Do not implement a reference-driven feature from memory unless sources are
  unavailable and the user accepts a clearly scoped memory-only exception.
- Keep reusable workflow in skills; keep project-specific facts and run commands in project docs.
- Do not commit stale fail audits as current proof. Refresh them, archive them
  as historical evidence, or call them out in the slice hygiene report and
  final/review notes.

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
