---
id: T0418
title: Simplify default agent delegation policy
status: done
project: P001
epic: E018
priority: P1
tags: [agents, workflow, context]
created: 2026-07-13
updated: 2026-07-13
quality: {"notApplicable":{"reason":"Process-policy and documentation change with no player-facing, runtime, asset, or release output."}}
---

## What

Replace mandatory delegation for broadly defined non-trivial work with a small
benefit-based rule. The lead agent should execute coherent local work directly
and delegate only independent bounded packets that materially reduce latency,
context load, or review risk.

## Done when

- [x] Routine related multi-file work no longer requires delegation or a
      delegation-decision ceremony.
- [x] Large independent research, parallel implementation, and adversarial
      review remain valid delegation cases using the closest catalog role.
- [x] Review budget follows risk: mechanical/docs/moves need no reviewer;
      normal logic gets one; security, concurrency, and release work gets two.
      Repeat review only after a high-risk finding or contract change.
- [x] Delegation is not used when packet description, context transfer, and
      reintegration cost more than direct work.
- [x] The change deliberately supersedes the earlier broad mandatory rule; it
      preserves useful subagents rather than changing to a never-delegate rule.
- [x] If host policy requires approval, one reusable session/chat approval is
      enough; routine tasks add no per-task delegation ceremony.
- [x] Existing fitting agent identities are reused instead of creating
      disposable roles, and event-driven/long waits replace tight
      `wait_agent` polling.
- [x] No new generic roles or policy layer are introduced.
- [x] AGENTS/workflow/enforcement tests describe the same compact rule.
- [x] Unrelated hot-path guards, telemetry, or agent infrastructure are not
      added as part of this policy simplification.

## Open questions

- None blocking.

## Log

- 2026-07-13: Lead accepted reducing mandatory delegation after the refactor
  retrospective showed repeated context and coordination overhead.
- 2026-07-13: Implemented benefit-based delegation and risk-tier review policy across AGENTS, Workflow, Orchestration, enforcement contract, Claude overlay, and routing docs. Evidence: doc reference check pass; enforcement contract valid; agent surfaces in sync; 13 focused validator tests pass. Review: not required by the mechanical docs/policy risk tier.
- 2026-07-13: Quality: not-applicable; reason: Process-policy and documentation change with no player-facing, runtime, asset, or release output.
