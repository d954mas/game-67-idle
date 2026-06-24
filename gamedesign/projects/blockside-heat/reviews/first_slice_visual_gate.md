# First Slice Visual Gate

Project: `blockside-heat` / Blockside Heat

Fill this before broad runtime or content expansion. This is a stop/go artifact,
not a notes dump.

## Session Contract

- Goal: A native first screen that reads as a low-poly city action game with an
  immediate package job, nearby car, NPCs, HUD, and first action.
- Non-goal: Full open world, multiple districts, deep police AI, weapon
  inventory, economy expansion, web build, or copied GTA/IP-specific content.
- Proof: Native 1280x720 screenshot plus state/probe evidence compared against
  `visual/targets/blockside-heat-first-slice-target.png`.
- Stop condition: If first-screen readability or strict visual product gate
  fails, fix composition/readability/assets before adding content.
- Likely files: `src/clean_seed_main.c`, `state/game_state.schema.json`,
  `gamedesign/projects/blockside-heat/data/*.json`,
  `gamedesign/projects/blockside-heat/reviews/*`, project-local `assets/`.

## Target

- Fake shot / visual target path:
  `gamedesign/projects/blockside-heat/visual/targets/blockside-heat-first-slice-target.png`
- Reference digest path, if any: not used yet. This is genre-memory and
  original-target driven, not a named-reference deconstruction.
- Art bible / style target path, if any:
  `gamedesign/projects/blockside-heat/gdd.md`

## Current Native Proof

- Native build/run command: configure with `cmake --preset native-debug` if
  needed, build with `cmake --build --preset native-debug`, run
  `build/_cmake/native-debug/game_seed.exe --devapi <port> --window-size 1280x720`.
- Current native screenshot path or capture plan: capture the runtime
  screenshot set through `tools/blockside-heat/capture_states.py`; current
  first-screen screenshot is
  `tmp/blockside-heat/first-native-screenshot-latest.png`.
- Screenshot-vs-target mismatch list:
  - [x] First-screen composition now reads as an intersection instead of
        floating assets.
  - [x] UI text/readability uses engine text renderer labels and packed font.
  - [x] Capture blocker resolved with `game.capture.framebuffer`.
  - [x] Main action readability: the bright yellow mission pad marks the
        package route in world space.
  - [x] Visual density/appeal: road markings and extra reused buildings/props
        make the block read as a prototype game space instead of a debug plane.

## Visual Critic Packet

- Packet command:
  ```powershell
  node tools/ai.mjs critic --project blockside-heat --task T0112 --surface desktop --screenshot tmp/blockside-heat/first-native-screenshot.png --target gamedesign/projects/blockside-heat/visual/targets/blockside-heat-first-slice-target.png --brief "Casual native PC first slice: blocky city pickup job, car entry, package marker, NPC pressure, readable HUD, Roblox-like low-poly style." --output gamedesign/projects/blockside-heat/reviews/first_slice_visual_critic_packet.md --json-output gamedesign/projects/blockside-heat/reviews/first_slice_visual_critic_packet.json
  ```
- Packet Markdown path: `gamedesign/projects/blockside-heat/reviews/first_slice_visual_critic_packet.md`
- Packet JSON path: `gamedesign/projects/blockside-heat/reviews/first_slice_visual_critic_packet.json`
- Use this packet for a self-review or separate critic pass before writing the strict product gate verdict.

## Live-State Matrix

- Matrix doc: `gamedesign/projects/blockside-heat/visual/live_state_acceptance_matrix.md`
- Matrix JSON: `gamedesign/projects/blockside-heat/visual/live_state_acceptance_matrix.json`
- Required first proof states:
  - [ ] `first_screen`
  - [ ] `hud_visible`
  - [ ] `primary_action_ready`
  - [ ] `primary_action_feedback`
  - [ ] `reward_active`
  - [ ] `locked_or_disabled_state`
  - [ ] `transient_stress_state`
- Any required state not captured by the current screenshot must be passed as
  `--not-covered-state <tag>:"<reason>"`, not silently implied.

## Product-Read Gate

- Gate command:
  ```powershell
  node tools/ai.mjs gate --project blockside-heat --task T0112 --surface desktop --screenshot tmp/blockside-heat/first-native-screenshot.png --verdict fail --strict --visual-strict --state-matrix gamedesign/projects/blockside-heat/visual/live_state_acceptance_matrix.json --require-state first_screen --covered-state first_screen:tmp/blockside-heat/first-native-screenshot.png --covered-state hud_visible:tmp/blockside-heat/hud-zoom.png --covered-state primary_action_ready:tmp/blockside-heat/first-native-screenshot.png --covered-state primary_action_feedback:tmp/blockside-heat/pickup-feedback.png --covered-state reward_active:tmp/blockside-heat/job-complete.png --covered-state transient_stress_state:tmp/blockside-heat/pursuer-stress.png --not-covered-state progression_panel_open:"not in this first slice; next job lock is HUD-only" --not-covered-state modal_or_choice_open:"not in this first slice yet" --not-covered-state resume_or_reentry_state:"not in this first slice yet" --where "one compact city block pickup job" --action "enter car or pick up the package" --response "package pickup raises wanted pressure and starts pursuer" --reward "cash reward and next job lock after drop-off" --game-look "low-poly city, car, NPCs, mission marker, HUD" --problem "<specific visual/player-read problem>" --next "<smallest next visual fix>" --visual-score composition=1 --visual-score readability=1 --visual-score ui_controls=1 --visual-score action_direction=1 --visual-score art_quality=1 --visual-score audience_fit=1 --visual-issue blocker:readability:"<concrete issue>"
  ```
- Gate artifact path:
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`
- Verdict: pass after
  `tmp/blockside-heat/first-native-screenshot-latest.png`;
  `gamedesign/projects/blockside-heat/reviews/product_read_gate_latest.json`
  records all six strict visual scores at 4 with only minor art-quality debt.
- Blocking player-read questions:
  - [x] What can the player do in the first 5 seconds?
  - [x] What is the reward/progress feedback?
  - [x] What looks unclear, ugly, unreadable, or unlike the target?
- Strict visual rubric:
  - [x] composition score 1-5
  - [x] readability score 1-5
  - [x] ui_controls score 1-5
  - [x] action_direction score 1-5
  - [x] art_quality score 1-5
  - [x] audience_fit score 1-5
  - [x] visual issues use severity `blocker`, `major`, or `minor`
  - [x] pass requires all six scores >= 4 and no blocker/major issue

## Expansion Decision

- Decision: first native slice passed strict product-read gate. Content/system
  expansion may continue in the next narrow slice.
- If blocked, smallest next fix: not blocked for this slice; keep minor visual
  density debt visible while expanding.
- If passed, exact content/system expansion allowed next: second job beat or
  improved vehicle/NPC behavior, not a second district.
