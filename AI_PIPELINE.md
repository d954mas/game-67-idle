# AI Pipeline

Portable map for human-led AI game work. `AGENTS.md` owns repo-specific rules;
this file routes agents to the right workflow source.

## Load Only What Applies

- Agent behavior, context policy, Markdown shape, or multi-agent use:
  `docs/ai-pipeline/agent-workflow.md`
- Done criteria, validation routing, product gates, or repeated failure stops:
  `docs/ai-pipeline/quality-validation.md`
- Profiling, prototype closeout, visual/asset routing, or portable export:
  `docs/ai-pipeline/profiling-reuse.md`

Default context: `AGENTS.md`, `node tools/taskboard/cli.mjs context`, one
task/evidence file, one matching skill, and at most one deep reference above.

## Operating Rules

- Keep hot Markdown short; move procedure/history behind references, skills,
  tasks, or validators.
- Profiling is automatic (PostToolUse hook); read a session with `node tools/ai.mjs status`.
- Make one scoped change, then run the narrowest command that proves it.
- Do not call a slice done from one green gate; the gate taxonomy and the
  fails-twice-change-path rule live in `docs/ai-pipeline/quality-validation.md`.
- If the lead says a game/prototype is done, stopped, or only a test, stop game
  implementation and follow task/status instructions.

## Change-Size Tiers

Match ceremony to change size; default TIER 1, escalate only on the lead's "keep
this" or a visual surface.

- TIER 1 spike/trivial: one narrowest proving command + a screenshot for a visible
  change. No packet/report/four-verdicts/`--strict`/2nd pass.
- TIER 2 kept slice: the full gate ceremony in `docs/ai-pipeline/quality-validation.md`.
- TIER 3 visual surface: TIER 2 + the screenshot vision check before close.

## Common Commands

```powershell
node tools/taskboard/cli.mjs context
node tools/ai.mjs orchestration-check --current --json
node tools/ai.mjs validate
node tools/ai.mjs validate --review
node tools/ai.mjs validate --full
```

Quick after normal edits; `--review` for context/caps; `--full` for
export/runtime/deep-asset gates. `orchestration-check` previews a subagent packet
(advisory, not an acceptance gate); acceptance gates the work product, not the
delegation.
