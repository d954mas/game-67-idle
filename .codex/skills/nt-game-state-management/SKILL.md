---
name: nt-game-state-management
description: "Use when adding, changing, testing, reviewing, or migrating schema-first game state, generated APIs, saves, migrations, DevAPI state commands, or state fixtures."
---

# NT Game State Management

Workflow router for the reusable Game State module. Its canonical contract
lives in `features/game-state/`, not in this skill.

## Start

1. Read `features/game-state/README.md` for the public surface, validation,
   compatibility, and extension boundaries.
2. Read `features/game-state/INSTALL.md` for wiring, generation, verification,
   and removal.
3. Read `features/game-state/feature.json` for the versioned machine contract.
4. Inspect the owning `state/*.schema.json` before changing call sites, then
   inspect `features/game-state/scripts/generate_state.py` before changing
   generated APIs.

## Change workflow

1. Change the schema or generator first.
2. Add migration and fixture coverage when persisted shape changes.
3. Regenerate outputs; do not hand-edit generated `game_state.*` files.
4. Update callers through domain actions rather than raw state mutations.
5. Run generator tests and the consumer's relevant save/runtime tests.

```powershell
py -3.12 features/game-state/scripts/generate_state.py
py -3.12 features/game-state/scripts/generate_state_test.py
```

Use explicit `--schema` and `--out-dir` for game variants or build-local
outputs.

## Routing

- Contract, save envelope, DevAPI, and domain actions:
  `features/game-state/references/contract.md`.
- Schema workflow and migrations: `features/game-state/references/workflow.md`.
- Review checklist: `features/game-state/references/review.md`.
- Runtime proof: `nt-runtime-automation`.
- Acceptance evidence: `nt-quality-checks`.
