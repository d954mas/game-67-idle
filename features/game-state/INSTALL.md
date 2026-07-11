# Game State Install

This feature currently uses manual install. The default template already has an
installed copy, so new games created from `templates/template` inherit it.

Use this manual when adding `game-state` to an existing template or game, or
when checking that the installed copy is still wired correctly.

## Install

For a new game copied from `templates/template`, no extra copy step is needed.
The feature is installed and always on (no build flag: the `FEATURE_GAME_STATE`
axis was removed 2026-07-07 — a game without state is impossible).

For an existing project, copy or recreate these installed pieces in the target
project:

```text
<project>/
  state/game_state.schema.json
  state/settings.schema.json
  state/migrations/
  src/game_state_json.c
  src/game_state_json.h
  src/game_storage.c
  src/game_storage.h
  src/game_save.c
  src/game_save.h
  src/game_save_devapi.c
  src/features/settings/settings.c
  src/features/settings/settings.h
```

The generated `game_state.c` now defines the `game_state_fragment` descriptor
itself (the hand-written `game_fragment.c` adapter was removed in A4), so there
is no separate fragment source to copy. The DevAPI dispatch is the hand-written
`src/game_save_devapi.c` (A5): universal over the fragment registry, compiled
only under `GAME_DEVAPI_ENABLED`.

`settings` (A6) is the second live fragment and the Р9 sample: its state layer
(`SettingsState`, defaults, (de)serialization, schema, descriptor) is GENERATED
from `state/settings.schema.json`; only the feature LOGIC —
`src/features/settings/settings.{c,h}` (getters/setters + clamp + `game_save_mark_dirty`)
— is hand-written and copied. The registry, shell, dispatch, and generator are
universal over `GameSaveFragment`, so a second fragment needs no edit to any of
them. The settings UI (gear + panel) lives inside the feature as
`src/features/settings/settings_screen.c`; it is template shell/UI wiring, not a
game-state deliverable, and is NOT part of this copy-list.

Then add CMake wiring equivalent to `templates/template/CMakeLists.txt`:

- run `features/game-state/scripts/generate_state.py` with the project schema
  (`--fragment game`);
- run the generator a SECOND time with the settings schema
  (`--fragment settings`) into the SAME generated dir — the two custom commands
  write different filenames, so a parallel build is fine;
- write generated outputs to `<build>/generated/game-state`;
- compile generated `game_state.c` and `settings_state.c`, the hand-written
  `src/features/settings/settings.c`, `src/game_storage.c`, and migrations
  unconditionally (`settings_state_events.gen.c` is an empty-events
  stub and is NOT linked; do not call `settings_ev_register`);
- compile the hand-written `src/game_save_devapi.c` only when
  `GAME_DEVAPI_ENABLED` is on;
- add the generated directory to the target include paths.

Wire runtime code in the app entry point. The DevAPI registration call must live
inside code that is compiled only when DevAPI is enabled:

```c
#include "game_state.h"
#include "settings_state.h" /* A6: second fragment; NOT settings_state_events.gen.h */

/* after core/runtime init -- game-state provisioning is always on (unconditional) */
game_save_register_fragment(&settings_state_fragment); /* settings before game */
game_save_register_fragment(&game_state_fragment);     /* generated descriptor; `game` last */
game_save_init();                            /* after all fragments are registered */
if (!fresh_state) {
    game_save_load_result_t r;
    game_save_load(&r);                      /* orchestrates FRESH/LOADED/RECOVERED/CORRUPT/NEWER */
    if (r.status == GAME_SAVE_LOAD_CORRUPT_RESET) {
        char err[128];
        (void)game_save_new_game(err, (int)sizeof err); /* only on_new_game on this path */
    }
} else {
    settings_state_fragment.reset();         /* --fresh-state skips load: seed both fragments */
    game_state_fragment.reset();
}
#ifdef NT_PLATFORM_WEB
game_save_install_web_flush();               /* synchronous visibility/pagehide flush */
#endif

/* per frame, after the game systems (and, with E1, after the record phase) */
if (!disable_autosave) {
    game_save_tick();                        /* debounced autosave */
}

/* inside the DevAPI startup path, after nt_devapi_register_default() */
game_save_register_devapi();  /* A5: registry dispatch (src/game_save_devapi.c) */
```

Compile `src/game_save.c` with the generated `game_state.c` (both
unconditional; the generated source now also defines the
`game_state_fragment` descriptor), and define the save knobs
(`GAME_SAVE_AUTOSAVE_SLOT`, `GAME_SAVE_DEBOUNCE_MS`, `GAME_SAVE_MAX_INTERVAL_MS`,
`GAME_SAVE_DOC_VERSION`) — see `templates/template/CMakeLists.txt`.

## Enable / Disable

In the template, runtime state is ALWAYS enabled — there is no on/off flag (the
`FEATURE_GAME_STATE` CMake option was removed 2026-07-07, lead: "a game without
state is impossible"). The state provisioning (custom_commands, generated
sources, `cjson` link) is unconditional CMake wiring; the only remaining knob is
DevAPI:

```powershell
cmake -S <project> -B <project>/build/release -DGAME_DEVAPI_ENABLED=OFF
```

## Generated Files

Do not hand-edit generated files. Per fragment, the generator writes
`<id>_state.h`, `<id>_state.c`, `<id>_state_schema.gen.h`,
`<id>_state_events.gen.h`, and `<id>_state_events.gen.c`. The default template
generates TWO fragments — `game` and `settings` — into the same generated dir:

```text
game_state.h              settings_state.h
game_state.c              settings_state.c
game_state_schema.gen.h   settings_state_schema.gen.h
game_state_events.gen.h   settings_state_events.gen.h
game_state_events.gen.c   settings_state_events.gen.c
```

A fragment with an empty `events` section still emits the events pair as a stub
(`<id>_ev_desc_count = 0`, empty `<id>_ev_register`); the stub source is NOT
linked into the target and its register function is never called.

The DevAPI dispatch is no longer generated: it is the hand-written
`src/game_save_devapi.c` (A5), a universal registry dispatch shared by every
fragment.

The default template generates them into:

```text
templates/template/build/<config>/generated/game-state/
```

For a game-local manual generation run:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state.py --schema games/<game-id>/state/game_state.schema.json
```

Pass `--out-dir` only when the project has an explicit generated-file policy.

## DevAPI

Generated DevAPI commands exist only under:

```text
GAME_DEVAPI_ENABLED
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
node ai_studio/dev_environment/python_run.mjs features/game-state/scripts/generate_state_test.py
```

Verify the template default build:

```powershell
cmake -S templates/template -B templates/template/build/feature-review-devapi -G Ninja -DGAME_DEVAPI_ENABLED=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build templates/template/build/feature-review-devapi --target game
```

Verify release excludes the DevAPI dispatch source (`src/game_save_devapi.c`):

```powershell
cmake -S templates/template -B templates/template/build/feature-review-release -G Ninja -DGAME_DEVAPI_ENABLED=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build templates/template/build/feature-review-release --target game
```

Run the template bot unit tests:

```powershell
node ai_studio/dev_environment/python_run.mjs templates/template/devapi/smoke_bot_test.py
```

With the template's two fragments registered (`settings` before `game`),
`game.state.get {path:""}` returns the multi-fragment aggregate
`{ "settings": {...}, "game": {...} }` (registration order) and
`game.state.schema` returns both fragment schemas. Retained orphan keys
(unknown feature blobs round-tripped per §14 п.16) appear under a separate
`"orphans"` section in `get {path:""}`, omitted entirely when there are none. A cross-fragment
`game.state.patch` applies each fragment atomically per key; a failure in one
fragment (e.g. `settings.master_volume:5.0` out of range) rolls that fragment
back without touching the other.

Run the native `game_storage`/`game_state_json` Unity tests (A2):

```powershell
ctest --test-dir templates/template/build/native-debug --output-on-failure
```

These now also assert that a read FAILURE of an existing save (not its absence)
is quarantined and reported `CORRUPT_RESET` rather than silently reborn as
`FRESH`, so a transient/hardware read error can never overwrite a live save with
defaults (absent → `FRESH` is unchanged; lead 2026-07-07).

Web persistence check (A2.4 item 2, CI-optional/advisory -- headless-localStorage
automation is capricious, so a failure here does not fail A2 acceptance):

```powershell
node ai_studio/dev_environment/python_run.mjs templates/template/tests/web_persistence_check.py
```

Builds the template for wasm with `GAME_DEVAPI_ENABLED=ON`, serves it, drives a
headless Chrome instance over the DevTools Protocol to `game.state.set` a known
non-default value (`game.hero.gold=424242`; the path's first segment is the
save-fragment id) and `game.state.save` it (the fixed autosave slot; the DevAPI
save/load handlers have no per-slot key)
via `window.__devapi.submit`, then FULLY QUITS Chrome (CDP `Browser.close`, not
a page reload -- reload barely exercises real persistence) and relaunches it
with the same `--user-data-dir`, and confirms `game.state.load` + `game.state.get`
return the same value -- i.e. the `GAME_STORAGE_APP_ID`-scoped localStorage key
actually survives a browser restart. Requires `EMSDK` (Emscripten toolchain) and
a local Chrome/Chromium; exits 2 (skip, not fail) if either is missing.
History: the wasm-devapi LINK was red until 2026-07-07. Investigation proved
both roots lived in the template's consumption, not the engine (the engine's
own devapi_host example is green): (1) the engine instruments Debug modules
with ASan/UBSan on non-Windows targets and expects the consumer executable to
carry the same flags -- fixed by `nt_set_sanitizer_flags(game)`; (2) the
web-devapi host contract (`nt_devapi_web.h`) requires exporting
`_nt_devapi_web_submit`/`_nt_devapi_web_poll` (which also pulls the EM_JS
object out of the archive so `nt_devapi_web_install_shim` resolves) plus
`-sEXPORTED_RUNTIME_METHODS=ccall` for the JS shim at runtime -- both now in
the template's EMSCRIPTEN link block, gated on `GAME_DEVAPI_ENABLED`. The
Debug wasm executable links and boots under ASan. T0333 then delivered the
template web packaging path (relative pack over HTTP, tracked `index.html`
shell, `tools/build_web.sh` + `tools/serve_web.mjs`, preset `wasm-devapi-debug`):
`node ai_studio/dev_environment/python_run.mjs templates/template/tests/web_devapi_check.py` now proves the shim
round-trip in a headless browser (`endpoints` + `command.describe` over
`window.__devapi.submit`) with one command. A full web BOT driver (browser
site+agent parity) is still future.

## Uninstall

There is no soft (CMake-flag) uninstall — game-state provisioning is always on
(the `FEATURE_GAME_STATE` on/off axis was removed 2026-07-07). Uninstalling the
feature is permanent-only: remove the CMake state-provisioning block, the
generated include path, runtime calls to
`game_save_register_fragment(&game_state_fragment)` and
`game_save_register_devapi()`, the hand-written `src/game_save_devapi.c`, and the
installed state/storage files if no other feature uses them.
