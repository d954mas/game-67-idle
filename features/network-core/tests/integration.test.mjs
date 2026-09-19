import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

const feature = resolve(dirname(fileURLToPath(import.meta.url)), "..").replaceAll("\\", "/");

function run(command, args, cwd) {
  const result = spawnSync(command, args, { cwd, encoding: "utf8" });
  assert.equal(result.status, 0, `${command} ${args.join(" ")}\n${result.error || ""}\n${result.stdout}\n${result.stderr}`);
  return result.stdout;
}

test("a bare consumer builds the transport and passes the roundtrip", () => {
  const dir = mkdtempSync(join(tmpdir(), "nt-network-core-"));
  writeFileSync(join(dir, "CMakeLists.txt"), [
    "cmake_minimum_required(VERSION 3.25)",
    "project(consumer LANGUAGES C)",
    `add_subdirectory("${feature}" network_core)`,
    `add_executable(roundtrip "${feature}/tests/roundtrip.c")`,
    "target_link_libraries(roundtrip PRIVATE nt::network_core)",
    "target_compile_options(roundtrip PRIVATE -Wall -Wextra -Werror)",
    "enable_testing()",
    "add_test(NAME roundtrip COMMAND roundtrip)",
    "set_tests_properties(roundtrip PROPERTIES LABELS core)",
    "",
  ].join("\n"));
  const compiler = process.env.CC || (process.platform === "win32" ? "C:/Program Files/LLVM/bin/clang.exe" : "cc");
  run("cmake", ["-S", dir, "-B", join(dir, "build"), "-G", "Ninja", `-DCMAKE_C_COMPILER=${compiler}`, "-DCMAKE_BUILD_TYPE=Debug"], dir);
  run("cmake", ["--build", join(dir, "build")], dir);
  run("ctest", ["--test-dir", join(dir, "build"), "--output-on-failure"], dir);
});

test("the vendored libwebsockets matches its pinned integrity record", () => {
  const manifest = JSON.parse(readFileSync(join(feature, "UPSTREAM.json"), "utf8"));
  assert.equal(manifest.license, "MIT");
  assert.equal(manifest.modifications.length, 0);
  assert.ok(manifest.files.length > 100);
  for (const row of manifest.files) {
    const bytes = readFileSync(join(feature, "vendor/libwebsockets", row.path));
    assert.equal(bytes.length, row.bytes, row.path);
    assert.equal(createHash("sha256").update(bytes).digest("hex"), row.sha256, row.path);
  }
});
