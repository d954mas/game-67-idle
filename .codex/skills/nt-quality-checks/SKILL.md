---
name: nt-quality-checks
description: "Select and record Studio quality checks for changed game output, assets, design, runtime behavior, or quality rules; also use for explicit Q-check requests."
---

# NT Quality Checks

Select evidence for the requested quality claim. Use the catalog in
`ai_studio/quality/README.md`; do not create project-local rule IDs or treat
every file change as a reason for a full review.

## Sources

- Quality entry: `ai_studio/quality/README.md`.
- Rule groups: `ai_studio/quality/rules/*/README.md`.
- Rule files: `ai_studio/quality/rules/*/checks/Q*_NNN_*.md`.

## Approach

Choose checks by the changed behavior, claim, or risk. Open the matching rule
directly from the catalog; read a group README only when its context is needed.
A group's `001` check applies only when its `Use When` matches.

Gather the selected rules' evidence, reusing current applicable proof. Repeat
or expand checks when changes, failures, or unresolved concerns require it.
One change can need multiple groups; one green rule cannot prove unrelated
quality dimensions.

## Outcome

Record `pass`, `block`, `review`, or `unverified` with evidence for each check.
When a task exists, use its structured `quality` state through the Taskboard CLI:

```powershell
node ai_studio/taskboard/cli.mjs set <id> --quality "QCLR_001=pass" --quality-evidence "QCLR_001=<durable proof>" --json
```

Use the task's owning store (`--game <game-id>` for private games). For multiple
checks, supply per-check evidence. The CLI appends history to the
log; log-only records do not satisfy closure. New `skip` decisions are rejected.
Only `pass` checks permit closure; omit quality when no rule applies. Follow
`ai_studio/taskboard/task-store-reference.md#done-and-evidence` for storage,
durable evidence, and lifecycle details.

Without a task, report outcomes and evidence in the response or requested
durable artifact. Do not create a task just for quality logging.

## Profiling Commands

- Usage summary: `node ai_studio/quality/profile.mjs`.
- Usage across game stores: `node ai_studio/quality/profile.mjs --include-private --json`.

Run profiling for a usage audit, not as an acceptance check. It counts task-log
history, including legacy outcomes, rather than proving current quality.

## Maintenance Commands

Use owner checks only when their inputs change:

- Map validation: `node ai_studio/architecture_map/validate_map.mjs`.
- Harness doc reference check: `node ai_studio/core_harness/validation/doc_reference_check.mjs`.
- Profiler tests when profiler behavior changes: `node --test ai_studio/quality/tests/profile.test.mjs`.

## Boundaries

- Do not add ad-hoc quality rules outside `ai_studio/quality/rules/`.
- Do not treat technical proof as player clarity, art, asset, GDD, or
  game-design approval.
- Do not force all checks before closeout; use the checks that match the current
  iteration and evidence state.
