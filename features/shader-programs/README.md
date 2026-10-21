# Shader Programs

## Purpose

One linked GPU program per (vertex, fragment) shader-resource pair, shared by
every material created through the cache. The engine's `nt_program_ref` links
one pair and `nt_material` only borrows a program; relinking after a context
loss is the game's job (`external/neotolis-engine/docs/spec/render/shader.md`).
This pack is that game-side bookkeeping, written once.

- A pair links once both stage resources are ready. A material created before
  that holds no program and is re-pointed when the link starts.
- After a context loss, `shader_programs_restore_gpu` drops every program and
  the next `shader_programs_update` links each pair again and re-points its
  materials.
- Materials stay caller-owned. A destroyed material frees its row, so a game
  that rebuilds materials does not grow the table.
- A failed `nt_material_create` asserts with the pool's name instead of leaving
  an invisible material.

## Public surface

`include/features/shader_programs/shader_programs.h`.

## Layer

L0: C over public engine APIs (`nt_gfx`, `nt_material`, `nt_resource`,
`material/nt_program_ref.h`). No save or platform dependency.

## Adopters

The live games build it from this checkout. Frozen games keep their own copies.

## Validation

`test_shader_programs` (CTest label `core`) in the feature consumer, with
link-time fakes of the engine calls, and `node features/validate_contracts.mjs`.

## Compatibility

`feature.json.version` is exact SemVer. PATCH changes internals; MINOR adds API;
MAJOR renames or removes a function or changes the call order.

## Extension points

A game that draws with a program it links itself keeps that program out of the
cache. The table sizes (`SHADER_PAIR_MAX`, `SHADER_MATERIAL_MAX`) assert by
name when a game outgrows them.
