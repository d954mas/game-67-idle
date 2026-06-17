---
id: T0013
title: Add reusable live-state UI acceptance matrix gate
status: todo
epic: E001
priority: P1
tags: [pipeline, ui, visual-gate, acceptance, regression, reusable]
created: 2026-06-17
updated: 2026-06-17
---

## What

Add a reusable UI acceptance matrix pattern for all future games. The matrix
defines which states must be captured before a prototype can be called visually
acceptable or product-readable.

Voxelheim is the first fixture/example because it exposed the failure, but the
deliverable must live in reusable process/tooling docs and be usable by the next
game without copying Voxelheim terms.

## Done when

- [ ] A reusable acceptance-matrix template exists outside the Voxelheim project
      folder, suitable for new game prototypes.
- [ ] Template requires generic state categories: first screen, primary action
      ready, action feedback/reward active, modal/choice open, progression or
      upgrade panel open, locked/disabled state, returning/offline/resume state
      when applicable, and stress state with transient effects active.
- [ ] Each state requires player-read answers: what can I do, what changed,
      what reward did I get, what is the next reason to continue.
- [ ] Each state names expected evidence: native screenshot, zoom/readability
      crop, probe/automation command, and asset-edge audit when UI sprites or
      generated/chroma-key assets are involved.
- [ ] Voxelheim gets a small fixture matrix that instantiates the reusable
      template, proving the template catches the missed Gate CTA + Blueprints +
      floater state.
- [ ] Purple/chroma edge audit is generalized as source-to-runtime edge audit
      for generated UI buttons/panels/icons.

## Open questions

- Should the reusable matrix be required for every visual/UI pass, or only when
  the pass is strict/final/handoff?

## Log
- 2026-06-17 created from process retrospective after a narrow offline/reward
  screenshot missed the live Gate CTA + Blueprints + floater overlap state.
- 2026-06-17 lead clarified the fix must be universal for all future games, not
  Voxelheim-specific. Task reframed as reusable matrix template plus Voxelheim
  fixture.
