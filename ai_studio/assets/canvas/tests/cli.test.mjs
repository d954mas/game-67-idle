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

  // ops-stats parity: the CLI reports the per-op timing rollup from the journal.
  const stats = run(env, "ops-stats", projectId);
  assert.equal(stats.projectId, projectId);
  const ops = Object.fromEntries(stats.ops.map((o) => [o.op, o]));
  assert.equal(ops.addImage.count, 1);
  assert.equal(ops.patchElement.count, 1);
  assert.equal(stats.errors.count, 0);
});

test("cli group-create/move/set/assign/delete smoke (no python)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-groups-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const a = join(dir, "a.png");
  const b = join(dir, "b.png");
  writeFileSync(a, solidPng(8, 8, [10, 20, 30]));
  writeFileSync(b, solidPng(6, 6, [30, 40, 50]));

  const projectId = run(env, "create", "--title", "CLI Groups").project.id;
  const elA = run(env, "add-image", projectId, "--file", a).element.id;
  const elB = run(env, "add-image", projectId, "--file", b).element.id;
  run(env, "move", projectId, "--element", elA, "--x", "10", "--y", "10");
  run(env, "move", projectId, "--element", elB, "--x", "30", "--y", "20");

  // group-create from two elements -> a bbox-padded group owning both.
  const created = run(env, "group-create", projectId, "--name", "Main Menu", "--elements", `${elA},${elB}`);
  const groupId = created.group.id;
  assert.equal(created.group.visible, true);
  assert.equal(created.project.elements.every((e) => e.groupId === groupId), true);

  // group-move translates members (verified via show).
  const g0 = created.group;
  run(env, "group-move", projectId, "--group", groupId, "--x", String(g0.x + 40), "--y", String(g0.y + 40));
  let shown = run(env, "show", projectId).project;
  assert.equal(shown.elements.find((e) => e.id === elA).x, 50); // 10 + 40

  // group-set renames + hides.
  run(env, "group-set", projectId, "--group", groupId, "--name", "Hidden", "--visible", "false");
  shown = run(env, "show", projectId).project;
  assert.equal(shown.groups[0].name, "Hidden");
  assert.equal(shown.groups[0].visible, false);

  // group-assign none clears a member's group; group-delete then removes the
  // group AND its remaining members (elB was ungrouped first, so it survives).
  run(env, "group-assign", projectId, "--elements", elB, "--group", "none");
  assert.equal(run(env, "show", projectId).project.elements.find((e) => e.id === elB).groupId, null);
  run(env, "group-delete", projectId, "--group", groupId);
  shown = run(env, "show", projectId).project;
  assert.equal(shown.groups.length, 0);
  assert.equal(shown.elements.length, 1, "member elA deleted with the group");
  assert.equal(shown.elements[0].id, elB);
});
