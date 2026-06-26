# Quality Rules

Quality is a set of small rules the agent selects by task type. This module is
not a global validator and does not replace module-owned tests.

## How To Use

Before closing work, choose the rules that match the changed surface:

- `rules/technical-runtime.md`: code, build, launch, runtime, save/load, input.
- `rules/product-readability.md`: player-facing screens, controls, UI meaning.
- `rules/game-loop.md`: playable loop, reward, reason to continue.
- `rules/visual-quality.md`: composition, UI layout, screenshots, presentation.
- `rules/asset-quality.md`: assets, provenance, licenses, publishability,
  source materials, runtime-ready formats.
- `rules/repeated-failure.md`: repeated strict/product failures or lead
  rejection.
- `rules/active-game-workflow.md`: active game expansion, references, and
  monolithic runtime risk.

Apply only relevant rules. Record applied rules and evidence in the task `## Log`
or final response when the work changes project state.

## Boundary

Quality rules describe when to run checks and what evidence is meaningful. They
do not own implementation tools. Current tools still live in their legacy
locations until their modules are reviewed:

- product gates: `tools/product_gate/`;
- active game workflow: `tools/game_context/`;
- module tests: the owning module.

## Principle

One green check is not acceptance. Technical proof, product readability, visual
quality, asset provenance, and workflow state can fail independently.
