# platform-sdk Install

This feature is installed in-place by the template. It is not copied into
`templates/template/src/features`; CMake references `../../features/platform-sdk`
the same way it references `items-core` and `progression-core`.

## Install

A consuming template or game should wire this as an L1 feature:

1. Add module paths near the other in-place modules:

   ```cmake
   set(PLATFORM_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../features/platform-sdk")
   set(PLATFORM_SDK_INC "${PLATFORM_SDK_DIR}/include")
   set(PLATFORM_SDK_SRC "${PLATFORM_SDK_DIR}/src")
   set(PLATFORM_SDK_WEB "${PLATFORM_SDK_DIR}/web")
   ```

2. Compile `features/platform-sdk/src/platform_sdk.c` and include
   `features/platform-sdk/include`. Web/Emscripten builds should also compile
   `features/platform-sdk/src/platform_sdk_web.c`; it installs the selected
   JavaScript backend behind the C facade.

3. Configure target platform through the CMake cache variable:

   ```text
   GAME_PUBLISH_TARGET=local|itch|poki|yandex|playgama
   ```

   CMake computes the SDK adapter from the target:

   ```text
   local    -> mock
   itch     -> mock
   poki     -> poki
   yandex   -> yandex
   playgama -> playgama
   ```

4. Make the target selection a build-time define/config value so only the
   selected SDK adapter is imported, compiled, copied, or linked. A release
   build must not ship unused portal SDK URLs.

5. Route game code through the wrapper only. Game code must not call platform
   globals directly.

6. Read runtime identity and policy from the C wrapper:

   ```text
   target platform
   platform SDK
   externalLinksAllowed
   adsSupported
   rewardedSupported
   storageSupported
   ```

   Prefer capability checks for UI decisions. Example: show Telegram links only
   when `externalLinksAllowed` is true.

7. Install the platform backend through `platform_sdk_set_backend()` for the
   current runtime. Web builds call `platform_sdk_install_web_backend()` before
   `platform_sdk_init()`; mobile/desktop builds can provide a native backend
   with the same C semantics.

8. Route input/gameplay/ad calls through the C facade. Async callbacks use
   `callback + userdata`; `userdata` is caller-owned context, not a platform
   player/account user.

9. Emit SDK-originated lifecycle/ad-flow events from the C facade through
   `features/game-events`. Do not emit events from the JavaScript backend or
   create a second event bus.

   A game that forwards selected typed events to portal analytics may include
   the internal `features/platform_sdk/platform_sdk_measure.h` sink header from
   one recorder/subscriber. Do not call it from gameplay verbs or UI code.

10. For web builds, keep the loading screen as template-owned HTML over the
    canvas. Import `platform-sdk.js` before appending `game.js`, expose only the
    internal progress/hide hooks used by `platform_sdk_web.c`, and let the C
    lifecycle call hide only after the first playable frame has been presented.

11. If the template/game wants the built-in debug controls, compile the
    template-local C/Clay UI module (`src/ui/platform_sdk_debug.c`) and pass
    `GAME_PLATFORM_SDK_DEBUG_UI=1` only in non-release builds. Release builds
    must keep that define off.

## Template Web Build

The current template command is:

```powershell
bash tools/build_web.sh --preset wasm-release --target poki
```

Web builds default to a checkout-local Emscripten cache:

```text
templates/template/build/emscripten-cache
```

The template CMake also prefixes Emscripten compile/link rules with that cache
when `EM_CACHE` is not already set. This keeps release web links independent
from a stale or locked global emsdk cache.

`local` keeps the historic output directory:

```text
templates/template/build/wasm-release/bin
```

Portal targets use target-specific directories:

```text
templates/template/build/wasm-release-itch/bin
templates/template/build/wasm-release-poki/bin
templates/template/build/wasm-release-yandex/bin
templates/template/build/wasm-release-playgama/bin
```

The CMake web step copies:

```text
platform-sdk.js          # internal web backend bootstrap for C bridge
platform-sdk-core.js
platform-sdk-adapter.js   # selected adapter only
```

The debug/test panel is C UI in the game/template binary. It is not copied as a
web JavaScript file.

## Verify

Current validation:

```powershell
node --test features/platform-sdk/tests/platform_sdk.test.mjs
node ai_studio/taskboard/cli.mjs validate --json
node ai_studio/architecture_map/validate_map.mjs
```

Artifact inspection:

```powershell
node features/platform-sdk/scripts/artifact_tools.mjs inspect --target itch --artifact templates/template/build/wasm-release-itch/bin
node features/platform-sdk/scripts/artifact_tools.mjs inspect --target poki --artifact templates/template/build/wasm-release-poki/bin
```

Scorecard fixture:

```powershell
node features/platform-sdk/scripts/scorecard.mjs --input features/platform-sdk/tests/fixtures/scorecard-sample.ndjson --pretty
```

Still required before real portal submission:

- one real-SDK smoke per portal;
- portal-side inspector/debug panel check;
- account-owned metadata review.

## Uninstall

Remove `platform_sdk.c` from target sources, remove `PLATFORM_SDK_*` compile
definitions, remove the web asset staging block, remove target config from
`web/index.html.in`, and remove platform-sdk tests from the consuming template
or game.
