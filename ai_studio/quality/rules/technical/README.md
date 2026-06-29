# Technical Rules

Use this group when changed work affects code, scripts, build files, generated
data, runtime behavior, build/run proof, input, state, save/load, packaging, or
automation.

## Not For

- player-facing clarity by itself: use
  [Player Clarity](../player_clarity/README.md);
- art direction or asset readiness: use [Art](../art/README.md) or
  [Assets](../assets/README.md);
- game-loop, reward, or progression design: use
  [Game Design](../game_design/README.md);
- GDD/design-source clarity: use [GDD](../gdd/README.md).

## Checks

### [QTECH_001 - Behavior Evidence](checks/QTECH_001_behavior_evidence.md)

Checks: changed technical claim is named and exercised by the narrowest useful
proof: test, command, parser/schema check, build/pack command, launch/smoke,
scenario, log, or generated-output check.

Use when: code, scripts, build files, generated data, state, save/load, input,
packaging, automation, launch behavior, or runtime behavior changed.

Record applied checks in the task log using the outcome format from the Quality
README.
