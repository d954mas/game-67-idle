# Software Cursor Install

## Build

Resolve the in-place module from the Studio root:

```cmake
set(SOFTWARE_CURSOR_DIR "${GAME_REPO_ROOT}/features/software-cursor")
set(SOFTWARE_CURSOR_INC "${SOFTWARE_CURSOR_DIR}/include")
set(SOFTWARE_CURSOR_SRC "${SOFTWARE_CURSOR_DIR}/src")

target_sources(game PRIVATE
  "${SOFTWARE_CURSOR_SRC}/software_cursor.c"
  "${SOFTWARE_CURSOR_SRC}/software_cursor_samples.c")
target_include_directories(game PRIVATE "${SOFTWARE_CURSOR_INC}")
target_compile_definitions(game PRIVATE FEATURE_SOFTWARE_CURSOR=1)
```

## Compose

Store `software_cursor_t` in the game's feature aggregate. Initialize it with a
sample or game-owned theme. Map the game's primary pointer to
`software_cursor_input_t`, call `software_cursor_update` once per frame, then
draw `software_cursor_presentation()` after every other UI layer.

The renderer maps the opaque `visual` id to a packed region. The sample ids are
declared in `software_cursor_samples.h`; a custom theme may use any non-zero ids.

When the engine exposes `nt_window_set_cursor_mode`, pass a callback that maps
`hidden=true` to `NT_CURSOR_HIDDEN` and `false` to `NT_CURSOR_NATIVE`.

## Sample art

Copy only the theme the game uses from `example/assets/` into its asset tree,
record the existing provenance in the game's asset manifest, and add named atlas
regions in the game-owned pack builder. The feature never edits a game pack.

## Verify

Build and run the focused consumer target:

```powershell
cmake --build games/<game-id>/build/<preset> --target test_software_cursor
ctest --test-dir games/<game-id>/build/<preset> -R test_software_cursor --output-on-failure
```

## Uninstall

Restore the native cursor, remove lifecycle/update/draw calls, remove the two
module sources and include path, then delete any copied sample art and atlas
regions that have no other consumer.

