---
id: QTECH_002
name: Shader Writing Rules
group: technical
description: Shader prohibitions, safe patterns and checks for agents.
---

# QTECH_002 Shader Writing Rules

## Use When

Writing, changing, importing or reviewing GLSL.

## What It Checks

- Calculate position in a local `highp vec4`. Write `gl_Position` once, as a
  whole vector, at the top level of vertex `main`.
- Never read `gl_Position`, change its components, pass it to helpers, or put
  it in macros, branches, loops or shared `.glsl` files. This avoids a known
  PowerVR compiler failure.
- Preserve precision, transform order and depth adjustments when refactoring.

```glsl
highp vec4 position = view_proj * world * vec4(a_position, 1.0);
position.z = max(position.z, -position.w);
gl_Position = position;
```

## Check

Run from Studio root, including all shared shader directories:

```powershell
node ai_studio/quality/check_shaders.mjs games/private/<game-id>/assets/shaders
```

Compile/link changed programs and compare output after changing calculations.
See [checker details](../../../check_shaders.md) only for CLI scope or limits.

## Evidence

Checker result, compile/link result, and output comparison when applicable.

## Do Not Use For

Certifying driver stability, performance or general GLSL correctness.

## Not Enough

A passing source check does not prove that a device crash is fixed.

## Source

PowerVR GE8320/GM9446: [Filament #5118](https://github.com/google/filament/issues/5118),
[fix #7051](https://github.com/google/filament/pull/7051). The same cause on
BXM-8-256 remains unconfirmed.
