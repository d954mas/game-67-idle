---
id: T0352
title: Decompose Canvas ops, UI, chat docs, and local configuration behind stable facades
status: doing
project: P001
epic: E015
priority: P1
tags: [canvas, architecture, decomposition]
created: 2026-07-10
updated: 2026-07-12
---

## What

Reduce Canvas context cost by splitting the large ops/UI/docs surfaces along
existing domains while retaining stable public facades and identical behavior.

This task owns physical module/file relocation after `T0351` has transport
parity. `T0351` changes transport behind the current boundary and must not
pre-emptively repeat this structural move.

## Done when

- [ ] `ops.mjs` remains the public facade while implementation moves one domain
      at a time with unchanged exports and focused parity tests.
- [ ] `inspector.js` and `workspace.js` are split behind stable UI facades with
      no DOM/CSS/interaction redesign.
- [ ] Canvas Chat runtime/docs move under Canvas ownership; Studio Shell keeps
      only hosting integration.
- [ ] Canvas README becomes a short router to domain contracts; the Canvas skill
      loads only the contract required by the request.
- [ ] Machine-local paths leave tracked `studio.config.json` for the existing
      ignored local override.
- [ ] No services layer, DI container, event system, or speculative framework is
      introduced.

## Open questions

## Log

- 2026-07-10: Mechanical decomposition only; security behavior belongs to
  `T0350`/`T0351`.
- 2026-07-12: 2026-07-12: Isolated implementation checkpoint at b02e8bfe7. Mechanical Canvas decomposition only; stable facades and behavior parity required; T0393 and games/web-dressup remain out of scope.
- 2026-07-12: TDD RED: the new decomposition contract preserved the baseline
  public exports but failed for missing ops/UI/Chat ownership paths and
  machine-local roots in tracked Studio config (1/3 pass, 2/3 expected fail).
- 2026-07-12: Implementation ready for review in the isolated worktree. Stable
  facades preserve 84 ops exports, 8 inspector exports, and 21 workspace
  exports. Real domain code moved to ops export-scale/project-lifecycle,
  inspector contracts/controls, and workspace cleanup/filter/animation modules;
  Canvas now owns Chat and Studio Shell retains host wiring only. README routes
  to scoped contracts and the previous deep reference remains available.
- 2026-07-12: Verification: decomposition 3/3; focused Canvas/UI 74 pass, 5
  expected Python/PIL skips, 0 fail; Canvas Chat 95/95; Studio Shell 1/1;
  Architecture Map staged-snapshot 0 unmapped/0 missing/0 duplicates; docs
  11/11; Taskboard validation 0 problems; diff check pass. Full Canvas suite
  was exercised but its Python-backed cases cannot pass in this isolated
  worktree because `.venv` is intentionally absent; focused non-Python paths
  and moved wiring are green.
- 2026-07-12: Quality: QTECH_001=unverified; evidence: isolated facade/domain,
  Chat, Studio Shell, config, and Architecture Map checks are green, but final
  acceptance requires the lead to run the full Canvas suite with the real root
  `.venv`, verify the ignored local override against the workstation roots, and
  complete the live `/canvas` browser open/edit/undo/Chat-cancel smoke on the
  integrated worktree.
- 2026-07-12: Review/fix cycle 1 resolved the architecture HIGH. The 7,759-line
  ops runtime was removed; real core/elements/groups/generation/history/image
  implementations are now 590/1122/841/2527/795/2719 lines, each below 3,000,
  with no domain back-importing a runtime facade. Inspector runtime fell
  3448->2691 lines (recipe-pack, generation-card, primitives modules) and
  workspace runtime 3022->2902 (render-culling, gesture-overlay,
  viewport-controls plus preview/filter domains).
- 2026-07-12: Cycle-1 focused gate: 125 pass, 9 expected Python skips, 0 fail.
  Direct deterministic workspace tests cover animation rAF/dispose/once-frame
  lifecycle, cleanup compare/Image boundaries, filter identity+tint cache and
  fixed 96-entry eviction, culling, gesture overlays, and viewport controls.
  Full isolated Canvas gate: 753 tests, 655 pass, 86 skip, 12 failures, all
  caused by the intentionally absent root `.venv`; no undefined/cross-domain
  wiring errors remain. Staged-snapshot Architecture Map is clean at 0
  unmapped/missing/duplicates; full-reference relative links were rebased and
  independently checked at 0 broken targets. Local ignored override remains
  present for the lead's integrated workstation verification.
- 2026-07-12: Review/fix cycle 2 pruned the copied monolith headers from every
  ops domain. Combined domain source fell 476,687->417,895 bytes (-58,792,
  12.3%) and import statements 158->65 (-93, 58.9%): core 24->6,
  elements 25->10, groups 26->5, generation 27->15, history 28->10,
  image pipeline 28->19. A regression now caps scoped import counts, rejects
  specialist-pipeline imports from core/elements/groups/history, and rejects
  obvious unused imported bindings. Syntax/import keeps the exact 84-export
  facade; focused ops/decomposition/Chat gate is 200 pass, 16 expected
  Python/sandbox skips, 0 fail. Lead-owned full-venv/browser proof remains
  intentionally unclaimed.
