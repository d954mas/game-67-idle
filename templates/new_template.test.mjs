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
  writeFileSync(join(root, "templates", "template", "src", "generated", "game.h"), "#pragma once\n", "utf8");
  writeFileSync(join(root, "templates", "template", "build", "stale.obj"), "generated\n", "utf8");
  return root;
}

test("new_template copies template, registers it, and refreshes VS Code files", (t) => {
  const root = tempRepo();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const output = execFileSync(process.execPath, [script, "--root", root, "--id", "mobile-template"], { encoding: "utf8" });

  assert.match(output, /new template 'mobile-template' created/);
  assert.match(output, /created taskboard project: P001/);
  assert.match(output, /updated VS Code tasks\/launch/);
  assert.equal(existsSync(join(root, "templates", "mobile-template", "CMakeLists.txt")), true);
  assert.equal(existsSync(join(root, "templates", "mobile-template", "assets", "readme.txt")), true);
  assert.equal(existsSync(join(root, "templates", "mobile-template", "src", "generated", "game.h")), true);
  assert.equal(existsSync(join(root, "templates", "mobile-template", "build", "stale.obj")), false);

  const registry = JSON.parse(readFileSync(join(root, "templates", "templates.json"), "utf8"));
  assert.deepEqual(registry.templates, [
    {
      id: "mobile-template",
      title: "mobile-template",
      folder: "templates/mobile-template",
      assets: "templates/mobile-template/assets",
      status: "active",
    },
    {
      id: "template",
      title: "Template",
      folder: "templates/template",
      assets: "templates/template/assets",
      status: "active",
    },
  ]);

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
  mkdirSync(dirname(join(root, "templates", "templates.json")), { recursive: true });

  const result = spawnSync(process.execPath, [script, "--root", root, "--id", "template"], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /already exists/);
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
