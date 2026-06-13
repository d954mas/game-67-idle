# AI Development Session Profiling

Purpose: show where an AI agent gets stuck without turning telemetry into a
second project.

Profiling is passive by default. Normal game work must not pause to repair
stale summaries, bundles, packets, drafts, reviews, follow-ups, or baselines.
Those are deep-retrospective artifacts, not everyday gates.

## What To Learn

A useful profile answers:

- which commands failed;
- which commands were slow;
- which context reads were large;
- where long manual/research/review gaps happened;
- whether broad validation repeated without a good reason.

If it does not answer one of those questions, do not collect it during normal
game work.

## Default Use

Use the facade, not `tools/ai_profile/*`, for normal work:

```powershell
node tools/ai.mjs run -- <command>
node tools/ai.mjs context --path <file>
node tools/ai.mjs context -- <read-only-command>
node tools/ai.mjs checkpoint "Reviewed generated assets"
node tools/ai.mjs status
node tools/ai.mjs reflect
```

Passive defaults:

- `run` records only failed commands or commands slower than
  `--profile-slow-ms` (default `30000`).
- `context` records only failed or large context reads over
  `--profile-context-chars` (default `10000`).
- `checkpoint` records only long gaps over `--min-gap-min` (default `10`).
- `status` prints the short diagnostic: unresolved failures, slowest recorded
  work, largest context input, and whether normal work needs action.
- `reflect` writes a short closeout summary.

Use `--profile-mode full` only when the task is explicitly about AI workflow,
profiling, or a requested retrospective. Use `--profile-mode off` when even
passive telemetry would be noise.

## Deep Retrospective

Use deep mode only when the user asks for AI workflow review, a long
postmortem, or profiler debugging:

```powershell
node tools/ai.mjs reflect --deep
node tools/ai.mjs status --verbose
```

Deep mode may use:

- `tools/ai_profile/review.mjs`
- `tools/ai_profile/followups.mjs`
- `tools/ai_profile/capture_baseline.mjs`
- `tools/ai_profile/compare_reviews.mjs`
- `tools/ai_profile/reflection_packet.mjs`
- `tools/ai_profile/reflection_draft.mjs`
- `tools/ai_profile/reflection_review.mjs`
- `tools/ai_profile/prepare_reflection.mjs`

Generated deep artifacts stay in `tmp/session_profiles/` unless the lead
explicitly asks to preserve them.

## Artifact Policy

Commit reusable profiling code and this policy. Do not commit raw telemetry by
default:

- `tmp/session_profiles/*.jsonl`
- generated summaries/reviews/followups/packets/drafts/comparisons;
- recovered thread dumps;
- one-off timing extracts.

Promote only durable lessons, task changes, rule changes, or tool fixes.

## When To Use It

Use passive profiling when:

- a session is expected to run longer than about an hour;
- a command/build/test loop is repeating;
- packaging, release, art generation, or reference research has many steps;
- the user asks where the agent got stuck;
- context compaction or repeated context loading becomes a risk.

Do not start profiling for a small direct code/doc change unless it is already
showing friction.

## Status Interpretation

`node tools/ai.mjs status` should usually end with:

```text
No profiling maintenance needed for normal game work.
```

If status reports unresolved failures, inspect them. If it only reports low
coverage, stale bundles, missing packets, or old historical issues in verbose
mode, ignore that during normal game development.

## Validation

After changing profiler behavior, run the narrow tests that cover the change:

```powershell
node --test tools/ai.test.mjs
node --test tools/ai_profile/test.mjs
```

Use broad validation only when the change affects the portable base or shared
workflow behavior.
