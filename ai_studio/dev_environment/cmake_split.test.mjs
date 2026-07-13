import assert from "node:assert/strict";
import { readdirSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import test from "node:test";

const root = resolve(import.meta.dirname, "../..");
const expected = ["GameAssets.cmake", "GameCodegen.cmake", "GameOptions.cmake", "GamePlatform.cmake", "GameTests.cmake"];
const expectedTests = {
  "templates/template": ["test_audio_core", "test_audio_resource", "test_audio_backend_native", "test_game_audio", "test_audio_web_library", "test_game_state_json", "test_game_storage", "test_game_save", "test_game_events", "test_game_events_overflow", "test_game_state_roundtrip", "test_game_events_typed", "test_game_event_render", "test_game_analytics", "test_game_events_log_mirror", "test_items_catalog", "test_items_api_core_only", "test_items_api", "generate_items_api_proof_test", "test_items_fragment", "items_ops_validate", "items_ops_test", "test_progression", "test_progression_curve", "test_game_format", "test_platform_sdk", "test_platform_lifecycle", "test_platform_sdk_events", "platform_sdk_node_test", "test_template_composition"],
};
const expectedCustomTargets = {
  "templates/template": ["game_asset_packs", "platform_sdk_web_assets", "platform_sdk_playgama_config_asset", "devapi_smoke", "quality_responsive", "progression_tracks_gen"],
};

function declarations(text, expression) {
  return [...text.matchAll(expression)].map((match) => match[1]);
}

for (const source of ["templates/template"]) {
  test(`${source} owns exactly five mechanical CMake concern files`, () => {
    const dir = join(root, source, "cmake");
    assert.deepEqual(readdirSync(dir).filter((file) => file.startsWith("Game") && file.endsWith(".cmake")).sort(), expected);
    for (const file of expected) assert.ok(readFileSync(join(dir, file), "utf8").trim().length > 0, file);
  });
}

for (const source of ["templates/template"]) {
  test(`${source} root is a five-include conductor`, () => {
    const override = source === "templates/template" ? process.env.T0357_TEMPLATE_CONDUCTOR : "";
    const text = readFileSync(override || join(root, source, "CMakeLists.txt"), "utf8");
    for (const file of expected) assert.equal(text.split(`include(cmake/${file})`).length - 1, 1, file);
  });

  test(`${source} preserves configured target and CTest declarations`, () => {
    const override = source === "templates/template" ? process.env.T0357_TEMPLATE_CONDUCTOR : "";
    const conductor = readFileSync(override || join(root, source, "CMakeLists.txt"), "utf8");
    const includes = expected.map((file) => readFileSync(join(root, source, "cmake", file), "utf8")).join("\n");
    const expanded = `${conductor}\n${includes}`;
    assert.deepEqual(declarations(expanded, /add_test\(NAME\s+([^\s\)]+)/g), expectedTests[source]);
    assert.deepEqual(declarations(expanded, /add_custom_target\(([^\s\)]+)/g), expectedCustomTargets[source]);
    assert.ok(declarations(expanded, /add_executable\(([^\s\)]+)/g).includes("${GAME_TARGET}"));
  });
}

test("codegen selects Studio Python by host platform for Emscripten builds", () => {
  for (const source of ["templates/template"]) {
    const text = readFileSync(join(root, source, "cmake", "GameCodegen.cmake"), "utf8");
    assert.match(text, /if\(CMAKE_HOST_WIN32\)/, source);
    assert.doesNotMatch(text, /if\(WIN32\)/, source);
  }
});

test("template audio ownership stays in the exact CMake concern files", () => {
  const dir = join(root, "templates", "template", "cmake");
  const conductor = readFileSync(join(root, "templates", "template", "CMakeLists.txt"), "utf8");
  const assets = readFileSync(join(dir, "GameAssets.cmake"), "utf8");
  const platform = readFileSync(join(dir, "GamePlatform.cmake"), "utf8");
  const tests = readFileSync(join(dir, "GameTests.cmake"), "utf8");
  assert.doesNotMatch(conductor, /AUDIO_CORE|game_audio|audio_web|demo_jingle|ui_click/);
  assert.match(assets, /ui_click\.wav[\s\S]*demo_jingle\.mp3/);
  assert.doesNotMatch(assets, /audio_backend|test_audio/);
  assert.match(platform, /AUDIO_CORE_DIR[\s\S]*src\/game_audio\.c[\s\S]*audio_backend_web\.c/);
  assert.doesNotMatch(platform, /add_test\(|demo_jingle|ui_click/);
  for (const name of ["test_audio_core", "test_audio_resource", "test_audio_backend_native", "test_game_audio", "test_audio_web_library"]) {
    assert.match(tests, new RegExp(`add_test\\(NAME\\s+${name}\\b`));
  }
  const executableTargets = [...tests.matchAll(/^\s*add_executable\(([^\s\)]+)/gm)].map((match) => match[1]).sort();
  const sanitizerTargets = tests.match(/set\(GAME_NATIVE_TEST_TARGETS([\s\S]*?)\)\s*foreach/)?.[1]
    .trim().split(/\s+/).sort();
  assert.deepEqual(sanitizerTargets, executableTargets, "every native test executable must link the sanitizer runtime");
  assert.match(tests, /foreach\(_test_target IN LISTS GAME_NATIVE_TEST_TARGETS\)[\s\S]*nt_set_sanitizer_flags\(\$\{_test_target\}\)/);
  assert.match(tests, /AUDIO_TEST_MP3_PATH=.*demo_jingle\.mp3/);
  assert.doesNotMatch(tests, /ui_click\.wav/);
});
