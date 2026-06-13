---
id: T0110
title: Add game iteration context pack for prototype work
status: review
epic: ""
priority: P1
tags: [pipeline, context, gameplay, prototype, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add a compact pre-code context pack for playable game iterations so future
agents keep the active concept, native runtime gate, reference-study gate,
visual/art gate, current project gate, and validation requirements in view
before implementing a prototype or game change.

Keep the pipeline fast: analytics should be available through a short facade,
and project-specific game tools should be physically separated from reusable
workflow/runtime tools.

## Done when

- [x] `tools/game_context/iteration_context.mjs` prints markdown and optional
      JSON context for playable game work.
- [x] The context pack preserves wrapped hard-gate bullets from `AGENTS.md`
      instead of clipping critical rules.
- [x] The context pack is included in portable AI base export and pipeline
      validation.
- [x] `game-feature-iteration` requires the context pack before playable
      implementation when the tool is available.
- [x] Profiling docs explain how to capture the context pack through
      `node tools/ai.mjs context`.
- [x] `tools/ai.mjs` provides the fast path for start, context, profiled run,
      status, and full reflection handoff.
- [x] `node tools/ai.mjs focus <iteration>` starts a fresh current-scope slice
      inside the current work item so fixed issues do not stay current forever.
- [x] `tools/ai.test.mjs` covers `ai.mjs start` option forwarding and `focus`
      scope reuse.
- [x] `node tools/ai.mjs context --path <file>` and `node tools/ai.mjs
      context -- <command>` wrap measured context capture for ordinary
      profiling, so analytics covers tool/context usage without exposing
      internal profiler scripts.
- [x] `tools/ai.test.mjs` covers measured context file and command facade
      paths.
- [x] Generated profile status/review/draft/review/follow-up advice points to
      `node tools/ai.mjs ...` fast paths when a facade exists.
- [x] Profile review emits `repeated_command_classification` so reflection can
      triage repeated commands as planned validation, validation-waste risk,
      failure/rework signal, guardrail rerun, or manual-review case before
      creating process tasks.
- [x] Profile review emits `tool_use_summary` so reflection can report which
      tool classes consumed time, failed, produced context, or created rework.
- [x] Profile review emits `missing_tool_metadata` when records lack `tools`,
      including follow-up guidance to use `ai.mjs` facades or profiler wrappers
      that fill tool ids.
- [x] `node tools/ai.mjs checkpoint "<intent>"` provides the fast path for
      thresholded wall-clock checkpoints after manual/research/review gaps.
- [x] `node tools/ai.mjs reflect` creates the full reflection handoff by
      default and keeps current-scope regressions visible instead of stopping
      before packet/draft/review generation.
- [x] `node tools/ai.mjs validate --change <kind> --risk <risk>` wraps the
      profiled validation batch runner so broad/final checks are recorded as
      batched validation evidence.
- [x] Current 67 World-specific scripts are grouped under
      `tools/project_67_world/` instead of being mixed into generic tool
      folders.
- [x] `tools/taskboard/cli.mjs summary` gives a short orientation path that
      avoids printing the full review backlog during routine validation.
- [x] Regression tests cover native/web hard gates and source discovery.
- [x] Full validation is complete and the work is ready to commit.

## Open questions

None.

## Log

- 2026-06-13: Started `T0110` scope with `tools/ai_profile/start.mjs`.
- 2026-06-13: Added `tools/game_context/iteration_context.mjs` and tests for
  wrapped native/web hard gates, source discovery, and JSON output.
- 2026-06-13: Added `tools/ai.mjs` as the short AI workflow facade and moved
  67 World-specific asset, balance, release, and DevAPI scenario scripts under
  `tools/project_67_world/`.
- 2026-06-13: Added `node tools/taskboard/cli.mjs summary` and switched
  `pipeline_validate` away from noisy full task listing.
- 2026-06-13: Summary validation passed: `node tools/taskboard/cli.mjs
  summary --tasks-limit 3`, `node --test tools/taskboard/test.mjs`, `node
  tools/skills_eval.mjs`, and `node tools/ai.mjs run -- node
  tools/pipeline_validate.mjs`.
- 2026-06-13: Changed `node tools/ai.mjs reflect` to generate the full
  reflection handoff by default; `--strict` preserves stop-on-regression
  behavior and `--quick` keeps cheap closeout behavior.
- 2026-06-13: Added `node tools/ai.mjs validate` as the short path for
  profiled validation batches, so broad/final gates carry batch metadata for
  later reflection instead of looking like unbatched repeated commands.
- 2026-06-13: Added `node tools/ai.mjs checkpoint` as the short path for
  thresholded wall-clock checkpoints, so long manual/research/review gaps can
  be captured without requiring agents to remember internal profiler scripts.
- 2026-06-13: Updated profile status/review/draft/review-summary text to
  recommend `node tools/ai.mjs checkpoint` instead of internal checkpoint
  scripts, so generated reflection handoff points future agents at the fast
  facade.
- 2026-06-13: Added `node tools/ai.mjs focus <iteration>` to start a fresh
  current-scope slice inside the current work item after a commit, process fix,
  or direction change.
- 2026-06-13: Added `tools/ai.test.mjs` after manual testing exposed that
  `ai.mjs start` did not forward `--scope` / `--profile` options; fixed the
  forwarding and included the test in reusable pipeline validation/export.
- 2026-06-13: Updated generated profile status, review, reflection draft,
  reflection review, and follow-up recommendation text to prefer
  `node tools/ai.mjs ...` fast paths instead of low-level profiler scripts when
  a facade exists.
- 2026-06-13: Extended `node tools/ai.mjs context` so `--path <file>` records
  measured local-file context and `-- <command>` records measured read-only
  command output, keeping tool/context analytics on the same fast facade as the
  rest of the workflow.
- 2026-06-13: Validation passed after measured context facade updates: `node
  --check` for touched profiler/facade scripts; `node --test tools/ai.test.mjs`;
  `node --test tools/ai_profile/test.mjs`; `node tools/skills_eval.mjs`; `node
  tools/taskboard/cli.mjs validate`; `git diff --check`; `node tools/ai.mjs
  validate --change profiling --change pipeline --change skills --risk medium`;
  `node tools/ai.mjs reflect`; `node tools/ai.mjs status` reported stable
  baseline comparison and clean current scope.
- 2026-06-13: Added `repeated_command_classification` to profile review,
  reflection draft, and reflection review so repeated command analysis starts
  from triage labels instead of raw repeat counts.
- 2026-06-13: Validation passed for repeated-command classification: `node
  --check` for touched profile generators; `node --test
  tools/ai_profile/test.mjs`; `node tools/skills_eval.mjs`; live
  `node tools/ai_profile/review.mjs` showed `validation_waste_risk`,
  `planned_validation`, `failure_recovery_or_rework`, and
  `guardrail_rerun_review`; `node tools/ai.mjs validate --change profiling
  --change pipeline --change skills --risk medium` passed.
- 2026-06-13: Added `tool_use_summary` to profile review, reflection draft,
  and reflection review so tool-use analysis is available without manually
  reopening raw JSONL records.
- 2026-06-13: Validation passed for tool-use summary: `node --check` for
  touched profile generators; `node --test tools/ai_profile/test.mjs`; `node
  tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`; live
  `node tools/ai_profile/review.mjs` showed `Tool Use Summary`; `node
  tools/ai.mjs validate --change profiling --change pipeline --change skills
  --risk medium` passed.
- 2026-06-13: Added `missing_tool_metadata` findings and follow-up drafts so
  `(unrecorded)` tool rows become explicit telemetry-quality actions instead
  of passive summary noise.
- 2026-06-13: Fixed `closeout.mjs` to record its own tool id and child
  profiler tools; live `node tools/ai.mjs reflect` then reported a clean
  current scope with `missing_tool_records: 0`, while historical unrecorded
  records stayed as retrospective lessons.
- 2026-06-13: Updated `node tools/ai.mjs reflect` to run a thresholded
  pre-reflection gap checkpoint before quick/full handoff, reducing the chance
  that long manual review/research stretches are lost from wall-clock coverage.
- 2026-06-13: Added `recovered_failure_classification` to profile review and
  reflection handoff so failed-then-passed commands are triaged as useful
  feedback, avoidable rework, or tool/environment noise before creating tasks.
- 2026-06-13: Added `repeated_unbatched_broad_final_occurrences` so reflection
  reports the scale of broad/final validation waste, not only the number of
  distinct repeated commands.
- 2026-06-13: Validation passed: `node --check tools/ai.mjs`;
  `node --check tools/project_67_world/package_native_release.mjs`;
  `py -3.12 -m py_compile` for moved release/art/scenario scripts;
  `node tools/ai.mjs context`; `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`; `node tools/ai.mjs run -- node
  tools/pipeline_validate.mjs`; `git diff --check`; `node tools/ai.mjs
  reflect`.
- 2026-06-13: Validation passed for the validation facade: `node --check
  tools/ai.mjs`; `node tools/ai.mjs validate --change pipeline --risk low
  --dry-run`; `node tools/ai.mjs validate --change pipeline --change skills
  --risk medium`; `node tools/taskboard/cli.mjs validate`.
- 2026-06-13: Validation passed for the checkpoint facade: `node --check
  tools/ai.mjs`; `node tools/ai.mjs checkpoint "Test forced checkpoint
  facade" --force --profile tmp/session_profiles/checkpoint_facade_test.jsonl
  --duration-ms 1234`; `node tools/ai.mjs checkpoint "Test thresholded
  checkpoint empty profile" --profile
  tmp/session_profiles/checkpoint_facade_empty.jsonl`; `node
  tools/skills_eval.mjs`; `node tools/taskboard/cli.mjs validate`; `node
  --test tools/ai_profile/test.mjs`; `node tools/ai.mjs validate --change
  profiling --change pipeline --change skills --risk medium`.
- 2026-06-13: Validation passed for the focus facade: `node --check
  tools/ai.mjs`; `node --test tools/ai.test.mjs`; temp-profile `node
  tools/ai.mjs start TFOCUS initial --scope ... --profile ...`; temp-profile
  `node tools/ai.mjs focus next --scope ... --profile ...`; live `node
  tools/ai.mjs focus focus-facade-validation --intent "Start validation slice
  for focus facade"`.
- 2026-06-13: Planned validation batch passed after adding facade tests to the
  portable base: `node tools/ai.mjs validate --change profiling --change
  pipeline --change skills --risk medium`; fresh `node tools/ai.mjs reflect`
  then reported stable baseline comparison and clean current scope.
- 2026-06-13: Validation passed after generated recommendation facade updates:
  `node --check` for touched profile generators and `node --test
  tools/ai_profile/test.mjs`; `node tools/ai.mjs validate --change profiling
  --change pipeline --change skills --risk medium`; fresh `node
  tools/ai.mjs reflect` reported stable baseline comparison and clean current
  scope.
- 2026-06-13: Added `context_use_summary` to reflection draft/review handoff so
  context hotspots and missing context inputs are visible without reopening raw
  review JSON or long docs.
- 2026-06-13: Added current-scope tool/context summaries to profile review and
  reflection handoff so iteration-local bottlenecks are visible before
  whole-profile history.
- 2026-06-13: Added current-scope snapshot to review/draft/review handoff so
  iteration records, profiled/wall-clock time, telemetry gaps, and failure
  counts are visible without reopening profile status.
- 2026-06-13: Added `Current Scope Readout` to reflection review so the final
  retrospective can start from synthesized current-iteration evidence instead
  of manually combining snapshot, tool use, and context sections.
- 2026-06-13: Added current-scope validation batch evidence to review, draft,
  and final reflection review so validation-runner tool cost can be identified
  as planned validation before labeling it waste.
- 2026-06-13: Added current-scope coverage confidence to reflection readout so
  low profile coverage limits time-spend claims instead of being hidden behind
  clean telemetry status.
- 2026-06-13: Added current-scope largest gap evidence to reflection
  draft/review handoff so partial coverage explains which wall-clock intervals
  are unprofiled before making time-spend claims.
- 2026-06-13: Added duration-kind labeling for profile tool-use summaries so
  checkpoint-captured elapsed time is not mistaken for tool runtime.
- 2026-06-13: Split runtime and captured elapsed review sections so fast
  reflection readers can distinguish command/tool cost from checkpointed
  manual/research/review spans without reinterpreting the raw table.
- 2026-06-13: Updated reflection top improvements to point directly at `Tool
  Runtime Review` and `Captured Elapsed Review` when captured elapsed rows are
  present, so future retrospectives optimize command cost and manual/research
  elapsed time separately.
