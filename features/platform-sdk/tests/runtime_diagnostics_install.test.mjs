import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const FEATURE = resolve(dirname(fileURLToPath(import.meta.url)), "..");

function fixture(t, prefix) {
  const root = mkdtempSync(join(tmpdir(), "platform-crash-install-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "cmake"));
  mkdirSync(join(root, "web"));
  writeFileSync(join(root, "cmake", "PlatformSdk.cmake"), readFileSync(join(FEATURE, "cmake", "PlatformSdk.cmake")));
  writeFileSync(join(root, "web", "runtime-diagnostics.js"), readFileSync(join(FEATURE, "web", "runtime-diagnostics.js")));
  writeFileSync(join(root, "game.json"), JSON.stringify({ storageNamespace: "generated-game" }));
  writeFileSync(join(root, "app.c"), "void app(void) {}\n");
  writeFileSync(join(root, "index.html.in"), '@PLATFORM_SDK_DIAGNOSTICS_JS@\nkey: @GAME_CRASH_STORAGE_PREFIX@.v1\n');
  writeFileSync(join(root, "CMakeLists.txt"), `cmake_minimum_required(VERSION 3.25)
project(crash_install C)
set(EMSCRIPTEN TRUE)
add_library(nt_crash INTERFACE)
add_library(app STATIC app.c)
${prefix === undefined ? "" : `set(GAME_CRASH_STORAGE_PREFIX [=[${prefix}]=])`}
include(cmake/PlatformSdk.cmake)
platform_sdk_crash(app)
configure_file(index.html.in index.html @ONLY)
file(GENERATE OUTPUT definitions.txt CONTENT "$<TARGET_PROPERTY:app,COMPILE_DEFINITIONS>")
file(GENERATE OUTPUT libraries.txt CONTENT "$<TARGET_PROPERTY:app,LINK_LIBRARIES>")
`);
  return root;
}

function cmake(args) {
  const result = spawnSync("cmake", args, { encoding: "utf8", windowsHide: true, timeout: 60000 });
  if (result.error) throw result.error;
  return result;
}

function configure(root) {
  return cmake(["-S", root, "-B", join(root, "build"), "-G", "Ninja",
    "-DCMAKE_C_COMPILER=clang", "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"]);
}

function passed(result) {
  assert.equal(result.status, 0, result.stderr || result.stdout);
}

test("generated identity produces the same C and shell key and shared-source edits regenerate HTML", (t) => {
  const root = fixture(t);
  passed(configure(root));
  const build = join(root, "build");
  const html = readFileSync(join(build, "index.html"), "utf8");
  assert.ok(html.includes("key: generated-game.crash.v1"));
  assert.match(readFileSync(join(build, "definitions.txt"), "utf8"), /GAME_CRASH_STORAGE_KEY="generated-game\.crash\.v1"/);
  assert.match(readFileSync(join(build, "libraries.txt"), "utf8"), /nt_crash/);
  const source = join(root, "web", "runtime-diagnostics.js");
  writeFileSync(source, readFileSync(source, "utf8") + "\nvar sourceChanged = true;\n");
  passed(cmake(["--build", build]));
  assert.match(readFileSync(join(build, "index.html"), "utf8"), /var sourceChanged = true;/);
});

test("an explicitly retained legacy prefix overrides the generated namespace", (t) => {
  const root = fixture(t, "legacy.crash");
  passed(configure(root));
  assert.match(readFileSync(join(root, "build", "index.html"), "utf8"), /key: legacy\.crash\.v1/);
  assert.match(readFileSync(join(root, "build", "definitions.txt"), "utf8"), /GAME_CRASH_STORAGE_KEY="legacy\.crash\.v1"/);
});

test("unsafe storage literals are rejected before they enter generated C or HTML", (t) => {
  const root = fixture(t, "bad'prefix");
  const result = configure(root);
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /Crash storage prefix must be a non-empty literal namespace/);
});
