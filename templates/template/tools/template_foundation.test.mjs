import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { runInNewContext } from "node:vm";
import test from "node:test";
import { findStudioRoot } from "./lib/studio_root.mjs";

const templateRoot = dirname(dirname(fileURLToPath(import.meta.url)));
const studioRoot = findStudioRoot(templateRoot);

test("blank starter declares state without gameplay feature defaults", {
  skip: !existsSync(join(templateRoot, "game-dependencies.json")),
}, () => {
  const dependencies = JSON.parse(readFileSync(join(templateRoot, "game-dependencies.json"), "utf8"));

  const ids = dependencies.features.map(({ id }) => id);
  assert.ok(ids.includes("game-state"));
  for (const optional of ["items-core", "progression-core", "audio-core", "ui-kit"]) {
    assert.equal(ids.includes(optional), false);
  }
});

test("configured web SDK config exposes the actual build's release boolean", (t) => {
  const root = mkdtempSync(join(tmpdir(), "template-web-config-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const cmakePath = (path) => path.replaceAll("\\", "/");
  for (const buildType of ["Debug", "Release"]) {
    const output = join(root, `${buildType}.html`);
    const script = join(root, `${buildType}.cmake`);
    writeFileSync(script, `
cmake_minimum_required(VERSION 3.25)
# Script mode has no targets; full builds cover compile definitions.
function(add_compile_definitions)
endfunction()
set(GAME_REPO_ROOT [=[${cmakePath(studioRoot)}]=])
set(CMAKE_BUILD_TYPE ${buildType})
set(EMSCRIPTEN ON)
set(GAME_EMSCRIPTEN_CACHE_DIR [=[${cmakePath(join(root, "cache"))}]=])
set(GAME_RUNTIME_BUILD_FINGERPRINT ${"a".repeat(64)})
set(GAME_RUNTIME_BUILD_PROFILE wasm-${buildType.toLowerCase()})
set(GAME_RUNTIME_BUILD_DEBUG_UI 0)
set(GAME_RUNTIME_BUILD_DEVAPI 0)
set(GAME_RUNTIME_BUILD_ANALYTICS 0)
set(GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR 0)
include([=[${cmakePath(join(templateRoot, "cmake", "GameOptions.cmake"))}]=])
configure_file([=[${cmakePath(join(templateRoot, "web", "index.html.in"))}]=] [=[${cmakePath(output)}]=] @ONLY)
`, "utf8");
    const result = spawnSync("cmake", ["-P", script], { cwd: root, encoding: "utf8" });
    assert.equal(result.status, 0, result.stderr || result.stdout);
    const assignment = readFileSync(output, "utf8").match(/window\.__PLATFORM_SDK_CONFIG__\s*=\s*Object\.freeze\(\{[\s\S]*?\}\);/);
    assert.ok(assignment);
    const window = {};
    runInNewContext(assignment[0], { window }, { timeout: 1000 });
    assert.equal(window.__PLATFORM_SDK_CONFIG__.release, buildType === "Release");
  }
});
