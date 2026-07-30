import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import {
  mkdirSync,
  mkdtempSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import {
  createRuntimeBuildRecord,
  validateRuntimeBuildRecord,
} from "./runtime_build.mjs";

function write(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, value);
}

function fixture(t) {
  const studioRoot = mkdtempSync(join(tmpdir(), "runtime-build-"));
  t.after(() => rmSync(studioRoot, { recursive: true, force: true }));
  const gameDir = join(studioRoot, "games", "test-game");
  const dependencies = {
    schema: "ai_studio.game.dependencies.v2",
    engine: {
      source: "external/neotolis-engine",
      version: "0.1.0",
      revision: "1".repeat(40),
      compatibility: "tested",
    },
    features: [{
      id: "platform-sdk",
      source: "features/platform-sdk",
      version: "1.1.0",
      revision: "2".repeat(40),
      compatibility: "tested",
    }],
    compatibility: "fixture",
  };
  write(join(gameDir, "CMakeLists.txt"), [
    "project(test_game)",
    "set(PLATFORM_SDK_DIR \"${GAME_REPO_ROOT}/features/platform-sdk\")",
    "",
  ].join("\n"));
  write(join(gameDir, "src", "main.c"), "int main(void) { return 0; }\n");
  write(join(gameDir, "dependencies.json"), `${JSON.stringify(dependencies, null, 2)}\n`);
  write(join(studioRoot, "external", "neotolis-engine", "engine", "core.c"), "void engine(void) {}\n");
  write(join(studioRoot, "features", "platform-sdk", "src", "sdk.c"), "void sdk(void) {}\n");
  return { studioRoot, gameDir, dependencies };
}

test("runtime build record deterministically binds game and dependency source trees", (t) => {
  const item = fixture(t);
  const one = createRuntimeBuildRecord(item);
  const two = createRuntimeBuildRecord(item);

  assert.deepEqual(one, two);
  assert.equal(one.schema, "ai_studio.runtime_build.v1");
  assert.match(one.fingerprint, /^[0-9a-f]{64}$/);
  assert.deepEqual(validateRuntimeBuildRecord(one), one);
  assert.deepEqual(one.inputs.map((input) => input.id), ["game", "engine", "feature:platform-sdk"]);

  write(join(item.gameDir, "build", "wasm-release", "bin", "game.wasm"), "ignored build output");
  write(join(item.gameDir, "capture", "catalog.json"), "ignored capture tooling state");
  write(join(item.gameDir, "release", "artifacts", "old.zip"), "ignored release output");
  write(join(item.gameDir, "tmp", "captures", "draft.png"), "ignored transient capture output");
  write(join(item.gameDir, "README.md"), "ignored documentation\n");
  write(join(item.gameDir, "tools", "runtime.test.mjs"), "ignored test\n");
  assert.deepEqual(createRuntimeBuildRecord(item), one);

  write(join(item.gameDir, "design", "reference", "mood.png"), "ignored design reference");
  assert.deepEqual(createRuntimeBuildRecord(item), one);
  write(join(item.gameDir, "design", "items", "balance.lua"), "return { cost = 2 }\n");
  assert.notEqual(createRuntimeBuildRecord(item).fingerprint, one.fingerprint);
  rmSync(join(item.gameDir, "design", "items"), { recursive: true, force: true });

  write(join(item.gameDir, "src", "build", "runtime.c"), "void nested_runtime(void) {}\n");
  assert.notEqual(createRuntimeBuildRecord(item).fingerprint, one.fingerprint);
  rmSync(join(item.gameDir, "src", "build"), { recursive: true, force: true });

  write(join(item.studioRoot, "features", "platform-sdk", "src", "sdk.c"), "void sdk_changed(void) {}\n");
  assert.notEqual(createRuntimeBuildRecord(item).fingerprint, one.fingerprint);
});

test("runtime build hashing rejects symbolic links", (t) => {
  const item = fixture(t);
  const outside = join(item.studioRoot, "outside.c");
  write(outside, "outside\n");
  try {
    symlinkSync(outside, join(item.gameDir, "src", "linked.c"), "file");
  } catch (error) {
    if (["EPERM", "EACCES", "ENOTSUP"].includes(error.code)) return t.skip(`symlink unavailable: ${error.code}`);
    throw error;
  }
  assert.throws(() => createRuntimeBuildRecord(item), /symbolic link|symlink/i);

});

test("runtime build rejects compiled in-place features missing from dependencies", (t) => {
  const item = fixture(t);
  write(
    join(item.gameDir, "CMakeLists.txt"),
    "set(SCENES_CORE_DIR \"${GAME_REPO_ROOT}/features/scenes-core\")\n",
  );
  write(
    join(item.studioRoot, "features", "scenes-core", "src", "scene.c"),
    "void scene(void) {}\n",
  );
  assert.throws(
    () => createRuntimeBuildRecord(item),
    /features\/scenes-core is compiled but not declared/,
  );
});

test("runtime build rejects declared features missing from canonical CMake wiring", (t) => {
  const item = fixture(t);
  write(join(item.gameDir, "CMakeLists.txt"), "project(test_game)\n");
  assert.throws(
    () => createRuntimeBuildRecord(item),
    /features\/platform-sdk is declared but not compiled/,
  );
});

test("runtime build scans nested CMake feature references and rejects dynamic ids", (t) => {
  const item = fixture(t);
  write(join(item.gameDir, "cmake", "nested", "Feature.cmake"),
    "set(HIDDEN \"${GAME_REPO_ROOT}/features/scenes-core\")\n");
  write(join(item.studioRoot, "features", "scenes-core", "src", "scene.c"), "void scene(void) {}\n");
  assert.throws(() => createRuntimeBuildRecord(item), /scenes-core is compiled but not declared/);
  write(join(item.gameDir, "cmake", "nested", "Feature.cmake"),
    "set(HIDDEN \"${GAME_REPO_ROOT}/features/${FEATURE_ID}\")\n");
  assert.throws(() => createRuntimeBuildRecord(item), /literal feature id/);
});

test("runtime build validation rejects malformed and non-canonical records", (t) => {
  const clean = fixture(t);
  const record = createRuntimeBuildRecord(clean);
  assert.throws(() => validateRuntimeBuildRecord({ ...record, fingerprint: "0".repeat(64) }), /fingerprint/i);
  assert.throws(() => validateRuntimeBuildRecord({ ...record, extra: true }), /unexpected fields/i);
  const wrongSourceInputs = record.inputs.map((input) => input.id === "engine" ? { ...input, source: "external/other" } : input);
  const wrongSource = {
    ...record,
    inputs: wrongSourceInputs,
    fingerprint: createHash("sha256").update(JSON.stringify(wrongSourceInputs)).digest("hex"),
  };
  assert.throws(() => validateRuntimeBuildRecord(wrongSource), /source must be exactly/i);
});
