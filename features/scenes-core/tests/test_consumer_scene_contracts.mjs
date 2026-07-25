import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

const repoRoot = path.resolve(
  path.dirname(fileURLToPath(import.meta.url)),
  "..",
  "..",
  "..",
);

function read(relativePath) {
  return fs.readFileSync(path.join(repoRoot, relativePath), "utf8");
}

function assertOrdered(source, earlier, later) {
  const earlierIndex = source.indexOf(earlier);
  const laterIndex = source.indexOf(later);
  assert.notEqual(earlierIndex, -1, `missing earlier token: ${earlier}`);
  assert.notEqual(laterIndex, -1, `missing later token: ${later}`);
  assert.ok(
    earlierIndex < laterIndex,
    `expected ${earlier} before ${later}`,
  );
}

function sliceFromRequired(source, marker) {
  const markerIndex = source.indexOf(marker);
  assert.notEqual(markerIndex, -1, `missing slice marker: ${marker}`);
  return source.slice(markerIndex);
}

test("template gates raw movement through scene input eligibility", () => {
  const source = read("templates/template/src/features/game_features.c");
  assert.match(
    source,
    /if\s*\(game_scenes_can_process_game_input\(\)\)\s*\{\s*sys_move\(w,\s*dt\);\s*\}/s,
  );
});

test("template gates platform gameplay through focused scene eligibility", () => {
  const source = read("templates/template/src/main.c");
  const trolley = read(
    "games/private/game-not-a-trolley-problem/src/main.c",
  );
  assert.match(
    source,
    /platform_lifecycle_update\(\s*playable_shell_ready,\s*game_scenes_can_process_game_input\(\)\s*\)/s,
  );
  assert.doesNotMatch(
    source,
    /platform_lifecycle_update\(\s*playable_shell_ready,\s*!settings_is_open\(\)\s*\)/s,
  );
  assert.match(
    trolley,
    /platform_lifecycle_update\(\s*playable_shell_ready,\s*s_game_runtime_ready\s*&&\s*game_scenes_can_process_world_input\(\)\s*\)/s,
  );
  assert.doesNotMatch(
    trolley,
    /platform_lifecycle_update\(\s*playable_shell_ready,\s*!settings_is_open\(\)\s*\)/s,
  );
});

test("settings close reports not-top without inventing a busy operation", () => {
  const header = read(
    "features/scenes-core/include/features/scenes/scene_manager.h",
  );
  const adapter = read("templates/template/src/game_scenes.c");
  assert.match(header, /SCENE_RESULT_NOT_TOP/);
  assert.match(
    adapter,
    /scene_manager_top\(&s_manager\)\.scene\s*!=\s*settings_scene[\s\S]*return SCENE_RESULT_NOT_TOP;/,
  );
});

test("template proves root scene start succeeds before initialization", () => {
  const source = read("templates/template/src/game_scenes.c");
  assert.match(
    source,
    /const scene_result_t start_result\s*=\s*scene_manager_start\(/,
  );
  assert.match(
    source,
    /if\s*\(\s*start_result\s*!=\s*SCENE_RESULT_ACCEPTED\s*\)\s*\{\s*abort\(\);\s*\}/s,
  );
});

test("consumers fail startup when scene DevAPI registration is rejected", () => {
  for (const path of [
    "templates/template/src/main.c",
    "games/private/game-not-a-trolley-problem/src/main.c",
  ]) {
    const source = read(path);
    assert.match(
      source,
      /if\s*\(\s*!scene_manager_register_devapi\(game_scenes_manager\(\)\)\s*\)[\s\S]*return false;/,
    );
  }
});

test("template and games link the shared scenes-core implementation", () => {
  for (const project of [
    "templates/template",
    "games/private/game-not-a-trolley-problem",
  ]) {
    const cmake = read(`${project}/CMakeLists.txt`);
    assert.match(
      cmake,
      /set\(SCENES_CORE_DIR[\s\S]*features\/scenes-core/,
    );
    assert.match(
      cmake,
      /"\$\{SCENES_CORE_SRC\}\/scene_manager\.c"/,
    );
    assert.equal(
      fs.existsSync(path.join(repoRoot, project, "src", "scene_manager.c")),
      false,
      `${project} must not carry a private scene manager implementation`,
    );
  }
});

test("template exposes an independent debug-scene build flag", () => {
  const options = read("templates/template/cmake/GameOptions.cmake");
  const cmake = read("templates/template/CMakeLists.txt");
  assert.match(options, /GAME_DEBUG_SCENES_ENABLED/);
  assert.match(
    cmake,
    /GAME_DEBUG_SCENES_ENABLED=1[\s\S]*GAME_DEBUG_SCENES_ENABLED=0/,
  );
});

test("core runtime remains fixed-storage and allocation-free", () => {
  const header = read(
    "features/scenes-core/include/features/scenes/scene_manager.h",
  );
  const source = read("features/scenes-core/src/scene_manager.c");
  assert.match(header, /SCENE_MANAGER_STORAGE_BYTES\s*=\s*32768/);
  assert.match(source, /_Static_assert\s*\(\s*sizeof\(scene_manager_impl_t\)/);
  assert.doesNotMatch(
    source,
    /\b(?:malloc|calloc|realloc|aligned_alloc|free)\s*\(/,
  );
});

test("template preserves scene resource and shutdown barriers", () => {
  const source = read("templates/template/src/main.c");
  assertOrdered(
    source,
    "nt_resource_step();",
    "game_scenes_step(++s_scene_frame_index",
  );
  const shutdown = sliceFromRequired(source, "devapi_shutdown_runtime();");
  assertOrdered(
    shutdown,
    "game_scenes_shutdown();",
    "nt_resource_step();",
  );
  assertOrdered(
    shutdown,
    "nt_resource_step();",
    "game_features_shutdown(&s_world);",
  );
  assertOrdered(
    shutdown,
    "game_features_shutdown(&s_world);",
    "nt_resource_shutdown();",
  );
});

test("transition pointer gate is declared after scene and global UI", () => {
  const source = read("templates/template/src/features/game_features.c");
  assertOrdered(
    source,
    "game_scenes_build_ui(ui_runtime_ctx());",
    "platform_sdk_debug_draw_ui(ui_runtime_ctx());",
  );
  assertOrdered(
    source,
    "platform_sdk_debug_draw_ui(ui_runtime_ctx());",
    "game_scenes_build_input_gate(ui_runtime_ctx());",
  );
});

test("trolley root scene keeps its shell UI present in passive mode", () => {
  const source = read(
    "games/private/game-not-a-trolley-problem/src/features/game_features.c",
  );
  assert.doesNotMatch(
    source,
    /if\s*\(!interactive\)\s*\{\s*return;\s*\}/,
  );
  assert.match(
    source,
    /scene_ui_is_interactive[\s\S]*!game_scenes_input_gated\(\)/,
  );
  assert.match(
    source,
    /upgrades_panel_draw_launcher\(\s*ctx,\s*w,\s*scene_ui_is_interactive\(mode\)\)/,
  );
  assert.match(
    source,
    /trolley_ladder_modal_draw_launcher\(\s*ctx,\s*w,\s*scene_ui_is_interactive\(mode\)\)/,
  );
  assert.match(
    source,
    /settings_draw_launcher\(\s*ctx,\s*scene_ui_is_interactive\(mode\)\)/,
  );
});

test("consumers gate world rendering through scene presentation", () => {
  const templateHeader = read("templates/template/src/game_scenes.h");
  const templateMain = read("templates/template/src/main.c");
  const trolleyHeader = read(
    "games/private/game-not-a-trolley-problem/src/scene/game_scenes.h",
  );
  const trolleyMain = read(
    "games/private/game-not-a-trolley-problem/src/main.c",
  );

  assert.match(templateHeader, /game_scenes_should_render_world/);
  assert.match(templateMain, /game_scenes_should_render_world\(\)/);
  assert.match(trolleyHeader, /game_scenes_should_render_world/);
  assert.match(trolleyMain, /game_scenes_should_render_world\(\)/);
});

test("trolley routes simulation and shutdown through scene lifecycle", () => {
  const adapter = read(
    "games/private/game-not-a-trolley-problem/src/scene/game_scenes.c",
  );
  const features = read(
    "games/private/game-not-a-trolley-problem/src/features/game_features.c",
  );
  const main = read(
    "games/private/game-not-a-trolley-problem/src/main.c",
  );

  assert.match(adapter, /\.on_update\s*=\s*update_screen/);
  assert.match(
    adapter,
    /update_screen[\s\S]*game_features_update_scene\(s_world,\s*dt\);[\s\S]*testbed_update\(s_world,\s*dt\);/,
  );
  assert.match(features, /void game_features_update_scene\s*\(/);
  assert.match(
    features,
    /void game_features_update_scene[\s\S]*sys_energy_update[\s\S]*sys_tram_update[\s\S]*sys_crowd_update/,
  );
  assert.doesNotMatch(main, /testbed_update\(&s_world/);
  assertOrdered(
    main,
    "game_scenes_shutdown();",
    "game_features_shutdown(&s_world);",
  );
});

test("overlay open queries use current presentation, not history membership", () => {
  const templateSettings = read(
    "templates/template/src/features/settings/settings_screen.c",
  );
  const trolleySettings = read(
    "games/private/game-not-a-trolley-problem/src/features/settings/settings.c",
  );
  const upgrades = read(
    "games/private/game-not-a-trolley-problem/src/ui/upgrades_panel.c",
  );
  const ladder = read(
    "games/private/game-not-a-trolley-problem/src/ui/trolley_ladder_modal.c",
  );

  assert.match(templateSettings, /game_scenes_is_presented/);
  for (const source of [trolleySettings, upgrades, ladder]) {
    assert.match(source, /scene_manager_is_presented/);
  }
});
