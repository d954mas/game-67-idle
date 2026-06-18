# First Slice Visual Gate

Project: `backrooms-liminal` / Backrooms Liminal

Fill this before broad runtime or content expansion. This is a stop/go artifact,
not a notes dump.

## Session Contract

- Goal: Native PC first-person backrooms corridor where the player can read the
  objective, see real light/shadow/fog, collect the fuse, and feel pursued on
  the return path.
- Non-goal: Full procedural maze, combat, inventory, web/mobile build, or
  release-final canon/lore.
- Proof: `build/captures/backrooms_first_screen.png` and
  `build/captures/backrooms_after_fuse.png`, plus strict product gate and UI
  readability zoom.
- Stop condition: If the first screenshot looks like a debug tool, lacks
  yellow corridor/fluorescent light/shadows, or fails player-read questions,
  feature/content expansion freezes until the screen is fixed.
- Likely files: `src/clean_seed_main.c`,
  `gamedesign/projects/backrooms-liminal/gdd.md`,
  `gamedesign/projects/backrooms-liminal/data/core_loop.json`,
  `tools/devapi/smoke.py`, and this review file.

## Target

- Fake shot / visual target path:
  `gamedesign/projects/backrooms-liminal/gdd.md#visual-target`
- Reference digest path, if any:
  `gamedesign/projects/backrooms-liminal/gdd.md#reference-digest`
- Art bible / style target path, if any:
  `gamedesign/projects/backrooms-liminal/gdd.md#art-direction-stub`

## Current Native Proof

- Native build/run command:
  `cmake --build --preset native-debug --target game_seed`
  then `build/game_seed/native-debug/game_seed.exe --devapi 9123 --window-size 1280x720`
- Current native screenshot path or capture plan:
  Use `tools/devapi/smoke.py` or a focused DevAPI scenario to capture
  `build/captures/backrooms_first_screen.png` and
  `build/captures/backrooms_after_fuse.png`.
- Screenshot-vs-target mismatch list:
  - [x] First-screen composition: current seed is 2D shapes, not a first-person corridor.
  - [x] Main action readability: current seed says cycle seed, not find fuse/escape.
  - [x] UI text/readability: current seed has no horror HUD or survival controls.
  - [x] Visual style/appeal: current seed is bright debug art, not liminal horror.
  - [ ] Performance or capture blocker: unknown until native build/smoke.

## Visual Critic Packet

- Packet command:
  ```powershell
  node tools/ai.mjs critic --project backrooms-liminal --task <task-id> --surface desktop --screenshot <native-screenshot.png> --target <fake-shot-or-target-path> --brief "<casual audience, core action, target style>" --output gamedesign/projects/backrooms-liminal/reviews/first_slice_visual_critic_packet.md --json-output gamedesign/projects/backrooms-liminal/reviews/first_slice_visual_critic_packet.json
  ```
- Packet Markdown path: `gamedesign/projects/backrooms-liminal/reviews/first_slice_visual_critic_packet.md`
- Packet JSON path: `gamedesign/projects/backrooms-liminal/reviews/first_slice_visual_critic_packet.json`
- Use this packet for a self-review or separate critic pass before writing the strict product gate verdict.

## Live-State Matrix

- Matrix doc: `gamedesign/projects/backrooms-liminal/visual/live_state_acceptance_matrix.md`
- Matrix JSON: `gamedesign/projects/backrooms-liminal/visual/live_state_acceptance_matrix.json`
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
  node tools/ai.mjs gate --project backrooms-liminal --task T0001 --surface desktop --screenshot build/captures/backrooms_first_screen.png --verdict pass --strict --visual-strict --state-matrix gamedesign/projects/backrooms-liminal/visual/live_state_acceptance_matrix.json --require-state first_screen --covered-state first_screen:build/captures/backrooms_first_screen.png --covered-state hud_visible:build/captures/backrooms_readability.png --covered-state primary_action_ready:build/captures/backrooms_first_screen.png --not-covered-state modal_or_choice_open:"not in this first slice" --not-covered-state resume_or_reentry_state:"not in this first slice" --where "Backrooms corridor with exit behind and fuse objective ahead" --action "Move with WASD, turn with arrows, press E at fuse/exit, toggle flashlight with F" --response "Fuse pickup changes objective, exit lighting, fear pressure, and silhouette state" --reward "Power the exit and escape before fear peaks" --game-look "3D yellow liminal corridor with textured walls, fluorescent light, fog, shadow, and horror HUD" --visual-score composition=4 --visual-score readability=4 --visual-score ui_controls=4 --visual-score action_direction=4 --visual-score art_quality=4 --visual-score audience_fit=4
  ```
- Gate artifact path:
- Verdict: pending
- Blocking player-read questions:
  - [ ] What can the player do in the first 5 seconds?
  - [ ] What is the reward/progress feedback?
  - [ ] What looks unclear, ugly, unreadable, or unlike the target?
- Strict visual rubric:
  - [ ] composition score 1-5
  - [ ] readability score 1-5
  - [ ] ui_controls score 1-5
  - [ ] action_direction score 1-5
  - [ ] art_quality score 1-5
  - [ ] audience_fit score 1-5
  - [ ] visual issues use severity `blocker`, `major`, or `minor`
  - [ ] pass requires all six scores >= 4 and no blocker/major issue

## Expansion Decision

- Decision: blocked until first native screenshots and strict gate are captured.
- If blocked, smallest next fix: improve corridor lighting/readability before
  adding maze generation or extra systems.
- If passed, exact content/system expansion allowed next: one route-choice fork
  or one entity behavior, not both.
