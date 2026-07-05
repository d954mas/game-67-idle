---
id: T0290
title: "Skill: in-game layout workflow from 2-day friction review"
status: done
project: P001
epic: E001
priority: P1
tags: [skills, ui-layout, in-game-ui, review]
created: 2026-07-05
updated: 2026-07-05
---

## What

Create a reusable AI Studio skill for fast, high-quality in-game UI/layout work.
The immediate problem is not that the layout stack is bad; it is that agents do
not reliably investigate the available engine widgets, layout modes, examples,
and constraints before implementing a screen. This causes avoidable loops where
the agent hand-rolls or works around behavior that already exists.

Start with an evidence-based review of the last 2 days of layout-heavy work
(target window: 2026-07-03 through 2026-07-05, Asia/Tashkent):

- Use `nt-chat-session-reflection` evidence order first: profiling status,
  recovered Codex failures if needed, taskboard summary/context, git status,
  relevant task logs, validation output, screenshots/reports, and conversation
  context last.
- Find where in-game layout work lost time: missed widget inventory, missed
  layout modes, bad assumptions about clipping/overflow, PC/phone responsive
  mismatches, over-custom code where engine/UI API already had a feature, weak
  visual verification, and any repeated command/debug loops.
- Identify concrete source anchors the skill must force agents to inspect:
  current game UI code, `external/neotolis-engine` public UI API/docs/examples,
  active UI showcase/tests, `ai_studio/runtime_automation` visual checks, and
  taskboard logs for the affected work.
- Capture at least one known pitfall explicitly: floating content can clip to
  the attached parent via Clay/engine UI configuration
  (`CLAY_CLIP_TO_ATTACHED_PARENT` pattern; see
  `external/neotolis-engine/engine/ui/nt_ui_input.c` for a source example). The
  skill must teach agents to look for this kind of existing mode before
  inventing a workaround.

Then author a thin skill, tentatively `.codex/skills/nt-game-ui-layout/SKILL.md`
(rename only if a better existing convention is found). It should route agents
through the right discovery and verification steps instead of duplicating all
widget documentation.

The skill should trigger on work such as: in-game HUD/screen layout, engine UI
widgets, responsive PC/phone game UI, modal/dialogue sheets, menus, floating
layers, clipping/scrolling/overflow, and requests to "make the layout fit" in a
game.

The skill must require this workflow before edits:

- Resolve game context from the explicit game id or the single game folder.
- Inventory the existing widget/layout surface before designing: public engine
  UI headers/docs, examples/showcases, current game UI modules, tests, and
  nearby completed screens.
- List available widgets/modes relevant to the requested screen, including
  floating, clipping, scroll/overflow, 2D vs 3D/raycast input, responsive sizing,
  text rendering, and interaction/capture behavior when relevant.
- Choose existing engine APIs before custom layout/render code.
- Preserve hard invariants: game/world/UI logic stays Y-up; convert Y-down only
  at boundaries; all user-visible text uses the engine text renderer and real
  fonts; no handmade `draw_text` for product UI.
- Verify with the smallest meaningful tests plus browser/runtime visual checks
  for desktop and phone-sized viewports when the output is visual.

## Done when

- [x] A short friction review exists and names evidence inspected, weak evidence
      gaps, where time was lost, and the top layout-process fixes.
- [x] The review identifies concrete widget/layout/API sources the future agent
      should inspect, not just generic advice.
- [x] `.codex/skills/nt-game-ui-layout/SKILL.md` exists (or a better named
      equivalent is justified) and is a thin router to source docs/code/examples.
- [x] The skill explicitly prevents the observed failure mode: implementing
      layout before inventorying available widgets and modes.
- [x] The skill includes the floating/parent-clipping pitfall and points agents
      to source examples instead of relying on memory.
- [x] The skill is synced to the Claude surface using
      `node ai_studio/core_harness/agent_surfaces/skills_sync.mjs`.
- [x] Skill/docs validation passes; new skill files are mapped in the
      architecture map, with remaining strict-map failures identified as
      unrelated pre-existing unmapped paths.
- [x] A live or simulated smoke proves an agent using only the new skill can
      discover the relevant widget/mode surface before proposing or editing a
      layout.

## Open questions

- Should the skill stay generic for all games, or include a short
  `rb-dark-rpg` appendix while that is the active layout-heavy game? Default:
  generic skill plus game-specific source discovery.
- If the 2-day profiling record is incomplete, mark the review partial and base
  findings on task logs/git/session artifacts instead of guessing time spent.

## Log

- 2026-07-05: Created from lead request after repeated in-game layout friction:
  layout mostly works, but agents miss existing widgets/modes and lose time on
  avoidable rediscovery/workarounds.
- 2026-07-05: Started execution. Delegated read-only evidence review and UI
  surface mapping; lead collecting profiling/task/git evidence and will author
  the skill.
- 2026-07-05: Completed implementation. Added
  `.codex/skills/nt-game-ui-layout/SKILL.md` as a thin pre-edit layout
  inventory gate and `.codex/skills/nt-game-ui-layout/references/friction-review-2026-07-05.md`
  as the evidence review. Synced `.claude/skills/nt-game-ui-layout/SKILL.md`
  via `node ai_studio/core_harness/agent_surfaces/skills_sync.mjs`. Mapped the
  skill folder in `ai_studio/architecture_map/tree/module-hot.json`. Validation:
  `quick_validate.py` passed with PyYAML loaded from `C:\tmp\codex_pyyaml`;
  `skills_sync.mjs --check` passed; `node --test ai_studio/core_harness/agent_surfaces/tests/skills_sync.test.mjs`
  passed; `doc_reference_check.mjs` passed; `git diff --check` passed for the
  touched files. `validate_map.mjs --strict` still fails on pre-existing
  unrelated unmapped AI Studio paths and `games/rb-dark-rpg`, but the new
  `nt-game-ui-layout` paths are no longer listed as unmapped. Simulated smoke
  passed: an explorer using only the new skill produced a pre-edit inventory
  for a phone dialogue clipping task, including `dialogue_panel.c`,
  `game_modal.c`, `ui_runtime.c`, engine modal/scroll/widget modes,
  attached-parent clipping sources, and desktop/phone proof plan before edits.
- 2026-07-05: Closed after skill, review reference, Claude sync, mapping,
  validation, and simulated smoke. Residual strict architecture-map failures are
  unrelated pre-existing unmapped paths; new skill paths are mapped.
