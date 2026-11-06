---
name: nt-game-feature
description: "Extract or integrate reusable game capabilities; choose shared modules, optional copyable references, or game-owned code."
---

# NT Game Feature

Use the owning feature's contract for extraction, integration, or maintenance.

## Start

Use `features/README.md` to select an owner or find existing modules. Read the
owner's relevant `README.md` sections; consult `INSTALL.md` for wiring and
`feature.json` for versions/dependencies. For copyable references, use
`ai_studio/feature_testing/consumer/src/features/README.md`.

## Choose the owner

- Byte-identical reusable code configured through data belongs under
  `features/`.
- A reusable reference games are expected to repaint or hand-edit belongs
  in an optional copyable feature example; keep it out of default startup.
- Game-specific seed, verbs, save history, and composition remain game-owned.

Consumer count does not decide the category. Do not add speculative switches,
registries, or hooks without a real consumer need.

## Workflow

- For a new owner, create local contract files and register it through
  `features/README.md`. Keep versions and consumer dependency records aligned.
- Keep layer edges downward and declare dependencies in `feature.json`.
- For relocation, preserve byte identity and public include spelling; verify
  generated-output identity and run the affected consumer checks.
- Prove a new or changed root-module contract through an isolated consumer;
  reuse a suitable fixture or create one with `games/new_game.mjs`.
- Keep increments buildable. Before merging shared-feature changes, run
  `node ai_studio/game_tools/live_games.mjs` and update dependency records.

For extraction, identify the candidate's dependencies and ownership across
lifecycle, state, content, assets, tooling, and platform bindings. Compare
existing engine APIs and feature packs before creating an owner.
Extract a concrete invariant API, not the game's composition or balance.
Record source provenance inside the private game; keep private identifiers and
restricted assets out of tracked Studio files. Prove the shared module with an
isolated consumer and the source game's focused checks before claiming it has
been integrated there.

Define the files being moved and excluded. Keep a
compatibility adapter when changing a source game's include/API boundary.
For a renamed or parameterized API, prove behavioral equivalence and invariants;
byte identity alone applies only to relocation. Verify the shared module and
adapted source consumer before removing the replaced implementation.
Run the owner's relevant checks and report integration evidence and gaps.

## Routing

- Items catalog/runtime workflow: `nt-game-items`.
- State schemas, generation, saves, and migrations: `nt-game-state-management`.
- Live runtime proof: `nt-runtime-automation`.
- Acceptance evidence: `nt-quality-checks`.
