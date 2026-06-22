# Cozy Automation

Project wiki for the active prototype `cozy-automation`.

## Concept

A small cozy automation game: place simple producers on a small grid; they generate and auto-route a resource you spend to unlock the next producer/step.

## Stage 0 Startup Gate

- Native-first implementation only until an explicit web/mobile exception is approved.
- First playable slice must name a fake shot, product-read gate, and native screenshot proof before broad runtime work.
- Visual-first session contract is required before runtime visual work: goal,
  non-goal, proof, stop condition, likely files.
- Before visual/runtime coding, compare current native screenshot or capture
  plan against the accepted fake shot/target and write a mismatch list.
- Beautiful/casual/generated-UI/fake-shot slices use the strict visual product
  gate rubric: six visual scores and blocker/major issue reporting.
- Keep reusable process learnings in `gamedesign/knowledge/`; keep project-specific facts here.

## First Slice

- Define the smallest playable loop in `gdd.md`.
- Fill `reviews/first_slice_visual_gate.md` before broad runtime work.
- Fill `visual/live_state_acceptance_matrix.md` before any broad UI/visual pass.
- For visually important slices, create the critic packet named in that gate
  before writing the strict product gate verdict.
- Capture visual/product proof in `reviews/` before expanding content.
- Product-read gates must use `visual/live_state_acceptance_matrix.json`
  with explicit covered or not-covered states.
- Update screenshot-vs-target mismatches after meaningful render changes.
