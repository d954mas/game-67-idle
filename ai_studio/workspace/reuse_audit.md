# Studio reuse audit

Snapshot: 2026-10-18. Scope: the local game catalog, its derivative worktrees,
the reusable feature library, agent skills, and the default starter.

## Catalog boundary

The scan found eleven independent private catalog games and three derivative
worktrees whose manifests retain the original game's identity. An additional
workspace container has no game manifest. These are distinct categories;
counting every directory as a game exaggerates the number of implementations.

Each catalog game and derivative now has a private
`design/studio-reuse-audit.md` with its architecture, verified source paths,
extraction candidates, coupling constraints, and game-owned exclusions. Private
identifiers and source paths remain inside those repositories.

The audit is source-based. It is not a playtest of all games or a claim that
every candidate has already migrated. Byte-identical files establish reuse
opportunities; they do not by themselves establish the correct public API.

## Ownership decisions

| Capability | Shared owner or extraction boundary |
| --- | --- |
| State, saves, migrations | Existing `features/game-state`; game schemas and save history remain local. |
| Items and progression | Existing `items-core` and `progression-core`; content, seed, verbs and balance remain local. |
| Platform, networking, scenes, audio, localization | Extend the existing feature owner before adding a parallel abstraction. |
| Planar limb solving | New opt-in `features/limb-ik`; rigs, animation, collision, levels and art stay local. |
| Settings and resource panel | Optional copy-then-own references in `ai_studio/feature_testing/consumer`; no default HUD or settings screen. |
| Shared shell and integration fixtures | The independent feature-testing module preserves the integration consumer and existing tests. |
| Touch controls and board drag | Candidates after separation of action semantics, picking and world data; inspect engine APIs first. |
| Cosmetics, rewarded offers and loot | Candidates over existing item/platform cores; separate acquisition, grants, persistence and presentation. |
| Merge grid, race flow and procedural levels | Candidates after a concrete data contract removes World, catalog and reward coupling. |
| Crowd, physics worlds and character rigs | Game-owned composition until a small invariant boundary is established; engine work uses its issue/PR process. |

Useful architecture patterns can become copyable examples even when their
runtime implementation should remain game-owned. Consumer count is not the
promotion criterion. Use byte-invariant implementation configured through data
for shared modules; use copy-then-own references where games need direct edits.

## Starter and integration consumer

`templates/template` is the blank foundation. It contains only its actual
runtime dependencies and explicit input/state/logic/render lifecycle seams.
`ai_studio/feature_testing/consumer` is an independent fixture used by the feature
test harness. Keeping these separate prevents sample economy, progression, HUD,
settings, audio and scene data from becoming every game's starting design.

`features/validate_contracts.mjs` discovers all template dependency seeds and
optional reference owners. It checks selected modules against exact versions,
rejects specified-only runtime dependencies, and does not force every mature
feature into the default starter. Runtime build records still check that a
declared dependency actually participates in the consumer build.

## Skill maintenance

Seventeen Studio skill descriptions were reviewed and shortened. The feature
skill now covers extraction boundaries, compatibility adapters, private
provenance and consumer proof. Taskboard routing preserves the human lead's
item-creation boundary. Portal timing facts are read from current sources.

The surface generator now provides current Codex `.agents/skills` discovery
and Claude pointers from one canonical source, preserves existing Codex
metadata, and translates explicit-only invocation for Claude. Same-name manual
collisions fail without overwriting the manual file. The official guides and
validation contract are in
[skill authoring](../core_harness/agent_surfaces/skill_authoring.md).

## Evidence limits

Module tests, source-consumer checks, starter launch evidence and full Studio
verification have different scopes. Individual game audit notes do not imply
that their complete builds or gameplay were exercised. Exact private dependency
locks require committed shared revisions; a locally tested uncommitted module
must not be represented as an immutable released dependency.

Registered derivative worktrees now retain one canonical catalog identity while
their physical roots remain included in privacy audits. Git registration, common
directory and identity checks reject unrelated aliases. Release versions and
download budgets may differ between legitimate iterations.

## Validation snapshot

- `node ai_studio/feature_testing/run.mjs`: pass for the complete features
  domain, including 75 native consumer tests. The consumer lives outside the
  template catalog.
- Blank foundation: two core tests pass; native launch evidence shows zero draw
  calls, UI contexts and entities; the web debug build succeeds. Legacy unknown
  save fragments may remain when reusing the old template storage namespace;
  game creation assigns each new game its own namespace.
- Game creation: 19 tests pass, including creation from the current foundation
  bindings and derivative storage isolation.
- Skill surface generator: nine tests pass; both host catalogs and hooks are in
  sync. Feature contracts validate seventeen modules and two optional references.
- Moved consumer assets: nineteen tracked binary blobs retain their previous
  public starter bytes. Additional exact sharing records were verified against
  the previous public commit and each private counterpart; they cover relocation,
  not new private content. Global asset integrity covers 63 tracked blobs and
  one engine boundary. The complete assets owner domain passes.
- The complete Studio gate passes on the saved implementation snapshot:
  all nine applicable owner domains, including native/web template release,
  pass; design has no deterministic technical checks. The release smoke proves
  an empty starter frame and two WebGL recovery cycles. Shared dependencies are
  committed; private game working-copy changes remain with their owning games.

Current Items CLI and pipeline measurements were reproduced into new dated
reports under `features/items-core/benchmarks/results/`; historical reports were
preserved. The command runner now preserves bounded stderr separately so noisy
generator stdout cannot hide the failure diagnostic.
