// Studio config reader tests. Run:
//   node --test ai_studio/assets/canvas/tests/config.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { canvasProjectsRoot, loadStudioConfig } from "../../../core_harness/tool_lib/studio_config.mjs";

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-config-"));
  mkdirSync(join(dir, "ai_studio"), { recursive: true });
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function writeConfig(root, name, data) {
  writeFileSync(join(root, "ai_studio", name), `${JSON.stringify(data)}\n`, "utf8");
}

test("loadStudioConfig merges local override over committed main", (t) => {
  delete process.env.CANVAS_PROJECTS_ROOT;
  const root = tempRoot(t);
  writeConfig(root, "studio.config.json", { schema: "ai_studio.studio_config.v1", canvasProjectsRoot: "C:/main/root" });
  writeConfig(root, "studio.config.local.json", { canvasProjectsRoot: "C:/local/override" });

  assert.equal(loadStudioConfig(root).canvasProjectsRoot, "C:/local/override");
  assert.equal(canvasProjectsRoot(root), resolve("C:/local/override"));
});

test("canvasProjectsRoot uses committed main when no local override exists", (t) => {
  delete process.env.CANVAS_PROJECTS_ROOT;
  const root = tempRoot(t);
  writeConfig(root, "studio.config.json", { schema: "ai_studio.studio_config.v1", canvasProjectsRoot: "C:/main/only" });

  assert.equal(canvasProjectsRoot(root), resolve("C:/main/only"));
});

test("CANVAS_PROJECTS_ROOT env overrides config and needs no config file", (t) => {
  const root = tempRoot(t);
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = "C:/env/override";
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
  });

  assert.equal(canvasProjectsRoot(root), resolve("C:/env/override"));
});

test("loadStudioConfig throws a clear error when no config exists", (t) => {
  delete process.env.CANVAS_PROJECTS_ROOT;
  const root = tempRoot(t);
  assert.throws(() => loadStudioConfig(root), /missing studio config/);
});
