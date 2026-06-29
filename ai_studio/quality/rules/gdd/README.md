# GDD Rules

Use this group when changed work creates, edits, or promotes a design source
package: GDD, design brief, concept document, tables, JSON/data contracts,
requirements, acceptance criteria, visual proof, web-GDD, or
implementation-facing handoff/spec.

## Not For

- player-facing clarity: use
  [Player Clarity](../player_clarity/README.md);
- art direction or asset readiness: use [Art](../art/README.md) or
  [Assets](../assets/README.md);
- runtime/build behavior: use [Technical](../technical/README.md);
- gameplay-loop quality, economy, or progression validation: use
  [Game Design](../game_design/README.md).

## Checks

### [QGDD_001 - Design Source Readiness](checks/QGDD_001_design_source_readiness.md)

Checks: design source package can act as current source of truth: entrypoint,
source order, file roles, scope, current decisions, open questions,
cross-file contradictions, and verifiable acceptance/proof.

Use when: GDD/concept/spec/table/data/handoff material is meant to guide
implementation, review, continuation, or an explicit lead decision.

Record applied checks in the task log using the outcome format from the Quality
README.
