---
name: nt-quality-checks
description: "Use this skill when selecting, running, reviewing, logging, profiling, or explaining AI Studio quality checks in this repository. Triggers include explicit user requests for quality checks, QCLR, QART, QASSET, QGDD, QDES, or QTECH rules, and agent self-checks after substantial work that changed player-facing output, assets, GDD/design docs, gameplay/design logic, technical behavior, or quality-rule files."
---

# NT Quality Checks

Use this as a thin router to `ai_studio/quality/`. Do not duplicate rule text
or create project-local quality rule IDs here.

Use it for explicit quality-check requests and for self-checking substantial
completed work. Do not trigger it for every changed file; trigger it when the
work created a player-facing, asset, design, gameplay, technical, or quality-rule
claim that needs evidence.

## Sources

- Quality entry: `ai_studio/quality/README.md`.
- Rule groups: `ai_studio/quality/rules/*/README.md`.
- Rule files: `ai_studio/quality/rules/*/checks/Q*_NNN_*.md`.

## Workflow

1. Start with `ai_studio/quality/README.md`.
2. Pick one or more groups from the changed work, claim, or risk.
3. Open only the matching group README files.
4. Open only rule files whose `Use When` section matches the task.
5. Use a group's `001` rule only when its `Use When` matches.
6. Gather the evidence requested by the selected rule files.
7. Record `pass`, `block`, `review`, `skip`, or `unverified` where the work is reported.

Do not run every rule. A single change can need multiple groups, but one green
rule is not acceptance for unrelated quality dimensions.

## Group Routing

- Player Clarity: visible player understanding, feedback, responsive viewports,
  virtual controls, and player-facing presentation.
- Art: art direction, generated art, visual target fit, polish, and avoiding
  debug/placeholder defaults.
- Assets: asset source, provenance, origin, license, integrity, runtime
  readiness, and material data.
- GDD: design source packages, source order, file roles, contradictions,
  requirements, and acceptance criteria.
- Game Design: playable loop, player motivation, economy, progression, feature
  fit, and playable-slice strength.
- Technical: changed code, scripts, generated data, runtime behavior,
  build/run proof, state, packaging, and automation.

## Outcome

When a task exists, write stable quality lines in task `## Log`:

```text
- YYYY-MM-DD: Quality: QCLR_001=pass; QTECH_001=review; evidence: <short proof or artifact>.
```

When no task exists, record the same outcome in the final response, PR/review
comment, or another durable artifact. Do not create a task only to satisfy
quality logging.

## Profiling Commands

- Usage summary: `node ai_studio/quality/profile.mjs`.
- Archived usage summary: `node ai_studio/quality/profile.mjs --include-archive --json`.
- Profiler tests: `node --test ai_studio/quality/tests/profile.test.mjs`.

Profiling only summarizes recorded quality-check usage. It does not prove that
the current work passed a quality check. It scans task logs only; outcomes
recorded only in final responses, PR/review comments, or other non-task
artifacts are not counted.

## Maintenance Commands

Use these only when changing quality docs, rule files, skill surfaces, or the
architecture tree:

- Map validation: `node ai_studio/architecture_map/validate_map.mjs`.
- Harness doc reference check: `node ai_studio/core_harness/validation/doc_reference_check.mjs`.

## Boundaries

- Do not add ad-hoc quality rules outside `ai_studio/quality/rules/`.
- Do not treat technical proof as player clarity, art, asset, GDD, or
  game-design approval.
- Do not force all checks before closeout; use the checks that match the current
  iteration and evidence state.
