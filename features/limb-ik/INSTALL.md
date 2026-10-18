# Limb IK Install

On Unix consumers, link the standard math library when it is a separate
library on the target toolchain:

```cmake
if(UNIX)
    target_link_libraries(game PRIVATE m)
endif()
```

The Windows CRT provides the math functions without a separate `m` library.

`limb-ik` is an opt-in in-place header-only feature.

## Build

Resolve the module from the Studio root and add its public include directory to
every target that includes the header:

```cmake
set(LIMB_IK_DIR "${GAME_REPO_ROOT}/features/limb-ik")
set(LIMB_IK_INC "${LIMB_IK_DIR}/include")

target_include_directories(game PRIVATE "${LIMB_IK_INC}")
target_compile_definitions(game PRIVATE FEATURE_LIMB_IK=1)
```

The feature has no source file and no lifecycle hook.

## Use

```c
#include "features/limb_ik/limb_ik.h"

limb_ik_solution_t solution;
if (limb_ik_solve_2link(root, target, upper, lower, bend, &solution)) {
    use_solution(solution);
}
```

Pass root-local Y-up points. Convert screen or platform coordinates before the
call. Keep the previous pose when the call returns `false`.

## Verify

```powershell
cmake -S features/limb-ik -B build/limb-ik
cmake --build build/limb-ik --target test_limb_ik
ctest --test-dir build/limb-ik -C Debug -L core --output-on-failure
```

Run the consuming game's focused IK tests after wiring the include directory.

## Uninstall

Remove the feature include directory and `FEATURE_LIMB_IK` definition from
consumer targets, then replace or remove consumer adapters and calls. The
feature owns no state, assets, or generated output.
