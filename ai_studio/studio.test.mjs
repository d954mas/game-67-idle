import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { spawnSync } from "node:child_process";
import test from "node:test";

import {
  describeStudio,
  isDeterministicTestPath,
  main,
  nativeConfigureCommand,
  parsePorcelainZ,
  selectChangedSuites,
  unassignedDeterministicTests,
  verifyStudio,
} from "./studio.mjs";

test("describe exposes stable owner routes without a Studio game proxy", () => {
  const result = describeStudio();
  assert.equal(result.schema, "ai_studio.studio.describe.v1");
  assert.deepEqual(result.routes.game.commands.map((entry) => entry.id), [
    "create", "doctor", "build", "run", "test", "playable", "package", "verify",
  ]);
  assert.equal(result.routes.game.commands[0].owner, "games/new_game.mjs");
  assert.equal(result.routes.game.commands.some((entry) => entry.owner === "ai_studio/studio.mjs"), false);
  assert.deepEqual(result.routes.game.commands.find((entry) => entry.id === "package").route, ["node", "<game-root>/tools/game.mjs", "package"]);
  assert.match(result.routes.game.commands.find((entry) => entry.id === "package").note, /build_web\.mjs only builds/);
  assert.equal(result.routes.canvas.surface, "/canvas/");
  assert.deepEqual(result.routes.canvas.cli, ["node", "ai_studio/assets/canvas/cli.mjs"]);
  assert.equal(result.routes.taskboard.surface, "/taskboard/");
  assert.equal(result.routes.assets.surface, "/asset_viewer/");
  assert.deepEqual(result.routes.game.commands.find((entry) => entry.id === "doctor").route, ["node", "<game-root>/tools/game.mjs", "doctor"]);
  assert.deepEqual(result.exitSemantics, { pass: 0, failed: 1, blockedOrSetup: 2 });
});

test("porcelain -z parser preserves spaces and both rename paths", () => {
  const input = " M ai_studio/a file.mjs\0R  templates/template/old name\0templates/template/new name\0?? games/new_game.test.mjs\0";
  assert.deepEqual(parsePorcelainZ(input), [
    "ai_studio/a file.mjs",
    "templates/template/old name",
    "templates/template/new name",
    "games/new_game.test.mjs",
  ]);
});

test("changed selection hard-excludes games while preserving the new_game root exception", () => {
  assert.deepEqual(selectChangedSuites(["games/web-dressup/src/main.c"]), []);
  assert.deepEqual(selectChangedSuites([
    "games/web-dressup/src/main.c",
    "ai_studio/taskboard/store.mjs",
  ]), ["studio.taskboard", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["games/new_game.mjs"]), ["workspace.game-create"]);
});

test("changed selection is narrow and unknown shared paths fall back to full", () => {
  assert.deepEqual(selectChangedSuites(["features/platform-sdk/web/platform-sdk.js"]), ["reference-template.web", "features.contracts", "features.platform-sdk", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/tools/build_web.mjs"]), ["reference-template.web", "reference-template", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/tools/package_web.mjs"]), ["reference-template.web", "reference-template", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/tools/portal_evidence.mjs"]), ["reference-template", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["mystery/shared.txt"]), ["full"]);
  assert.deepEqual(selectChangedSuites(["features/audio-core/src/audio.c"]), [
    "reference-template.native", "features.contracts", "features.audio.web", "studio.validation", "audio.native",
  ]);
  assert.deepEqual(selectChangedSuites(["features/audio-core/vendor/miniaudio/README.md"]), [
    "features.contracts", "features.audio.web", "studio.validation",
  ]);
  assert.deepEqual(selectChangedSuites(["ai_studio/assets/canvas/site/workspace.js"]), ["studio.assets.canvas", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["features/items-core/scripts/items_ops.py"]), ["reference-template.native", "reference-template.web", "features.contracts", "features.items-core", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/src/main.c"]), ["reference-template.native", "reference-template.web", "reference-template", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/cmake/GamePlatform.cmake"]), ["reference-template.native", "reference-template.web", "reference-template", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/assets/ui/README.md"]), ["reference-template", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/assets/ui/panel.png"]), ["reference-template.native", "reference-template.web", "reference-template", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/src/features/README.md"]), ["reference-template", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/game-dependencies.json"]), ["features.contracts", "reference-template", "studio.validation"]);
  assert.deepEqual(selectChangedSuites(["templates/template/devapi/smoke_bot.py"]), ["reference-template", "reference-template.python", "studio.validation"]);
  assert.deepEqual(selectChangedSuites([".codex/skills/nt-asset-image-generation/scripts/test_expand_jobs.py"]), ["studio.skills.python", "studio.validation"]);
});

test("C-backed feature docs stay local while runtime paths route native", () => {
  for (const feature of ["audio-core", "game-events", "game-state", "items-core", "platform-sdk", "progression-core"]) {
    assert.equal(selectChangedSuites([`features/${feature}/README.md`]).includes("reference-template.native"), false, feature);
    assert.ok(selectChangedSuites([`features/${feature}/src/runtime.c`]).includes("reference-template.native"), feature);
  }
});

test("full plan pins native before asset and exposes runnable audio platform commands", () => {
  const result = describeStudio();
  assert.equal(result.verification.execution, "sequential");
  assert.equal(result.verification.suites.some((suite) => suite.id === "studio.assets.canvas"), true);
  assert.equal(result.verification.suites.some((suite) => suite.id === "features.contracts"), true);
  assert.equal(result.verification.suites.some((suite) => suite.id === "reference-template"), true);
  const audio = result.verification.suites.find((suite) => suite.id === "audio.native");
  assert.equal(audio.availability, "available");
  assert.deepEqual(audio.commandPlan.windows[0], [
    "cmake", "--build", "templates/template/build/native-debug", "--target",
    "test_audio_core", "test_audio_resource", "test_audio_backend_native", "test_game_audio",
  ]);
  assert.deepEqual(audio.commandPlan.linux, [["bash", "features/audio-core/tests/run_linux.sh"]]);
  const ids = result.verification.suites.map((suite) => suite.id);
  const native = result.verification.suites.find((suite) => suite.id === "reference-template.native");
  assert.ok(native.commandPlan[0].includes("-DCMAKE_CXX_COMPILER=clang++"));
  assert.ok(nativeConfigureCommand("linux").includes("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined"));
  assert.equal(nativeConfigureCommand("win32").some((arg) => arg.startsWith("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=")), false);
  assert.ok(ids.indexOf("reference-template.native") < ids.indexOf("studio.assets"));
  assert.ok(ids.indexOf("reference-template.native") < ids.indexOf("studio.assets.canvas"));
  assert.equal(ids.indexOf("reference-template.web"), ids.indexOf("reference-template.native") + 1);
  const web = result.verification.suites.find((suite) => suite.id === "reference-template.web");
  assert.deepEqual(web.commandPlan[0], ["node", "templates/template/tools/build_web.mjs", "--preset", "wasm-release", "--target", "itch", "--no-debug-ui"]);
  assert.match(web.commandPlan[1].at(-1), /game\.js.*game\.wasm.*game\.ntpack/);
  assert.deepEqual(web.commandPlan[2], ["node", "templates/template/tools/game.mjs", "verify", "--target", "itch", "--no-build", "--template-proof", "--skip-tests", "--out", "build/package-proof"]);
});

test("portal evidence tests run in the fast reference suite and never route to web/CMake", async () => {
  const t0401Paths = [
    "templates/template/tools/portal_evidence.mjs",
    "templates/template/tools/portal_evidence.test.mjs",
    "games/new_game.test.mjs",
    "templates/new_template.test.mjs",
    "ai_studio/studio.mjs",
    "ai_studio/studio.test.mjs",
    "templates/template/release/README.md",
    "features/platform-sdk/references/publish-targets.md",
  ];
  const selection = selectChangedSuites(t0401Paths);
  assert.deepEqual(new Set(selection), new Set([
    "reference-template",
    "workspace.game-create",
    "studio.facade",
    "features.contracts",
    "features.platform-sdk",
    "studio.validation",
  ]));
  assert.equal(selection.includes("full"), false);
  assert.equal(selection.some((id) => id === "reference-template.web" || id === "reference-template.native" || id === "audio.native"), false);

  const seen = new Map();
  await verifyStudio({ mode: "full" }, {
    runSuite: async (suite) => {
      seen.set(suite.id, suite);
      return { status: 0 };
    },
  });
  assert.ok(seen.get("reference-template").testFiles.includes("templates/template/tools/portal_evidence.test.mjs"));
  assert.equal((seen.get("reference-template.web").testFiles || []).includes("templates/template/tools/portal_evidence.test.mjs"), false);
});

test("runtime automation runs the functional iterate contract only on Linux", () => {
  const runtime = describeStudio().verification.suites.find((suite) => suite.id === "studio.runtime-automation");
  const iterateCommand = [
    "xvfb-run", "-a", "node", "ai_studio/dev_environment/python_run.mjs",
    "ai_studio/runtime_automation/iterate.py", "--no-capture", "--json",
  ];
  assert.equal(runtime.commandPlan.windows.some((command) => command.join("\0") === iterateCommand.join("\0")), false);
  assert.equal(runtime.commandPlan.linux.filter((command) => command.join("\0") === iterateCommand.join("\0")).length, 1);
  assert.ok(runtime.commandPlan.windows.some((command) => command.includes("ai_studio/runtime_automation/iterate_test.py")));
  assert.ok(runtime.commandPlan.linux.some((command) => command.includes("ai_studio/runtime_automation/iterate_test.py")));
});

test("missing EMSDK blocks the required web suite with setup exit 2", async () => {
  const result = await verifyStudio({ mode: "full" }, {
    runSuite: async (suite) => suite.id === "reference-template.web"
      ? { status: 2, classification: "setup-blocked", code: "missing-emsdk", stderr: "EMSDK required" }
      : { status: 0 },
  });
  const web = result.suites.find((suite) => suite.id === "reference-template.web");
  assert.deepEqual(web, { id: "reference-template.web", status: "blocked", code: "missing-emsdk", reason: "EMSDK required" });
  assert.equal(result.status, "blocked");
  assert.equal(result.exitCode, 2);
});

test("verify emits compact stable results, bounds failure tails, and passes when every gate passes", async () => {
  const calls = [];
  const result = await verifyStudio({ mode: "changed", paths: ["ai_studio/taskboard/store.mjs"] }, {
    runSuite: async (suite) => {
      calls.push(suite.id);
      return { status: 1, stdout: "", stderr: Array.from({ length: 100 }, (_, i) => `line-${i}`).join("\n") };
    },
  });
  assert.deepEqual(calls, ["studio.taskboard", "studio.validation"]);
  assert.equal(result.schema, "ai_studio.studio.verify.v1");
  assert.equal(result.ok, false);
  assert.equal(result.exitCode, 1);
  assert.ok(result.suites[0].tail.split("\n").length <= 40);

  const full = await verifyStudio({ mode: "full" }, {
    runSuite: async () => ({ status: 0, stdout: "ok", stderr: "" }),
  });
  assert.equal(full.ok, true);
  assert.equal(full.status, "pass");
  assert.equal(full.exitCode, 0);
  assert.equal(full.suites.find((suite) => suite.id === "audio.native").status, "pass");
});

test("every tracked deterministic shared test is assigned to an owner suite", () => {
  assert.equal(isDeterministicTestPath(".codex/skills/nt-asset-image-generation/scripts/test_expand_jobs.py"), true);
  assert.deepEqual(unassignedDeterministicTests(), []);
});

test("tracked graduated asset owner tests are assigned to the Studio assets suite", () => {
  const root = mkdtempSync(join(tmpdir(), "studio-asset-owner-inventory-"));
  const testPaths = [
    "ai_studio/assets/catalog/tests/index.test.mjs",
    "ai_studio/assets/intake/tests/intake.test.mjs",
    "ai_studio/assets/licenses/restricted_assets_guard.test.mjs",
    "ai_studio/assets/manifests/tests/manifest.test.mjs",
    "ai_studio/assets/previews/tests/cache.test.mjs",
    "ai_studio/assets/sources/tests/libraries.test.mjs",
    "ai_studio/assets/tests/ownership.test.mjs",
  ];
  try {
    for (const path of testPaths) {
      mkdirSync(dirname(join(root, path)), { recursive: true });
      writeFileSync(join(root, path), "import test from 'node:test'; test('owner', () => {});\n");
    }
    assert.equal(spawnSync("git", ["init", "-q"], { cwd: root }).status, 0);
    assert.equal(spawnSync("git", ["add", ...testPaths], { cwd: root }).status, 0);
    assert.deepEqual(unassignedDeterministicTests(root), []);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("new tracked native tests fail closed until explicitly assigned", () => {
  const root = mkdtempSync(join(tmpdir(), "studio-native-inventory-"));
  const testPath = "features/example/tests/test_new.c";
  try {
    mkdirSync(join(root, "features/example/tests"), { recursive: true });
    writeFileSync(join(root, testPath), "int main(void) { return 0; }\n");
    assert.equal(spawnSync("git", ["init", "-q"], { cwd: root }).status, 0);
    assert.equal(spawnSync("git", ["add", testPath], { cwd: root }).status, 0);
    assert.deepEqual(unassignedDeterministicTests(root), [testPath]);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("no changed files is a successful hard-zero plan", async () => {
  const result = await verifyStudio({ mode: "changed", paths: ["games/example/main.c"] }, {
    runSuite: async () => { throw new Error("must not run"); },
  });
  assert.equal(result.ok, true);
  assert.equal(result.exitCode, 0);
  assert.deepEqual(result.suites, []);
});

test("CLI contract returns stable JSON usage and setup envelopes", async () => {
  const log = console.log;
  const error = console.error;
  const lines = [];
  console.log = (line) => lines.push(line);
  console.error = () => {};
  try {
    assert.equal(await main(["describe", "--json"]), 0);
    lines.length = 0;
    assert.equal(await main(["verify", "--changed", "--full", "--json"]), 2);
    assert.deepEqual(JSON.parse(lines.at(-1)), {
      schema: "ai_studio.studio.error.v1",
      ok: false,
      error: { code: "usage", message: "usage: node ai_studio/studio.mjs describe [--json] | verify (--changed|--full) [--json]" },
      exitCode: 2,
    });
    lines.length = 0;
    assert.equal(await main(["verify", "--changed", "--json"], { changedPaths: () => { throw new Error("git unavailable"); } }), 2);
    assert.equal(JSON.parse(lines.at(-1)).error.code, "setup");
    lines.length = 0;
    assert.equal(await main(["verify", "--full"], { runSuite: async () => ({ status: 0 }) }), 0);
    assert.match(lines.at(-1), /^pass\tfull\t/);
    lines.length = 0;
    assert.equal(await main(["verify", "--changed"], {
      changedPaths: () => ["ai_studio/taskboard/store.mjs"],
      runSuite: async () => ({ status: 1, stderr: "assertion context\nexact failure" }),
    }), 1);
    assert.ok(lines.some((line) => line === "fail\tstudio.taskboard"));
    assert.ok(lines.some((line) => line === "  assertion context\n  exact failure"));
  } finally {
    console.log = log;
    console.error = error;
  }
});
