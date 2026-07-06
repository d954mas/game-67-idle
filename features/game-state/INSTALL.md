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
  src/game_state_json.c
  src/game_state_json.h
  src/game_storage.c
  src/game_storage.h
  src/game_save.c
  src/game_save.h
  src/game_save_devapi.c
```

The generated `game_state.c` now defines the `game_state_fragment` descriptor
itself (the hand-written `game_fragment.c` adapter was removed in A4), so there
is no separate fragment source to copy. The DevAPI dispatch is the hand-written
`src/game_save_devapi.c` (A5): universal over the fragment registry, compiled
only under `GAME_DEVAPI_ENABLED`.

Then add CMake wiring equivalent to `templates/template/CMakeLists.txt`:

- define `FEATURE_GAME_STATE`, defaulting on or off for the project;
- run `features/game-state/scripts/generate_state.py` with the project schema;
- write generated outputs to `<build>/generated/game-state`;
- compile generated `game_state.c`, `src/game_storage.c`, and migrations when
  `FEATURE_GAME_STATE` is on;
- compile the hand-written `src/game_save_devapi.c` only when both
  `FEATURE_GAME_STATE` and `GAME_DEVAPI_ENABLED` are on;
- add the generated directory to the target include paths.

Wire runtime code in the app entry point. The DevAPI registration call must live
inside code that is compiled only when DevAPI is enabled:

```c
#if FEATURE_GAME_STATE
#include "game_state.h"
#endif

/* after core/runtime init */
#if FEATURE_GAME_STATE
game_save_register_fragment(&game_state_fragment); /* generated descriptor; `game` last */
game_save_init();                            /* after all fragments are registered */
if (!fresh_state) {
    game_save_load_result_t r;
    game_save_load(&r);                      /* orchestrates FRESH/LOADED/RECOVERED/CORRUPT/NEWER */
    if (r.status == GAME_SAVE_LOAD_CORRUPT_RESET) {
        char err[128];
        (void)game_save_new_game(err, (int)sizeof err); /* only on_new_game on this path */
    }
} else {
    game_state_fragment.reset();             /* --fresh-state skips load: seed defaults */
}
#ifdef NT_PLATFORM_WEB
game_save_install_web_flush();               /* synchronous visibility/pagehide flush */
#endif
#endif

/* per frame, after the game systems (and, with E1, after the record phase) */
#if FEATURE_GAME_STATE
if (!disable_autosave) {
    game_save_tick();                        /* debounced autosave */
}
#endif

/* inside the DevAPI startup path, after nt_devapi_register_default() */
#if FEATURE_GAME_STATE
game_save_register_devapi();  /* A5: registry dispatch (src/game_save_devapi.c) */
#endif
```

Compile `src/game_save.c` with the generated `game_state.c` (both under
`FEATURE_GAME_STATE`; the generated source now also defines the
`game_state_fragment` descriptor), and define the save knobs
(`GAME_SAVE_AUTOSAVE_SLOT`, `GAME_SAVE_DEBOUNCE_MS`, `GAME_SAVE_MAX_INTERVAL_MS`,
`GAME_SAVE_DOC_VERSION`) — see `templates/template/CMakeLists.txt`.

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
game_state_schema.gen.h
game_state_events.gen.h
game_state_events.gen.c
```

The DevAPI dispatch is no longer generated: it is the hand-written
`src/game_save_devapi.c` (A5), a universal registry dispatch shared by every
fragment.

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

Verify release excludes the DevAPI dispatch source (`src/game_save_devapi.c`):

```powershell
cmake -S templates/template -B templates/template/build/feature-review-release -G Ninja -DFEATURE_GAME_STATE=ON -DGAME_DEVAPI_ENABLED=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build templates/template/build/feature-review-release --target game
```

Run the template bot unit tests:

```powershell
py -3.12 templates/template/devapi/smoke_bot_test.py
```

Run the native `game_storage`/`game_state_json` Unity tests (A2):

```powershell
ctest --test-dir templates/template/build/native-debug --output-on-failure
```

Web persistence check (A2.4 item 2, CI-optional/advisory -- headless-localStorage
automation is capricious, so a failure here does not fail A2 acceptance):

```powershell
python templates/template/tests/web_persistence_check.py
```

Builds the template for wasm with `GAME_DEVAPI_ENABLED=ON`, serves it, drives a
headless Chrome instance over the DevTools Protocol to `game.state.set` a known
non-default value (`hero.gold=424242`) and `game.state.save` it to a probe key
via `window.__devapi.submit`, then FULLY QUITS Chrome (CDP `Browser.close`, not
a page reload -- reload barely exercises real persistence) and relaunches it
with the same `--user-data-dir`, and confirms `game.state.load` + `game.state.get`
return the same value -- i.e. the `GAME_STORAGE_APP_ID`-scoped localStorage key
actually survives a browser restart. Requires `EMSDK` (Emscripten toolchain) and
a local Chrome/Chromium; exits 2 (skip, not fail) if either is missing. Known
pre-existing blocker as of A5 (verified on HEAD, engine-side, unrelated to
`game-state`): a `GAME_DEVAPI_ENABLED=ON` wasm build compiles clean (every game
TU, including `main.c` and `src/game_save_devapi.c`, builds warning-clean under
`-Werror`) but fails to LINK the emscripten executable on engine-provided
symbols — the EM_JS `nt_devapi_web_install_shim` resolves undefined out of the
`nt_devapi_web` static archive, and (Debug wasm only) the ASan/UBSan-instrumented
engine libs pull `__asan_*`/`__ubsan_*` runtime symbols the executable link does
not provide. The engine is read-only; this script reports SKIP until the engine
web-devapi link is fixed separately. (The earlier `-Werror`
`devapi_shutdown_runtime` note was stale: `main.c` already carries a
`#ifndef NT_PLATFORM_WEB` web stub and compiles clean on wasm.)

## Uninstall

Soft uninstall is preferred for experiments:

```powershell
cmake -S <project> -B <project>/build/no-state -DFEATURE_GAME_STATE=OFF
```

Permanent uninstall requires removing the CMake feature block, generated include
path, runtime calls to `game_save_register_fragment(&game_state_fragment)` and
`game_save_register_devapi()`, the hand-written `src/game_save_devapi.c`, and the
installed state/storage files if no other feature uses them.
