---
id: QTECH_001
name: Behavior Evidence
group: technical
description: Use when technical work changes behavior, contracts, wiring, generated output, or integration.
---

# QTECH_001 Behavior Evidence

## What It Checks

The technical claim and the proof match: the changed behavior, contract, wiring,
generated output, or integration path is named and then exercised by the
narrowest useful evidence.

## Use When

Use when code, scripts, build files, generated data, state, save/load, input,
packaging, automation, launch behavior, or runtime behavior changes what the
system does, accepts, emits, loads, saves, builds, starts, or wires together.

Use this for technical proof even when another quality group is also relevant.
For example, a player-facing UI change may need this rule for behavior evidence
and Player Clarity for visible quality.

## Do Not Use For

- player-facing clarity by itself;
- art direction or asset readiness;
- game-loop, reward, or progression design;
- GDD/design-source clarity;
- docs, comments, formatting, or file moves that do not change technical
  behavior, contracts, wiring, generated output, or integration.

## Check

- state the changed technical claim in one sentence;
- choose the narrowest proof that exercises that claim. Use unit/module tests
  for isolated logic, CLI/script commands for command behavior,
  parser/schema/golden samples for generated data/config/manifests, build/pack
  commands for packaging or resource wiring, launch/smoke/scenario/log proof
  for runtime integration, and save/load scenarios for persistence;
- confirm the proof result matches the expected behavior, not only exit code 0;
- if proof cannot run, report `unverified`, the reason, and the next concrete
  command or artifact needed;
- logs and command output do not contain ignored errors relevant to the changed
  path.

## Evidence

Changed claim, command/test name, key output, build/launch result,
smoke/scenario result, parser/schema result, generated-file parse result,
log excerpt, screenshot, video, or explicit unverified reason with next proof.

## Not Enough

- A generic green command that does not exercise the changed path.
- Build success used as proof of changed runtime behavior when the changed path
  did not run.
- Exit code 0 with no check that the expected output/state changed.
- Technical proof used as proof of player-facing clarity, art quality, asset
  provenance, or game-design quality.
- "Could not test" treated as pass instead of `unverified`.
- Logs or command output with relevant errors that are ignored.
