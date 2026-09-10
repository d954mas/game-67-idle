# Install Box3D

1. Resolve `GAME_REPO_ROOT` using the existing game scaffold, then add:

   ```cmake
   include("${GAME_REPO_ROOT}/features/box3d/cmake/Box3D.cmake")
   box3d_enable(game)
   ```

   Call `box3d_enable(test_target)` for every physics test executable too.
   Do not add another vendor copy or set `PROJECT_VERSION` in the game.

2. Add to the consuming game's `dependencies.json` features array:

   ```json
   {"id":"box3d","source":"features/box3d","version":"0.1.0","compatibility":"portable native/WASM Box3D profile; consumer tests required"}
   ```

3. Keep the upstream include and API spelling:
   `#include "box3d/box3d.h"`. Create/destroy worlds explicitly, step them with
   the game's fixed timestep, and choose shapes/materials in game code.
   `FEATURE_BOX3D=1` comes from the linked target; no extra flag is required.

4. Include all of `THIRD_PARTY_NOTICES.txt` in distributed binary notices.
   It contains Box3D, qsort, rapidhash and verstable copyright/permission notices.
   Run `node --test features/box3d/tests/integration.test.mjs` from Studio,
   then `node tools/game.mjs test --all` and the game's web build/browser smoke.
   Release proof uses `node tools/game.mjs verify --target poki` as usual.

To disable/uninstall, remove game physics calls, the CMake include/helper calls
and the dependency row. No save migration or asset cleanup is installed by this
module. Do not remove the shared feature while another game consumes it.

For HTML/WASM shells, the game can read `THIRD_PARTY_NOTICES.txt` with CMake
`file(READ ... GAME_BOX3D_NOTICES)` and retain `@GAME_BOX3D_NOTICES@` inside an
inert `<script type="text/plain" id="box3d-notices">` in its configured HTML.
Check the final minified package still contains all four notices.
Native builds need the same text beside the distributed executable or in credits.
