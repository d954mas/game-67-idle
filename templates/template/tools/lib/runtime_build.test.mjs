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
  write(join(gameDir, "CMakeLists.txt"), "project(test_game)\n");
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
  write(join(item.gameDir, "release", "artifacts", "old.zip"), "ignored release output");
  write(join(item.gameDir, "README.md"), "ignored documentation\n");
  write(join(item.gameDir, "tools", "runtime.test.mjs"), "ignored test\n");
  assert.deepEqual(createRuntimeBuildRecord(item), one);

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
