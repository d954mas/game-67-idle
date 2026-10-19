---
id: QTECH_002
name: Shader Position Compatibility
group: technical
description: Keep vertex position calculations local and write the built-in output once.
---

# QTECH_002 Shader Position Compatibility

## What It Checks

Authored GLSL uses `gl_Position` only as a final output: one whole-vector
assignment at the top level of `main` in the vertex entry file. Calculate and
adjust the position in a local highp vector. Do not read `gl_Position`, update
its components, pass it to helpers, or hide its writes behind macros.

```glsl
highp vec4 position = view_proj * world * vec4(a_position, 1.0);
position.z = max(position.z, -position.w);
gl_Position = position;
```

Helpers return values or operate on local variables; shared `.glsl` includes
must not access `gl_Position`. Preserve precision, transform order and depth
adjustments when changing an existing shader.

## Use When

Adding, changing, importing or reviewing vertex shaders or shared GLSL includes,
especially shadow depth clamping and sky depth adjustments.

## Do Not Use For

Proving general GLSL correctness, fragment color behavior, driver stability,
performance, or that a specific device crash has been fixed.

## Check

From the Studio root, check the game's shader source directory:

```powershell
node ai_studio/quality/check_shader_positions.mjs games/private/<game-id>/assets/shaders
```

The same script accepts individual `.vert`, `.vs`, `.vsh` and `.glsl` files,
multiple directories, and `--json`. Include every directory that supplies
shared shader sources. Exit codes are `0` for no violations, `1` for rule
violations, and `2` for invalid or unavailable input. Diagnostics name the file,
line and rule. An empty input tree is not a passing check.

Studio checks its template, shared features and reference consumer with:

```powershell
node ai_studio/quality/check_shader_positions.mjs templates features ai_studio/feature_testing/consumer
```

The checker joins backslash-continued lines, then ignores comments and string
literals. It checks authored syntax,
including inactive preprocessor branches; it does not expand macros/includes
or analyze compiler output. It does not require a shader to contain an output
assignment: the GLSL compiler still owns completeness and language validity.
Directory scans skip `.git`, `node_modules`, `build`, `release`, `tmp` and
symlinks. Generated output can be checked by naming its real file explicitly.
Embedded GLSL strings in C/JS require extraction before checking.

## Evidence

Checker output for all affected source directories, shader compilation/linking,
and before/after output comparison when replacing existing calculations.
The shared-template/feature check runs in full Studio CI and harness-domain
verification. Template/feature-only owner checks do not run it. Private games
must be named explicitly and are never silently traversed by Studio CI.

## Not Enough

A passing source check does not guarantee that a translator or optimizer will
preserve the source form, or that a retail GPU driver will succeed. Do not turn
this compatibility rule into a claim that all PowerVR link failures have the
same cause.

## Source

[Filament issue #5118](https://github.com/google/filament/issues/5118) records a
PowerVR compiler crash after assigning `gl_Position` and then modifying its
`z` component. Its maintainer's temporary-variable workaround was merged in
[PR #7051](https://github.com/google/filament/pull/7051). The report names
GE8320/GM9446; it does not prove the same cause for PLW's BXM-8-256 failure.
This is a conservative Studio portability rule, not a GLSL language restriction.
