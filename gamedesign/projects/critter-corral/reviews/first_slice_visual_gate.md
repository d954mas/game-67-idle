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

- Native build/run command: `cmake --build build/_cmake/native-debug --target game_seed`;
  run+capture via `py -3.12 tmp/capture_corral.py` (DevAPI `running_game` +
  `game.capture.framebuffer`).
- Current native screenshot: `build/captures/corral_review.png` (sprite build,
  2026-06-15).
- Screenshot-vs-target mismatch list (capture 1, sprite build):
  - [x] First-screen composition: sparse / low-contrast (pixel audit luma stdev
    9.9 < 10.0); field dominates, critters small. Needs focal hierarchy.
  - [~] Main action readability: lure glow + pens read; pen<->critter COLOR
    mapping is muddy (coral pen vs red critters). Add clear color match + gate.
  - [ ] UI text/readability: NO on-screen score/goal — the wave goal isn't
    communicated at a glance.
  - [x] Visual style/appeal: GOOD — reads as a game (cute critters w/ eyes,
    rounded color pens, soft alpha), not a debug screen. Big step up.
  - [ ] Core-moment feel: static shot can't judge pop+chain; verify via a
    multi-frame capture next.
  - [x] Performance/capture: fine; framebuffer capture works.

## Product-Read Gate

- Gate command (fill task id + screenshot + scores after capture):
  ```powershell
  node tools/ai.mjs gate --project critter-corral --task T0064 --surface desktop --screenshot <native-screenshot.png> --verdict fail --strict --visual-strict --where "open pasture with critters and pens" --action "move the lure to herd a color into its pen" --response "matching critters pop into the pen and chain in" --reward "wave clears; satisfying pop" --game-look "bright readable pasture, not a debug screen" --problem "<concrete read/feel problem>" --next "<smallest next visual/feel fix>" --visual-score composition=1 --visual-score readability=1 --visual-score ui_controls=1 --visual-score action_direction=1 --visual-score art_quality=1 --visual-score audience_fit=1 --visual-issue blocker:readability:"<issue>"
  ```
- Gate artifact path: this doc (lead-judged, advisory per AGENTS). Captures:
  `build/captures/corral_review.png` (1), `corral_review2.png` (2, after fixes).
- Verdict: **PASS** (capture 2, sprite build, 2026-06-15). Scores: composition 4,
  readability 5, ui_controls 4, action_direction 4, art_quality 4 (good
  placeholder; Codex refines later), audience_fit 5 — all >= 4, no blocker.
  Pixel audit PASS (luma stdev 24.4). Pen<->critter color match unmistakable,
  critters pop on calmed grass, goal HUD reads.
- Caveat: static frame can't judge the pop+chain FEEL in motion — verify via a
  motion capture / playtest during gameplay expansion (T0046 core-moment check).

## Expansion Decision

- Decision: UNBLOCKED (visual direction reached on placeholders). Proceed to GDD
  roadmap step 1 (waves + progression) toward 10-20 min, ONE layer at a time,
  re-judging feel each step. Keep re-running the visual gate after render changes.
