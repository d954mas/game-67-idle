# Box3D physics

## Purpose

Optional in-place L1 module for rigid-body physics in native and WASM games.
One pinned MIT Box3D copy is shared by consumers. The template does not link it
unless a game opts in. Worlds, bodies, collision rules, timing, level content,
rendering and balance belong to each game. There is no singleton wrapper.

## Public surface

Include `cmake/Box3D.cmake`, then call `box3d_enable(target)` for each consumer.
The helper links `nt::box3d`, propagates system includes and `FEATURE_BOX3D=1`.
Keep `#include "box3d/box3d.h"` and use the upstream `b3*` API directly.
A second include/consumer reuses the same library target.
No engine internals, assets, saves, UI, DevAPI or lifecycle hooks are installed.

## Validation

`node --test features/box3d/tests/integration.test.mjs` configures two consumers,
checks parent CMake version isolation, then runs collision/stepping/world-isolation
smoke in CTest (core). The same test accepts `CC=clang-cl` after loading the
MSVC developer environment; it does not require a C++ language or compiler. It requires CMake, Ninja and a C17 compiler (`CC` override).
`UPSTREAM.json` records exact selected source bytes and the original revision;
`THIRD_PARTY_NOTICES.txt` carries all four MIT notices for redistribution.
A private consumer game provides the native 30-level and real-browser proof.

## Compatibility

Feature version 0.1.0 wraps Box3D 0.1.0 at the revision in `UPSTREAM.json`.
The supported profile is static C17, float coordinates, SIMD disabled,
profiling and optional heavy validation disabled. No network fetch occurs. Native targets propagate `Threads::Threads`;
WASM stays on the existing non-pthread profile. C-only MSVC hosts use their C
compiler identity for upstream's legacy CXX-based warning selection.
PATCH fixes wiring/docs without changing public API or solver behavior.
MINOR adds compatible supported capability. MAJOR changes public integration,
ABI, or established physics behavior. Every upstream update requires a reviewed
revision/hash update and consumer physics evidence, regardless of version label.
No cross-platform bitwise determinism is promised.

## Extension points

Configure worlds and shapes with upstream structs in the game. Keep gameplay
rules and rendering there. Add a shared adapter only when a concrete consumer
needs invariant integration code; do not mirror the entire upstream API.
Installation and removal are documented in [INSTALL.md](INSTALL.md).
