# AI Pipeline Iteration Log

Purpose: record occasional retrospectives about AI-assisted development in this
repository: where the agent spent time poorly, where instructions or tools were
unclear, and what would speed up future work.

This is not the task board, not live project status, and not a design source of
truth. Use it only when there is a concrete process lesson worth preserving.

## When To Write Here

- A task took noticeably longer because the workflow was unclear.
- The agent repeated avoidable discovery or validation work.
- The agent made a wrong assumption that better instructions could prevent.
- A tool, skill, validator, or source-of-truth rule needs improvement.
- A recurring friction appears across sessions.

Do not write here for every normal task. Prefer short entries when the lesson is
specific and reusable.

## Entry Format

```markdown
## YYYY-MM-DD - Short title

- Context:
- Friction:
- Time sink:
- Likely cause:
- Proposed improvement:
- Follow-up owner:
- Status:
```

## 2026-06-13 - Current scope tool use needs shares first

- Context: Whole-profile runtime/captured-elapsed sections had totals and
  shares, but `Current Scope Tool Use` still listed raw durations only.
- Friction: After a focused game or pipeline iteration, a reader still had to
  calculate whether validation, shell commands, or checkpoints dominated the
  just-finished slice.
- Time sink: The reflection handoff could steer attention back to historical
  totals instead of the current iteration's actual bottleneck.
- Likely cause: Share output was added to whole-profile review sections before
  current-scope sections were made symmetrical.
- Proposed improvement: Add current-scope runtime/captured-elapsed totals and
  per-tool shares directly under `Current Scope Tool Use`.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Top improvements should carry bottleneck shares

- Context: Runtime, captured elapsed, context, and repeated-command sections
  now had totals and shares, but `Top Improvements` still used generic summary
  wording without the dominant percentages.
- Friction: A reader had to scroll into lower sections before seeing which
  runtime tool, context input, or repeated-command scope dominated.
- Time sink: The first decision layer still required manual cross-reading even
  though the measured shares were already available.
- Likely cause: Ranking data was added to review sections before the top
  recommendation generator was made share-aware.
- Proposed improvement: Include dominant shares directly in top improvements
  for repeated commands, runtime/captured elapsed, and context pressure.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Repeated commands need totals and shares

- Context: Reflection review classified repeated commands by scope and cause,
  but did not show total repeated occurrences or each class's share.
- Friction: A reader could see many repeated commands, but still had to sum
  scoped/preflight/broad-final counts before deciding where repetition was
  concentrated.
- Time sink: Repeated-command analysis stayed partly manual after runtime and
  context sections had already gained totals and shares.
- Likely cause: Repeated command review was built as a diagnostic list before
  the profiling handoff switched to ranked bottleneck summaries.
- Proposed improvement: Add total repeated occurrences, classified occurrence
  totals, and per-row shares to generated reflection review JSON/Markdown.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Context hotspots need totals and shares

- Context: Reflection review listed context hotspot character counts, but did
  not show total context volume or each hotspot's share.
- Friction: A reader could see large files and commands, but still had to
  calculate whether one source dominated context pressure.
- Time sink: Context-use analysis stayed partly manual, especially when
  deciding whether to optimize taskboard context, docs, or status reads first.
- Likely cause: Context analytics was propagated into the handoff before the
  ranking output was made symmetrical with tool runtime shares.
- Proposed improvement: Add total hotspot chars and per-hotspot shares to
  generated reflection review JSON/Markdown.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Time reviews need totals and shares

- Context: Reflection review listed tool/runtime rows and captured elapsed
  rows, but did not show total time or each tool's share.
- Friction: A reader still had to mentally sum durations before deciding
  whether validation, shell commands, context reads, or manual checkpoints were
  the real bottleneck.
- Time sink: Tool-use analysis remained partly manual even after runtime and
  elapsed time were separated.
- Likely cause: The review optimized for attribution labels before prioritizing
  percent-of-total ranking.
- Proposed improvement: Include runtime/captured elapsed totals and per-row
  shares in the generated reflection review and JSON.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Top improvements must name measured time sections

- Context: Reflection review already split command/tool runtime from
  checkpoint-captured elapsed time, but `Top Improvements` still pointed to the
  older combined `tool_use_summary` wording.
- Friction: A fast reader could mistake checkpointed manual/research/review
  time for slow tool execution, then optimize the wrong part of the pipeline.
- Time sink: The agent had to manually explain that `gap_checkpoint` and
  `checkpoint` rows were elapsed-time evidence, not expensive commands.
- Likely cause: The summary sections evolved faster than the generated
  recommendation text.
- Proposed improvement: When captured elapsed rows exist, make top
  improvements direct readers to `Tool Runtime Review` and `Captured Elapsed
  Review` explicitly.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Context hotspots need to reach reflection handoff

- Context: Profile review JSON already reported `context_hotspots` and
  `missing_context_inputs`, but the generated reflection draft/review did not
  surface them next to tool-use and repeated-command evidence.
- Friction: To explain context spend, the agent had to reopen heavier review
  artifacts or rely on memory instead of reading a compact handoff section.
- Time sink: Extra context was spent rediscovering that
  `node tools/taskboard/cli.mjs context` and profiling docs were among the
  largest inputs.
- Likely cause: Context analytics was collected in `review.mjs`, but not
  propagated through the normal reflection handoff.
- Proposed improvement: Carry `context_use_summary` into reflection draft and
  review, and require retrospectives to read it before opening raw review JSON
  or long docs.
- Follow-up owner: Current and future pipeline agents.
- Status: Implemented.

## 2026-06-13 - Current iteration analytics must not drown in history

- Context: Reflection review had a clean current scope, but whole-profile tool
  use and repeated validation history still dominated the generated review.
- Friction: A future agent could read historical tool/context totals as the
  just-finished iteration's bottleneck.
- Time sink: The agent had to manually compare current scope status against
  whole-profile findings before deciding what was actually current.
- Likely cause: `review.mjs` computed current-scope health, but did not expose
  current-scope tool/context summaries in the same handoff path.
- Proposed improvement: Emit current-scope tool/context summaries before
  whole-profile summaries, and label historical validation repeat evidence as
  historical when current scope is clean.
- Follow-up owner: Current and future pipeline agents.
- Status: Implemented.

## 2026-06-13 - Reflection needs an iteration-size snapshot

- Context: Reflection review showed current-scope tools/context, but the
  first screen still lacked the basic size/quality metrics for the current
  slice: records, profiled time, wall-clock span, telemetry gaps, and failures.
- Friction: The agent had to open `node tools/ai.mjs status` or lower review
  sections to explain how much of the iteration was actually measured.
- Time sink: Extra status reads and cross-checks were needed before writing a
  simple claim about the current iteration.
- Likely cause: The current-scope data existed in review JSON and status, but
  not in the reflection handoff's first decision section.
- Proposed improvement: Add a current-scope snapshot to review, draft, and
  final reflection review before tool/context details.
- Follow-up owner: Current and future pipeline agents.
- Status: Implemented.

## 2026-06-13 - Reflection needs a synthesized current readout

- Context: Current-scope snapshot, tool use, and context use were present, but
  the agent still had to manually combine them before writing the first
  retrospective paragraph.
- Friction: Top improvements stayed dominated by historical lessons, so the
  fastest path to "what happened in this iteration" was still not the first
  generated section.
- Time sink: Manual synthesis across current decision, snapshot, tool use, and
  context use.
- Likely cause: The reflection review exposed evidence but did not produce a
  compact current-iteration readout.
- Proposed improvement: Add `Current Scope Readout` to generated reflection
  review with clean/actionable state, coverage, telemetry gaps, failures,
  largest tool cost, and largest current context input.
- Follow-up owner: Current and future pipeline agents.
- Status: Implemented.

## 2026-06-13 - Current validation cost needs batch evidence

- Context: The current readout showed `ai_profile/validation_run.mjs` as the
  largest tool cost, but did not say whether that cost was planned validation
  or avoidable waste.
- Friction: The agent still had to inspect validation batch evidence before
  interpreting the top tool cost.
- Time sink: Manual cross-check between tool-use rows and validation batch
  rows.
- Likely cause: Current-scope validation batches were only available in
  whole-profile validation sections, not in the current-scope handoff.
- Proposed improvement: Carry current-scope validation batches into review,
  draft, final review, and the current readout.
- Follow-up owner: Current and future pipeline agents.
- Status: Implemented.

## 2026-06-13 - Current time claims need coverage confidence

- Context: Current-scope readout showed records, profiled time, and wall-clock
  time, but did not interpret whether the coverage was enough for precise time
  claims.
- Friction: A clean current scope with low coverage could still invite
  overconfident claims about where the whole iteration time went.
- Time sink: The agent had to manually compare profiled/wall-clock ratio before
  deciding how strong a time-spend claim could be.
- Likely cause: The handoff exposed coverage numbers but not a confidence
  label.
- Proposed improvement: Add a coverage-confidence sentence to `Current Scope
  Readout`: unknown, partial, limited, or usable.
- Follow-up owner: Current and future pipeline agents.
- Status: Implemented.

## 2026-06-13 - Partial coverage needs gap evidence

- Context: Reflection review could say current coverage was partial, but the
  first handoff screen did not name which wall-clock gaps caused that caution.
- Friction: A later retrospective still had to reopen lower-level profile
  status/review evidence to explain missing time.
- Time sink: Extra artifact reads were needed before making honest claims
  about where time went.
- Likely cause: `review.mjs` computed `largest_gaps`, but `reflection_draft`
  and `reflection_review` did not carry current-scope gaps forward.
- Proposed improvement: Include current-scope `largest_gaps` in the compact
  snapshot/readout and require agents to name the largest gap before precise
  time-spend claims.
- Follow-up owner: Current and future profiling/reflection agents.
- Status: Implemented.

## 2026-06-13 - Checkpoint time is not tool runtime

- Context: `tool_use_summary` can show `ai_profile/gap_checkpoint.mjs` as a
  high-duration row because it records elapsed manual/research/review time.
- Friction: A retrospective could misread a checkpoint row as the profiler
  tool itself being expensive.
- Time sink: Agents would have to manually reopen profile records to separate
  captured elapsed time from actual command/tool runtime.
- Likely cause: Tool-use aggregation summed `duration_ms` without labeling the
  source of that duration.
- Proposed improvement: Add `duration_kind`, `captured_elapsed_ms`, and
  runtime breakdown fields; reflection readout should report runtime and
  captured checkpoint time separately.
- Follow-up owner: Current and future profiling/reflection agents.
- Status: Implemented.

## 2026-06-13 - Runtime and checkpoint elapsed need separate review sections

- Context: Even after adding `duration_kind`, the main tool-use table could
  still start with checkpoint-captured elapsed time because it sorts by total
  duration.
- Friction: Fast reflection readers had to mentally split actual runtime from
  checkpointed manual/research/review spans.
- Time sink: Extra interpretation was needed before naming the real tool cost
  bottleneck.
- Likely cause: The profile had the right labels but not separate review
  sections for the two time meanings.
- Proposed improvement: Add `Tool Runtime Review` and `Captured Elapsed
  Review` sections, plus JSON summaries for each.
- Follow-up owner: Current and future profiling/reflection agents.
- Status: Implemented.

## 2026-06-12 - Concept setup and visual proof sequencing

- Context: The user selected a child-friendly meme evolution game direction,
  then redirected the tone toward meme-loud, bright, polished visuals.
- Friction: The pipeline correctly created a concept seed and task, but the
  transition into image generation happened before the generated fake shot was
  moved into the project and documented.
- Time sink: Extra context reads and artifact-location discovery were needed
  after image generation.
- Likely cause: The image generation save-path rule is known, but there is no
  project-local checklist for "generate fake shot -> copy to durable design
  path -> update concept/task/status".
- Proposed improvement: Add a short visual-proof checklist to the
  `primary-gdd-pipeline` workflow or this project's design conventions before
  producing more fake shots.
- Follow-up owner: Next agent touching visual GDD work.
- Status: Implemented first corrective pass in native PC (`src/main.c`) with
  evidence screenshot
  `build/captures/scenarios/first_67_loop_field_first_v5.png`; further
  release-quality art kit work remains.

## 2026-06-12 - Wrong web detour during PC prototype work

- Context: The user asked for a child-testable playable prototype while project
  rules say native desktop/PC is the preferred development and automation
  harness. Web/mobile builds should run only for web/mobile behavior or visual
  GDD surfaces.
- Friction: I created a browser playtest path after deciding the native screen
  was too debug-like. That bypassed the stated PC-build workflow and added
  irrelevant server/browser troubleshooting.
- Time sink: Time went into HTTP server setup, browser discovery, and failed
  screenshot capture instead of improving the native PC build.
- Likely cause: I over-corrected from "native screenshot is ugly" to "web is
  faster for pretty UI" without rechecking `AGENTS.md` validation rules.
- Proposed improvement: When a playable game prototype is requested in this
  repo, improve the native PC build first. Use web only when the task explicitly
  targets web/mobile or a visual GDD page.
- Follow-up owner: Current and future implementation agents.
- Status: Open.

## 2026-06-12 - Visual request treated as placeholder polish

- Context: The user asked for generated art visual and UI for a child-testable
  67 World prototype.
- Friction: I spent time improving shape-renderer placeholder rectangles and
  procedural mascot drawing instead of making generated bitmap art and UI the
  source of truth.
- Time sink: Native screenshot iterations improved mechanics visibility but did
  not solve the actual visual quality problem.
- Likely cause: The existing game feature workflow did not have an explicit
  art-first gate for requests that say generated visual, UI, beautiful,
  polished, or release-quality.
- Proposed improvement: Added `.codex/skills/game-visual-art-direction/` and
  an `AGENTS.md` rule requiring generated/UI asset work before placeholder
  renderer polish for visual-quality requests.
- Follow-up owner: Current and future implementation agents.
- Status: Implemented.

## 2026-06-12 - Art pipeline lacked reusable asset contract

- Context: The user accepted the generated character direction, then clarified
  that UI must be reusable runtime UI: slice9 buttons/panels, separate icons,
  and board parts instead of one baked gameplay image.
- Friction: The agent generated a usable-looking sheet, but the integration
  plan still depended on manual crops and briefly dismissed the pack/material
  route as heavy without checking the builder/cache path.
- Time sink: Discussion shifted to whether to bypass the engine asset pipeline
  instead of immediately producing a manifest, slicing assets, packing them, and
  validating in native PC.
- Likely cause: The art skill had an art-first gate but no mandatory art
  request packet, crop/slice9 manifest, or rule to measure pack builds before
  choosing direct PNG shortcuts.
- Proposed improvement: Added `gamedesign/knowledge/ai_art_iteration_pipeline.md`,
  updated visual and asset skills, and added eval anchors for request packets,
  reusable kinds, must-not-bake lists, slice9 insets, and measured pack builds.
- Follow-up owner: Current and future art/runtime agents.
- Status: Implemented.

## 2026-06-12 - Framebuffer evidence was masked by fallback capture

- Context: Native PC art integration produced a black-looking screenshot while
  the gameplay scenario still passed.
- Friction: The screenshot path silently fell back from framebuffer capture to
  window capture because PPM-to-PNG conversion consumed the first pixel byte
  when it matched whitespace.
- Time sink: Time went into diagnosing art/rendering before noticing the PNG
  dimensions were 964x587 instead of the expected 960x540 framebuffer.
- Likely cause: The DevAPI PPM parser skipped arbitrary whitespace after the
  header, which is unsafe for binary P6 pixel data.
- Proposed improvement: Fixed `tools/devapi/devapi_client.py` to consume only
  the header separator, and treat screenshot dimensions/source as part of
  visual QA evidence.
- Follow-up owner: Current and future runtime automation agents.
- Status: Implemented.

## 2026-06-12 - Art iteration still too manual

- Context: The user pointed out that the art workflow was still too slow and
  asked to research how other skills, agents, and AI game-art pipelines handle
  art.
- Friction: The project had packet/manifest rules, but no quick command to
  start a new art job and no explicit candidate-batch contract for parallel
  research, slicing, runtime integration, and visual QA.
- Time sink: Too much work still depended on manually remembering where to put
  source sheets, rejected candidates, crop manifests, pack commands, and native
  screenshots.
- Likely cause: The previous rules described the desired pipeline but did not
  make the art job a concrete scaffoldable unit of work.
- Proposed improvement: Added research notes, a `tools/assets/new_art_job.mjs`
  scaffold, skill/eval anchors, and an explicit art-job loop built around
  candidate batches, crop/slice manifests, pack commands, and native evidence.
- Follow-up owner: Current and future art/runtime agents.
- Status: Implemented.

## 2026-06-12 - Pipeline validation depended on brittle Python launcher

- Context: Full reusable-base validation was needed after adding a portable
  art-job scaffold.
- Friction: `pipeline_validate.mjs` invoked `py -3.12` through Node
  `spawnSync`; in this environment that launcher returned exit code 101 even
  though the same command worked from PowerShell.
- Time sink: The first full validation failed on tool launch instead of the
  actual pipeline changes.
- Likely cause: Windows Python launcher/WindowsApps resolution behaved
  differently under direct shell execution and Node child process execution.
- Proposed improvement: Updated `pipeline_validate.mjs` to probe `py -3.12`,
  `python`, and `python3`, then use the first runner that actually starts.
- Follow-up owner: Pipeline/tooling agents.
- Status: Implemented.

## 2026-06-12 - Cow Evolution reference was too shallow for implementation

- Context: The user rejected the current native screenshot as ugly, badly
  cropped, visually incoherent, and not understandable as Cow Evolution-like
  gameplay.
- Friction: I had captured high-level Cow Evolution mechanics, but did not
  translate the actual screen grammar deeply enough before implementing:
  field-first play, crates in-world, compact HUD, direct creature merging, and
  reward popups over the field.
- Time sink: Iterations went into polishing a board/card/button composition
  that should have been challenged earlier.
- Likely cause: The research doc listed useful patterns but did not include a
  hard mismatch audit against the current screenshot.
- Proposed improvement: Added a corrective deconstruction doc and a dedicated
  redesign task. Next implementation should rebuild the first screen around
  field-first gameplay before adding variants or more UI polish.
- Follow-up owner: Current implementation agent.
- Status: Open.

## 2026-06-12 - Reference research needed a hard implementation gate

- Context: After the Cow Evolution mismatch, the user asked to improve the
  pipeline by adding a rule for how to study references.
- Friction: Existing reference research rules required borrow/avoid/copy-risk,
  but did not force screen grammar, first-60-seconds behavior, video/screenshot
  evidence, or a mismatch audit before coding.
- Time sink: A broad feature-list understanding allowed implementation to move
  before the actual player-facing interaction model was proven.
- Likely cause: The GDD research playbook and implementation skills did not
  share the same "reference deconstruction before implementation" gate.
- Proposed improvement: Added `gamedesign/knowledge/reference_deconstruction.md`
  and updated GDD, feature, visual, and skill-eval rules so reference-driven
  work must document screen grammar, first-60-seconds actions, evidence, and a
  mismatch audit.
- Follow-up owner: Current and future implementation agents.
- Status: Implemented.

## 2026-06-12 - Green chroma key breaks green game assets

- Context: While integrating the generated field-first art kit, green-heavy
  assets such as grass, green button, shadow, selection effects, and crate
  variants went through the same green-screen removal path.
- Friction: Several runtime PNGs looked acceptable in source-sheet preview but
  became fully transparent or atlas-invalid during pack build. The pack builder
  caught the issue late, after slicing and C pack-id work.
- Time sink: I had to identify and exclude broken assets one by one before the
  pack could build.
- Likely cause: The art job used a single green chroma-key rule even though the
  asset family itself contains green UI/field elements.
- Proposed improvement: Added art-pipeline rules to choose key color by asset
  family, prefer magenta keying for green-heavy sheets, support `chroma_mode:
  none` for full rectangular tiles, and run alpha-bbox validation before pack
  building.
- Follow-up owner: Current and future asset-pipeline agents.
- Status: Implemented. Added magenta key modes in `tools/assets/build_67_world_art.py`,
  a pre-pack validator in `tools/assets/validate_67_world_pack_inputs.py`, and
  repaired field assets validated in native screenshot
  `build/captures/scenarios/first_67_loop_field_repair_v2.png`.

## 2026-06-12 - Generated sprite slicing needs a first-class contact-sheet tool

- Context: Batch 2 67 character sprites were generated as a single sheet, then
  cropped through `art_crop_manifest.json` into runtime PNGs.
- Friction: Initial crop boxes included tiny pieces of neighboring characters.
  The issue was visible only after manually building a temporary contact sheet.
- Time sink: I also lost time fighting PowerShell/Python quoting while creating
  the contact sheet through inline `py -c`.
- Likely cause: The pipeline has crop manifests and pack validation, but no
  standard command that creates a labeled contact sheet and checks edge
  contamination before pack integration.
- Proposed improvement: Add a reusable `tools/assets/contact_sheet.py` or
  project-agnostic asset QA command that reads an art job/asset manifest,
  renders selected sprites on a dark background, and reports nontransparent
  pixels near crop edges.
- Follow-up owner: Future asset-pipeline/tooling pass.
- Status: Open.

## 2026-06-13 - Project rules drifted behind active game state

- Context: The project had already moved into a 67 World release-quality native
  PC/mobile track, but `AGENTS.md` still said there was no active concept and
  that `src/main.c` was only a placeholder screen.
- Friction: A future agent following the minimal context protocol could trust
  the stale rule file over `tasks/STATUS.md`, restart concept discovery, or
  avoid continuing release work.
- Time sink: Continuation work had to reconcile contradictory source-of-truth
  files before choosing the next release-quality increment.
- Likely cause: Early seed-project rules were not updated when the user
  selected and repeatedly confirmed 67 World as the active concept.
- Proposed improvement: When a concept becomes active, update `AGENTS.md` in
  the same session as `tasks/STATUS.md`, and keep `STATUS.md` as the live gate
  for current proof/evidence.
- Follow-up owner: Current and future implementation agents.
- Status: Implemented in `T0036`.

## 2026-06-13 - Reference study needs a pre-code lock

- Context: The user challenged whether Cow Evolution had actually been studied
  after seeing gameplay/UI that did not clearly match the reference loop.
- Friction: Even with a deconstruction rule, an agent could still jump from
  "I will research the ref" to implementation without first naming the exact
  question, source packet, doc path, current native mismatch plan, and proof.
- Time sink: Feedback cycles were spent debating whether the ref was studied
  instead of reading one auditable artifact and comparing it to native evidence.
- Likely cause: The pipeline had a study template but no explicit lock that
  blocks coding/final art until the template contains observed facts and a
  current-build mismatch.
- Proposed improvement: Added Reference Lock to `AGENTS.md`,
  `AI_PIPELINE.md`, `gamedesign/knowledge/reference_deconstruction.md`, and
  `primary-gdd-pipeline`: mode, question, doc path, source packet, native
  capture plan/path, no-coding/no-final-art boundary, and expected proof.
- Follow-up owner: Current and future implementation agents.
- Status: Implemented in `T0020`.

## 2026-06-13 - Reference study needs a ready/not-ready state

- Context: After adding the Reference Lock, the user asked to improve the rule
  for how refs are studied before more gameplay work.
- Friction: A lock alone can still be treated as a checklist to fill later
  while implementation continues. The missing state was an explicit "not ready
  for implementation" answer when evidence is incomplete.
- Time sink: Ref feedback turns can drift into debate unless the next agent can
  point to a durable ready checklist and either unlock implementation or stop.
- Likely cause: The pipeline described what to collect, but did not name a
  Definition of Ready that blocks code/final art until source evidence, native
  mismatch, and proof target are present.
- Proposed improvement: Added Reference Study Definition of Ready and stop
  behavior to rules, pipeline docs, skills, and the reusable deconstruction
  template.
- Follow-up owner: Current and future design/implementation agents.
- Status: Implemented in `T0020`.

## 2026-06-13 - Reference study needs a visible digest

- Context: The user asked again to improve the pipeline around studying refs
  after gameplay/UI work did not visibly track the Cow Evolution reference.
- Friction: Even a durable deconstruction can be too hidden if the agent moves
  straight into implementation without showing what was actually observed and
  what mismatch is being fixed.
- Time sink: User review turns become debates about whether research happened
  instead of a concrete check of sources, observations, mismatch, and proof.
- Likely cause: The pipeline had an internal readiness gate but no required
  user-facing digest before coding or final art resumed.
- Proposed improvement: Added Reference Digest to rules, pipeline docs, skills,
  and the reusable deconstruction template.
- Follow-up owner: Current and future design/implementation agents.
- Status: Implemented in `T0020`.

## 2026-06-13 - Reference study needs source order

- Context: The user asked again to improve the pipeline rule for studying refs
  after seeing that gameplay/UI work could still feel disconnected from the
  named Cow Evolution reference.
- Friction: The gate said sources were required, but did not force an explicit
  order separating observed gameplay/screenshots from secondary summaries.
- Time sink: Agents can waste implementation turns by reading about a game
  before watching how it is actually played, then coding from interpreted
  feature lists.
- Likely cause: The existing rules had a source packet but no Source Ladder.
- Proposed improvement: Added Source Ladder to project rules, reusable
  pipeline docs, reference deconstruction, and relevant skills: user-provided
  material, official/store/trailer visuals, raw gameplay or long screenshot
  evidence, then guides/reviews/lectures/deconstructions/wikis/community notes
  as supporting interpretation.
- Follow-up owner: Current and future design/implementation agents.
- Status: Implemented in `T0020`.

## 2026-06-13 - Long sessions need an explicit reflection skill

- Context: The 67 World session ran for more than 24 hours across concept,
  generated art, native gameplay, release packaging, validation, and repeated
  pipeline corrections.
- Friction: Retrospectives were happening ad hoc in chat or as scattered log
  entries. There was no reusable procedure forcing the agent to separate
  progress from time sinks, mistakes, weak tool use, context loss, planning
  gaps, product-quality risks, and prompt/system fixes.
- Time sink: The same process problems had to be rediscovered after user
  corrections: web detour, shallow reference study, slow art pipeline, stale
  project rules, and visual QA that passed automated gates while still looking
  bad to the human lead.
- Likely cause: The pipeline had task/status and skill rules, but no dedicated
  "deep session reflection" skill with an evidence-first checklist.
- Proposed improvement: Added `.codex/skills/chat-session-reflection/` and an
  eval anchor. Use it after long sessions, failed cycles, major user
  corrections, or before starting another 24+ hour run.
- Follow-up owner: Current and future pipeline/reflection agents.
- Status: Implemented in `T0060`.

## 2026-06-13 - Profiling must be captured during work, not reconstructed

- Context: After the retrospective, the user asked for real profiling of
  chat/tool/context spend and then challenged whether raw telemetry belonged in
  git.
- Friction: Post-hoc thread extraction can recover turn durations,
  compactions, and some file/tool patterns, but not exact token usage or a
  complete command/tool list. Manual profiling also risks becoming more work
  than the development it measures.
- Time sink: Reconstructing telemetry from chat consumed extra turns and still
  produced partial data.
- Likely cause: The pipeline had reflection docs and a summary script, but no
  low-overhead way to collect command timing and checkpoint events while work
  happened.
- Proposed improvement: Add local, ignored JSONL profiling with
  `tools/ai_profile/run.mjs` for substantial commands and
  `tools/ai_profile/event.mjs` for sparse checkpoints. Keep raw profiles in
  `tmp/session_profiles/`; commit only reusable rules/tools and compact
  lessons.
- Follow-up owner: Future implementation/reflection agents.
- Status: Implemented in `T0062`.

## 2026-06-13 - Profile review should produce action items automatically

- Context: After adding low-overhead profile collection, the remaining gap was
  reflection prep: a summary says where time went, but does not directly tell
  the agent what must be explained or improved.
- Friction: Without an analyzer, the agent still manually scans JSONL/summary
  output for waste, failed commands, repeated validations, context hotspots,
  and missing closeout events.
- Time sink: Manual interpretation of profiles risks becoming another
  reflection task instead of accelerating the next session.
- Likely cause: The initial profiler stopped at collection and aggregation. It
  did not convert profile records into process-review prompts.
- Proposed improvement: Add `tools/ai_profile/review.mjs` to generate
  reflection-ready findings and suggested pipeline actions from JSONL profiles.
- Follow-up owner: Future reflection agents.
- Status: Implemented in `T0064`.

## 2026-06-13 - Broad validation needs a ladder before it runs

- Context: The first live profile review found repeated broad checks in one
  tooling/reflection loop: taskboard validation, skill sync/eval, portable
  pipeline validation, and diff checks were rerun multiple times while the
  scope was still changing.
- Friction: The rule "run narrowest validation first" existed only as prose, so
  the agent still defaulted to broad reassurance after small edits.
- Time sink: Full reusable-base and skill gates are useful, but repeated too
  early they consume time and profile budget without adding new evidence.
- Likely cause: No executable pre-validation planner separated preflight,
  scoped checks, and broad/final checks for the current change kind.
- Proposed improvement: Add `tools/ai_profile/plan_validation.mjs` to print a
  narrow-to-broad validation ladder from change tags or touched files. Broad
  gates should run once at the end of a batch unless a previous gate failed or
  the risk changed.
- Follow-up owner: Future implementation/reflection agents.
- Status: Implemented in `T0065`.

## 2026-06-13 - Current status needs a compact context entrypoint

- Context: During pipeline work the live `tasks/STATUS.md` grew past 80k
  characters. Agents were still instructed to read it at the start of long
  work, which made context loading expensive and increased stale-detail risk.
- Friction: The status file is useful as an index, but once it becomes large,
  full reads mix current gates with long evidence history and make every
  resume more expensive.
- Time sink: Agents spend context and attention on old release evidence or long
  active-work lists before identifying the one task or gate they need.
- Likely cause: The minimal-context protocol named `tasks/STATUS.md` directly
  instead of a bounded digest command.
- Proposed improvement: Add `node tools/taskboard/cli.mjs context` as the
  default current-context entrypoint. It reports status size, selected current
  sections, task counts, and a limited actionable task list, then tells agents
  to inspect only linked task/evidence files.
- Follow-up owner: Future implementation/reflection agents.
- Status: Implemented in `T0066`.

## 2026-06-13 - Profile review must normalize command spellings

- Context: After adding the context digest, the live profile review still
  reported separate repeats for `tools/taskboard/...` and `tools\taskboard\...`.
- Friction: The repeated-command count was directionally right but noisy,
  especially on Windows where slash style changes between shell/tool calls.
- Time sink: Reflection can chase duplicate-looking findings instead of seeing
  the true repeated command count.
- Likely cause: `review.mjs` counted raw command strings with no normalization.
- Proposed improvement: Normalize slash style and whitespace before counting
  repeated commands, while preserving original variants for debugging.
- Follow-up owner: Future profiling/reflection agents.
- Status: Implemented in `T0067`.

## 2026-06-13 - Repeated validation needs scope, not only counts

- Context: Live profile review showed repeated commands after validation ladder
  and context digest work, but the report still mixed cheap preflight checks
  with broad/final pipeline gates.
- Friction: A repeated `git diff --check` and repeated
  `node tools/pipeline_validate.mjs` do not have the same cost or meaning, but
  the review listed both as plain repeated commands.
- Time sink: Retrospectives can over-focus on harmless narrow repeats or miss
  the expensive broad/final reruns that should be batched.
- Likely cause: `review.mjs` had command counts but no validation scope
  taxonomy.
- Proposed improvement: Classify repeated commands as `preflight`, `scoped`,
  `broad/final`, or `unknown`, summarize repeats by scope, and call out
  repeated broad/final gates separately.
- Follow-up owner: Future profiling/reflection agents.
- Status: Implemented in `T0068`.

## 2026-06-13 - Context input sizes should be measured, not hand-entered

- Context: Live profile review kept reporting medium/high context records that
  lacked `context_inputs`. The agent could say context was expensive, but not
  always name which file or source consumed it.
- Friction: Manual `--context-input path:chars:reason` is easy to skip and easy
  to mistype, especially with Windows paths.
- Time sink: Retrospectives then spend effort reconstructing context cost from
  memory or broad file reads.
- Likely cause: The pipeline had `event.mjs` for manual context checkpoints but
  no measured helper for local file inputs.
- Proposed improvement: Add `tools/ai_profile/context.mjs` to measure local
  file character counts and append `files_read` plus `context_inputs`
  automatically. `review.mjs` should also list missing context-input lines and
  intents, not just the count.
- Follow-up owner: Future profiling/reflection agents.
- Status: Implemented in `T0069`.

## 2026-06-13 - Profile review should be consumable by tools

- Context: `review.mjs` had become useful for humans: it reported repeated
  validation scope, missing context inputs, failures, blockers, and suggested
  actions. But every next automation would have to scrape markdown.
- Friction: Markdown is fine for reflection, but brittle for follow-up tools
  that want to create draft tasks, compare profiles, or track recurring waste.
- Time sink: Agents would otherwise re-parse human text or rerun analysis
  instead of using structured findings.
- Likely cause: The review analyzer was initially designed as a readable
  checklist, not an automation boundary.
- Proposed improvement: Add `--json-output` to `tools/ai_profile/review.mjs`
  with schema version, findings, repeated command scopes, missing context
  input details, and suggested pipeline actions.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0070`.

## 2026-06-13 - Review JSON needs safe follow-up drafts

- Context: `review.mjs --json-output` made findings consumable by tools, but
  the next step still required a human/agent to manually translate those
  findings into possible tasks or rule changes.
- Friction: Direct auto-task creation risks duplicating existing tasks or
  preserving findings that were already fixed in the current iteration.
- Time sink: Without a draft generator, every reflection loop repeats the same
  interpretation of repeated broad validation, missing context inputs, failures,
  and waste/rework.
- Likely cause: Structured review output existed, but there was no safe
  intermediate artifact between "finding" and "task".
- Proposed improvement: Add `tools/ai_profile/followups.mjs` to generate
  reviewable markdown/JSON follow-up drafts from review JSON. Promotion to real
  task/rule/tool remains a deliberate agent decision after checking current
  state.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0071`.

## 2026-06-13 - Daily profiles need work-item segmentation

- Context: Live AI profiling now spans many small pipeline increments in one
  day, each with its own task, validation, and commit.
- Friction: A single daily JSONL profile can make repeated broad/final
  validation look worse than it is because independent tasks are mixed
  together.
- Time sink: Reflection has to infer which repeated commands belonged to the
  same work item instead of reading it directly from telemetry.
- Likely cause: Profile records had phase/category/intent, but no durable
  task or iteration metadata.
- Proposed improvement: Add optional `work_item` and `iteration` fields to
  profile events, document `--work-item`/`--iteration`, and have `review.mjs`
  summarize repeated broad/final commands by work item.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0072`.

## 2026-06-13 - Session profiles need wall-clock coverage

- Context: Profile summaries and reviews report recorded command/event
  duration, but long AI sessions also contain manual review, design thinking,
  waiting, user feedback, and unlogged tool use.
- Friction: A profile could look precise while only covering a small slice of
  the real wall-clock session.
- Time sink: Retrospectives then over-explain logged commands and under-explain
  the largest unprofiled gaps.
- Likely cause: `review.mjs` had no wall-clock span, merged profiled interval,
  coverage ratio, or gap list.
- Proposed improvement: Add wall-clock coverage and largest-gap reporting to
  the profile review markdown/JSON, and require retrospectives to explain
  low-coverage gaps or add sparse checkpoints next cycle.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0073`.

## 2026-06-13 - Reflection closeout should be one command

- Context: A complete profiled-session closeout requires summary, review
  markdown/JSON, and follow-up drafts.
- Friction: The workflow asked agents to remember and run multiple commands
  after already finishing the real work.
- Time sink: Reflection prep became another manual checklist and could be
  skipped or run out of order.
- Likely cause: `closeout.mjs` only wrote a summary; `review.mjs` and
  `followups.mjs` were separate manual steps.
- Proposed improvement: Make `closeout.mjs` write the full scratch reflection
  bundle by default, with opt-outs for summary-only or no-followups cases.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0074`.

## 2026-06-13 - Profiling needs a mid-session health check

- Context: The profiler can record events and produce a final closeout bundle,
  but agents still need to know during work whether telemetry is good enough.
- Friction: Without a quick status command, missing work-item metadata,
  missing context inputs, low coverage, failed records, or missing closeout
  artifacts are discovered late during reflection.
- Time sink: Late discovery turns telemetry repair into another manual
  retrospective step.
- Likely cause: `review.mjs` and `closeout.mjs` are end-of-cycle tools; there
  was no read-only health check for the current profile.
- Proposed improvement: Add `tools/ai_profile/status.mjs` to report profile
  health and one next profiling action without appending records or generating
  artifacts.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0075`.

## 2026-06-13 - Failed profile records need recovery state

- Context: The live profile status reported failed records from earlier
  `skills_eval` runs even though later matching `skills_eval` runs passed.
- Friction: Recovered failures looked like unresolved current health issues,
  which made the next action noisy.
- Time sink: Reflection would have to manually inspect whether each failure
  was later fixed.
- Likely cause: Profile analysis counted failed records but did not compare
  them against later passing records for the same normalized command.
- Proposed improvement: Classify failed records as recovered when the same
  command later passes, and report recovered versus unresolved failures in
  review, status, and follow-up drafts.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0076`.

## 2026-06-13 - Work-item metadata needs low-friction defaults

- Context: `status.mjs` showed low work-item coverage even after
  `--work-item` support was added.
- Friction: Repeating `--work-item` and `--iteration` on every profiling
  command is easy to skip during fast tool loops.
- Time sink: Reflection then has to segment a daily profile after the fact.
- Likely cause: Metadata capture required per-command discipline instead of a
  session-level default.
- Proposed improvement: Support `AI_PROFILE_WORK_ITEM` and
  `AI_PROFILE_ITERATION` environment defaults, with explicit CLI flags still
  overriding them for exceptions.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0077`.

## 2026-06-13 - Tool loops need persistent profile scope

- Context: `AI_PROFILE_WORK_ITEM` and `AI_PROFILE_ITERATION` reduce repeated
  CLI flags, but Codex tool calls may run in separate shell processes.
- Friction: Environment defaults can work inside one command but are not a
  reliable "set once for the session" mechanism across tool calls.
- Time sink: Agents still need to remember metadata flags or inline env setup
  for every command.
- Likely cause: The previous default-context design assumed a persistent
  terminal environment.
- Proposed improvement: Add `tools/ai_profile/scope.mjs` to write an ignored
  `tmp/session_profiles/current_scope.json` fallback. Profile records use
  explicit flags first, then env vars, then the persistent scope file.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0078`.

## 2026-06-13 - Profiler tools need regression tests

- Context: The AI profiler accumulated multiple coupled behaviors: scope
  precedence, status health, closeout bundles, recovered failures, and
  follow-up drafts.
- Friction: Live command checks prove the current change but do not guard
  future edits from breaking earlier profiler behavior.
- Time sink: Regressions would be rediscovered during reflection instead of
  caught during validation.
- Likely cause: `pipeline_validate.mjs` covered taskboard and skills but had no
  AI profile test suite.
- Proposed improvement: Add `tools/ai_profile/test.mjs` and run it in source
  and exported pipeline validation.
- Follow-up owner: Future profiling/tooling agents.
- Status: Implemented in `T0079`.

## 2026-06-13 - Profiling start should be one command

- Context: Persistent scope reduced repeated `--work-item` flags, but starting
  an iteration still required remembering separate `scope.mjs` and `event.mjs`
  commands.
- Friction: Agents can skip the start checkpoint or forget scope until several
  tool calls have already happened.
- Time sink: Retrospectives then spend time classifying missing metadata
  instead of using complete profiles.
- Likely cause: The profiling pipeline had capture tools but no default
  "start this work item now" entry point.
- Proposed improvement: Add `tools/ai_profile/start.mjs` to write persistent
  scope and append a `phase_start` event in one command.
- Follow-up owner: Future profiling/tooling agents.
- Status: Implemented in `T0080`.

## 2026-06-13 - Profile status must separate current setup from history

- Context: After adding `start.mjs`, live `status.mjs` showed current scope set
  but still recommended setting scope because older records lacked work-item
  metadata.
- Friction: The next action made a historical profile quality issue look like
  an immediate setup problem.
- Time sink: Agents can waste turns resetting scope instead of addressing the
  next real current issue, such as missing context inputs or low coverage.
- Likely cause: `status.mjs` used aggregate work-item coverage to choose the
  next action without first checking whether current scope was valid.
- Proposed improvement: Make status recommend `start.mjs` for new/missing
  setup, but ignore historical missing metadata for next-action priority once
  current scope is valid.
- Follow-up owner: Future profiling/tooling agents.
- Status: Implemented in `T0081`.

## 2026-06-13 - Command-produced context needs measurement

- Context: `status.mjs` correctly surfaced missing context-input details as the
  next current profiling issue.
- Friction: `context.mjs` measures local files, but important context often
  comes from read-only commands such as `node tools/taskboard/cli.mjs context`.
- Time sink: Reflection can see that context was consumed, but cannot measure
  command output size unless the agent manually records it.
- Likely cause: The profiling pipeline had no wrapper for command-produced
  context.
- Proposed improvement: Add `tools/ai_profile/context_command.mjs` to run a
  read-only command, print its output, and record stdout/stderr character count
  as `context_inputs`.
- Follow-up owner: Future profiling/tooling agents.
- Status: Implemented in `T0082`.

## 2026-06-13 - Profile status needs current-scope health

- Context: After command context was measured, live `status.mjs` still
  recommended fixing four old missing context-input records from earlier
  iterations.
- Friction: The status next action could keep pointing at stale history instead
  of the current work item.
- Time sink: Agents can waste turns trying to repair old telemetry that cannot
  be made precise after the fact.
- Likely cause: `status.mjs` used whole-profile aggregate context metrics for
  next-action priority even when persistent scope had a current `updated_at`.
- Proposed improvement: Add current-scope health based on `scope.updated_at`
  and use current-scope missing context/work-item counts for next action while
  keeping whole-profile totals for retrospective history.
- Follow-up owner: Future profiling/tooling agents.
- Status: Implemented in `T0083`.

## 2026-06-13 - Wall-clock gaps need duration checkpoints

- Context: After current-scope status stopped chasing stale context gaps, the
  next live health issue was low wall-clock profile coverage.
- Friction: `event.mjs` can mark a checkpoint, but without `duration_ms` it
  does not improve coverage or explain elapsed manual time.
- Time sink: Reflection still has to reconstruct long manual/research/design
  stretches from memory.
- Likely cause: The profiler had command duration wrappers but no low-overhead
  manual checkpoint that derives duration from the previous profile event.
- Proposed improvement: Add `tools/ai_profile/checkpoint.mjs` to record
  non-command work duration since the last profile record, with a default cap
  to avoid claiming unknown overnight gaps.
- Follow-up owner: Future profiling/tooling agents.
- Status: Implemented in `T0084`.

## 2026-06-13 - Complete profile bundles can still be stale

- Context: Live profile status reported a complete closeout bundle while the
  existing review artifact described 159 records and the profile had already
  grown past 200 records.
- Friction: Agents can treat old summaries/reviews/followups as current simply
  because all files exist.
- Time sink: Reflection then explains stale findings or misses new telemetry.
- Likely cause: `status.mjs` checked artifact existence but not whether the
  artifacts were newer than the profile JSONL.
- Proposed improvement: Add bundle freshness detection, stale artifact names,
  and a status rule to rerun closeout/review before relying on generated
  reflection artifacts.
- Follow-up owner: Future profiling/tooling agents.
- Status: Implemented in `T0085`.

## 2026-06-13 - Wall-clock coverage status needs current-scope view

- Context: After adding checkpoint capture, live status still recommended
  checkpoint work because whole-profile wall-clock coverage was low.
- Friction: Old low coverage can keep driving the next action even when the
  current focused iteration has its own clean scope.
- Time sink: Agents may add extra checkpoints to repair stale historical gaps
  instead of acting on current telemetry health.
- Likely cause: `status.mjs` scoped context/work-item health but still used
  whole-profile wall-clock coverage for next-action priority.
- Proposed improvement: Add current-scope wall-clock coverage and use it for
  next actions when current scope is active, leaving whole-profile coverage as
  retrospective history.
- Follow-up owner: Future profiling/tooling agents.
- Status: Implemented in `T0086`.

## 2026-06-13 - External observability needs a gate

- Trigger: The lead asked whether ready-made AI observability tools were too
  setup-heavy and whether we should build our own.
- Symptom: Without a decision gate, agents can either overbuild local tooling
  forever or prematurely add LangSmith/Phoenix/Langfuse/Braintrust/OpenTelemetry
  plumbing before a concrete dashboard/eval/review need exists.
- Time sink: Tool research can become another manual reflection task if it does
  not end in an operational rule.
- Likely cause: The pipeline described local profiling but did not define when
  external tracing/eval systems are worth piloting.
- Proposed improvement: Add `AI_PIPELINE_OBSERVABILITY_TOOLS.md` and
  `tools/ai_profile/observability_gate.mjs`; keep local JSONL as the baseline,
  require a bounded pilot for external systems, and adopt only after proven
  time savings or shared review/eval value.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0087`.

## 2026-06-13 - Validation plans need machine-readable artifacts

- Trigger: Profile follow-ups still reported 27 repeated broad/final
  `node tools/pipeline_validate.mjs` runs even after adding a validation
  ladder planner.
- Symptom: The planner helped humans, but automation and later agents had to
  parse markdown or rely on chat memory to know whether broad/final checks were
  allowed.
- Time sink: Broad gates are expensive, and repeated validation noise makes
  reflection spend time explaining preventable reruns.
- Likely cause: `plan_validation.mjs` had stdout JSON but no durable
  `--json-output` artifact with compact broad/final counts and next action.
- Proposed improvement: Add `--json-output`, `checks_by_tier`,
  `broad_final_count`, `deferred_broad_count`, and `next_action` so future
  agents can consume validation decisions directly.
- Follow-up owner: Future profiling/validation/tooling agents.
- Status: Implemented in `T0088`.

## 2026-06-13 - Profile follow-ups need current-scope filtering

- Trigger: `status.mjs` reported no urgent action for the active scope, while
  refreshed review/followups still promoted old missing context, missing
  work-item metadata, low coverage, and repeated broad validation as P1 draft
  tasks.
- Symptom: Agents could keep fixing historical telemetry from earlier
  iterations instead of using it as retrospective context.
- Time sink: Reflection time shifts from current problems to stale findings
  that cannot be repaired precisely after the fact.
- Likely cause: `review.mjs` and `followups.mjs` used whole-profile aggregate
  findings only, while `status.mjs` had already learned current-scope health.
- Proposed improvement: Add `current_scope` to review JSON/markdown and make
  follow-up drafts suppress historical-only issues when the active scope is
  clean, while still naming them for retrospective history.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0089`.

## 2026-06-13 - Recovered failure follow-ups need current-scope filtering

- Trigger: After current-scope filtering, live followups dropped from five
  suggestions to one, but the remaining recovered-failure suggestion was also
  historical-only.
- Symptom: Old failures that were already recovered could keep appearing as
  draft tasks even when the current scope had no recovered failures.
- Time sink: Reflection agents spend time reclassifying known historical
  negative feedback instead of checking whether the current iteration repeated
  it.
- Likely cause: `followups.mjs` suppressed historical validation/context/
  coverage issues, but not historical recovered failures.
- Proposed improvement: Add recovered/unresolved failure summaries to
  `current_scope` and suppress historical-only recovered failures in follow-up
  drafts while keeping them visible as retrospective lessons.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0090`.

## 2026-06-13 - Profile review needs current-scope-first output

- Trigger: Followups were current-scope filtered, but review markdown still
  opened with historical whole-profile findings as priority findings.
- Symptom: Later agents could chase old issues despite clean current scope.
- Time sink: Reflection has to manually reconcile status, followups, and
  review priority ordering.
- Likely cause: `review.mjs` had current-scope data but markdown ordering still
  emphasized whole-profile history.
- Proposed improvement: Render `Current Scope Findings` and
  `Current Scope Actions` first, keep `Historical Whole-Profile Findings`
  separate, and expose `current_scope.findings` /
  `current_scope.suggested_actions` in JSON.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0091`.

## 2026-06-13 - Clean profiles need baseline comparison

- Trigger: Follow-up drafts reported a clean profile and suggested using it as
  a baseline for a later session.
- Symptom: Without a compare tool, future agents must manually reconcile two
  review JSON files before saying whether the AI-development process improved
  or regressed.
- Time sink: Trend analysis becomes another reflection reading task and can
  confuse current-scope problems with old whole-profile telemetry debt.
- Likely cause: The profiling pipeline could generate review/follow-up
  artifacts but had no baseline-vs-current comparison step.
- Proposed improvement: Add `tools/ai_profile/compare_reviews.mjs` to compare
  two review JSON files, report current-scope regressions separately from
  historical whole-profile deltas, and emit machine-readable JSON.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0092`.

## 2026-06-13 - Baselines need a capture step before comparison

- Trigger: Follow-up drafts recommended using a clean review JSON as a
  baseline, but normal closeout/review commands write stable daily filenames.
- Symptom: A later closeout can overwrite the review JSON that was meant to be
  the comparison anchor.
- Time sink: Future agents may manually reconstruct which review was the clean
  baseline or compare against the wrong artifact.
- Likely cause: The compare tool existed, but the pipeline had no explicit
  baseline capture step.
- Proposed improvement: Add `tools/ai_profile/capture_baseline.mjs` to copy a
  clean review JSON to `tmp/session_profiles/baselines/<label>.review.json`
  and write a manifest with the compare command.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0093`.

## 2026-06-13 - Status should know whether baseline was captured

- Trigger: After adding baseline capture, `status.mjs` still reported "Use
  this profile as baseline" without saying whether a baseline already existed.
- Symptom: On resume, agents could repeatedly run baseline capture or ask
  whether the clean review had already been preserved.
- Time sink: Reflection setup becomes a manual scratch-artifact inspection
  instead of one status check.
- Likely cause: Baseline capture wrote manifests, but status did not read the
  baseline directory.
- Proposed improvement: Teach `status.mjs` to report captured baseline
  manifests and change next action to capture only when no baseline exists.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0094`.

## 2026-06-13 - Status should know whether baseline was compared

- Trigger: `status.mjs` reported captured baselines but did not know whether
  the current review JSON had already been compared against the latest
  baseline.
- Symptom: Agents still had to inspect scratch files or rerun comparison
  manually before making trend claims.
- Time sink: Reflection setup can repeat compare work or miss a stale
  comparison artifact.
- Likely cause: Baseline capture and baseline comparison were separate tools,
  but status only knew about capture manifests.
- Proposed improvement: Teach `status.mjs` to report comparison status
  (`missing`, `stale`, `regressed`, `fresh`) and print the exact
  `compare_reviews.mjs` command when comparison evidence is missing or stale.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0095`.

## 2026-06-13 - Reflection needs one compact evidence packet

- Trigger: Status, review, followups, baseline capture, and baseline comparison
  were all available, but writing a retrospective still required manually
  opening several scratch artifacts.
- Symptom: Reflection setup can waste context and time by repeatedly reading
  summary, review, follow-up, and comparison files.
- Time sink: Agents spend the first part of a retrospective reconstructing the
  evidence map instead of analyzing the work.
- Likely cause: The profiling pipeline produced useful artifacts but no compact
  handoff packet for the reflection step.
- Proposed improvement: Add `tools/ai_profile/reflection_packet.mjs` to gather
  current-scope findings/actions, follow-up drafts, suppressed historical
  findings, baseline comparison trend, and source artifact paths into one
  scratch markdown/JSON packet.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0096`.

## 2026-06-13 - Reflection packets should suppress satisfied follow-ups

- Trigger: The reflection packet showed readiness `ready` and stable baseline
  comparison, but still listed "Use clean AI profile as baseline" as an active
  follow-up.
- Symptom: Agents can repeat already-completed baseline capture/comparison work
  during reflection.
- Time sink: Satisfied process tasks stay visible as pending work and require
  manual interpretation.
- Likely cause: Follow-up drafts are generated from review JSON only, while
  the packet has broader scratch evidence such as baseline manifests and
  comparison JSON.
- Proposed improvement: Have `reflection_packet.mjs` classify follow-ups as
  pending or satisfied using packet-level evidence, starting with clean-profile
  baseline follow-ups.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0097`.

## 2026-06-13 - Reflection packet needs a draft step

- Trigger: The reflection packet reduced evidence gathering, but writing the
  retrospective still started from a blank page.
- Symptom: Agents can re-expand compact evidence into ad hoc prose and lose the
  current-state, follow-up, and symptom/cause/fix structure.
- Time sink: Rebuilding retrospective shape repeatedly instead of analyzing the
  next process improvements.
- Likely cause: The packet is an evidence map, not a structured retrospective
  starter.
- Proposed improvement: Add `tools/ai_profile/reflection_draft.mjs` to convert
  packet plus review JSON into scratch markdown/JSON sections, while requiring
  the agent to edit the draft with judgment before sharing.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0098`.

## 2026-06-13 - Repeated commands need scope-aware interpretation

- Trigger: The first reflection draft rendered `repeated_commands` with a
  generic "human review needed" cause/fix even though review JSON already had
  repeated-command scopes and examples.
- Symptom: Retrospectives can over-treat all repeats as waste, or waste time
  manually reopening review JSON to see which repeats were scoped, preflight,
  or broad/final.
- Time sink: The agent still has to reconstruct whether repeated commands were
  justified by fresh edits, batchable, or validation waste.
- Likely cause: The draft generator consumed review findings but ignored the
  structured repeated-command evidence adjacent to those findings.
- Proposed improvement: Include repeated-command scope breakdown, top commands,
  and broad/final by-work-item examples in the draft, and require explicit
  classification before creating process work.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0099`.

## 2026-06-13 - Status should cover the full reflection handoff

- Trigger: `status.mjs` reported fresh closeout bundle and fresh baseline
  comparison, but did not say whether reflection packet or draft artifacts were
  missing or stale.
- Symptom: Agents still had to remember the final packet/draft commands before
  writing a retrospective.
- Time sink: The last handoff step could be repeated, skipped, or manually
  inspected even though artifact freshness is mechanically knowable.
- Likely cause: Status had grown around telemetry health, bundle freshness, and
  baseline comparison, but the newer packet/draft tools were not integrated
  into the status decision tree.
- Proposed improvement: Add reflection artifact status with missing/stale/fresh
  states and exact packet/draft generation commands.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0100`.

## 2026-06-13 - Reflection prep should be one command

- Trigger: Even after status reported packet/draft freshness, preparing a
  retrospective still required agents to manually run closeout, baseline
  comparison, packet, and draft commands in the right order when artifacts were
  stale.
- Symptom: Routine handoff setup could be repeated, skipped, or performed in
  the wrong order despite every step being mechanically detectable.
- Time sink: Agents spend attention orchestrating scratch artifacts instead of
  analyzing the session and improving the process.
- Likely cause: The pipeline had good individual tools and a status decision
  tree, but no wrapper that executed the decision tree.
- Proposed improvement: Add `prepare_reflection.mjs` to run only stale/missing
  handoff steps while refusing automatic baseline capture and stopping on
  current-scope regressions.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0101`.

## 2026-06-13 - Reflection draft needs a decision review

- Trigger: `prepare_reflection.mjs` can prepare a fresh draft, but the next
  step still requires the agent to manually decide what is current action,
  historical-only context, and top next-cycle improvement.
- Symptom: The final retrospective can either over-promote historical lessons
  into new tasks or under-use the draft's structured evidence.
- Time sink: Agents spend extra attention converting generated draft sections
  into concise decisions before writing final prose.
- Likely cause: `reflection_draft.mjs` is intentionally a starter, not a
  decision review.
- Proposed improvement: Add `reflection_review.mjs` to consume draft JSON and
  emit current verdict, historical lesson status, repeated-command summary, and
  top improvements.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0102`.

## 2026-06-13 - Reflection review should be part of handoff status

- Trigger: `reflection_review.mjs` existed, but `status.mjs` and
  `prepare_reflection.mjs` still treated a fresh draft as the final handoff
  state.
- Symptom: Agents could skip the new decision-review step or regenerate it
  manually outside the normal status/prep path.
- Time sink: The last reflection decision artifact depended on memory instead
  of a mechanically detectable freshness check.
- Likely cause: The review generator was added after the packet/draft status
  chain, but the wrapper and status tree were not extended in the same
  iteration.
- Proposed improvement: Add reflection review missing/stale/waiting/fresh
  status, exact generation command, and automatic `prepare_reflection.mjs`
  execution after draft freshness.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0103`.

## 2026-06-13 - Validation plans need a profiled runner

- Trigger: The reflection review showed repeated validation commands across
  the profile, especially repeated broad/final `pipeline_validate.mjs` runs.
- Symptom: Agents can plan the right validation ladder but still manually run
  checks one by one, repeat broad gates, and forget to profile individual
  command cost.
- Time sink: Validation orchestration consumes attention and later reflection
  has weaker evidence about which checks were useful, failed, skipped, or
  repeated.
- Likely cause: `plan_validation.mjs` generated the ladder but did not execute
  or profile the concrete commands.
- Proposed improvement: Add `validation_run.mjs` to consume a validation plan
  or build one from change/risk inputs, execute non-placeholder checks by tier,
  record each command in the AI profile, and skip broad/final checks after
  earlier failures unless explicitly overridden.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0104`.

## 2026-06-13 - Validation batches need explicit reflection context

- Trigger: `validation_run.mjs` records each command separately, which is good
  telemetry, but repeated-command analysis can still read a planned validation
  batch as ordinary command repetition.
- Symptom: Retrospectives may over-classify a healthy validation batch as
  waste, especially when scoped checks and one broad/final gate appear near the
  same handoff.
- Time sink: The agent has to manually reconstruct whether repeated commands
  came from a planned batch or ad hoc reruns.
- Likely cause: Profile records lacked a shared validation batch id and
  `review.mjs` had no validation-batch section.
- Proposed improvement: Add `validation_batch_id`, plan risk/changes, check id
  and tier metadata to `validation_run.mjs` records; summarize batches in
  `review.mjs`, `reflection_draft.mjs`, and `reflection_review.mjs`.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0105`.

## 2026-06-13 - Wall-clock gaps need a thresholded checkpoint

- Trigger: The reflection review still reports low whole-profile wall-clock
  coverage, while normal `checkpoint.mjs` always writes a record even for short
  pauses.
- Symptom: Agents either leave manual/research/review gaps unprofiled or add
  noisy checkpoints for small pauses.
- Time sink: Later reflection has to guess whether gaps were real work,
  short idle time, or unmeasured review.
- Likely cause: The profile had a duration-inference checkpoint, but no
  thresholded helper that only records meaningful gaps.
- Proposed improvement: Add `gap_checkpoint.mjs` to write a checkpoint only
  when elapsed time since the latest profile record exceeds `--min-gap-min`,
  with cap and previous-record metadata.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0106`.

## 2026-06-13 - Broad validation repeats need batched/unbatched classification

- Trigger: `validation_run.mjs` added planned validation batches, but
  repeated broad/final analysis still counted planned final gates together
  with ad hoc reruns.
- Symptom: A healthy validation batch could make the retrospective look worse
  by increasing the broad/final repeat count.
- Time sink: Agents had to manually inspect batch records to decide whether a
  repeated `pipeline_validate.mjs` was planned or wasteful.
- Likely cause: `review.mjs` had total repeated broad/final counts but no
  batched/unbatched split.
- Proposed improvement: Add `batched_broad_final_commands` and
  `repeated_unbatched_broad_final_commands`; use unbatched repeats for waste
  findings while keeping planned batch gates visible as evidence.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0107`.

## 2026-06-13 - Historical validation waste must not become current action

- Trigger: `reflection_review.mjs` reported `current_action_required` even
  after current-scope baseline compare was stable and unbatched broad/final
  repeats were zero.
- Symptom: A historical repeated broad/final follow-up stayed pending and made
  a clean current scope look blocked.
- Time sink: Agents had to manually inspect status, review, draft, and
  follow-up artifacts to prove the action was historical-only.
- Likely cause: `followups.mjs` still used total
  `current_scope.repeated_broad_final_commands`, which includes planned
  batched final gates.
- Proposed improvement: Generate current broad/final follow-ups from
  `current_scope.repeated_unbatched_broad_final_commands` with a backward
  compatible fallback for older review JSON.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0108`.

## 2026-06-13 - Clean reflection reviews need zero real current actions

- Trigger: Fresh `reflection_review.mjs` output had `Verdict: current_clean`
  but still printed `Current actions: 1`.
- Symptom: The no-action explanatory sentence was stored in
  `current.actions`, so automation or a later agent could count a clean
  handoff as having one pending action.
- Time sink: Humans/agents had to read the action text to discover it was not
  actually work.
- Likely cause: The review JSON mixed status messaging with action lists.
- Proposed improvement: Keep `current.actions` for real pending work only and
  put clean-state prose in `current.status_message`.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0109`.

## 2026-06-13 - Game prototype work needs a pre-code context pack

- Trigger: During 67 World prototyping, the agent lost or underweighted
  project context: native PC is the primary harness, web is not a shortcut,
  Cow Evolution reference study is a gate, and generated UI/art must be
  reusable runtime assets.
- Symptom: The agent spent time on the wrong harness and weak visual/art
  integration before returning to the native playable path.
- Time sink: The agent had to rediscover project rules and user corrections
  after implementation had already drifted.
- Likely cause: Context was spread across `AGENTS.md`, task status, skills,
  design docs, art requests, and validation runbooks with no compact pre-code
  artifact for playable game iterations.
- Proposed improvement: Add `tools/game_context/iteration_context.mjs` and
  require playable work to start from a compact context pack, preferably
  captured through AI profiling with `context_command.mjs`.
- Follow-up owner: Future game-feature and profiling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Routine validation should not print the whole task backlog

- Trigger: `node tools/pipeline_validate.mjs` started with
  `node tools/taskboard/cli.mjs list`.
- Symptom: Validation output printed the full active review pile, which made
  the fast path noisy and pushed old review items into the agent's attention.
- Time sink: Agents had to visually filter dozens of non-current tasks before
  seeing whether the pipeline was healthy.
- Likely cause: The taskboard had `list` for full inspection and `context` for
  deeper orientation, but no small "status at a glance" command.
- Proposed improvement: Add `node tools/taskboard/cli.mjs summary` for counts,
  current goal, blockers, next priorities, open doing/todo/backlog work, and
  review count. Use it in routine validation.
- Follow-up owner: Future taskboard/pipeline agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Reflection facade must produce the whole handoff

- Trigger: After `node tools/ai.mjs reflect`, profile status still asked for a
  baseline comparison, reflection packet, draft, and review.
- Symptom: The command name promised reflection readiness, but the agent still
  had to run or interpret a multi-step handoff chain.
- Time sink: Future retrospectives would again start with tool maintenance
  instead of reading a ready reflection artifact.
- Likely cause: The facade exposed closeout behavior under the broader
  `reflect` name.
- Proposed improvement: Make `node tools/ai.mjs reflect` run the full
  `prepare_reflection` handoff by default. Continue through current-scope
  regressions so they become reflection evidence; reserve `--strict` for
  stopping on regressions and `--quick` for cheap closeout.
- Follow-up owner: Future profiling/reflection agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Validation facade must batch broad/final gates

- Trigger: Reflection review reported repeated unbatched broad/final commands
  after the agent validated pipeline work through `node tools/ai.mjs run --
  node tools/pipeline_validate.mjs`.
- Symptom: A legitimate final gate looked like ad hoc repeated validation
  waste because it had no validation batch metadata.
- Time sink: Agents had to inspect review JSON and command history to separate
  planned validation from avoidable reruns.
- Likely cause: The fast facade had `run`, `status`, and `reflect`, but no
  short command for planned validation batches.
- Proposed improvement: Add `node tools/ai.mjs validate --change <kind>
  --risk <risk>` as the normal path for AI pipeline/tooling validation. It
  wraps the validation batch runner so later reflection can classify broad and
  final gates correctly.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Wall-clock checkpoints need a facade too

- Trigger: Fresh profile status after validation showed current-scope
  wall-clock coverage near 3% even though command telemetry and reflection
  handoff were working.
- Symptom: Long manual reading, diff review, and decision time still appeared
  as unexplained gaps unless the agent remembered the internal
  `tools/ai_profile/*checkpoint.mjs` paths.
- Time sink: Reflections had to explain low coverage instead of using the
  profile directly to separate useful manual review from unknown idle time.
- Likely cause: Checkpoint tools existed, but the fast `tools/ai.mjs` facade did
  not expose them alongside `start`, `context`, `run`, `validate`, `status`,
  and `reflect`.
- Proposed improvement: Add `node tools/ai.mjs checkpoint "<intent>"` as the
  normal path for manual/research/review gaps. Keep it thresholded by default
  to avoid noisy short-pause records, with `--force` for exact captures.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Current scope must be slice-sized

- Trigger: After adding validation and checkpoint facades, reflection still
  reported current-scope regressions because `T0110/game-iteration-context-pack`
  included older unbatched final checks from before the fix.
- Symptom: Fixed process issues kept appearing as current action items because
  the active scope covered several process iterations.
- Time sink: Agents had to inspect raw JSONL lines to prove the issue was
  historical within the same umbrella work item.
- Likely cause: `start` was easy for a new work item, but there was no short
  facade command for starting a fresh iteration inside the existing work item.
- Proposed improvement: Add `node tools/ai.mjs focus <iteration>` to reuse the
  current work item and reset current-scope analysis after a commit, process
  fix, or direction change.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Generated advice must point to the fast facade

- Trigger: Fresh reflection became current-clean, but historical lessons and
  suggested pipeline actions still mentioned low-level profiler scripts such as
  `start.mjs`, `scope.mjs`, `event.mjs`, `validation_run.mjs`, and
  `plan_validation.mjs`.
- Symptom: A future agent could follow the generated advice and bypass the
  simpler `node tools/ai.mjs ...` workflow we had just created.
- Time sink: Agents would need to translate scratch reflection output back into
  the intended fast path before acting on it.
- Likely cause: The facade was added after the profile review/draft/follow-up
  generators, but their user-facing copy still described internal tools.
- Proposed improvement: Update generated status/review/draft/review/follow-up
  recommendations to prefer `node tools/ai.mjs start`, `focus`, `checkpoint`,
  `run`, and `validate`; keep low-level profiler scripts only for debugging or
  custom profiler work.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Measured context capture needs a facade

- Trigger: Reflection/status advice could point to the fast `ai.mjs` facade for
  start, focus, checkpoint, run, validate, and reflect, but context-file and
  context-command measurement still depended on remembering internal profiler
  scripts.
- Symptom: Future agents could skip measured context capture or call
  `tools/ai_profile/*` directly, which makes analytics feel like extra
  bureaucracy instead of a cheap part of normal work.
- Time sink: Retrospectives would continue to report missing context inputs,
  then spend time explaining why tool/context usage was not measurable.
- Likely cause: `node tools/ai.mjs context` only covered the game iteration
  context pack, not arbitrary local files or read-only command output.
- Proposed improvement: Extend `node tools/ai.mjs context` with `--path` for
  measured local files and `-- <command>` for measured command output, and make
  generated reflection advice prefer those facade paths.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Repeated command counts need triage labels

- Trigger: Reflection review reported repeated commands by scope and
  validation batches, but the next action still required manually deciding
  whether each repeat was planned validation, useful guardrail rerun, rework,
  or waste.
- Symptom: Agents could turn raw repeat counts into process tasks too early,
  or spend extra reflection time reopening profile records to interpret them.
- Time sink: Repeated-command analysis remained a manual judgment step even
  though the profile already knows scope, validation batch metadata, failures,
  work items, and line ranges.
- Likely cause: `review.mjs` summarized counts but did not attach a triage
  classification and next action to each repeated command.
- Proposed improvement: Add `repeated_command_classification` to profile
  review JSON/markdown and carry it through reflection draft/review.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Tool use needs its own summary

- Trigger: The reflection workflow asked for tool counts and tool-use audit,
  but profile review focused on commands, validation batches, and context
  rather than aggregating `record.tools`.
- Symptom: A retrospective could say which commands repeated, but still had to
  manually inspect JSONL to answer which tool classes consumed time, failed, or
  created rework.
- Time sink: Tool-use analysis stayed manual despite the profile already
  storing `tools`, `duration_ms`, `result`, `value`, `commands`, and
  `context_inputs`.
- Likely cause: `review.mjs` treated command text as the main tool signal and
  did not emit a separate `tool_use_summary`.
- Proposed improvement: Add `tool_use_summary` to profile review JSON/markdown
  and carry it into reflection draft/review.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Unrecorded tool use must become a finding

- Trigger: `tool_use_summary` exposed `(unrecorded)` profile records, but the
  reflection handoff did not yet flag those records as an explicit telemetry
  quality issue.
- Symptom: A retrospective could see incomplete tool-use attribution but still
  miss the process fix because `(unrecorded)` was buried inside a summary
  table.
- Time sink: Future agents would manually inspect raw JSONL to decide whether
  missing tool ids were historical noise or a current profiling problem.
- Likely cause: Review output aggregated tool ids but did not create a
  `missing_tool_metadata` finding/action from records without `tools`.
- Proposed improvement: Add whole-profile and current-scope missing tool
  metadata findings, plus follow-up drafts that point agents back to `ai.mjs`
  facades and profiler wrappers.
- Extra fix: `closeout.mjs` now records its own tool id and child profiler
  tools, because reflection handoff itself was creating a fresh missing-tool
  record.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Reflection should auto-capture the last long gap

- Trigger: Fresh reflection review still reported low wall-clock coverage and
  recommended manual `node tools/ai.mjs checkpoint "<intent>"` records after
  long non-command stretches.
- Symptom: The agent could do useful manual review/research, then remember to
  run `reflect` but forget the checkpoint immediately before handoff.
- Time sink: Retrospectives would keep explaining missing wall-clock coverage
  even when the handoff command had enough information to capture the final
  gap cheaply.
- Likely cause: `node tools/ai.mjs reflect` prepared artifacts but did not run
  the existing thresholded gap helper first.
- Proposed improvement: Make `reflect` run a thresholded pre-reflection gap
  checkpoint before quick/full handoff, with `--no-gap-checkpoint` reserved
  for telemetry debugging.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Recovered failures need triage labels

- Trigger: Reflection review still said to classify recovered failures as
  useful feedback, avoidable rework, or tool noise, but the review artifact did
  not provide those labels.
- Symptom: A retrospective had to reopen raw profile records to decide whether
  failed-then-passed commands should become process work.
- Time sink: Failure analysis stayed manual even though the profile already
  stores result, value, waste reason, command details, validation batch id, and
  recovery line.
- Likely cause: `review.mjs` detected recovered failures but did not emit a
  `recovered_failure_classification` handoff section.
- Proposed improvement: Add generated recovered-failure labels and next
  actions to review JSON/Markdown and carry them into reflection draft/review.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.

## 2026-06-13 - Broad/final repeat findings need occurrence counts

- Trigger: Reflection review reported `1` unbatched repeated broad/final
  command, while the underlying profile had 30 unbatched occurrences of that
  command.
- Symptom: The finding was technically true as a distinct-command count, but
  understated the scale of repeated broad/final validation.
- Time sink: A retrospective had to reopen arrays and manually sum counts to
  explain whether repeated final validation was a small issue or a large
  historical waste pattern.
- Likely cause: `review.mjs` used the number of repeated command entries in
  the finding message instead of the sum of unbatched occurrences.
- Proposed improvement: Add `repeated_unbatched_broad_final_occurrences` and
  carry it through review, reflection draft, and reflection review.
- Follow-up owner: Future profiling/reflection/tooling agents.
- Status: Implemented in `T0110`.
