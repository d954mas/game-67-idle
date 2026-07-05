# In-Game Layout Friction Review (2026-07-03 to 2026-07-05)

Scope: historical evidence for `T0290`, focused on why agents lost time on
in-game layout and what the `nt-game-ui-layout` skill must prevent. Treat this
as audit evidence, not current task state.

## Evidence Inspected

- Current task archive:
  `ai_studio/taskboard/items/archive/E001/T0290-skill-in-game-layout-workflow-from-2-day-frictio.md`.
- Task logs: `T0271`, `T0272`, `T0276`, `T0285`, `T0288`, plus archived or
  linked combat/world-map work referenced by subagent review.
- Git history from 2026-07-03 through 2026-07-05: several large
  `rb-dark-rpg` UI commits, including first screen HUD, dialogue panel,
  bottom nav, equipment, combat flow, and responsive visual follow-ups.
- Current profiling: `tmp/session_profiles/sessions/2026-07-05__codex__019f31ab.jsonl`
  covers only the current T0290 run. Older profile files exist for 2026-07-03
  through 2026-07-05, but coverage should be treated as partial.
- Engine UI docs/source: `external/neotolis-engine/docs/neotolis_engine_spec_1.md`
  and `external/neotolis-engine/engine/ui/`.
- Runtime proof tools: `ai_studio/runtime_automation/`,
  `games/rb-dark-rpg/devapi/responsive_viewports.py`, and game DevAPI scenarios.

## Findings

1. The failure mode is process-level: agents were reaching for implementation
   before inventorying existing widgets, layout modes, examples, and proof
   tools. `T0290` records this directly and should remain the source task.

2. The engine UI surface is broad enough that layout work must start from
   source discovery. The spec documents Clay as public surface and `nt_ui` as
   building blocks rather than a full theme framework
   (`neotolis_engine_spec_1.md:108-135`), 2D and 3D/raycast UI modes
   (`:155-173`), stateful widgets and slider/progress (`:253-291`), custom
   scroll and virtual lists (`:293-345`), text input (`:359-375`), popup-core
   for modal/dropdown/tooltip/context menus (`:391-393`), atlas-backed image
   and panel widgets (`:445-448`), and rich text (`:3215-3228`).

3. The specific floating/parent-clipping example from the lead is already in
   engine and game code. `nt_ui_input.c:604-627` opens a clipped content child,
   then floats text/caret/selection with `CLAY_CLIP_TO_ATTACHED_PARENT`; the
   comment explains that a raw floating clip would leak past outer scroll.
   `world_map_screen.c` also uses attached-parent clipping around markers/routes
   at `:313`, `:767`, `:846`, `:870`, and `:929`.

4. `rb-dark-rpg` already has reusable game-local UI wrappers and patterns.
   `ui_runtime.c` owns logical layout scaling and font binding. `dialogue_panel.c`
   uses `game_modal_visible`, game modal styling, buttons, scroll style, and
   image styles. `dialogue_panel.c:673-830` builds an adaptive modal with panels,
   portrait vs landscape directions, scrollable content, and a sticky choice
   tray. `equipment_screen.c:934-971` uses `nt_ui_scroll_begin` for inventory
   overflow. `combat_flow.c:1087-1131` uses modal style plus floating close and
   content anchors.

5. Responsive work was reactive. `T0271` requires phone bottom sheet, desktop
   variant of the same hierarchy, sticky bottom action, and component/state ids.
   `T0272` required identical bottom-nav runtime size/hitbox, readable icons,
   compact bottom-sheet hooks, `ui.tree` hitbox evidence, and a contact sheet.
   `T0276` required landscape/portrait equipment usability and logged an
   equipment responsive matrix. `T0288` exists specifically because desktop and
   phone combat captures still needed overlap, clipped-label, and readability
   fixes.

6. Runtime proof tools exist and should be mandatory for visual surfaces.
   `games/rb-dark-rpg/devapi/README.md:48-72` points to `quality_responsive`
   and scenario hooks. `responsive_viewports.py:2-5` states it captures
   engine-native screenshots and `ui.tree` bounds; `:48-53` defines phone and
   desktop viewport cases; `:144-173` records screenshots and stable ids; and
   `:222-242` writes a contact sheet plus `summary.json` with
   "screenshots + runtime ui.tree bounds" evidence. `ui_readability.py:151-157`
   explicitly warns not to judge text from full screenshots alone.

7. The DevAPI UI contract removes the need for ad hoc coordinate guessing:
   `neotolis_engine_spec_1.md:2711-2715` says reads and writes share Y-up layout
   pixels, stable ids resolve to projected bounds centers, and `ui.click` /
   `ui.drag` are range-checked.

8. Profiling and subagent review show repeated debug loops consistent with
   layout/runtime friction, but the profile coverage is partial. Treat exact
   time-spent conclusions as weak unless a future audit reopens the raw JSONL.

## Skill Requirements Derived

- Force a pre-edit inventory: game analogs, engine UI docs/headers/examples,
  current tests, DevAPI scenarios, and proof tools.
- Require the agent to name the widgets/modes it found before coding.
- Make floating/parent clipping a known pitfall with source anchors.
- Prefer existing `game_modal`, `nt_ui_scroll`, `nt_ui_panel`, `nt_ui_image`,
  `nt_ui_button`, `nt_ui_label`, rich text, semantic ids, and DevAPI `ui.tree`
  over custom layout/render code.
- Require desktop and phone runtime proof for visual UI, with evidence paths in
  the task log.

## Evidence Gaps

- Profiling is partial. The current T0290 profile covers only minutes; older
  profile files exist but have unknown completeness unless separately audited.
- Chat transcript evidence was not used as primary evidence; findings are based
  on task logs, code, git history, and subagent reports.
- Some screenshot evidence paths are ignored temporary outputs, so task logs may
  be more durable than the images themselves.
