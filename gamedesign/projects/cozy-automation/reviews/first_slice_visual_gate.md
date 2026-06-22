# First Slice Visual Gate

Project: `cozy-automation` / Cozy Automation — "The Little Garden" first screen.

Stop/go artifact. Verdict pending until the first native screenshot is captured.

## Session Contract

- Goal: a readable, cozy first screen that proves the core loop end to end —
  bushes auto-generate berries that auto-route to a basket; spend 10 to plant a
  second bush; one locked greenhouse unlocks at 50 and raises the rate.
- Non-goal: more producers/grid, economy depth, audio, persistence, animation
  polish, web/mobile, or any system expansion before this gate passes.
- Proof: native debug build + `tools/cozy-automation/first_screen_smoke.py`
  driving the loop and capturing the live-state-matrix screenshots via the
  `frame.screenshot` backbuffer endpoint; product gate on the first screen.
- Stop condition: first screen passes the product-read gate (the five reads
  land and no blocker/major visual issue).
- Likely files: `src/cozy_automation_main.c`, `src/build_packs.c`,
  `state/game_state.schema.json`, `assets/raw/cozy/*.png`, `assets/fonts/cozy_ui.ttf`,
  `CMakeLists.txt`, `tools/cozy-automation/first_screen_smoke.py`.

## Target

- Fake shot / visual target path: none external. The lead chose "full real art
  now"; the visual target is the GDD art direction (bright, saturated, friendly,
  readable cozy garden) realized by the generated cozy sprite atlas
  (`assets/raw/cozy/*.png`) + the real engine UI font (Roboto). The native
  screenshot is judged against that art direction, not a pixel reference.
- Reference digest path, if any: n/a (in-house art direction; no named external ref).
- Art bible / style target path, if any: `gamedesign/projects/cozy-automation/gdd.md` (Art Direction).

## Current Native Proof

- Native build/run command:
  ```powershell
  cmake --preset native-debug
  cmake --build build/_cmake/native-debug --target game_seed   # auto-builds the pack + assets header
  python tools/cozy-automation/first_screen_smoke.py            # launches --devapi, drives the loop, captures states
  ```
  (Run from the project root so `assets/cozy_automation.ntpack` resolves.)
- Current native screenshot path or capture plan:
  `build/captures/cozy/first_screen.png` (+ per-state captures) written by the
  game's `frame.screenshot` endpoint (glReadPixels backbuffer PNG, ~0.95 MB,
  non-blank). All seven state shots captured by `first_screen_smoke.py` (20/20
  checks pass).
- Screenshot-vs-target mismatch list (vs the GDD cozy art direction):
  - [x] First-screen composition: PASS. Garden + HUD + CTA + lock all visible and
        ordered. Minor (non-blocking): the bottom third is empty grass and the
        three plots sit low-center; optional polish is to enlarge/raise the plot
        row to fill the frame.
  - [x] Main action readability: PASS. "Plant Bush" + berry-cost icon read clearly;
        the locked greenhouse shows a padlock + "Unlock at 50".
  - [x] UI text/readability: PASS. Real engine Roboto; counts, rate ("+N / tick"),
        and progress label are crisp.
  - [x] Visual style/appeal: PASS. Cohesive bright cozy illustrated sprites + sky
        background. Minor: the small berry icon reads slightly dark/purple (a quick
        single re-gen would brighten it).
  - [x] Performance or capture blocker: NONE. Atlas + font load; backbuffer capture
        works headless.

## Live-State Matrix

- Matrix doc: `gamedesign/projects/cozy-automation/visual/live_state_acceptance_matrix.md`
- Matrix JSON: `gamedesign/projects/cozy-automation/visual/live_state_acceptance_matrix.json`
- Capture mapping (from `first_screen_smoke.py`):
  - `first_screen` → `build/captures/cozy/first_screen.png` (scene + HUD + first action)
  - `hud_visible` → same shot (basket + berry count + rate + progress bar)
  - `primary_action_ready` → `primary_action_ready.png` (Plant Bush affordable at 10)
  - `primary_action_feedback` → `primary_action_feedback.png` (after plant: 2nd bush, rate 2)
  - `reward_active` → `reward_active.png` (greenhouse built after unlock, rate 5)
  - `locked_or_disabled_state` → `first_screen.png` (greenhouse locked "Unlock at 50", Plant disabled at 0)
  - `transient_stress_state` → `transient_auto_route.png` (berries drifting bush→basket)
  - `progression_panel_open` → not covered (no separate panel in first slice)
  - `modal_or_choice_open` → not covered (no modal in first slice)
  - `resume_or_reentry_state` → not covered (no resume/persistence in first slice)

## Product-Read Gate

- Gate command (fill the real screenshot path + scores after capture):
  ```powershell
  node tools/ai.mjs gate --project cozy-automation --task T0104 --surface desktop `
    --screenshot build/captures/cozy/first_screen.png --verdict pass --strict --visual-strict `
    --state-matrix gamedesign/projects/cozy-automation/visual/live_state_acceptance_matrix.json `
    --require-state first_screen `
    --covered-state first_screen:build/captures/cozy/first_screen.png `
    --covered-state hud_visible:build/captures/cozy/first_screen.png `
    --covered-state primary_action_ready:build/captures/cozy/primary_action_ready.png `
    --covered-state primary_action_feedback:build/captures/cozy/primary_action_feedback.png `
    --covered-state reward_active:build/captures/cozy/reward_active.png `
    --covered-state locked_or_disabled_state:build/captures/cozy/first_screen.png `
    --covered-state transient_stress_state:build/captures/cozy/transient_auto_route.png `
    --not-covered-state progression_panel_open:"no separate panel in first slice" `
    --not-covered-state modal_or_choice_open:"no modal in first slice" `
    --not-covered-state resume_or_reentry_state:"no resume/persistence in first slice" `
    --where "A cozy garden with three plots, a berry basket, and a Plant button" `
    --action "Plant a second berry bush for 10 berries; later unlock the greenhouse at 50" `
    --response "Berries tick up and drift to the basket; planting adds a bush and raises the rate" `
    --reward "A locked greenhouse promises a bigger rate jump at 50 berries" `
    --game-look "Bright, friendly, illustrated cozy garden with real font UI" `
    --visual-score composition=4 --visual-score readability=4 --visual-score ui_controls=4 `
    --visual-score action_direction=4 --visual-score art_quality=4 --visual-score audience_fit=4
  ```
- Gate artifact path: `gamedesign/projects/cozy-automation/reviews/product_read_gate_latest.json`
  (+ `product_read_gate_2026-06-22T11-27-24-973Z_desktop.{md,json}`)
- Verdict: **PASS** (2026-06-22)
- Blocking player-read questions:
  - [x] What can the player do in the first 5 seconds? Watch berries rise and route
        to the basket; plant a second bush when affordable (10).
  - [x] What is the reward/progress feedback? Berry counter + "+N / tick" rate +
        progress bar + a built greenhouse when the lock clears.
  - [x] What looks unclear/ugly/unreadable? Nothing blocking; only the two minor
        polish notes above (empty lower frame, slightly dark berry icon).
- Strict visual rubric (all six >= 4, no blocker/major → PASS):
  - [x] composition 4 / readability 5 / ui_controls 4 / action_direction 4 /
        art_quality 4 / audience_fit 5

## Expansion Decision

- Decision: **PASSED** — the first-screen product-read gate is green; the core loop
  is proven end to end (auto-route → plant → unlock).
- Optional minor polish (non-blocking, do opportunistically): tighten composition
  (enlarge/raise the plot row), brighten the berry icon, soften the progress-bar
  corner stretch.
- Exact expansion allowed next (one narrow slice at a time): the next producer /
  economy step (e.g. a 4th plot or a second resource), or real auto-route routing
  visuals — each behind its own product-read gate.
