# Localization Install

In-place module (precedent `features/game-state`, `features/items-core`,
`features/progression-core`) — no copy step. A consuming template/game
references this module's source and scripts from its own `CMakeLists.txt` via
`${GAME_REPO_ROOT}/features/localization`, which works for templates, public
games, and `games/private/<id>`.

## Dependencies

The engine only: `nt_mem_scratch` (formatted strings), `nt_log` (the fallback
warning), and `nt_core` (`NT_ASSERT` — the contracts in `loc.h` are asserts). No feature edges — in particular, digit grouping comes from
the language block in the data, never from a game-side number formatter.

## CMake wiring

Define the module path variables once (near `ENGINE_DIR` / the other
`features/*` blocks, before the game's `add_executable`):

```cmake
set(LOCALIZATION_DIR     "${GAME_REPO_ROOT}/features/localization")
set(LOCALIZATION_INC     "${LOCALIZATION_DIR}/include")
set(LOCALIZATION_SRC     "${LOCALIZATION_DIR}/src")
set(LOCALIZATION_SCRIPTS "${LOCALIZATION_DIR}/scripts")
```

Codegen (writes into the game's OWN generated dir, not the module):

```cmake
set(LOC_STRINGS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/content/loc/strings.json")
set(LOC_GENERATOR "${LOCALIZATION_SCRIPTS}/loc.py")
add_custom_command(
    OUTPUT
        "${GAME_SOURCE_GENERATED_DIR}/loc_strings.gen.h"
        "${GAME_SOURCE_GENERATED_DIR}/loc_strings.gen.c"
        "${GAME_SOURCE_GENERATED_DIR}/loc_keys.gen.json"
        "${GAME_SOURCE_GENERATED_DIR}/loc_charset.gen.h"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_SOURCE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${LOC_GENERATOR}" generate
        --strings "${LOC_STRINGS_JSON}"
        --out-dir "${GAME_SOURCE_GENERATED_DIR}"
        --source-label "content/loc/strings.json"
    DEPENDS "${LOC_STRINGS_JSON}" "${LOC_GENERATOR}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Generating localization string table"
    VERBATIM)
```

Sources and include path:

```cmake
target_sources(${GAME_TARGET} PRIVATE
    "${LOCALIZATION_SRC}/loc.c"
    "${GAME_SOURCE_GENERATED_DIR}/loc_strings.gen.c")
target_include_directories(${GAME_TARGET} PRIVATE "${LOCALIZATION_INC}")
```

`${GAME_SOURCE_GENERATED_DIR}` must already be on the include path (it is, for
the other generators). Link `nt_mem_scratch`, `nt_log`, and `nt_core` — most games already do.

## The font charset follows the corpus

`loc_charset.gen.h` is the fourth output and it is not optional: it carries
`LOC_CHARSET_NON_ASCII`, every non-ASCII codepoint the corpus can render. The
pack builder prepends `NT_CHARSET_ASCII` — digits and debug text never pass
through the corpus — and packs the union:

```c
#include "generated/loc_charset.gen.h"

nt_builder_add_font(ctx, font_path,
                    &(nt_font_opts_t){.charset = NT_CHARSET_ASCII LOC_CHARSET_NON_ASCII,
                                      .resource_name = "game/font"});
```

Two CMake edges make that hold. The header is a SOURCE of the pack-builder
executable, which orders the codegen ahead of compiling it; and it is a DEPENDS
of the pack command, so editing a string repacks the atlas. Without the second
edge a new character regenerates the header, the atlas is NOT repacked, and the
glyph ships as a blank box with every gate green.

```cmake
add_executable(build_game_packs src/build_packs.c
    "${GAME_SOURCE_GENERATED_DIR}/loc_charset.gen.h")
target_include_directories(build_game_packs PRIVATE src)
```

The packed font must actually carry the glyphs: `loc.py fonts` reports per-font
coverage and names the font and the codepoint it lacks.

## Startup

Bind the table once, before any accessor runs and after
`nt_mem_scratch_init()`:

```c
#include "loc_strings.gen.h"

nt_mem_scratch_init(...);
loc_init();
```

`loc_init()` leaves the active language at the table's `fallback`. A game that
persists a language setting calls `loc_set_lang()` once the save has loaded.

## The frame loop — load-bearing

`loc_format()` returns memory from the engine's per-frame arena, so the reset
MUST be the first thing the frame does:

```c
static void frame(void) {
    nt_mem_scratch_reset();   // BEFORE anything allocates
    ...
}
```

`nt_mem_scratch.h` states this contract. A reset that happens later in the same
frame frees pointers already handed to `nt_ui` (which copies the text it is
given, but only at the moment of the call).

Arena exhaustion is `NT_ASSERT`, which traps in release — size with headroom
and watch `nt_mem_scratch_high_water_mark()`. A worst frame of ~120 formatted
strings at ≤200 B is ~24 KB, against a typical 512 KB budget.

## Text widgets

Wrap the game's text-bearing widgets in ONE game-side file (`src/ui/loc_widgets.c`)
that takes `LocStr` and forwards to the raw widget. Every UI surface calls the
wrappers. That is what turns the localization gate into a grep on a function
name instead of a heuristic over string contents.

## Tests

```cmake
add_executable(test_loc tests/test_loc.c "${LOCALIZATION_SRC}/loc.c")
target_link_libraries(test_loc PRIVATE unity nt_mem_scratch nt_log nt_core)
target_include_directories(test_loc PRIVATE "${LOCALIZATION_INC}" "${ENGINE_DIR}/engine")
add_test(NAME test_loc COMMAND test_loc)

add_test(NAME loc_generator_test
    COMMAND "${Python3_EXECUTABLE}" "${LOCALIZATION_SCRIPTS}/loc_test.py")
```

The runtime test hand-writes its own fixture table, so it proves `loc.c`
independently of whatever the game corpus currently holds.

## Port tools that need key names

Do not re-implement the key → identifier mangling. Regenerate the index into a
temp dir and read it:

```
loc.py generate --index-only --strings content/loc/strings.json --out-dir <tmp>
```

`loc_keys.gen.json` carries, per key: the accessor name, the `LocKey`
enumerator, the `LocKey0` enumerator (or `null` when the key takes arguments),
the declared argument list, and which languages carry text.
