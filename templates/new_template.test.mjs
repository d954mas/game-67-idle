import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { execFileSync, spawnSync } from "node:child_process";

const script = resolve("templates/new_template.mjs");

function taskboardItems(root) {
  return join(root, "ai_studio", "taskboard", "items");
}

function tempRepo() {
  const root = mkdtempSync(join(tmpdir(), "new-template-"));
  mkdirSync(join(root, "templates", "template", "assets"), { recursive: true });
  mkdirSync(join(root, "templates", "template", "src", "generated"), { recursive: true });
  mkdirSync(join(root, "templates", "template", "build"), { recursive: true });
  writeFileSync(join(root, "templates", "template", "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.25)\n", "utf8");
  writeFileSync(join(root, "templates", "template", "assets", "readme.txt"), "asset\n", "utf8");
  for (const [rel, body] of [
    ["tools/game.mjs", "// game-owned CLI\n"],
    ["tools/package_web.mjs", "// package owner\n"],
    ["tools/portal_evidence.mjs", "// evidence owner\n"],
    ["tools/lib/zip_store.mjs", "// ZIP owner\n"],
    ["release/README.md", "# Release owner\n"],
    [".github/workflows/game-verify.yml", "name: game verify\n"],
    [".gitignore", "build/\nsrc/generated/\n.ai_studio/evidence/\nrelease/artifacts/\n"],
  ]) {
    mkdirSync(dirname(join(root, "templates", "template", rel)), { recursive: true });
    writeFileSync(join(root, "templates", "template", rel), body, "utf8");
  }
  writeFileSync(join(root, "templates", "template", "src", "generated", "game.h"), "#pragma once\n", "utf8");
  mkdirSync(join(root, "templates", "template", ".ai_studio", "evidence"), { recursive: true });
  writeFileSync(join(root, "templates", "template", ".ai_studio", "evidence", "private.txt"), "private\n", "utf8");
  writeFileSync(join(root, "templates", "template", "build", "stale.obj"), "generated\n", "utf8");
  writeFileSync(join(root, "templates", "template", "template.json"), JSON.stringify({
    schema: "ai_studio.template.v1", id: "template", title: "Template", storageNamespace: "template",
  }), "utf8");
  writeFileSync(join(root, "templates", "template", "game-dependencies.json"), JSON.stringify({
    schema: "ai_studio.game.dependencies.seed.v2",
    engine: { source: "external/neotolis-engine", version: "0.1.0", compatibility: "tested" },
    features: [],
    compatibility: "tested",
  }), "utf8");
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "tests@example.invalid"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.name", "Tests"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["add", "."], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });
  return root;
}

test("new_template copies template, registers it, and refreshes VS Code files", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writeFileSync(join(root, "templates", "template", "fresh-source.txt"), "fresh\n", "utf8");

  const output = execFileSync(process.execPath, [script, "--root", root, "--id", "mobile-template"], { encoding: "utf8" });

  assert.match(output, /new template 'mobile-template' created/);
  assert.match(output, /created taskboard project: P001/);
  assert.match(output, /updated VS Code tasks\/launch/);
  assert.equal(existsSync(join(root, "templates", "mobile-template", "CMakeLists.txt")), true);
  assert.equal(existsSync(join(root, "templates", "mobile-template", "assets", "readme.txt")), true);
  assert.equal(readFileSync(join(root, "templates", "mobile-template", "fresh-source.txt"), "utf8"), "fresh\n");
  assert.equal(existsSync(join(root, "templates", "mobile-template", "src", "generated", "game.h")), false);
  assert.equal(existsSync(join(root, "templates", "mobile-template", ".ai_studio", "evidence", "private.txt")), false);
  for (const rel of ["tools/game.mjs", "tools/package_web.mjs", "tools/portal_evidence.mjs", "tools/lib/zip_store.mjs", "release/README.md", ".github/workflows/game-verify.yml", ".gitignore"]) {
    assert.equal(readFileSync(join(root, "templates", "mobile-template", rel), "utf8"), readFileSync(join(root, "templates", "template", rel), "utf8"), rel);
  }
  assert.equal(existsSync(join(root, "templates", "mobile-template", "build", "stale.obj")), false);

  assert.equal(existsSync(join(root, "templates", "mobile-template", "template.json")), true);
  assert.equal(existsSync(join(root, "templates", "mobile-template", "game-dependencies.json")), true);
  const dependencySeed = JSON.parse(readFileSync(join(root, "templates", "mobile-template", "game-dependencies.json"), "utf8"));
  assert.equal(dependencySeed.schema, "ai_studio.game.dependencies.seed.v2");
  assert.equal(dependencySeed.engine.version, "0.1.0");

  const tasks = JSON.parse(readFileSync(join(root, ".vscode", "tasks.json"), "utf8"));
  const launch = JSON.parse(readFileSync(join(root, ".vscode", "launch.json"), "utf8"));
  assert.ok(tasks.tasks.some((task) => task.label === "Template: mobile-template: build packs native debug"));
  assert.ok(launch.configurations.some((config) => config.name === "Debug Template: mobile-template (native debug)"));

  const taskboardProject = readFileSync(join(taskboardItems(root), "projects", "P001-mobile-template.md"), "utf8");
  assert.match(taskboardProject, /kind: template/);
  assert.match(taskboardProject, /target: templates\/mobile-template/);
});

test("new_template rejects existing template folders without --force", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "templates"), { recursive: true });

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "template"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /already exists/);

});

test("new_template --force fully replaces the target without disturbing sibling templates", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  execFileSync(process.execPath, [script, "--root", root, "--id", "mobile-template"], { encoding: "utf8" });
  const target = join(root, "templates", "mobile-template");
  writeFileSync(join(target, "target-only.txt"), "must stay\n", "utf8");
  writeFileSync(join(root, "templates", "template", "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.30)\n", "utf8");

  mkdirSync(join(root, "templates", "other", "assets"), { recursive: true });
  writeFileSync(join(root, "templates", "other", "template.json"), JSON.stringify({
    schema: "ai_studio.template.v1", id: "other", title: "Other", storageNamespace: "other",
  }), "utf8");
  execFileSync(process.execPath, [script, "--root", root, "--id", "mobile-template", "--force"], { encoding: "utf8" });

  assert.equal(existsSync(join(target, "target-only.txt")), false);
  assert.match(readFileSync(join(target, "CMakeLists.txt"), "utf8"), /3\.30/);
  assert.equal(existsSync(join(root, "templates", "other", "template.json")), true);
});

test("new_template --help prints usage", () => {
  const result = spawnSync(process.execPath, [script, "--help"], { encoding: "utf8" });

  assert.equal(result.status, 0);
  assert.match(result.stdout, /usage: node templates\/new_template\.mjs/);
});

test("new_template rejects unknown arguments", () => {
  const result = spawnSync(process.execPath, [script, "--wat"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /unknown argument: --wat/);
  assert.match(result.stderr, /usage: node templates\/new_template\.mjs/);
});
