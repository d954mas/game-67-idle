# AI Pipeline History

Dated history of AI-workflow iteration lessons and retrospectives. This is not
the task board, not live status, and not the canonical process doc (`AI_PIPELINE.md`).
Add a new entry only when there is a reusable process lesson; if a lesson creates
real work, put the work in `tasks/`. For deeper retrospectives of long multi-turn
sessions, use the `chat-session-reflection` skill.

## Iteration Log

Compact reusable lessons about AI-assisted development. Keep each entry under
about 10 lines.

### 2026-06-19 - Portal Visual Work Needs An Architecture Gate Before More Polish

- Context: Backrooms T0010 improved the impossible-room portal through many
  native overlay/material/light slices, but the strict product gate still failed
  art quality and audience fit.
- Root cause: iteration kept adding authored shell geometry and shader lighting
  after the lead had identified the real issue: it still read like a
  shader/composite trick, not a production rendered room.
- Fix: when a visual feature depends on missing render architecture, create the
  engine-facing task early, add a native perf/control gate, and freeze further
  content/polish until the next slice proves the real render path.

### 2026-06-18 - Native PC Scale/Focus Must Be Early Evidence

- Context: Mine Cards T0001 passed several screenshot/product-read gates, but
  lead later rejected the actual PC window as too small, unfocused, and full of
  pseudo-buttons.
- Root cause: the runtime UI was authored in raw framebuffer pixels and the
  review optimized screenshots/art direction before proving `nt_ui_scale`,
  real-window readability, and first-player action focus.
- Fix: playable native UI must use a reference-resolution scale layer before
  review; first-screen focus review must ask "where am I, what is active, what
  can I click now" before calling a slice ready. Evidence lives in
  `gamedesign/projects/mine-cards/reviews/t0001_ui_scale_rejection_2026-06-18.md`.

### 2026-06-17 - A Product Gate Pass Only Covers The Captured State

- Context: Voxelheim rescue produced multiple native product gates, but the lead
  later found a live UI state with muddy CTA text, bad Blocks icon placement,
  floaters over UI, and returned purple button edges.
- Root cause: the pass evidence was state-narrow (offline/reward screenshot) but
  treated as broad UI acceptance. The durable `assets/raw/button.png` still had
  chroma contamination, so rebuilding the atlas could reintroduce the purple
  edge.
- Fix: add reusable state coverage to product gates (T0012), add a universal
  live-state UI matrix template with Voxelheim as the first fixture (T0013), and
  clean contradictory current-state docs (T0011). Gate logs must say which
  states passed and which remain untested.

### 2026-06-16 - Shipped a Pretty Screen, Not a Game (Visual Gates Hid the Missing Loop)

- Context: Built Voxelheim from a "Roblox RPG like Skyrim" brief up to a
  "release-candidate" first screen; lead: "looks nice but I don't get what the
  game is, the loop, the idea, the refs - is it idle?"
- Root cause: the pipeline OPERATIONALIZES the visual gate (`ai.mjs gate`) and
  the teachability gate, but had NO operational gate for the GAME (core loop,
  progression, hook, reason-to-play, reference grounding). I optimized to the
  gates I had; the player/critic subagents were briefed to judge the SCREEN's
  look + teachability, not the game. Reference deconstruction was skipped (fake
  shots were style-only). The visual-first freeze became design-never; the
  "first slice = one goal/one action" FTUE rule got conflated with "the whole
  game = one screen, one fight".
- Fix: added a Game / core-loop gate to AGENTS.md (beside visual + teachability)
  with a separate game-design critic, and "design the loop + ref digest BEFORE
  art/screens; release-ready is never a first-screen visual verdict". The real
  game-design work goes to `tasks/`.
- Profiling: 4.5h session, 1.7% wall-clock coverage, 33 unresolved fails -
  subagent gaps not checkpointed; checkpoint long delegated stretches.

### 2026-06-16 - Generated Art Must Prove Source-To-Runtime, Not Just Look Better

- Context: Lead rejected a procedural "polish" pass because it did not visibly
  use generated art and still looked draft-like.
- Friction: The agent initially integrated generated assets before checking
  source/runtime orientation and before setting an AI profiling scope.
- Lesson: Generated visual work needs an early source-to-runtime proof:
  selected generated source, runtime cut/contact sheet, atlas pack, screenshot,
  and a directional-orientation check for arrows/asymmetric art.
- Status: T0070/T0072 added generated sources, provenance, pixel audits,
  native gates, atlas transform disable, and sprite Y-scale fix. T0071 remains
  open for chroma-key hot-path optimization.

### 2026-06-14 - Automation Green Did Not Mean Product Good

- Context: A long native RPG prototype run added systems, routes, state,
  DevAPI scenarios, and screenshots, but the lead rejected the visible game as
  unclear and visually unacceptable.
- Friction: The agent treated passing probes as progress while the first
  screen still failed product-read, FTUE, and art-direction quality.
- Lesson: For visual/gameplay-heavy game work, stop content expansion until a
  native screenshot passes the player-read gate: where am I, what do I do,
  what changed, what is the reward, and does it look like a game.
- Status: Added pipeline and skill stop-gates; T0006 is the visual rescue task.

### 2026-06-13 - Tooling Must Stay Passive

- Context: Profiling, validation, reflection, and task scripts grew into their
  own workflow.
- Friction: Agents started servicing stale diagnostics, review queues, broad
  validation, and generated handoff artifacts instead of working on the game.
- Lesson: AI tools should default to quiet, bounded, advisory output. Exhaustive
  output and generated artifacts need explicit `--verbose`, `--deep`,
  `--review`, `--all`, or `--include-final`.
- Status: Implemented for profiling/status/taskboard/validation planner; keep
  applying this rule to new scripts.

### 2026-06-13 - Live Status Must Be Short

- Context: `tasks/STATUS.md` grew into a long evidence log.
- Friction: Every orientation risked loading old history instead of current
  blockers and next actions.
- Lesson: `STATUS.md` is only a live index. Detailed evidence belongs in task
  logs, reports, screenshots, and git history.
- Status: `tasks/STATUS.md` was reset to a compact current index.

### 2026-06-13 - Review Queues Are Not Current Work

- Context: Many finished tasks remained in `review`.
- Friction: List/context output treated review backlog like active work.
- Lesson: Default taskboard commands should show `doing/todo/backlog` only.
  Use `list --review` explicitly for review cleanup.
- Status: Implemented in taskboard CLI and task-manager skill.

### 2026-06-13 - Reference Gates Need Short Front Doors

- Context: Reference-study rules were duplicated in `AGENTS.md`, `AI_PIPELINE.md`,
  skills, and playbooks.
- Friction: The correct rule became hard to scan and looked heavier than the
  implementation task.
- Lesson: Main agent files should contain the short gate and point to the
  detailed playbook only when a named reference actually drives the work.
- Status: Main docs shortened; detailed reference docs remain opt-in.

## Retrospective - 2026-06-13

Scope: long 67 World AI-assisted development session across concept work,
reference study, generated art, native gameplay, balance, release packaging,
and AI workflow rules.

### Summary

The game made substantial native progress, but the AI workflow became too
heavy. Tooling for profiling, validation, reflection, task status, reference
study, and release evidence started creating process obligations that competed
with the actual game work.

Current product truth remains simple: automated release/package checks can pass,
but the game is not release-ready until real manual child-test/user acceptance
is returned and any resulting fixes are applied.

### Main Problems

- The agent sometimes optimized for faster-looking paths instead of the native
  PC harness.
- Reference work was initially too shallow, then overcorrected into a bulky
  gate duplicated across docs.
- Visual work sometimes polished placeholders instead of moving through the art
  asset pipeline.
- `tasks/STATUS.md` became an evidence log instead of a short current index.
- Many completed tasks stayed in `review`, making current work look noisy.
- Profiling/reflection tools grew into a maintenance workflow.
- Broad validation was too easy to run repeatedly.

### Current Fixes

- Tool defaults are passive and advisory.
- Deep AI workflow artifacts require explicit `--deep`, `--verbose`,
  `--review`, `--all`, or `--include-final`.
- `tasks/STATUS.md` is compact again.
- Taskboard hides review tasks from normal list/context output.
- Profiling records slow/failing/large-gap signals by default instead of every
  small step.
- Validation planner defers broad/final checks unless explicitly requested.

### Next Rule

If AI tooling creates work that does not directly help answer "what should we
build, change, or verify next in the game?", simplify the tool or move the
behavior behind an explicit deep mode.

## External AI Observability Decision Criteria

The project profiles AI sessions with the local `tools/ai_profile/` JSONL
profiler and stays local-first. Do not add an external tracing/eval platform
(LangSmith, Phoenix, Langfuse, Braintrust, OpenTelemetry export, etc.) just
because reflection needs more data. Run a bounded side-by-side external pilot
only when a concrete trigger exists: multiple humans/agents need a shared trace
dashboard; human review/labels/annotations are part of the workflow;
datasets/experiments/evals must compare agent or model changes over the same
inputs; production AI app telemetry/cost/latency or online evals are required;
OTLP export is needed for a wider stack; or local JSONL review cannot answer an
important repeated question without manual reconstruction. Reject a tool that
needs accounts, keys, servers, or SDK wiring before the first useful question is
known, that would capture prompts/screenshots/child-test notes off the machine
without an explicit privacy decision, or that only duplicates local
summary/review/follow-up outputs. A pilot earns adoption only after it shows
lower reflection/debug time, better cross-agent/human review than local
markdown/JSON, reusable datasets/evals that prevent regressions, or production
monitoring the project actually needs. Local JSONL in `tmp/session_profiles/`
stays the baseline evidence source unless the lead explicitly changes that rule.

## Retrospective - 2026-06-16 (E003 cleanup + E004 Critter Corral)

Long 24h+ session: pipeline cleanup (E003 T0043-T0063) then built Critter Corral
from concept to release-candidate (E004) via many subagent executors. Key
lessons + durable fixes applied:

- **Profiler never fired (0 records).** Root cause: it was an opt-in CLI the
  agent must remember. Fix: wired it into BOTH harnesses as a hook
  (`.claude/settings.json` + `.codex/hooks.json` -> `tools/ai_profile/hook_record.mjs`)
  so coverage is guaranteed by the harness, not the agent. Verified live on Claude.
- **Built game visuals on the debug shape renderer -> full rework to sprites.**
  Fix: AGENTS.md rule "game visuals ALWAYS use real assets; shape renderer is
  debug-only." Clarify the render/asset path before building visuals.
- **Declared "release-candidate" but it was unteachable** (no text, opaque
  upgrades, no FTUE) - the visual gate checks appearance, not comprehension.
  Proposed fix: a "first-player teachability" gate beside the visual gate
  (would a new player understand in 10s? are systems explained?). Not yet added.
- **Built option C (image-similarity) then reverted** - it contradicted the
  just-made "qualitative gate" decision. Don't build a lead-decision option
  before confirming, especially against a just-set principle.
- **Proposed an existing game ("cow evolution")** - verify concept novelty.
- **CRLF warnings on every commit** - fixed with `.gitattributes` + autocrlf=false.
- Judged everything from static screenshots; never played the game / verified the
  core-moment feel in motion. Future: judge first-player UX + motion feel, not
  just composition.

## Retrospective - 2026-06-17 (Voxelheim UI/visual-noise pass)

Short, lead-driven visual pass on the idle prototype (6 commits: scaling, panel
rework, real icons, HUD polish, art-noise). Evidence: git log + screenshots +
profiler JSONL (it DID record: 137 claude events today). CORRECTION to a first
wrong read: the profiler was NOT useless. The "8.46h gap / guard
`current_scope_low_wall_clock_coverage`" is an OVERNIGHT IDLE (01:02 -> 09:30
+05) the coverage metric mis-penalizes as missing work -- scope T0005 was opened
16th 22:45 and never rolled, so its window spans the idle night. Reconstructed
active time (gaps<5m; the codex/* parallel design-source workstream excluded):
NIGHT depth-triage + balance v4 ~55 min (00:00-01:02); MORNING visual pass ~37
active min over ~65 wall (09:30-10:35): scaling 7, panel 6, icons 8 (most calls),
HUD 3, noise+2 design-agents 3 active / ~22 wall (parallel agents), retro 8. Real
tooling gap (NOT "no data"): the coverage guard should EXCLUDE idle gaps > ~1h or
auto-roll the scope per work session; and `status` should report active-time, not
raw wall coverage. Lessons + durable fixes:

- **Misleading asset reuse shipped twice.** Stand-ins read as the WRONG thing
  (castle keep = "Armor", signpost = "Boots"), and rock.png baked a rock+sign
  into ONE region so scene decoration drew a stray signpost ("выбивается").
  Fix queued (T0008): asset semantic-fidelity + composability gate (a labeled
  sprite must depict EXACTLY its one concept; don't bake multiple objects into
  one region you place as one). Extends the delegated-image-generation
  composable-asset rule. Real fix applied: agy-gen icons + crop rock-only.
- **Heavy 4-way text outline ON solid plates = "font noise".** Fix applied:
  emit_text_soft (single drop shadow) for all plate-backed text; 4-way outline
  reserved for over-scene floaters. Rule to add: plate-backed text defaults to
  single soft shadow, never stacked outline+glow (matches the new source note
  gamedesign/sources/avoiding_visual_noise_mobile_art_2026-06-17.md).
- **Stale taskboard overclaimed "RELEASE-CANDIDATE, UI/HUD 9"** while the lead
  called the same UI "отвратительно стрёмный" with a stretch bug. Recurring
  "thin claimed as done" in visual form. Status must drop off RC when the lead
  reports concrete defects.
- **ui_readability false-positives**: flags ~2px "hairline" on the minimap's
  decorative lines, not text -> wasted a confirm beat each iteration. The band
  metric should exclude decorative sprites / weight the eyeball montage.
- **What worked (keep):** two parallel design subagents (researcher ->
  gamedesign/sources note; art-director critic -> ranked top-6) mapped 1:1 to
  fixes; staged commits each with before/after + ui_readability --compare;
  delegated agy image-gen -> chroma-cut produced usable icons in one call;
  fixed root cause at the asset (cropped rock.png) not in code.
