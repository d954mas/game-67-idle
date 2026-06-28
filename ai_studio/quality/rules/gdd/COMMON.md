---
id: QGDD_COMMON
name: GDD Common
group: gdd
description: Use first when a GDD, concept document, requirements, or implementation-facing game spec changed and you need a cheap pass for clarity, contradictions, unresolved decisions, and acceptance criteria.
---

# GDD Common

Use this first when changed work affects a GDD, concept document, requirements,
or implementation-facing game spec.

## What It Checks

Catches obvious document blockers before deeper GDD review.

## Use When

A GDD, concept document, requirements page, acceptance criteria, or
implementation-facing game spec changed.

## Do Not Use For

- player-facing screen clarity;
- art direction or asset readiness;
- runtime/build behavior;
- detailed economy, balance, or progression validation.

## Check

- the document says what is being built;
- the target player or use case is clear enough for decisions;
- core loop, controls, and success/failure are not contradictory;
- implementation notes do not hide unresolved design decisions;
- acceptance criteria are concrete enough to verify.

If any item fails, improve the document before deeper GDD review.

## Evidence

Use the doc path, changed section, acceptance criteria, or a short summary of
the contradiction/open gap.

## Not Enough

- A broad concept pitch with no buildable target.
- Implementation notes that hide unresolved design decisions.
- Acceptance criteria that cannot be verified.

## Record As

```text
Quality: QGDD_COMMON=pass; evidence: <doc path or section>
```
