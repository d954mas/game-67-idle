# Limb IK

Status: opt-in, header-only L1 foundation feature.

## Purpose

`limb-ik` solves one planar two-link chain from a root, target, upper length,
lower length, and bend direction. Inputs and outputs use root-local Y-up
coordinates. A reachable target becomes the endpoint. A target outside the
annular reach is clamped to its nearest reachable endpoint. Invalid input
returns `false` and leaves caller-owned output unchanged.

The feature owns only the numerical solve. Consumers own rig data, animation,
coordinate conversion at platform boundaries, rendering, and input.

## Public surface

Include `features/limb_ik/limb_ik.h`.

- `limb_ik_point_t` contains planar `x` and `y` coordinates.
- `limb_ik_solution_t` contains `joint` and `end` points.
- `limb_ik_solve_2link(root, target, upper_length, lower_length, bend, out)`
  solves the chain. `bend` is `-1` or `1`.

The function accepts finite positions and positive finite lengths. It returns
`false` for invalid arguments or an unrepresentable result without writing to
`out`.

## Validation

`tests/test_limb_ik.c` covers reachable and unreachable targets, coincident
root and target, bend side, a finite target sweep, fixed bone lengths, and the
unchanged-output failure contract.

```powershell
cmake -S features/limb-ik -B build/limb-ik
cmake --build build/limb-ik --target test_limb_ik
ctest --test-dir build/limb-ik -C Debug -L core --output-on-failure
```

## Compatibility

Before `1.0.0`, this header is the public contract.

- PATCH fixes behavior or documentation while preserving types, signatures,
  coordinate convention, validation, and output-write semantics.
- MINOR adds backward-compatible helpers or types after a real consumer needs
  them.
- MAJOR may change type layout, function signatures, coordinate convention, or
  solve behavior.

## Extension points

Consumers may adapt their own vector and joint types at their game boundary,
map a UI or platform coordinate system to root-local Y-up, and select bend
from their rig state. Three-dimensional, multi-link, constrained, temporal,
and physics-aware solves belong in separate features or game-owned code.
