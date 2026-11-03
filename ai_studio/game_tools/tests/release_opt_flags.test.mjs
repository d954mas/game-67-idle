import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import { executeGameCommand } from "../game_cli.mjs";
import {
  assertReleaseBuild, assertReleaseSources, inspectCommands, readBuildCommands, scanGameSources, scanStudioSources,
} from "../release_opt_flags.mjs";

function scratch(t, files = {}) {
  const dir = mkdtempSync(join(tmpdir(), "release-opt-flags-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  for (const [path, text] of Object.entries(files)) {
    mkdirSync(dirname(join(dir, path)), { recursive: true });
    writeFileSync(join(dir, path), text);
  }
  return dir;
}

const EMCC = "C:\\emsdk\\upstream\\emscripten\\emcc.bat";
const compile = (flags, unit = "src/a.c") => ({
  command: `"C:/Program Files/CMake/bin/cmake.exe" -E env "EM_CACHE=C:/c" ${EMCC} -DX=1 -IC:/inc ${flags} -o CMakeFiles/game.dir/${unit}.o -c C:/g/${unit}`,
  output: `CMakeFiles/game.dir/${unit}.o`,
});
const link = (flags) => ({
  command: `C:\\WINDOWS\\system32\\cmd.exe /C "cd . && "C:/Program Files/CMake/bin/cmake.exe" -E env "EM_CACHE=C:/c" ${EMCC} ${flags} CMakeFiles/game.dir/src/a.c.o -o bin\\game.js && cd ."`,
  output: "bin/game.js",
});

test("the Studio's own templates, features and game tools carry no size flags", () => {
  assert.deepEqual(scanStudioSources(), []);
});

test("source scan reports every size flag with file and line, and ignores comments", (t) => {
  const game = scratch(t, {
    "CMakeLists.txt": "# never -Oz: size budgets remove code\nproject(g C)\n",
    "cmake/present.cmake": "set(A 1)\ntarget_compile_options(ui PRIVATE \"$<$<CONFIG:Release>:-Oz>\") # tiny\n",
    "cmake/audio.cmake": "target_link_options(game PRIVATE -Os)\n",
    "CMakePresets.json": "{ \"cacheVariables\": { \"CMAKE_C_FLAGS_RELEASE\": \"-Os\" } }\n",
    "tools/build_web.mjs": "// -Oz is forbidden\nconst args = [\"-DCMAKE_BUILD_TYPE=MinSizeRel\", \"-DCMAKE_C_FLAGS=-Oz\"];\n",
    "src/notes.js": "const x = '-Oz';\n",
    "build/wasm-release/CMakeFiles/x.cmake": "set(F -Oz)\n",
  });
  const hits = scanGameSources(game).map((v) => `${v.file}:${v.line} ${v.flag}`).sort();
  assert.deepEqual(hits, [
    "CMakePresets.json:1 -Os",
    "cmake/audio.cmake:1 -Os",
    "cmake/present.cmake:2 -Oz",
    "tools/build_web.mjs:2 -Oz",
  ]);
  const clean = scratch(t, { "CMakeLists.txt": "target_compile_options(game PRIVATE $<$<NOT:$<CONFIG:Release>>:-O2>)\nset(T \"#-Oz\")\n" });
  assert.deepEqual(scanGameSources(clean).map((v) => v.flag), ["-Oz"], "a flag inside a quoted string is not a comment");
});

test("a build type of MinSizeRel is a size flag; matching on it is not", (t) => {
  const game = scratch(t, {
    "a.cmake": "if(build_type MATCHES \"^(Release|MinSizeRel)$\")\nendif()\n",
    "b.cmake": "set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING \"\")\n",
  });
  assert.deepEqual(scanGameSources(game).map((v) => `${v.file}:${v.line}`), ["b.cmake:1"]);
});

test("the source gate fails loudly with the rule and the fix", (t) => {
  const game = scratch(t, { "cmake/ui.cmake": "\n\ntarget_compile_options(ui PRIVATE -Oz)\n" });
  assert.throws(() => assertReleaseSources({ gameDir: game }), (error) => {
    assert.match(error.message, /RELEASE OPTIMIZATION GATE FAILED/);
    assert.match(error.message, /cmake\/ui\.cmake:3 {2}-Oz/);
    assert.match(error.message, /removing code or data, never by -Oz\/-Os/);
    return true;
  });
  assert.doesNotThrow(() => assertReleaseSources({ gameDir: scratch(t, { "CMakeLists.txt": "project(g C)\n" }) }));
});

test("effective flags: speed-optimized compiles and link pass", () => {
  assert.deepEqual(inspectCommands([
    compile("-O3 -DNDEBUG"), compile("-O3 -O2", "src/b.c"), link("-O3 --emit-symbol-map"),
    { command: `${EMCC.replace("emcc", "emar")} qc libx.a a.o`, output: "libx.a" },
    { command: "", output: "all" },
  ]), []);
});

test("effective flags: a size flag from any source fails, even when a later -O3 wins", () => {
  const found = inspectCommands([
    compile("-O3 -Oz"),
    compile("-Oz -O3", "src/b.c"),
    link("-O3 -Oz"),
    { command: `cmake -E env EMCC_CFLAGS=-Os ${EMCC} -O3 -o bin/game.js a.o`, output: "bin/game.js" },
    { command: "wasm-opt -Oz bin/game.wasm -o bin/game.wasm", output: "bin/game.wasm.stamp" },
  ]);
  assert.deepEqual(found.map((v) => `${v.kind} ${v.target} ${v.flag}`), [
    "compile game -Oz", "compile game -Oz", "link bin/game.js -Oz", "link bin/game.js -Os", "link bin/game.wasm.stamp -Oz",
  ]);
});

test("effective flags: a release compile or link below -O2 fails", () => {
  const found = inspectCommands([compile("-O3 -O0"), compile("-O1", "src/b.c"), compile("-DNDEBUG", "src/c.c"), link("--emit-symbol-map")]);
  assert.deepEqual(found.map((v) => `${v.kind} ${v.flag}`), [
    "compile -O0", "compile -O1", "compile (no -O: emcc/clang default -O0)", "link (no -O: emcc/clang default -O0)",
  ]);
});

test("the build gate groups violations by target and states the rule", () => {
  const read = () => [compile("-O3 -Oz"), compile("-O3 -Oz", "src/b.c"), link("-O3 -Oz")];
  assert.throws(() => assertReleaseBuild("build/wasm-release", { read }), (error) => {
    assert.match(error.message, /target game: 2 translation unit\(s\) compile at -Oz \(e\.g\. src\/a\.c\.o\)/);
    assert.match(error.message, /link bin\/game\.js: -Oz/);
    assert.match(error.message, /never compiled or linked for size/);
    return true;
  });
  assert.throws(() => readBuildCommands(join(tmpdir(), "no-such-release-tree")), /no build\.ninja/);
});

const hasNinja = spawnSync("ninja", ["--version"]).status === 0;

test("a flag hidden in a Ninja response file is still seen", { skip: !hasNinja && "ninja is not installed" }, (t) => {
  const tree = scratch(t, {
    "build.ninja": [
      "rule link",
      "  command = emcc -O3 @$out.rsp -o $out",
      "  rspfile = $out.rsp",
      "  rspfile_content = $in -Oz",
      "build bin/game.js: link a.o",
      "",
    ].join("\n"),
    "a.o": "",
  });
  assert.throws(() => assertReleaseBuild(tree), /link bin\/game\.js: -Oz/);
});

test("package checks the tree beside the game's artifact and removes an archive built with size flags", async (t) => {
  const gameDir = scratch(t, { "release/game.zip": "zip" });
  const seen = [];
  const dependencies = {
    gameDir,
    doctor: () => ({ gameId: "example" }),
    loadMetadata: () => ({ dependencies: {}, proof: "game" }),
    verifyDependencies: () => {},
    artifactDir: (dir, target) => join(dir, "build", `${target}-custom`, "site"),
    package: () => ({ zipPath: join(gameDir, "release", "game.zip") }),
  };
  const args = { command: "package", target: "itch", build: false, templateProof: false, outDir: "" };
  await assert.rejects(executeGameCommand(args, dependencies, {
    releaseBuild: (dir) => { seen.push(dir); throw new Error("RELEASE OPTIMIZATION GATE FAILED"); },
  }), /RELEASE OPTIMIZATION GATE FAILED/);
  assert.deepEqual(seen, [join(gameDir, "build", "itch-custom")]);
  assert.equal(existsSync(join(gameDir, "release", "game.zip")), false);
  // A game's own dependencies do not reach the gate: with no release tree the real one fails.
  await assert.rejects(executeGameCommand(args, { ...dependencies, releaseBuild: () => {} }), /no build\.ninja/);
});

test("every build or test command scans sources before it runs anything", async (t) => {
  const gameDir = scratch(t, { "cmake/ui.cmake": "target_compile_options(ui PRIVATE -Oz)\n" });
  const calls = [];
  await assert.rejects(executeGameCommand({ command: "test", target: "local", build: false, templateProof: false, only: [], tier: "", all: false }, {
    gameDir,
    doctor: () => ({ gameId: "example" }),
    loadMetadata: () => ({ dependencies: {}, proof: "game" }),
    verifyDependencies: () => calls.push("dependencies"),
    nodeTest: () => calls.push("node"),
    nativeTest: () => calls.push("native"),
  }), /cmake\/ui\.cmake:1 {2}-Oz/);
  assert.deepEqual(calls, []);
});
