# Playable Feature Gates And Build Reference

Load this reference when a game feature touches product feel, visual quality,
named references, build/launch/release configuration, or prototype handoff.

## Reference-Driven Work

If the feature is based on a named reference, verify reference deconstruction
exists in the active project wiki before coding. Keep the reference-driven
gameplay/UI/economy/balance lane locked until it is ready.

The durable doc must:

- declare study mode;
- gather evidence through the Source Ladder and Reference Evidence Board;
- use observed frames, not secondary summaries;
- cover first screen, first-60-seconds, 1-5 minute loop, control, response,
  reward, progression UI, and friction;
- record screen grammar, mechanics/balance notes, reward/UI hierarchy,
  borrow/avoid/copy-risk, and a mismatch audit against the current build;
- reach Definition of Ready as a pre-code gate;
- end in a Reference Digest: mode, sources checked, observed facts,
  current-build mismatch, borrow/avoid/copy-risk, next native screenshot or
  scenario proof.

Run Reference Intake when the user names a ref or says the build is not like it.
If any part is missing, state that the reference study is not ready for
implementation and gather sources instead of coding. Parallel reference work is
research-only; implementation waits for the digest, mismatch audit, and next
native proof.

Reference-driven work is not ready for implementation until the Reference
Digest, mismatch audit, and next native proof exist.

Full method: `gamedesign/knowledge/reference_deconstruction.md`.

## Product And Visual Gates

For visual quality, FTUE, feel, audience testing, or a casual product target,
inspect the first playable screen before broadening scope. Do not add more
locations, quests, systems, or content until the screen answers:

- where the player is;
- what they can do now;
- what changed after input;
- what reward they got;
- why it looks like a game rather than a debug tool.

Write the gate with `node tools/product_gate/review.mjs` or
`node tools/product_gate/review.mjs`. For UI/visual/player-read work, define live-state
coverage from `gamedesign/knowledge/live_state_acceptance_matrix.md` and pass
it with `--state-matrix`, `--require-state`, `--covered-state`, and
`--not-covered-state`; a pass only proves covered states. Use
`node tools/product_gate/close_slice.mjs` before handoff when the slice depends on
product-read evidence.

If the task carries `lead-rejection` or lead-rejected wording, strict closeout
must also pass `--resolved-rejection "<exact rejected issue and proof>"`.

For visual-first work, write the 5-line session contract: goal, non-goal, proof,
stop condition, likely files. Before coding, compare the current native
screenshot or capture plan against the accepted fake shot/reference/art target
and list the mismatch list. After meaningful render changes, capture a new
screenshot and update the mismatch list before expanding content.

Native PC scale/focus proof is part of the first playable UI gate. Before a
native UI slice is reviewable, prove it in a real desktop window using the
project's reference-resolution scale layer (`nt_ui_scale` or equivalent logical
viewport), not raw framebuffer-pixel layout.

Apply the `AGENTS.md` definition of done for playable/visual work: the slice is
done when the native screen reaches the direction and quality bar the fake shot
points to and the core moment feels right, not when probes are green. The fake
shot is aspirational inspiration, not a pixel target; judge composition,
readability, art quality, mood/palette, and "looks like a game", never image
similarity.

Product gate fail blocks feature/content expansion unless the lead explicitly
accepts the debt for the current slice.

## Core-Moment Feel Check

For the first playable slice, capture a short sequence or recording of the
single core action. Confirm:

1. the action produces an immediate, readable response;
2. the payoff reads as one satisfying moment, not a silent state change;
3. in motion it resembles the named reference's feel, not just static layout.

If any fails, the core moment is the next work, not more content.

## Implementation Rules

- Treat the local primary runtime as a hard platform gate.
- Do not create, serve, validate, or pivot to a web prototype for playable work
  unless the current user request explicitly asks for web/mobile/browser output
  or the user approves a clearly stated exception.
- Before starting a web server, opening localhost for a web build, creating
  HTML/CSS/JS prototype files, or installing frontend/browser tooling, restate
  the explicit permission that allows it.
- Make one coherent gameplay increment at a time.
- Before writing code, climb the build-less ladder: cut speculative scope; reuse
  existing skill/engine/DevAPI/schema/taskboard/dependency; use stdlib/native
  APIs; make one small edit; only then write minimal new code.
- Keep product/readability, game-loop/fun, art-source/assets, and
  technical/build gates separate.
- Preserve engine/vendor boundaries.
- Do not silently wire asset-pack generation into every normal game build.
- Include a simple way for the user to observe new state, input, or rendering.
- Do not implement a "like reference X" feature from a feature list alone;
  translate the reference into player-facing screen grammar and visible runtime
  evidence first.
- Do not commit stale fail audits as current proof. Refresh them, archive them,
  or call them out in slice hygiene and final notes.
- When the lead says a prototype was a test run or is no longer active, stop
  game implementation and follow the latest explicit task/status instruction.

## Build, Launch, And Release Tasks

When the work is about build/launch/release configuration:

1. Discover local build sources before inventing commands: `CMakePresets.json`,
   `.vscode/tasks.json`, `.vscode/launch.json`, package manager scripts, engine
   docs or examples.
2. Separate configure, build, run, release, serve, and package tasks.
3. Give important tasks clear IDE names such as `Build: native debug`,
   `Release: web`, `Pack: build game pack`.
4. Keep asset-pack generation explicit unless the project intentionally requires
   automatic packs.
5. After editing config, parse JSON/YAML/TOML, list available presets/tasks if
   supported, run the smallest affected build, and state output paths.

## Slice Handoff

Before committing or handing off a prototype slice, run:

```powershell
node tools/product_gate/slice_hygiene.mjs --strict `
  --build-evidence "<build command/result>" `
  --probe-evidence "<probe/scenario result>" `
  --product-gate "<gate.json>" `
  --screenshot "<latest screenshot>"
```

Split a normal slice over 30 changed files unless the lead explicitly requested
an end-of-experiment snapshot. Do not promise push until push/upstream state is
checked. If changed review/audit files still contain fail verdicts, refresh
them, archive them as historical evidence, or pass `--known-red-gate` and name
the debt in final notes.
