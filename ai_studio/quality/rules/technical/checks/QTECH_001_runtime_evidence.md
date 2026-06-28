---
id: QTECH_001
name: Runtime Evidence
group: technical
description: Use when code, build files, runtime automation, state, save/load, input, packaging, or launch behavior changed and the changed behavior needs narrow proof.
---

# QTECH_001 Runtime Evidence

## What It Checks

Checks whether the changed runtime/system behavior has the narrowest proof that
actually exercises it.

## Use When

Code, build files, runtime automation, state, save/load, input, packaging, or
launch behavior changed.

## Do Not Use For

- player-facing clarity by itself;
- art direction or asset readiness;
- game-loop, reward, or progression design;
- GDD/document clarity.

## Evidence

Use the narrowest proof that exercises the changed behavior:

- module tests for changed module logic;
- build or launch for runtime changes;
- smoke/scenario for player-visible runtime behavior;
- screenshot/video when visual output matters;
- parser/schema validation for generated data.

## Rule

Technical checks prove the changed system works. They do not prove screen
readability, game-loop quality, or visual quality by themselves.

Prefer named acceptance checks over generic pass/fail output.

## Not Enough

- A generic green command that does not exercise the changed behavior.
- Build success used as proof of player-facing clarity or art quality.
- Logs with visible errors that are ignored.
