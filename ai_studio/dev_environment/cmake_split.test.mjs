import assert from "node:assert/strict";
import { readdirSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import test from "node:test";

const root = resolve(import.meta.dirname, "../..");
const expected = ["GameAssets.cmake", "GameCodegen.cmake", "GameOptions.cmake", "GamePlatform.cmake", "GameTests.cmake"];
const expectedTests = {
  "games/web-dressup": ["test_game_state_json", "test_game_storage", "test_game_save", "test_game_events", "test_game_events_overflow", "test_game_state_roundtrip", "test_game_events_typed", "test_game_event_render", "test_game_analytics", "test_game_events_log_mirror", "test_items_catalog", "test_items_fragment", "items_ops_validate", "items_ops_test", "test_progression", "test_progression_curve", "test_game_format", "test_dress_room", "test_platform_sdk", "test_platform_lifecycle", "test_platform_sdk_events", "platform_sdk_node_test", "test_template_composition"],
  "templates/template": ["test_game_state_json", "test_game_storage", "test_game_save", "test_game_events", "test_game_events_overflow", "test_game_state_roundtrip", "test_game_events_typed", "test_game_event_render", "test_game_analytics", "test_game_events_log_mirror", "test_items_catalog", "test_items_api_core_only", "test_items_api", "generate_items_api_proof_test", "test_items_fragment", "items_ops_validate", "items_ops_test", "test_progression", "test_progression_curve", "test_game_format", "test_platform_sdk", "test_platform_lifecycle", "test_platform_sdk_events", "platform_sdk_node_test", "test_template_composition"],
};
const expectedCustomTargets = ["game_asset_packs", "platform_sdk_web_assets", "platform_sdk_playgama_config_asset", "devapi_smoke", "quality_responsive", "progression_tracks_gen"];

function declarations(text, expression) {
  return [...text.matchAll(expression)].map((match) => match[1]);
}

for (const source of ["games/web-dressup", "templates/template"]) {
  test(`${source} owns exactly five mechanical CMake concern files`, () => {
    const dir = join(root, source, "cmake");
    assert.deepEqual(readdirSync(dir).filter((file) => file.startsWith("Game") && file.endsWith(".cmake")).sort(), expected);
    for (const file of expected) assert.ok(readFileSync(join(dir, file), "utf8").trim().length > 0, file);
  });
}

for (const source of ["games/web-dressup", "templates/template"]) {
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
    assert.deepEqual(declarations(expanded, /add_custom_target\(([^\s\)]+)/g), expectedCustomTargets);
    assert.ok(declarations(expanded, /add_executable\(([^\s\)]+)/g).includes("${GAME_TARGET}"));
  });
}

test("template T0357 include files contain no T0393 audio overlay", () => {
  const dir = join(root, "templates", "template", "cmake");
  const text = expected.map((file) => readFileSync(join(dir, file), "utf8")).join("\n");
  assert.doesNotMatch(text, /AUDIO_CORE|game_audio|audio_web|demo_jingle|ui_click/);
});
