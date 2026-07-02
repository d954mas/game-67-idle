// Canvas CLI smoke test for the increment-2 commands. Drives the real cli.mjs as
// a child process with the projects root redirected to a temp dir (no Python).
// Run: node --test ai_studio/assets/canvas/tests/cli.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { solidPng } from "./png_fixture.mjs";

const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function run(env, ...args) {
  const stdout = execFileSync(process.execPath, [CLI, ...args], {
    env: { ...process.env, ...env },
    encoding: "utf8",
  });
  const line = stdout.trim().split("\n").filter(Boolean).at(-1);
  return JSON.parse(line);
}

test("cli create/add-image/undo/redo/history/export smoke", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(9, 6, [12, 34, 56]));

  const projectId = run(env, "create", "--title", "CLI Canvas").project.id;
  const added = run(env, "add-image", projectId, "--file", pngPath);
  const elementId = added.element.id;
  assert.equal(added.element.w, 9);

  run(env, "move", projectId, "--element", elementId, "--x", "25", "--y", "10");
  const undone = run(env, "undo", projectId);
  assert.equal(undone.project.elements[0].x, 0);
  const redone = run(env, "redo", projectId);
  assert.equal(redone.project.elements[0].x, 25);

  const history = run(env, "history", projectId);
  assert.deepEqual(history.entries.map((entry) => entry.op), ["addImage", "patchElement", "undo", "redo"]);

  const exported = run(env, "export", projectId, "--all");
  assert.equal(exported.items.length, 1);
  assert.equal(exported.manifest.schema, "ai_studio.canvas.export.v1");
});
