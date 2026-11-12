---
name: nt-game-state-management
description: "Change schema-first game state, generated APIs, saves, migrations, DevAPI state commands, or state fixtures."
---

# NT Game State Management

Use the schema-first contract owned by `features/game-state/`.

## Start

Identify the owning schema and affected callers. Read relevant
`features/game-state/README.md` sections for contracts,
`features/game-state/INSTALL.md` for wiring, and
`features/game-state/feature.json` for versions/dependencies. Inspect the generator when its
output or behavior must change.

## Change workflow

- For storage/API changes, edit the schema or generator and regenerate; never
  hand-edit generated state files. Add migrations and fixtures for persisted
  shape changes.
- For gameplay or semantic DevAPI behavior, change domain actions and callers;
  change the schema only when the state contract needs it.
- Verify affected save/runtime behavior. Run generator tests when generator or
  generated-output behavior changes; complete the owner's applicable checks.

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state.py
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state_test.py
```

Use explicit `--schema` and `--out-dir` for game variants or build-local outputs.
Report changed contracts, checks, and any unverified migration/runtime behavior.

## Routing

- Contract, save envelope, DevAPI, and domain actions:
  `features/game-state/references/contract.md`.
- Schema workflow and migrations: `features/game-state/references/workflow.md`.
- Review checklist: `features/game-state/references/review.md`.
- Runtime proof: `nt-runtime-automation`.
- Acceptance evidence: `nt-quality-checks`.
