---
id: T0355
title: Consolidate Workspace Catalog, game identity, and tested dependency records
status: done
project: P001
epic: E015
priority: P0
tags: [workspace, games, dependencies]
created: 2026-07-10
updated: 2026-07-11
---

## What

Replace duplicate public/local identity registries with one versioned
Workspace-owned mount catalog. Games and templates own identity in their own
manifests; the Workspace records only how this checkout mounts them.

## Done when

- [x] Tracked `ai_studio/workspace/catalog.json` and ignored
      `ai_studio/workspace/catalog.local.json` use the same explicit
      `ai_studio.workspace.catalog.v1` document schema.
- [x] Catalog entries contain mount facts only: `kind`, repo-relative `root`,
      visibility, git/commit policy, enabled stores, and optional safe aliases;
      game/template id, title, and storage namespace are read from
      `game.json` or `template.json` and are never duplicated.
- [x] Unknown schema versions, invalid roots, missing identity manifests,
      duplicate derived IDs/store IDs, and public/private collisions fail
      early; there is no quiet `Template` fallback or silent coercion.
- [x] Migration order is fixed: create/validate identity manifests; generate
      both new catalogs from current registries; prove public/private mount
      parity and leak guards; switch the one Workspace resolver and all
      consumers; then remove `games/games.json`,
      `templates/templates.json`, and
      `ai_studio/workspace/games.local.json` in the same cutover.
- [x] Production code never dual-reads old and new registries. A one-shot
      migration command may read old inputs only to produce and validate the
      atomic cutover.
- [x] Assets, VS Code generation, Studio Shell, Taskboard routing, and game/
      template creation consume the same Workspace resolver.
- [x] Existing T0341-T0348 privacy preflight, parent-Git guard, ignored local
      overlay, and explicit private inclusion semantics remain green.
- [x] Each game owns `dependencies.json` with tested engine/feature versions
      and compatibility; no snapshot archive, cache, sync/link command, tag
      strategy, or automatic old-game repair is introduced.

## Open questions

None.
## Log

- 2026-07-10: Final convergence selected workspace/catalog.json plus the
  ignored catalog.local.json overlay. Identity stays solely in game/template
  manifests.
- 2026-07-10: T0356 depends on this cutover contract.
- 2026-07-10: Resolved planning detail: catalog schema/version and one-shot migration order are fixed in What/Done when.
- 2026-07-11: Checkpoint: current production truth is duplicated across games/games.json, templates/templates.json, and ignored ai_studio/workspace/games.local.json; no game.json, template.json, or dependencies.json manifests exist. Workspace privacy resolver and guards already exist and must be preserved. Starting one-schema catalog cutover and consumer migration only; T0356 transactional creation/prototype deletion and T0361 feature-contract work remain out of scope, and T0393 audio WIP is untouched.
- 2026-07-11: Added strict Workspace catalog and identity/dependency manifests. Catalog rows contain mount facts only; ids, titles, namespaces, store ids, and asset roots are derived from game/template-owned manifests with schema, confinement, realpath, collision, and policy validation.
- 2026-07-11: Cut over assets, Canvas, Items Viewer, VS Code, Studio Shell chat fixtures, Taskboard, Architecture Map, and game/template creation to the Workspace resolver. Removed the two tracked legacy registries, changed the ignored private overlay to the same-schema catalog, and confirmed production has no legacy registry reads or fallback.
- 2026-07-11: New games require exact clean parent/template/feature/engine revisions, verify the engine checkout matches the parent gitlink, consume an explicit template dependency seed, remove that seed after copy, and write one validated authoritative dependencies.json. Existing public mount parity remains assets-only.
- 2026-07-11: Verification: broad affected consumer/creation/privacy/validator suite 197/197 pass; live Workspace list derives both public game identities and assets-only stores; privacy preflight returns ok with no violations; Architecture Map strict reports 335 mapped / 764 scanned with 0 unmapped, missing, duplicate, or invalid-description issues; Taskboard validation and cached diff check pass. Items Viewer catalog paths pass in the broad suite; direct list/schema/validate probes pass through C:\Python312\python.exe while the pre-existing hard-coded Windows `py -3.12` launcher issue remains owned by T0357.
- 2026-07-11: Independent review cycle 1 found 0 HIGH and actionable parity, dependency accuracy, atomic-write, comment, and fixture issues; all were fixed. Cycle 2 found one remaining copied dependency-seed issue; it was fixed. Cycle 3 reported 0 HIGH and 0 actionable MEDIUM/LOW across architecture, correctness, ownership, tests, process, performance, and context cost. Lead integration then mapped the three new Workspace catalog files and repeated strict validation.
- 2026-07-11: Quality: QTECH_001=pass; evidence: strict catalog/identity/dependency tests, broad consumer and creation regressions, privacy preflight, production legacy scan, Architecture Map strict validation, and three independent review cycles.
- 2026-07-11: Closed after strict catalog cutover, identity/dependency records, consumer/privacy parity, broad regressions, Architecture Map integration, and three review cycles passed.
