---
id: T0338
title: "AI agent token-usage discipline: scoped search, compressed logs, short outputs"
status: doing
project: P001
epic: E001
priority: P1
tags: [ai-studio, agents, token-usage, workflow]
created: 2026-07-08
updated: 2026-07-08
---

## What

Reduce AI-agent token usage across `C:\projects\game-67-idle` first, then
carry the accepted policy shape into `C:\projects\neotolis-engine`.

Focus areas:

- tool output discipline;
- focused grep/search discipline;
- compressed test/build logs;
- short final reports;
- scoped subagent packets and concise subagent returns;
- taskboard/profiling support for large-output detection.

Decision workflow for this task:

1. The agent investigates one point at a time.
2. The agent explains the current state, options, tradeoffs, and proposed patch.
3. The lead accepts, rejects, or changes the decision.
4. Only accepted decisions are implemented.

Do not read old thread history for this work. Use repo files, current task logs,
and explicitly requested sources only.

## Decision queue

- [x] D1: decide durable instruction surfaces for `game-67-idle`
      (`AGENTS.md` vs `ai_studio/core_harness/workflow/README.md` vs skills).
- [x] D2: define focused search/grep discipline (`rg --files`, scoped `rg`,
      exclusions, stop-after-signal).
- [x] D3: define compressed command/test/build-log reporting.
- [x] D4: update subagent packet and return contracts for bounded research and
      no raw dumps.
- [x] D5: decide whether taskboard needs a compact `show`/section mode.
- [x] D6: decide whether profiling/status should flag large stdout/stderr.
- [ ] D7: mirror accepted rules into `neotolis-engine` without changing its
      engine code.
- [ ] D8: pilot the accepted rules on 2-3 real tasks before promoting them to
      hooks/skills/automation.

## Done when

- [ ] Each decision in the queue has an accepted/rejected/deferred note in
      `## Log`.
- [ ] Accepted `game-67-idle` documentation/tooling changes are implemented and
      validated with the narrowest relevant checks.
- [ ] Accepted `neotolis-engine` changes are captured as a separate repo plan or
      patch path; no direct engine-code edit is made from this repo.
- [ ] Final report is short: changed files, checks run, unresolved decisions.

## Open questions

- Should D1 start with root `AGENTS.md`, or should root stay minimal and the
  first implementation target be `ai_studio/core_harness/workflow/README.md`?
- For `neotolis-engine`, should the work be a local patch proposal only, or do
  we prepare an issue/PR plan after the `game-67-idle` pilot?

## Log

- 2026-07-08: Created after research on Claude/Codex/AI-studio token-efficiency
  practices. Initial owner: P001/E001 because this is reusable AI Studio pipeline
  work, not game-specific content. Next step: D1 investigation and decision.
- 2026-07-08: D1 rejected/no-op by lead. Existing harness and repo docs already
  cover the durable-instruction surface well enough (`AGENTS.md` as short route
  map, workflow README for scoped context/search/output behavior). Do not add a
  new token-discipline pointer or policy layer unless repeated failures prove a
  real gap. Main residual issue is narrower: `fast-worker` asks for "test
  results verbatim", which belongs under D3/D4, not D1.
- 2026-07-08: Independent plan critique completed after lead challenged D1.
  Consensus: do not add a new general token-discipline layer. D2/D4/D5 are
  already covered well enough by existing workflow/orchestration/taskboard
  contracts; D7 should be deferred instead of mirrored wholesale into
  neotolis-engine. Real narrow candidates are D3 (`fast-worker` "test results
  verbatim" vs compact output) and D6 (profiling does not persist/flag
  stdout/stderr size). D8 remains the anti-bloat gate before any hooks/skills.
- 2026-07-08: D2 review/critique/research agents completed. Two recommend
  DROP/no-op because workflow/taskboard/skill surfaces already enforce scoped
  context/search. One critic recommends only a tiny workflow README command-shape
  hint for `rg --files` + scoped `rg`, mainly for explicit repo-wide/build grep
  gates. Awaiting lead decision: D2 DROP/no-op, or accept the tiny hint.
- 2026-07-08: D2 lead decision: DROP/no-op. Add nothing. Existing scoped
  search/context rules are sufficient; revisit only if a pilot exposes repeated
  broad-search failures.
- 2026-07-08: D3 lead decision: point fix only. Updated `.codex/agents/fast-worker.toml`
  to replace "test results verbatim" with command + pass/fail status + relevant
  failure excerpt + full log path if available. No general test/build log wrapper
  added.
- 2026-07-08: D4 lead decision: DROP/no-op. Existing `AGENTS.md`, `.codex/agents/README.md`,
  and workflow orchestration docs already define bounded packets, `Return`/`Stop`,
  fresh subagent context, and compressed findings instead of transcripts. D3 fixed
  the only concrete conflicting subagent-return wording.
- 2026-07-08: D5 lead decision: DROP/no-op. Taskboard already provides compact
  `summary`, `context`, and `list` payloads, and `show` intentionally returns one
  document body for editing. Do not add compact `show`/section mode unless pilot
  evidence shows repeated misuse on oversized task bodies.
- 2026-07-08: D6 lead decision: IMPLEMENT minimal numeric telemetry. Added
  `output_chars`/`output_lines` to existing profiling result records for JS
  fallback, Codex recovered failures, and native `hook_record_fast`, without
  storing stdout/stderr text. `status.mjs` now reports a compact "Top Noisy
  Outputs" rollup by command key. Validation: `node --test
  ai_studio/core_harness/profiling/tests/profiling.test.mjs` passed (27/27).
  Speed check against a temporary `HEAD` build of `hook_record_fast`: 500
  PostToolUse invocations averaged old/new 7.813/8.247 ms for 3-char output and
  8.409/8.514 ms for 96 KB output; added cost is process-spawn-level noise.
