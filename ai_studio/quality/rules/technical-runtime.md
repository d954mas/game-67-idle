# Technical Runtime Rule

Apply when code, build files, runtime automation, state, save/load, input,
packaging, or launch behavior changes.

## Evidence

Use the narrowest proof that exercises the changed behavior:

- module tests for changed module logic;
- build or launch for runtime changes;
- smoke/scenario for player-visible runtime behavior;
- screenshot/video when visual output matters;
- parser/schema validation for generated data.

## Rule

Technical checks prove the changed system works. They do not prove product
readability, game-loop quality, or visual quality by themselves.

Prefer named acceptance checks over generic pass/fail output.
