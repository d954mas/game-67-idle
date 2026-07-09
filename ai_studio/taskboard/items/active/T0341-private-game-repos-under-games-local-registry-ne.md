---
id: T0341
title: Game-owned Studio artifacts and private nested game repos
status: backlog
project: P001
epic: E014
priority: P1
tags: [private-repos, workspace, games, taskboard, canvas]
created: 2026-07-09
updated: 2026-07-09
---

## What

Design and implement a game-owned Studio artifact model.

Agents still start in the AI Studio checkout, but every concrete game owns its
own game-specific Studio artifacts under the game folder. The Studio repo owns
the tools and mounts game stores; it does not own private game work state.

The intended model:

- public prototypes/samples can remain normal tracked folders under `games/<id>`;
- private commercial games can be nested Git repositories at `games/<id>/.git`;
- every game, public or private, owns game-specific files like:
  `games/<id>/.ai_studio/taskboard/items/`,
  `games/<id>/.ai_studio/canvas/projects/`,
  `games/<id>/.ai_studio/evidence/`, and game-local provenance/manifests;
- Studio tools resolve games from public and local/private registries, then
  mount game-owned taskboard/canvas/asset roots;
- public Studio files must not contain private game names, task logs, canvas
  project JSON, generated evidence, asset provenance, or IDE entries;
- agents keep using the Studio cwd, skills, taskboard UI, feature packs, and
  CLI, but must resolve game-owned stores before reading or writing
  game-specific work state.

## Decisions

- Private/local game stores are excluded from parent Studio aggregate views by
  default. They are visible only through an explicit active workspace or
  `--include-private` command/API flag.
- Private game names are treated as confidential by default. Public Studio
  outputs may show only an explicit public alias or redacted store label.
- The ignored local registry lives under `ai_studio/workspace/games.local.json`.
  The public registry remains `games/games.json` and contains public/tracked
  games only.
- Store IDs use `studio`, `game:<gameId>`, and `template:<templateId>`. Aggregate
  UIs and APIs display qualified IDs such as `game:rb-dark-rpg:T0001`; bare
  `P###`/`E###`/`T####` IDs are accepted only inside one selected store.
- Every game store may have its own local `P001` project. Store qualification,
  not global ID allocation, prevents collisions.
- Existing public history is not scrubbed by this work. The first deliverable
  stops future leaks; any history rewrite, rename, or published artifact cleanup
  must be a separate explicit task.
- Public games also own their taskboard/canvas/evidence state. Publishing a
  public game is still allowed to require a sanitize/export step.
- Private Canvas exports to parent tracked destinations are hard-rejected. There
  is no unsafe override for this path; exports must stay inside the owning game
  store or another ignored/private destination.
- Canvas project folders move as complete units. Old undo/cache continuity is
  best-effort through aliases; loss is acceptable only when logged in the
  migration evidence.

## Implementation slices

This card is the parent decision record. Implementation must happen through the
child tasks below, in order, because privacy guardrails are a prerequisite for
mounting or migrating private data.

1. `T0342` - Private workspace registry and leak-guard preflight.
   - Gate 0. No private registry entry may be consumed by generators, aggregate
     views, or agent-context payloads until this passes.
2. `T0343` - Game-owned scaffold and private new-game flow.
   - Adds `.ai_studio/` to the template/new-game path and splits public/private
     creation.
3. `T0344` - Store-qualified Taskboard CLI API and UI.
   - Adds store identity, ambiguous-ID refusal, private opt-in, and validation.
4. `T0345` - Mount-aware Canvas refs writes and export guard.
   - Adds v2 refs, store-routed mutations, and private export destination checks.
5. `T0346` - IDE generators assets and reports privacy boundary.
   - Prevents private registry entries from leaking through generated parent
     files, asset discovery outputs, architecture maps, and reports.
6. `T0347` - Migrate current game Studio artifacts into game stores.
   - Moves current `rb-dark-rpg` taskboard/canvas/evidence only after the
     previous gates are in place.

## Safety gates

- Gate 0: local registry path, ignore/exclude bootstrap, nested `.git` detection,
  parent Git guard, and tracked-file leak scan exist before private stores are
  read by any parent Studio generator.
- Gate 1: public and private new-game flows write different registries and
  output locations; private flow never mutates tracked parent `.vscode`, public
  registry, public Taskboard, or public Canvas roots.
- Gate 2: Taskboard commands and APIs require store selection for ambiguous
  short IDs; private stores never appear in `/api/agent/context`, board search,
  list output, or generated reports without explicit opt-in.
- Gate 3: Canvas writes, exports, manifests, `tool_runs.jsonl`, render reports,
  and provenance are rooted in the owning store. Private Canvas export to a
  parent tracked path is always rejected.
- Gate 4: migration moves files only after mount-aware reads/writes work and
  after preflight proves the parent repo will not stage private roots or leak
  private identifiers.

## Review / Critique

Subagent review found two blocking risks:

- **Public Taskboard leak:** a local private game registry does not protect
  private game titles, task logs, evidence paths, screenshots, acceptance
  criteria, or release plans if game-specific work continues to be written into
  `ai_studio/taskboard/items/`.
- **Generated parent-file leak:** tools that merge private registry entries and
  then write tracked parent files, especially `.vscode/tasks.json` and
  `.vscode/launch.json`, can reintroduce private game names and paths even if
  `games/<id>` itself is ignored.

Additional critique:

- Current `games/new_game.mjs` is public-by-default: it registers assets in
  `games/games.json`, creates a public Taskboard project, and refreshes tracked
  VS Code files. Private creation needs a separate `--private` path or command
  that never mutates public files by default.
- "Agents detect nested `.git`" is too vague unless the plan defines exact Git
  root checks before status/add/commit/clean operations.
- Registry overlay semantics need to be explicit: merge order, duplicate ID
  handling, schema version, disabled entries, and a rule that consumers use one
  resolver instead of parsing JSON ad hoc.
- Private repos do not relax asset-source rules: paid/non-redistributable
  binaries still stay out of git, and committed assets still need provenance,
  integrity, and origin.
- Canvas is a major side channel: project JSON, immutable files, exports,
  `tool_runs.jsonl`, manifests, render reports, and provenance records can leak
  private source/output paths unless private projects are stored outside the
  public canvas project root.

Refined critique after the lead accepted game-owned stores:

- Moving only private games is too narrow. Public games should also own their
  game-specific taskboard/canvas/evidence data so the model is consistent and a
  future public-to-private transition is a Git visibility change, not a data
  model migration.
- Task IDs can collide once every game has its own task store. The Taskboard UI
  and API need source-store identity, not just bare `T####` IDs.
- A game-owned Canvas root needs write routing, not only read listing. Every
  mutate/export operation must carry the owning store root so private projects
  do not fall back to the public canvas root.
- Migrating current `rb-dark-rpg` will expose existing public-history questions:
  the new model stops future leaks but cannot erase already-published commits.
- Mount-aware APIs must land before file migration. Moving files first would
  break existing Taskboard/Canvas reads and risks losing Canvas undo/chat
  continuity.
- Global aggregated views are a privacy boundary: private mounted stores can
  leak through UI search, API responses, generated reports, chat context,
  `tool_runs`, export manifests, and error logs unless mount visibility and
  commit policy are part of the data model.

## Current Research Notes

- `rb-dark-rpg` is currently in committed `games/games.json`.
- Tracked `.vscode/tasks.json` and `.vscode/launch.json` currently contain
  `rb-dark-rpg` build/run/capture entries.
- `ai_studio/taskboard/items/` currently contains 46 `rb-dark-rpg` taskboard
  files outside the game: 1 project, 3 epics, 28 active task files, and 14
  archived task files.
- `P003`, `E011`, `E012`, `E013`, and many active/archive `T####` files under
  `ai_studio/taskboard/items/` are game-specific `rb-dark-rpg` work state.
- Current game-specific Canvas projects under `ai_studio/assets/canvas/projects/`
  include:
  - `rb-dark-rpg-combat-actor-sprites-01-b24b3f`
  - `rb-dark-rpg-hud-gold-coin-5f30ae`
  - `rb-dark-rpg-location-art-2026-07-05-6b4dce`
- At least one Canvas project stores direct `games/rb-dark-rpg/...` source and
  `runtimeFile` paths in `project.json`.
- Some Studio docs/tests reference `rb-dark-rpg` as a fixture or example; the
  migration must distinguish reusable public fixture references from
  game-owned private work state.
- Runtime asset provenance/manifests are already mostly game-owned under
  `games/rb-dark-rpg/assets/...`; the larger remaining migration is the
  studio-side source records and work state, not the final asset outputs.

## Done when

- [ ] `T0342` through `T0347` exist as the executable slices for this parent
      plan.
- [ ] Blocking decisions are captured here: private opt-in default, registry
      location, store ID format, short-ID rules, game-local project policy,
      history-scrub scope, public-game sanitize requirement, and Canvas
      undo/cache policy.
- [ ] The child-task order makes leak guard/preflight the first implementation
      gate, before private store reads, generator integration, or migration.
- [ ] Each child task has checkable done criteria and names its privacy boundary.
- [ ] `node ai_studio/taskboard/cli.mjs validate --json` passes, or unrelated
      legacy validation failures are recorded explicitly.

## Open questions

- Does the first private project need public aliases for launcher/UI display, or
  is redaction enough until publishing?

## Log

- 2026-07-09: Captured lead decision direction: prefer Studio-hosted private
  nested game repos under `games/<id>` over public parent submodules, while
  preserving tracked public sample games. Plan explicitly includes taskboard,
  canvas, asset/provenance, IDE, and parent-Git leak boundaries.
- 2026-07-09: Delegated read-only review to `deep-reasoner` and `fast-worker`.
  Integrated critique: public Taskboard, tracked `.vscode`, Canvas project
  stores, canvas/asset manifests, architecture reports, and public-by-default
  `new_game.mjs` are the main leak paths, not only `games/<id>` file tracking.
- 2026-07-09: Lead accepted the stronger ownership model: taskboard items,
  canvas projects, evidence, asset/provenance sidecars, and game-specific
  Studio metadata should move inside each game under `.ai_studio/`; Studio apps
  mount these stores instead of owning them. Added current-game migration scope
  for existing `rb-dark-rpg` taskboard/canvas/registry/IDE references.
- 2026-07-09: Delegated refined research/critique to `fast-worker` and
  `deep-reasoner`. Integrated concrete current-game research: `rb-dark-rpg`
  has 46 taskboard files outside the game, 3 game-specific Canvas projects in
  the public canvas root, and 2 tracked `.vscode` files with hardcoded game
  paths. Integrated critique: store-qualified Taskboard IDs, mount-aware Canvas
  refs, mount visibility/commit policy, and mount-aware APIs must precede
  migration.
- 2026-07-09: Review fix: split implementation into child tasks `T0342` through
  `T0347`, made private stores opt-in by default, chose
  `ai_studio/workspace/games.local.json` as the ignored private registry path,
  made leak guard/preflight Gate 0, added CLI/store-ID decisions, and moved
  current-game migration behind the privacy and mount-aware gates.
- 2026-07-09: Closed residual Canvas export policy gap from follow-up review:
  private Canvas exports to parent tracked destinations are hard-rejected with
  no unsafe override.
