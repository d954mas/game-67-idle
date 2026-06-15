# First Slice Visual Gate

Project: `critter-corral` / Critter Corral · Task: T0064

Fill this before broad runtime or content expansion. Stop/go artifact.

## Session Contract

- Goal: a playable CORE MOMENT on primitives — a lure herds wandering critters;
  a matching color pops into its pen with a same-color chain + juice; clear one
  calm 2-color wave.
- Non-goal: art assets, waves/progression/meta, extra tools, web/mobile.
- Proof: a native screenshot of the core moment + a short captured playtest of
  herding one color in, judged against the concept fake-shot DIRECTION.
- Stop condition: the visual/feel gate fails (screen does not reach the
  direction, OR the pop+chain does not read as one satisfying event) -> fix the
  screen/feel before ANY expansion (visual-first freeze).
- Likely files: `src/clean_seed_main.c` (or a new `src/` game unit), maybe
  `state/game_state.schema.json`, `CMakeLists.txt`.

## Target

- Fake shot / visual target: `../concept.md` "First-screen fake-shot DIRECTION"
  (text direction; actual fake-shot image deferred to Codex). Judge by DIRECTION
  (mood/palette/composition/readability/"looks like a game"), never pixel-match.
- Reference digest: `references/sort_puzzle_deconstruction.md`.
- Art bible / style target: none yet (primitives; bright, high-contrast,
  friendly, tactile).

## Current Native Proof

- Native build/run command: TBD — pending runtime-harness map (cmake preset
  `native-debug` builds `game_seed`; run + screenshot/DevAPI capture path to be
  confirmed, then filled here).
- Current native screenshot path or capture plan: TBD (capture a frame of the
  core moment via the DevAPI/screenshot path once known).
- Screenshot-vs-target mismatch list (fill after first capture):
  - [ ] First-screen composition:
  - [ ] Main action readability (is the lure->herd->pen read obvious?):
  - [ ] UI text/readability:
  - [ ] Visual style/appeal (does it read as a game, not a debug screen?):
  - [ ] Core-moment feel (does the pop+chain land as ONE satisfying event?):
  - [ ] Performance or capture blocker:

## Product-Read Gate

- Gate command (fill task id + screenshot + scores after capture):
  ```powershell
  node tools/ai.mjs gate --project critter-corral --task T0064 --surface desktop --screenshot <native-screenshot.png> --verdict fail --strict --visual-strict --where "open pasture with critters and pens" --action "move the lure to herd a color into its pen" --response "matching critters pop into the pen and chain in" --reward "wave clears; satisfying pop" --game-look "bright readable pasture, not a debug screen" --problem "<concrete read/feel problem>" --next "<smallest next visual/feel fix>" --visual-score composition=1 --visual-score readability=1 --visual-score ui_controls=1 --visual-score action_direction=1 --visual-score art_quality=1 --visual-score audience_fit=1 --visual-issue blocker:readability:"<issue>"
  ```
- Gate artifact path:
- Verdict: pending
- Pass requires all six scores >= 4 and no blocker/major issue.

## Expansion Decision

- Decision: blocked until the core moment is built, captured, and the gate +
  feel check pass.
- If blocked, smallest next fix: build the core moment on primitives, capture,
  judge.
- If passed, exact expansion allowed next: GDD roadmap step 1 (waves +
  progression) — one layer at a time.
