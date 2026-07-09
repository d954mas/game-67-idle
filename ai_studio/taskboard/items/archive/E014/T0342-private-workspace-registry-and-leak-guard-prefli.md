---
id: T0342
title: Private workspace registry and leak-guard preflight
status: done
project: P001
epic: E014
priority: P1
tags: [private-repos, workspace, leak-guard, registry]
created: 2026-07-09
updated: 2026-07-09
---

## What

Implement the first privacy gate for private/local games.

This slice owns the ignored local registry location, mount-record schema,
private opt-in default, nested Git detection, and preflight leak guard. No other
slice should consume private registry entries until this task is done.

## Done when

- [x] `ai_studio/workspace/games.local.json` is documented as the ignored
      private/local registry, and the required `.gitignore` rule is in place.
- [x] The public registry remains `games/games.json` and is documented as
      public/tracked games only.
- [x] Mount records include `storeId`, `kind`, `gameId`, `root`, `visibility`,
      `gitRoot`, `commitPolicy`, enabled stores, optional public alias, and
      schema version.
- [x] Resolver rules are documented and implemented: public registry first,
      ignored local registry overlay second, duplicate IDs rejected unless a
      local entry explicitly overrides a public fixture.
- [x] Private/local stores are excluded from aggregate views and agent-context
      payloads unless an active workspace or `--include-private` flag is
      provided.
- [x] A preflight guard proves private roots are ignored, not tracked, not
      staged, and not recorded as parent gitlinks before any generator consumes
      them.
- [x] A tracked-file leak scan covers private game IDs, aliases, paths, remotes,
      gitlinks, task metadata, canvas refs, evidence paths, and local registry
      content.
- [x] Parent Git operations that would add, clean, or commit nested private game
      roots fail with a clear message.
- [x] Tests or dry-run fixtures cover one tracked public game and one ignored
      private nested game.
- [x] `node ai_studio/taskboard/cli.mjs validate --json` passes, or unrelated
      validation failures are recorded.

## Open questions

- Closed: private preflight failures are hard failures. There is no unsafe
  override in this slice.

## Log

- 2026-07-09: Created as Gate 0 child task from `T0341` review. This task must
  land before private registry entries are consumed by generators, aggregate
  views, or migration tools.
- 2026-07-09: Started implementation of Gate 0: local private games registry, resolver, and leak-guard preflight. Existing active workstreams are left untouched.
- 2026-07-09: Implemented `ai_studio/workspace/games.mjs`,
  `ai_studio/workspace/README.md`, ignored
  `ai_studio/workspace/games.local.json`, and generated Codex/Claude shell
  hooks for `hook-guard`. Guard behavior is public-only by default, private
  mounts require explicit opt-in/preflight, staged/index-only leaks fail,
  wrapper shell Git commands are parsed, and unscanned staged text fails closed.
  Review fixed blockers for staged blobs, index-only local registry, wrapper
  commands, and parent Git add/clean/commit protection. PASS:
  `node --test ai_studio/workspace/tests/private_games_registry.test.mjs
  ai_studio/assets/backlog/storage/sources/tests/games.test.mjs
  ai_studio/dev_environment/vscode_projects.test.mjs
  ai_studio/core_harness/agent_surfaces/tests/hooks_sync.test.mjs`; PASS:
  `node ai_studio/core_harness/agent_surfaces/hooks_sync.mjs --check`; PASS:
  `node ai_studio/core_harness/validation/doc_reference_check.mjs`; PASS:
  `node ai_studio/architecture_map/validate_map.mjs` with existing
  `unmapped_ai_studio=84`, `unmapped_outside_ai_studio=11`, `missing=0`,
  `duplicates=0`; PASS: `node ai_studio/taskboard/cli.mjs validate --json`;
  PASS: `git diff --check` for touched files. Subagent final recheck reported
  Gate 0 PASS with only non-blocking risk for deliberate command obfuscation.
- 2026-07-09: Done: Gate 0 private workspace registry and leak-guard preflight implemented and verified. See task log for focused test, hook sync, doc-reference, architecture map, taskboard, and diff-check evidence.
