# First Slice Visual Gate

Project: `little-lives` / Little Lives

Stop/go artifact for Milestone 1 (3D house sandbox).

## Session Contract

- Goal: A native 3D house lot where several Sims live by needs — readable at a
  glance as "The Sims in 3D" (people, furniture, need bars, mood).
- Non-goal: City/multiple lots (M2), careers/relationships (M3), real textured
  meshes (T0108), engine font text in HUD (T0107). No photoreal art.
- Proof: Native build/run + DevAPI smoke (decay/eat/work/build all pass) +
  framebuffer screenshot showing the room, Sims at furniture, and the need HUD.
- Stop condition: lead "ugly/unclear/not Sims" rejection, or product gate fail.
- Likely files: `src/clean_seed_main.c`, `state/game_state.schema.json`,
  `gamedesign/projects/little-lives/*`.

## Target

- Fake shot / visual target: described direction in `gdd.md` (Visual Target):
  bright, saturated "dollhouse" diorama, distinct candy-colored furniture, one
  cheerful Sim per home, need bars readable by color. No raster fake shot yet
  (image pipeline not run for M1; tracked under T0108 art pass).
- Reference digest path: n/a (mechanics-grounded in The Sims motives system,
  see `data/core_loop.json`).

## Current Native Proof

- Native build/run command:
  - `cmake --build build/_cmake/native-debug --target game_seed`
  - `build/game_seed/native-debug/game_seed.exe --devapi 9123 --window-size 1280x720`
- Current native screenshot: `gamedesign/projects/little-lives/reviews/first_slice_native.png`
- Screenshot-vs-target mismatch list:
  - [x] First-screen composition: 3D room reads clearly; OK. Minor: Sims read a
        bit small and cluster at the back/left wall (furniture is wall-aligned).
  - [x] Main action readability: need bars + buttons clear; commanding works.
  - [ ] UI text/readability: NO TEXT yet (color/icon only). Numbers via DevAPI.
        Debt -> T0107 (engine font/text renderer).
  - [ ] Visual style/appeal: procedural shape art (debug debt) -> T0108 meshes.
  - [x] Performance or capture blocker: none; framebuffer capture works.

## Live-State Matrix

- Matrix doc: `gamedesign/projects/little-lives/visual/live_state_acceptance_matrix.md`
- Required first proof states:
  - [x] `first_screen` — room + Sims + HUD on launch.
  - [x] `hud_visible` — need bars, portraits, money/day, buttons.
  - [x] `primary_action_ready` — selected Sim + need buttons / objects clickable.
  - [x] `primary_action_feedback` — need bar rises while using (DevAPI verified).
  - [ ] `reward_active` — work payout is the reward; not framed as a celebratory
        state in M1 (not-covered: deferred).
  - [x] `locked_or_disabled_state` — build place blocked when funds < price.
  - [ ] `transient_stress_state` — low-need red crisis exists in data; no special
        alert UI yet (not-covered: deferred to polish).

## Product-Read Gate (self-review)

- Verdict: PASS for an M1 functional 3D slice, with explicit art/text debt.
- Strict visual rubric (self-assessed, 1-5; shape-art debt acknowledged):
  - composition: 3 (clear room, Sims small/clustered)
  - readability: 3 (bars clear; no text labels yet)
  - ui_controls: 4 (portraits/need buttons/mode/work all present + clickable)
  - action_direction: 4 (lowest need + buttons point at next action)
  - art_quality: 2 (procedural debug shapes — tracked debt T0108)
  - audience_fit: 3 (reads as a toy dollhouse life sim)
- Honest note: not a strict-pass on art quality; M1 proves the *game* (systems +
  3D + loop), not final art. Strict `--visual-strict` art pass belongs to T0108
  after real meshes + T0107 text.

## Expansion Decision

- Decision: PROCEED to polish + next systems. M1 core loop is real and verified.
- Next smallest visual fixes: (1) engine font text for need labels + money/clock
  (T0107); (2) spread furniture / tune default camera so Sims read larger and
  more central; (3) real meshes (T0108).
- Allowed expansion next: M2 city scaffolding (T0109) and/or the T0107/T0108
  visual upgrades — lead's call.
