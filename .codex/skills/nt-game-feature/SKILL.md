---
name: nt-game-feature
description: "Use when creating a reusable feature or deciding whether game code belongs in a root module, a template feature-pointer, or game-owned code."
---

# NT Game Feature

Workflow router for reusable modules and template feature-pointers. Canonical
feature contracts live with their owners, not in this skill.

## Start

1. Read `features/README.md` for category selection and current modules.
2. Read `templates/template/src/features/README.md` for template-pointer
   conventions, layers, lifecycle, state, and asset boundaries.
3. For every existing owner being changed, read its local `README.md`,
   `INSTALL.md`, and `feature.json`. Those files own its public surface,
   wiring, validation, compatibility, and extension rules.

## Choose the owner

- Byte-identical reusable code configured through data belongs under
  `features/`.
- A reusable reference games are expected to repaint or hand-edit belongs
  under `templates/template/src/features/`.
- Game-specific seed, verbs, save history, and composition remain game-owned.

Consumer count does not decide the category. Do not add speculative switches,
registries, or hooks without a real consumer need.

## Workflow

1. Follow the closest owner's local contract instead of copying it here.
2. For a new owner, create local contract files and register it through
   `features/README.md`; synchronize its version with dependency records.
3. Keep layer edges downward and declare dependencies in `feature.json`.
4. For relocation, preserve byte identity and public include spelling, change
   only paths/wiring, verify generated-output identity, then run consumer tests.
5. Prove a root module through a throwaway `games/new_game.mjs` consumer.
6. Keep each increment buildable.

## Routing

- Items catalog/runtime workflow: `nt-game-items`.
- State schemas, generation, saves, and migrations: `nt-game-state-management`.
- Live runtime proof: `nt-runtime-automation`.
- Acceptance evidence: `nt-quality-checks`.
