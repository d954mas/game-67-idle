# First Slice Visual Gate

Project: `dragon-grove` / Dragon Grove

## Session Contract

- Goal: First native screen reads as an original dragon-grove merge puzzle with
  one obvious merge action and visible restore progress.
- Non-goal: No full clone, no store economy, no timers, no monetization, no
  broad collection systems, no final/reference art parity.
- Proof: Native screenshot/capture plus task log showing build and one merge
  scenario evidence.
- Stop condition: Product/readability fail, lead says it is not a game/UX is
  wrong, or Y-up invariant is violated.
- Likely files: `src/clean_seed_main.c`, `state/game_state.schema.json`,
  `gamedesign/projects/dragon-grove/*`, `tasks/active/T0029-*.md`.

## Target

- Fake shot / visual target path: `gamedesign/projects/dragon-grove/reference_digest.md`
  plus GDD art stub; no generated/final fake shot accepted yet.
- Reference digest path: `gamedesign/projects/dragon-grove/reference_digest.md`
- Art bible / style target path: none for this first tiny slice.

## Current Native Proof

- Native build/run command: `cmake --build --preset native-debug --target game_seed`
- Current native screenshot path or capture plan: run native executable and
  capture first screen under `tmp/dragon-grove/`.
- Screenshot-vs-target mismatch list:
  - [ ] First-screen composition: current clean seed has no grove grid.
  - [ ] Main action readability: current clean seed has no merge-ready group.
  - [ ] UI text/readability: current clean seed has no merge/restore HUD.
  - [ ] Visual style/appeal: current clean seed has no dragon-grove fantasy.
  - [ ] Performance or capture blocker: native capture command still needs proof.

## Live-State Matrix

- Matrix JSON: `gamedesign/projects/dragon-grove/visual/live_state_acceptance_matrix.json`
- Required first proof states:
  - [ ] `first_screen`
  - [ ] `hud_visible`
  - [ ] `primary_action_ready`
  - [ ] `primary_action_feedback`
  - [ ] `locked_or_disabled_state`

## Product-Read Gate

- Gate artifact path: pending native screenshot.
- Verdict: pending
- Blocking questions:
  - [ ] What can the player do in the first five seconds?
  - [ ] What changed after the merge?
  - [ ] What reward/progress is visible?

## Expansion Decision

- Decision: blocked until first native merge screen exists and is reviewed.
- If blocked, smallest next fix: make the first merge action and reward readable.
- If passed, exact expansion allowed next: add one object family or one better
  art pass, not broad economy/content.
