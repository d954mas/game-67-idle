import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import test from "node:test";

const root = process.cwd();

function tempRoot(t, active = true) {
  const dir = mkdtempSync(join(tmpdir(), "ember-pack-guard-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  mkdirSync(join(dir, "tasks"), { recursive: true });
  mkdirSync(join(dir, "tools", "ember-road"), { recursive: true });
  mkdirSync(join(dir, "src"), { recursive: true });
  writeFileSync(join(dir, "tasks", "STATUS.md"), active ? "Build `Ember Road` (ember-road).\n" : "No active game concept is selected.\n", "utf8");
  return dir;
}

function writeValidContract(dir) {
  writeFileSync(
    join(dir, "CMakeLists.txt"),
    `
add_executable(build_ember_road_packs tools/ember-road/build_packs.c)
set(EMBER_ROAD_BASE_PACK "\${EMBER_ROAD_PACK_DIR}/ember_road_base.ntpack")
set(EMBER_ROAD_RUNTIME_PACK "\${EMBER_ROAD_RUNTIME_ASSET_DIR}/ember_road_base.ntpack")
add_custom_command(
  OUTPUT "\${EMBER_ROAD_RUNTIME_PACK}"
  COMMAND \${CMAKE_COMMAND} -E copy_if_different "\${EMBER_ROAD_BASE_PACK}" "\${EMBER_ROAD_RUNTIME_PACK}"
  DEPENDS external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf)
add_custom_target(ember_road_runtime_assets DEPENDS "\${EMBER_ROAD_RUNTIME_PACK}")
add_dependencies(\${GAME_TARGET} ember_road_runtime_assets)
target_compile_definitions(\${GAME_TARGET} PRIVATE EMBER_ROAD_BASE_PACK_PATH="\${EMBER_ROAD_RUNTIME_PACK_C}")
`,
    "utf8",
  );
  writeFileSync(
    join(dir, "tools", "ember-road", "build_packs.c"),
    `
int main(void) {
  const char *font_path = "external/neotolis-engine/assets/fonts/LilitaOne-RussianChineseKo.ttf";
  if (!file_exists(font_path)) { return 1; }
  char charset[512];
  snprintf(charset, sizeof(charset), "%s%s", NT_CHARSET_ASCII, CYRILLIC_CHARSET);
  nt_builder_add_font(ctx, font_path, &(nt_font_opts_t){ .charset = charset, .resource_name = "ember_road/font_ui" });
  const nt_build_result_t result = nt_builder_finish_pack(ctx);
  if (result != NT_BUILD_OK) { return 1; }
}
`,
    "utf8",
  );
  writeFileSync(
    join(dir, "src", "clean_seed_main.c"),
    `
#include "ember_road_base.h"
void load(void) {
  nt_resource_load_auto(s_asset_pack_id, EMBER_ROAD_BASE_PACK_PATH);
  nt_resource_request(ASSET_FONT_EMBER_ROAD_FONT_UI, NT_ASSET_FONT);
  nt_text_renderer_draw("Ember Road", model, size, color, 0.0F, 0.0F);
}
`,
    "utf8",
  );
}

function runGuard(dir) {
  return spawnSync(process.execPath, ["tools/ember-road/asset_pack_contract_guard.mjs", "--root", dir, "--json"], {
    cwd: root,
    encoding: "utf8",
    stdio: "pipe",
  });
}

test("skips when Ember Road is not active", (t) => {
  const dir = tempRoot(t, false);
  const result = runGuard(dir);
  assert.equal(result.status, 0, result.stderr);
  assert.equal(JSON.parse(result.stdout).skipped, true);
});

test("accepts clean-build runtime font pack contract", (t) => {
  const dir = tempRoot(t);
  writeValidContract(dir);
  const result = runGuard(dir);
  assert.equal(result.status, 0, result.stderr);
  assert.equal(JSON.parse(result.stdout).status, "pass");
});

test("rejects missing packed UI font resource", (t) => {
  const dir = tempRoot(t);
  writeValidContract(dir);
  writeFileSync(join(dir, "tools", "ember-road", "build_packs.c"), "int main(void) { return 0; }\n", "utf8");
  const result = runGuard(dir);
  assert.notEqual(result.status, 0);
  const report = JSON.parse(result.stdout);
  assert.match(report.problems.map((p) => p.rule).join(","), /font-resource/);
});
