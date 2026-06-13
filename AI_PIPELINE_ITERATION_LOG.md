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
