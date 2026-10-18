import assert from "node:assert/strict";
import { readdirSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import test from "node:test";

const root = resolve(import.meta.dirname, "../..");
const expected = ["GameAssets.cmake", "GameCodegen.cmake", "GameOptions.cmake", "GamePlatform.cmake", "GameTests.cmake"];
const consumer = "ai_studio/feature_testing/consumer";
const requiredTests = {
  [consumer]: ["test_audio_core", "test_audio_resource", "test_audio_backend_native", "test_game_audio", "test_audio_web_library", "test_game_state_json", "test_game_state_nested", "test_game_storage", "test_game_storage_web_backend", "test_game_save_blocked", "test_game_save", "test_game_events", "test_game_events_overflow", "test_game_state_roundtrip", "test_game_events_typed", "test_game_event_render", "test_game_analytics", "test_game_events_log_mirror", "test_items_api_core_only", "test_items_api", "items_c_catalog_test", "test_items_fragment", "test_items_fragment_assert_off", "items_catalog_validate", "test_progression", "test_progression_curve", "loc_generator_test", "test_loc_e2e", "loc_font_charset_check", "test_game_storage_write_modes", "test_game_format", "test_platform_sdk", "test_game_input", "test_platform_lifecycle", "test_platform_sdk_events", "platform_sdk_node_test", "test_template_composition"],
};
const expectedCustomTargets = {
  [consumer]: ["feature_test_font", "game_asset_packs", "items_catalog_gen", "platform_sdk_web_assets", "platform_sdk_playgama_config_asset", "devapi_smoke", "quality_responsive", "template_audio_test_fixture", "progression_tracks_gen"],
};

function declarations(text, expression) {
  return [...text.matchAll(expression)].map((match) => match[1]);
}

test("blank template keeps only foundation CMake concerns", () => {
  const text = readFileSync(join(root, "templates/template/CMakeLists.txt"), "utf8");
  assert.doesNotMatch(text, /GamePlatform|items_api|game_audio|test_items|test_audio/);
});

for (const source of [consumer]) {
  test(`${source} owns exactly five mechanical CMake concern files`, () => {
    const dir = join(root, source, "cmake");
    assert.deepEqual(readdirSync(dir).filter((file) => file.startsWith("Game") && file.endsWith(".cmake")).sort(), expected);
    for (const file of expected) assert.ok(readFileSync(join(dir, file), "utf8").trim().length > 0, file);
  });
}

for (const source of [consumer]) {
  test(`${source} root is a five-include conductor`, () => {
    const text = readFileSync(join(root, source, "CMakeLists.txt"), "utf8");
    for (const file of expected) assert.equal(text.split(`include(cmake/${file})`).length - 1, 1, file);
  });

  test(`${source} preserves configured target and CTest declarations`, () => {
    const conductor = readFileSync(join(root, source, "CMakeLists.txt"), "utf8");
    const includes = expected.map((file) => readFileSync(join(root, source, "cmake", file), "utf8")).join("\n");
    const expanded = `${conductor}\n${includes}`;
    const configuredTests = new Set(declarations(expanded, /add_test\(NAME\s+([^\s\)]+)/g));
    for (const name of requiredTests[source]) {
      assert.ok(configuredTests.has(name), `missing required CTest declaration: ${name}`);
    }
    assert.deepEqual(declarations(expanded, /add_custom_target\(([^\s\)]+)/g), expectedCustomTargets[source]);
    assert.ok(declarations(expanded, /add_executable\(([^\s\)]+)/g).includes("${GAME_TARGET}"));
  });
}

test("codegen selects Studio Python by host platform for Emscripten builds", () => {
  for (const source of [consumer]) {
    const text = readFileSync(join(root, source, "cmake", "GameCodegen.cmake"), "utf8");
    assert.match(text, /if\(CMAKE_HOST_WIN32\)/, source);
    assert.doesNotMatch(text, /if\(WIN32\)/, source);
  }
});

test("template production Items catalog is generated from the semantic Lua route", () => {
  const text = readFileSync(join(root, consumer, "cmake", "GameCodegen.cmake"), "utf8");
  assert.match(text, /add_custom_target\(items_catalog_gen\b/);
  assert.match(text, /items_cli\.py[\s\S]*--project-root[\s\S]*build[\s\S]*--out-dir/);
  assert.match(text, /items\.lua\.json/);
  assert.match(text, /design\/items\/\*\.lua/);
  assert.match(text, /items\.snapshot\.json[\s\S]*items_catalog\.gen\.c/);
});

test("template game compiles the generated Items catalog and packs no catalog blob", () => {
  const codegen = readFileSync(join(root, consumer, "cmake", "GameCodegen.cmake"), "utf8");
  const assets = readFileSync(join(root, consumer, "cmake", "GameAssets.cmake"), "utf8");
  const builder = readFileSync(join(root, consumer, "src", "build_packs.c"), "utf8");
  assert.match(codegen, /target_sources\(\$\{GAME_TARGET\} PRIVATE[\s\S]*items_api\.c[\s\S]*ITEMS_CATALOG_SOURCE/);
  assert.match(assets, /build_game_packs>\s+"\$\{GAME_PACK_DIR\}"\s+"\$\{GAME_FONT_SOURCE\}"/);
  assert.match(builder, /argc\s*!=\s*3/);
  assert.doesNotMatch(builder, /items\/catalog/);
});

test("feature-test consumer audio ownership stays in the exact CMake concern files", () => {
  const dir = join(root, consumer, "cmake");
  const conductor = readFileSync(join(root, consumer, "CMakeLists.txt"), "utf8");
  const assets = readFileSync(join(dir, "GameAssets.cmake"), "utf8");
  const platform = readFileSync(join(dir, "GamePlatform.cmake"), "utf8");
  const tests = readFileSync(join(dir, "GameTests.cmake"), "utf8");
  assert.doesNotMatch(conductor, /AUDIO_CORE|game_audio|audio_web|demo_jingle|ui_click/);
  assert.match(assets, /ui_click\.wav[\s\S]*demo_jingle\.mp3/);
  assert.match(assets, /assets\/\*\.glsl/, "shader include edits must invalidate game.ntpack");
  assert.doesNotMatch(assets, /audio_backend|test_audio/);
  assert.match(platform, /AUDIO_CORE_DIR[\s\S]*src\/game_audio\.c[\s\S]*audio_backend_web\.c/);
  assert.doesNotMatch(platform, /add_test\(|demo_jingle|ui_click/);
  for (const name of ["test_audio_core", "test_audio_resource", "test_audio_backend_native", "test_game_audio", "test_audio_web_library"]) {
    assert.match(tests, new RegExp(`add_test\\(NAME\\s+${name}\\b`));
  }
  const nativeTests = tests.slice(0, tests.indexOf("\nif(EMSCRIPTEN)"));
  const executableTargets = [...nativeTests.matchAll(/^\s*add_executable\(([^\s\)]+)/gm)]
    .map((match) => match[1])
    .filter((name) => !name.includes("$") && !["test_focus_prompt", "test_game_asset_paths", "test_platform_hooks", "write_audio_test_fixture"].includes(name))
    .sort();
  const sanitizerSection = nativeTests.slice(
    nativeTests.indexOf("set(GAME_NATIVE_TEST_TARGETS"),
    nativeTests.indexOf("foreach(_test_target IN LISTS GAME_NATIVE_TEST_TARGETS"),
  );
  const sanitizerTargets = [...new Set(
    [...sanitizerSection.matchAll(/\b(?:test|benchmark)_[A-Za-z0-9_]+\b/g)].map((match) => match[0]),
  )].sort();
  const sanitizerTargetSet = new Set(sanitizerTargets);
  for (const target of executableTargets) {
    assert.ok(sanitizerTargetSet.has(target), `native test executable lacks sanitizer runtime: ${target}`);
  }
  for (const suite of ["lifecycle", "navigation", "ordering", "presentation"]) {
    assert.ok(
      sanitizerTargetSet.has(`test_scenes_core_${suite}`),
      `generated scenes-core suite lacks sanitizer runtime: ${suite}`,
    );
  }
  assert.match(tests, /foreach\(_test_target IN LISTS GAME_NATIVE_TEST_TARGETS\)[\s\S]*nt_set_sanitizer_flags\(\$\{_test_target\}\)/);
  assert.match(tests, /AUDIO_TEST_MP3_PATH=.*demo_jingle\.mp3/);
});
