---
id: T0370
title: Define per-domain numeric and requirement policies
status: done
project: P001
epic: E016
priority: P1
tags: [numeric, requirements, policy]
created: 2026-07-10
updated: 2026-07-14
quality: {"notApplicable":{"reason":"planning and routing cleanup only"}}
---

## What

Define only the still-open per-domain ranges and requirement-result governance
from real game examples. The accepted double normalization and rounded checked
`int64_t` foundation remains closed in `T0364`.

## Done when

- [ ] At least one representative idle/economy model exercises every proposed
      domain and shows the consequence of each error/warning boundary.
- [ ] The lead accepts ranges, severities, and waiver ownership/location;
      accepted rules move into owning contracts.

## Open questions

- What ranges/types/rounding apply to costs, rewards, signed deltas, counts,
  caps, probabilities, durations, rates, and analytics-only values?
- Which requirement failures are errors versus warnings, who may waive them,
  where is the reason recorded, and when does CI block?
- Which requirement presets, if any, are reusable without hiding game intent?

## Log

- 2026-07-10: Explicit structural/runtime failures are accepted. Domain warning
  thresholds and waiver governance remain design decisions.
- 2026-07-14: Closure: waived; reason: superseded during full Taskboard grooming without claiming new implementation; evidence: reusable severity evidence and waiver seam merged into T0384, game thresholds remain game-owned
- 2026-07-14: Quality: not-applicable; reason: planning and routing cleanup only
