# Game State Install

This feature currently uses manual install. The default template already has an
installed copy, so new games created from `templates/template` inherit it.

Use this manual when adding `game-state` to an existing template or game, or
when checking that the installed copy is still wired correctly.

## Install

For a new game copied from `templates/template`, no extra copy step is needed.
The feature is installed and enabled by default through `FEATURE_GAME_STATE=ON`.

For an existing project, copy or recreate these installed pieces in the target
project:

```text
<project>/
  state/game_state.schema.json
  state/migrations/
  src/game_storage.c
  src/game_storage.h
```

Then add CMake wiring equivalent to `templates/template/CMakeLists.txt`:

- define `FEATURE_GAME_STATE`, defaulting on or off for the project;
- run `features/game-state/scripts/generate_state.py` with the project schema;
- write generated outputs to `<build>/generated/game-state`;
- compile generated `game_state.c`, `src/game_storage.c`, and migrations when
  `FEATURE_GAME_STATE` is on;
- compile generated `game_state_devapi.c` only when both `FEATURE_GAME_STATE`
  and `GAME_DEVAPI_ENABLED` are on;
- add the generated directory to the target include paths.

Wire runtime code in the app entry point. The DevAPI registration call must live
inside code that is compiled only when DevAPI is enabled:

```c
#if FEATURE_GAME_STATE
#include "game_state.h"
#endif

/* after core/runtime init */
#if FEATURE_GAME_STATE
game_state_init();
#endif

/* inside the DevAPI startup path, after nt_devapi_register_default() */
#if FEATURE_GAME_STATE
game_state_register_devapi();
#endif
```

## Enable / Disable

Enable runtime state:

```powershell
cmake -S <project> -B <project>/build/state-on -DFEATURE_GAME_STATE=ON
```

Disable runtime state:

```powershell
cmake -S <project> -B <project>/build/state-off -DFEATURE_GAME_STATE=OFF
```

Keep runtime state but remove DevAPI commands:

```powershell
cmake -S <project> -B <project>/build/release -DFEATURE_GAME_STATE=ON -DGAME_DEVAPI_ENABLED=OFF
```

## Generated Files

Do not hand-edit generated files. The generator writes:

```text
game_state.h
game_state.c
game_state_devapi.c
game_state_schema.gen.h
```

The default template generates them into:

```text
templates/template/build/<config>/generated/game-state/
```

For a game-local manual generation run:

```powershell
py -3.12 features/game-state/scripts/generate_state.py --schema games/<game-id>/state/game_state.schema.json
```

Pass `--out-dir` only when the project has an explicit generated-file policy.

## DevAPI

Generated DevAPI commands exist only under:

```text
FEATURE_GAME_STATE && GAME_DEVAPI_ENABLED
```

Commands:

```text
game.state.schema
game.state.get
game.state.set
game.state.patch
game.state.save
game.state.load
game.state.reset
```

Use these raw commands for debug/editor overrides, fixtures, and targeted state
tests. Gameplay bots should prefer semantic `game.action.*` commands when a game
adds them.

## Verify

Run generator tests:

```powershell
py -3.12 features/game-state/scripts/generate_state_test.py
```

Verify the template default build:

```powershell
cmake -S templates/template -B templates/template/build/feature-review-devapi -G Ninja -DFEATURE_GAME_STATE=ON -DGAME_DEVAPI_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build templates/template/build/feature-review-devapi --target game
```

Verify the feature can be disabled:

```powershell
cmake -S templates/template -B templates/template/build/feature-review-no-state -G Ninja -DFEATURE_GAME_STATE=OFF -DGAME_DEVAPI_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build templates/template/build/feature-review-no-state --target game
```

Verify release excludes generated DevAPI source:

```powershell
cmake -S templates/template -B templates/template/build/feature-review-release -G Ninja -DFEATURE_GAME_STATE=ON -DGAME_DEVAPI_ENABLED=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build templates/template/build/feature-review-release --target game
```

Run the template bot unit tests:

```powershell
py -3.12 templates/template/devapi/smoke_bot_test.py
```

## Uninstall

Soft uninstall is preferred for experiments:

```powershell
cmake -S <project> -B <project>/build/no-state -DFEATURE_GAME_STATE=OFF
```

Permanent uninstall requires removing the CMake feature block, generated include
path, runtime calls to `game_state_init()` and `game_state_register_devapi()`,
and the installed state/storage/migration files if no other feature uses them.
