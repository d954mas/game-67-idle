# Test Goldens Install

## Build

Resolve the in-place module from the Studio root and link it into the test
targets that need it:

```cmake
set(TEST_GOLDENS_DIR "${GAME_REPO_ROOT}/features/test-goldens")
set(TEST_GOLDENS_INC "${TEST_GOLDENS_DIR}/include")
set(TEST_GOLDENS_SRC "${TEST_GOLDENS_DIR}/src")

game_add_c_test(test_planet_layout
    SOURCES tests/test_planet_layout.c "${TEST_GOLDENS_SRC}/test_goldens.c"
    INCLUDES "${TEST_GOLDENS_INC}")
```

The helper compiles into the test binary, never into the game.

## Enable

The runner decides the mode and the bank location. `tools/game.mjs test` passes
both to CTest:

- `GAME_GOLDENS_DIR` -- absolute path to `<game>/tests/goldens`.
- `GAME_UPDATE_GOLDENS=1` -- set only by `test --update-goldens`.

Create the bank directory once and keep it in git:

```powershell
mkdir games/<game-id>/tests/goldens
```

## Use

```c
#include "features/test_goldens/test_goldens.h"

const uint64_t digest = world_layout_digest(&world);
TEST_ASSERT_EQUAL_UINT64(test_golden_u64("planet_layout", "world_digest", digest), digest);
```

```js
import { golden } from "../../features/test-goldens/lib/test_goldens.mjs";

assert.equal(golden("web_budget", "wasm_bytes", bytes), bytes);
```

## Record

After an intended design change:

```powershell
node tools/game.mjs test --update-goldens --only test_planet_layout
git diff tests/goldens
```

Review the diff like any other change: a golden that moved without a reason in
the same commit is a regression that recorded itself.

## Verify

```powershell
ctest --test-dir games/<game-id>/build/native-debug -R test_test_goldens --output-on-failure
```

## Uninstall

Drop the sources from the test targets, delete `tests/goldens/`, and put the
values back into the tests or replace them with invariants.
