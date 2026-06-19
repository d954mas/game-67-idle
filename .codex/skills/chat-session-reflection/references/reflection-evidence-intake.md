# Reflection Evidence Intake

Load this reference when starting a retrospective or checking what evidence can
support claims about bottlenecks, mistakes, wasted time, weak tool use, context
loss, planning gaps, quality risks, profiler/telemetry gaps, or pipeline
improvement.

## Inputs

Use durable evidence before memory:

1. `AGENTS.md`, `AI_PIPELINE.md`, `tasks/README.md`.
2. `node tools/taskboard/cli.mjs summary`; use
   `node tools/taskboard/cli.mjs context` only if the summary is not enough.
3. `git status --short` to identify changed durable artifacts without implying
   they were all part of the reflected session.
4. Relevant task logs, reports, screenshots, package logs, or validation
   outputs.
5. Passive profiling telemetry in `tmp/session_profiles/` when present.
6. Conversation context only after checking durable state.

Avoid high-cost context reads. Treat full `tasks/STATUS.md`, old task logs,
generated profile reviews, and archived material as optional evidence.

If the reflection suggests external AI observability/eval tooling, consult the
External AI Observability Decision Criteria in `AI_PIPELINE_HISTORY.md` first.
Do not infer time spent from memory when profiler coverage is broken; mark those
intervals `unknown` or `likely`.

## Evidence Quality

Report:

- profile file and whether it exists;
- record count;
- unresolved failures and recovered ones;
- wall-clock coverage;
- slowest recorded work and most-run (repeated) commands;
- long manual/research/review gaps;
- stale bundles or stale artifacts if they limit confidence.

Treat tests, green checks, and generated reports as evidence only after checking
that they cover the reflected claim.
