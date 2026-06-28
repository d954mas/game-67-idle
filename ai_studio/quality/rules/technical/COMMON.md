---
id: QTECH_COMMON
name: Technical Common
group: technical
description: Use first when code, scripts, runtime behavior, build, launch, state, save/load, input, packaging, or automation changed and you need a cheap pass for obvious validation blockers.
---

# Technical Common

Use this first when changed work affects code, scripts, runtime behavior, build,
launch, state, save/load, input, packaging, or automation.

## What It Checks

Catches obvious technical proof blockers before spending time on numbered
technical checks.

## Use When

Code, scripts, runtime behavior, build, launch, state, save/load, input,
packaging, or automation changed.

## Do Not Use For

- player-facing clarity by itself;
- art direction or asset readiness;
- game-loop, reward, or progression design;
- GDD/document clarity.

## Check

- changed code has a narrow validation command;
- the command exercises the changed behavior;
- failing logs are not ignored;
- generated files or data still parse/load;
- player-facing quality is not claimed from technical green output alone.

If any item fails, fix it before using numbered technical checks.

## Evidence

Use command output, test result, build/launch result, parser/schema output,
runtime log, smoke/scenario result, or screenshot/video when output matters.

## Not Enough

- A generic green command that does not exercise the changed behavior.
- Ignored failing logs.
- Claiming player-facing quality from technical success alone.

## Record As

```text
Quality: QTECH_COMMON=pass; evidence: <command and result>
```
