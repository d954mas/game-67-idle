import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { copyFileSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
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

test("a bare consumer builds the transport and passes roundtrip and idle-ping wire checks", () => {
  const dir = mkdtempSync(join(tmpdir(), "nt-network-core-"));
  writeFileSync(join(dir, "CMakeLists.txt"), [
    "cmake_minimum_required(VERSION 3.25)",
    "project(consumer LANGUAGES C CXX)",
    "function(nt_set_sanitizer_flags target)",
    "  if(NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES \"Clang|GNU\")",
    "    target_compile_options(${target} PRIVATE -fsanitize=undefined -fno-sanitize-recover=all)",
    "    target_link_options(${target} PUBLIC -fsanitize=undefined -fno-sanitize-recover=all)",
    "  endif()",
    "endfunction()",
    `include("${feature}/tests/curl.cmake")`,
    `add_subdirectory("${feature}" network_core)`,
    `add_executable(roundtrip "${feature}/tests/roundtrip.c")`,
    "target_link_libraries(roundtrip PRIVATE nt::network_core)",
    "target_compile_options(roundtrip PRIVATE -Wall -Wextra -Werror)",
    "nt_set_sanitizer_flags(roundtrip)",
    `add_executable(idle_ping "${feature}/tests/idle_ping.c")`,
    "target_link_libraries(idle_ping PRIVATE nt::network_core)",
    "target_compile_options(idle_ping PRIVATE -Wall -Wextra -Werror)",
    "nt_set_sanitizer_flags(idle_ping)",
    "enable_testing()",
    "add_test(NAME roundtrip COMMAND roundtrip)",
    "set_tests_properties(roundtrip PROPERTIES LABELS core)",
    "add_test(NAME idle_ping COMMAND idle_ping)",
    "set_tests_properties(idle_ping PROPERTIES LABELS core TIMEOUT 15)",
    "",
  ].join("\n"));
  const compiler = process.env.CC || (process.platform === "win32" ? "C:/Program Files/LLVM/bin/clang.exe" : "cc");
  run("cmake", ["-S", dir, "-B", join(dir, "build"), "-G", "Ninja", `-DCMAKE_C_COMPILER=${compiler}`, "-DCMAKE_BUILD_TYPE=Debug"], dir);
  run("cmake", ["--build", join(dir, "build"), "--parallel", "2"], dir);
  run("ctest", ["--test-dir", join(dir, "build"), "--output-on-failure"], dir);
});

for (const [record, dir, license, least] of [
  ["UPSTREAM.uwebsockets.json", "vendor/uwebsockets", "Apache-2.0", 40],
  ["UPSTREAM.libuv.json", "vendor/libuv", "MIT", 40],
]) {
  test(`${dir} matches its pinned integrity record`, () => {
    const manifest = JSON.parse(readFileSync(join(feature, record), "utf8"));
    assert.equal(manifest.license, license);
    assert.ok(manifest.files.length > least);
    for (const row of manifest.files) {
      const bytes = readFileSync(join(feature, dir, row.path));
      assert.equal(bytes.length, row.bytes, row.path);
      assert.equal(createHash("sha256").update(bytes).digest("hex"), row.sha256, row.path);
    }
    // Every local change is a recorded patch: undone, it gives back the upstream bytes.
    for (const change of manifest.modifications) {
      assert.ok(manifest.files.some((row) => row.path === change.path), change.path);
      assert.ok(change.reason && change.patch, change.path);
      const scratch = mkdtempSync(join(tmpdir(), "network-core-patch-"));
      try {
        mkdirSync(dirname(join(scratch, change.path)), { recursive: true });
        copyFileSync(join(feature, dir, change.path), join(scratch, change.path));
        run("git", ["-c", "core.autocrlf=false", "apply", "-R", join(feature, change.patch)], scratch);
        const upstream = readFileSync(join(scratch, change.path));
        assert.equal(upstream.length, change.upstream_bytes, change.path);
        assert.equal(createHash("sha256").update(upstream).digest("hex"), change.upstream_sha256, change.path);
      } finally {
        rmSync(scratch, { recursive: true, force: true });
      }
    }
  });
}
