import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "workflow-guard-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function writeBase(root, { active = true } = {}) {
  mkdirSync(join(root, "tasks", "active"), { recursive: true });
  mkdirSync(join(root, "gamedesign", "projects", "ember-test", "data"), { recursive: true });
  mkdirSync(join(root, "src"), { recursive: true });
  writeFileSync(
    join(root, "AGENTS.md"),
    "# AGENTS\n\n## Project\n\n- Shared AI Studio harness rules.\n",
    "utf8",
  );
  writeFileSync(
    join(root, "GAME_PROJECT.md"),
    active
      ? "# GAME_PROJECT\n\n## Active Game\n\nStatus: active\n\n- Game id: `ember-test`\n- Game folder: `gamedesign/projects/ember-test/`\n"
      : "# GAME_PROJECT\n\n## Active Game\n\nStatus: none\n\nThere is no active game concept.\n",
    "utf8",
  );
  writeFileSync(join(root, "src", "clean_seed_main.c"), "int main(void){return 0;}\n", "utf8");
}

function writeTask(root, id, fields = {}, body = "") {
  const title = fields.title || `Task ${id}`;
  const task = `---
id: ${id}
title: ${title}
status: ${fields.status || "doing"}
epic: ${fields.epic || "E001"}
priority: ${fields.priority || "P1"}
tags: [${(fields.tags || []).join(", ")}]
created: 2026-06-21
updated: 2026-06-21
---

## What

${body || title}

## Done when

- [ ] done

## Open questions

## Log
`;
  writeFileSync(join(root, "tasks", "active", `${id}-${title.toLowerCase().replace(/[^a-z0-9]+/g, "-")}.md`), task, "utf8");
}

function run(root, args = []) {
  return spawnSync(process.execPath, ["tools/game_context/workflow_guard.mjs", "--root", root, ...args], {
    cwd: process.cwd(),
    encoding: "utf8",
    stdio: "pipe",
  });
}

test("workflow guard skips clean seed with no active game concept", (t) => {
  const root = tempRoot(t);
  writeBase(root, { active: false });
  writeTask(root, "T0001", { title: "Pipeline cleanup", tags: ["pipeline"] }, "Improve reusable tooling.");

  const result = run(root);

  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /skipped \(no active game concept\)/);
});

test("workflow guard blocks feature expansion while lead rejection is unresolved", (t) => {
  const root = tempRoot(t);
  writeBase(root);
  writeTask(root, "T0001", { title: "Fix rejected visual target", tags: ["visual", "lead-rejection"] }, "Lead rejected the current screen.");
  writeTask(root, "T0002", { title: "Add more quests", tags: ["content"] }, "Add quests, enemies, rewards, and new locations.");

  const result = run(root);

  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /feature\/content expansion is active while lead rejection is unresolved/);
  assert.match(result.stderr, /T0002/);
  assert.match(result.stderr, /T0001/);
});

test("workflow guard blocks implementation when reference grounding is explicitly not ready", (t) => {
  const root = tempRoot(t);
  writeBase(root);
  writeFileSync(
    join(root, "gamedesign", "projects", "ember-test", "data", "core_loop.json"),
    `${JSON.stringify({ reference_grounding: { status: "not_ready_for_implementation" } }, null, 2)}\n`,
    "utf8",
  );
  writeTask(root, "T0003", { title: "Build combat scene", tags: ["runtime", "gameplay"] }, "Implement combat runtime and playable scene.");

  const result = run(root);

  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /reference_grounding\.status is "not_ready_for_implementation"/);
  assert.match(result.stderr, /T0003/);
});

test("workflow guard flags monolithic active runtime unless recovery task exists", (t) => {
  const root = tempRoot(t);
  writeBase(root);
  writeFileSync(join(root, "src", "clean_seed_main.c"), Array.from({ length: 110 }, (_, i) => `int x${i};`).join("\n"), "utf8");
  writeTask(root, "T0004", { title: "Build first playable", tags: ["native-first"] }, "Implement the first playable.");

  const blocked = run(root, ["--runtime-max-lines", "100"]);

  assert.notEqual(blocked.status, 0);
  assert.match(blocked.stderr, /src\/clean_seed_main\.c is 110 lines/);
  assert.match(blocked.stderr, /no active architecture\/decomposition task/);

  writeTask(root, "T0005", { title: "Decompose runtime systems", tags: ["architecture", "decomposition"] }, "Split runtime into systems, entities, and files.");
  const allowed = run(root, ["--runtime-max-lines", "100"]);

  assert.equal(allowed.status, 0, allowed.stderr);
  assert.match(allowed.stdout, /passed for active concept ember-test/);
});
