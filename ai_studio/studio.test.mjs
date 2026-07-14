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
  runOwnedDomain,
  selectChangedDomains,
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

test("verification exposes ten coarse owner domains instead of suite categories", () => {
  const verification = describeStudio().verification;
  assert.deepEqual(verification.domains.map((domain) => domain.id), [
    "harness",
    "workspace",
    "architecture",
    "shell",
    "assets",
    "work-management",
    "design",
    "runtime",
    "features",
    "template-release",
  ]);
  assert.equal("suites" in verification, false);
  assert.equal(verification.domains.some((domain) => domain.id === "studio.validation"), false);
  assert.equal(verification.domains.some((domain) => domain.id === "audio.native"), false);
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
  assert.deepEqual(selectChangedDomains(["games/example-game/src/main.c"]), []);
  assert.deepEqual(selectChangedDomains([
    "games/example-game/src/main.c",
    "ai_studio/taskboard/store.mjs",
  ]), ["work-management"]);
  assert.deepEqual(selectChangedDomains(["games/new_game.mjs"]), ["workspace"]);
  assert.throws(
    () => selectChangedDomains(["games/root-shared.txt"]),
    /unowned shared path: games\/root-shared\.txt/,
  );
});

test("changed selection is coarse and unknown shared paths fail ownership", () => {
  assert.deepEqual(selectChangedDomains(["features/platform-sdk/web/platform-sdk.js"]), ["features"]);
  assert.deepEqual(selectChangedDomains(["templates/template/tools/build_web.mjs"]), ["template-release"]);
  assert.deepEqual(selectChangedDomains(["templates/template/devapi/smoke_bot.py"]), ["template-release"]);
  assert.deepEqual(selectChangedDomains(["features/audio-core/src/audio.c"]), ["features"]);
  assert.deepEqual(selectChangedDomains(["ai_studio/assets/canvas/site/workspace.js"]), ["assets"]);
  assert.deepEqual(selectChangedDomains([".codex/skills/nt-asset-image-generation/scripts/test_expand_jobs.py"]), ["harness"]);
  assert.deepEqual(selectChangedDomains(["ai_studio/studio.config.json"]), ["harness"]);
  assert.deepEqual(selectChangedDomains(["ai_studio/studio.config.local.json"]), ["harness"]);
  assert.deepEqual(selectChangedDomains(["ai_studio/config.mjs", "ai_studio/config.test.mjs"]), ["harness"]);
  assert.deepEqual(selectChangedDomains([".vscode/tasks.json", ".vscode/launch.json"]), ["harness"]);
  assert.throws(() => selectChangedDomains(["mystery/shared.txt"]), /unowned shared path: mystery\/shared\.txt/);
});

test("feature docs and runtime paths stay in the same owner domain", () => {
  for (const feature of ["audio-core", "game-events", "game-state", "items-core", "platform-sdk", "progression-core"]) {
    assert.deepEqual(selectChangedDomains([`features/${feature}/README.md`]), ["features"], feature);
    assert.deepEqual(selectChangedDomains([`features/${feature}/src/runtime.c`]), ["features"], feature);
  }
});

test("full plan exposes owner domains and marks template release proof", () => {
  const result = describeStudio();
  assert.equal(result.verification.execution, "owner-domains");
  assert.equal(result.verification.nodeTestConcurrency, 4);
  assert.equal(result.verification.domains.length, 10);
  assert.equal(result.verification.domains.find((domain) => domain.id === "template-release").releaseProof, true);
  assert.equal(result.verification.domains.find((domain) => domain.id === "features").releaseProof, true);
  assert.ok(nativeConfigureCommand("linux").includes("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=-fsanitize=address,undefined"));
  assert.equal(nativeConfigureCommand("win32").some((arg) => arg.startsWith("-DCMAKE_EXE_LINKER_FLAGS_DEBUG=")), false);
});

test("mixed changed paths collapse to stable owner domains", () => {
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
  assert.deepEqual(selectChangedDomains(t0401Paths), ["harness", "workspace", "features", "template-release"]);
});

test("runtime domain declares platform release proof without exposing command inventory", () => {
  const runtime = describeStudio().verification.domains.find((domain) => domain.id === "runtime");
  assert.equal(runtime.releaseProof, true);
  assert.deepEqual(Object.keys(runtime).sort(), ["checks", "id", "releaseProof"]);
});

test("features domain keeps the strict Linux audio runner as release-only proof", async () => {
  const calls = [];
  const result = await runOwnedDomain({ id: "features", checks: ["features.audio.linux"] }, {
    platform: "linux",
    includeRelease: true,
    runCommand: async (command, args) => { calls.push([command, ...args]); return { status: 0, tail: "" }; },
  });
  assert.equal(result.status, 0);
  assert.deepEqual(calls, [["bash", "features/audio-core/tests/run_linux.sh"]]);

  const changed = await runOwnedDomain({ id: "features", checks: ["features.audio.linux"] }, {
    platform: "linux",
    includeRelease: false,
    runCommand: async () => { throw new Error("release proof must not run in changed mode"); },
  });
  assert.equal(changed.classification, "not-applicable");
});

test("an owner domain batches its Node tests into one process", async () => {
  const root = mkdtempSync(join(tmpdir(), "studio-domain-batch-"));
  const files = [
    "ai_studio/studio.test.mjs",
    "ai_studio/studio_ci.test.mjs",
    "ai_studio/quality/tests/profile.test.mjs",
  ];
  try {
    for (const path of files) {
      mkdirSync(dirname(join(root, path)), { recursive: true });
      writeFileSync(join(root, path), "");
    }
    const calls = [];
    const result = await runOwnedDomain({ id: "harness", checks: ["studio.facade", "studio.quality"] }, {
      root,
      runCommand: async (command, args) => { calls.push([command, ...args]); return { status: 0, tail: "" }; },
    });
    assert.equal(result.status, 0);
    assert.equal(calls.length, 1);
    assert.equal(calls[0][1], "--test");
    for (const path of files) assert.ok(calls[0].includes(path), path);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("asset Python unittests share one interpreter while dot-named files stay explicit", async () => {
  const calls = [];
  const result = await runOwnedDomain({ id: "assets", checks: ["studio.assets.python"] }, {
    runCommand: async (command, args) => { calls.push([command, ...args]); return { status: 0, tail: "" }; },
  });
  assert.equal(result.status, 0);
  assert.equal(calls.length, 4);
  assert.deepEqual(calls[0].slice(0, 4), ["node", "ai_studio/dev_environment/python_run.mjs", "-m", "unittest"]);
  assert.equal(calls[0].filter((arg) => arg.endsWith("_test.py")).length, 20);
  assert.equal(calls.slice(1).every((command) => command.at(-1).endsWith(".test.py")), true);
});

test("owner Node and Python test lanes overlap before explicit validation commands", async () => {
  const root = mkdtempSync(join(tmpdir(), "studio-domain-lanes-"));
  const files = [
    "ai_studio/assets/canvas/example.test.mjs",
    "ai_studio/assets/canvas/tools/example_test.py",
    "ai_studio/architecture_map/example.test.mjs",
  ];
  try {
    for (const path of files) {
      mkdirSync(dirname(join(root, path)), { recursive: true });
      writeFileSync(join(root, path), "");
    }
    const events = [];
    const result = await runOwnedDomain({
      id: "mixed-owner",
      checks: ["studio.assets.canvas", "studio.assets.canvas-python", "studio.architecture-map"],
    }, {
      root,
      runCommand: async (_command, args) => {
        const isNodeTests = args[0] === "--test";
        const isPython = args.includes("ai_studio/dev_environment/python_run.mjs");
        const lane = isNodeTests ? "node" : isPython ? "python" : "validate";
        events.push(`${lane}:start`);
        if (isNodeTests) await new Promise((resolveWait) => setTimeout(resolveWait, 30));
        events.push(`${lane}:finish`);
        return { status: 0, tail: "" };
      },
    });
    assert.equal(result.status, 0);
    assert.ok(events.indexOf("python:start") < events.indexOf("node:finish"), events.join(", "));
    assert.ok(events.indexOf("validate:start") > events.indexOf("node:finish"), events.join(", "));
    assert.ok(events.indexOf("validate:start") > events.indexOf("python:finish"), events.join(", "));
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("parallel owner test lanes report Node failure first and skip validation", async () => {
  const root = mkdtempSync(join(tmpdir(), "studio-domain-lane-failure-"));
  const files = [
    "ai_studio/assets/canvas/example.test.mjs",
    "ai_studio/assets/canvas/tools/example_test.py",
    "ai_studio/architecture_map/example.test.mjs",
  ];
  try {
    for (const path of files) {
      mkdirSync(dirname(join(root, path)), { recursive: true });
      writeFileSync(join(root, path), "");
    }
    const calls = [];
    const result = await runOwnedDomain({
      id: "mixed-owner",
      checks: ["studio.assets.canvas", "studio.assets.canvas-python", "studio.architecture-map"],
    }, {
      root,
      runCommand: async (_command, args) => {
        const isNodeTests = args[0] === "--test";
        const isPython = args.includes("ai_studio/dev_environment/python_run.mjs");
        calls.push(isNodeTests ? "node" : isPython ? "python" : "validate");
        if (isNodeTests) return { status: 1, tail: "node failed" };
        if (isPython) return { status: 1, tail: "python failed" };
        return { status: 0, tail: "" };
      },
    });
    assert.deepEqual(calls.sort(), ["node", "python"]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /^\[mixed-owner\]\nnode failed$/);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("new Python tests under an owned root need no registry edit", async () => {
  const root = mkdtempSync(join(tmpdir(), "studio-python-discovery-"));
  const path = "ai_studio/assets/tools/example/new_tool_test.py";
  try {
    mkdirSync(dirname(join(root, path)), { recursive: true });
    writeFileSync(join(root, path), "");
    const calls = [];
    const result = await runOwnedDomain({ id: "assets", checks: ["studio.assets.python"] }, {
      root,
      runCommand: async (command, args) => { calls.push([command, ...args]); return { status: 0, tail: "" }; },
    });
    assert.equal(result.status, 0);
    assert.equal(calls.some((command) => command.includes(path)), true);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("Python discovery ignores tool caches", async () => {
  const root = mkdtempSync(join(tmpdir(), "studio-python-cache-"));
  const source = "ai_studio/assets/tools/example/source_test.py";
  const cached = "ai_studio/assets/tools/example/.pytest_cache/cached_test.py";
  try {
    for (const path of [source, cached]) {
      mkdirSync(dirname(join(root, path)), { recursive: true });
      writeFileSync(join(root, path), "");
    }
    const calls = [];
    await runOwnedDomain({ id: "assets", checks: ["studio.assets.python"] }, {
      root,
      runCommand: async (command, args) => { calls.push([command, ...args]); return { status: 0, tail: "" }; },
    });
    assert.equal(calls.some((command) => command.includes(source)), true);
    assert.equal(calls.some((command) => command.includes(cached)), false);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("missing EMSDK blocks the template release domain with setup exit 2", async () => {
  const result = await verifyStudio({ mode: "full" }, {
    runDomain: async (domain) => domain.id === "template-release"
      ? { status: 2, classification: "setup-blocked", code: "missing-emsdk", stderr: "EMSDK required" }
      : { status: 0 },
  });
  const template = result.domains.find((domain) => domain.id === "template-release");
  const { durationMs, ...templateResult } = template;
  assert.deepEqual(templateResult, { id: "template-release", status: "blocked", code: "missing-emsdk", reason: "EMSDK required" });
  assert.ok(Number.isFinite(durationMs) && durationMs >= 0);
  assert.equal(result.status, "blocked");
  assert.equal(result.exitCode, 2);
});

test("verify emits compact stable results, bounds failure tails, and passes when every gate passes", async () => {
  const calls = [];
  const result = await verifyStudio({ mode: "changed", paths: ["ai_studio/taskboard/store.mjs"] }, {
    runDomain: async (domain) => {
      calls.push(domain.id);
      return { status: 1, stdout: "", stderr: Array.from({ length: 100 }, (_, i) => `line-${i}`).join("\n") };
    },
  });
  assert.deepEqual(calls, ["work-management"]);
  assert.equal(result.schema, "ai_studio.studio.verify.v2");
  assert.equal(result.ok, false);
  assert.equal(result.exitCode, 1);
  assert.ok(result.domains[0].tail.split("\n").length <= 40);

  const full = await verifyStudio({ mode: "full" }, {
    runDomain: async () => ({ status: 0, stdout: "ok", stderr: "" }),
  });
  assert.equal(full.ok, true);
  assert.equal(full.status, "pass");
  assert.ok(full.domains.every((domain) => Number.isFinite(domain.durationMs) && domain.durationMs >= 0));
  assert.equal(full.exitCode, 0);
  assert.equal(full.domains.length, 10);
});

test("full verification runs at most two independent owner domains concurrently", async () => {
  let active = 0;
  let maxActive = 0;
  const result = await verifyStudio({ mode: "full" }, {
    runDomain: async () => {
      active += 1;
      maxActive = Math.max(maxActive, active);
      await new Promise((resolveWait) => setTimeout(resolveWait, 5));
      active -= 1;
      return { status: 0 };
    },
  });
  assert.equal(result.ok, true);
  assert.equal(maxActive, 2);
  assert.deepEqual(result.domains.map((domain) => domain.id), describeStudio().verification.domains.map((domain) => domain.id));
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
    "ai_studio/assets/manifests/tests/integrity.test.mjs",
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
    runDomain: async () => { throw new Error("must not run"); },
  });
  assert.equal(result.ok, true);
  assert.equal(result.exitCode, 0);
  assert.deepEqual(result.domains, []);
});

test("an owner domain without mechanical checks reports not-applicable", async () => {
  const result = await verifyStudio({ mode: "domain", domain: "design" });
  assert.equal(result.ok, true);
  assert.deepEqual(result.domains.map(({ durationMs: _durationMs, ...domain }) => domain), [{
    id: "design",
    status: "not-applicable",
    reason: "no deterministic technical checks",
  }]);
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
      error: { code: "usage", message: "usage: node ai_studio/studio.mjs describe [--json] | verify (--changed|--domain <id>|--full) [--json]" },
      exitCode: 2,
    });
    lines.length = 0;
    assert.equal(await main(["verify", "--changed", "--json"], { changedPaths: () => { throw new Error("git unavailable"); } }), 2);
    assert.equal(JSON.parse(lines.at(-1)).error.code, "setup");
    lines.length = 0;
    assert.equal(await main(["verify", "--full"], { runDomain: async () => ({ status: 0 }) }), 0);
    assert.match(lines.at(-1), /^pass\tfull\t/);
    lines.length = 0;
    const domainCalls = [];
    assert.equal(await main(["verify", "--domain", "assets", "--json"], {
      runDomain: async (domain) => { domainCalls.push(domain.id); return { status: 0 }; },
    }), 0);
    assert.deepEqual(domainCalls, ["assets"]);
    assert.equal(JSON.parse(lines.at(-1)).domain, "assets");
    lines.length = 0;
    assert.equal(await main(["verify", "--changed"], {
      changedPaths: () => ["ai_studio/taskboard/store.mjs"],
      runDomain: async () => ({ status: 1, stderr: "assertion context\nexact failure" }),
    }), 1);
    assert.ok(lines.some((line) => line === "fail\twork-management"));
    assert.ok(lines.some((line) => line === "  assertion context\n  exact failure"));
  } finally {
    console.log = log;
    console.error = error;
  }
});
