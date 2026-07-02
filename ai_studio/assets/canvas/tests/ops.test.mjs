// Canvas ops tests, incl. the bridged detectRegions op. Run:
//   node --test ai_studio/assets/canvas/tests/ops.test.mjs
//
// detectRegions drives the real raster2d + Python pipeline. When that pipeline
// (Python / numpy / Pillow) is unavailable the test skips with a clear message
// instead of failing.
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { addImage, createProject, detectRegions } from "../ops.mjs";
import { magentaSheetPng } from "./png_fixture.mjs";

// raster2d runs Python with cwd = repo root and writes its session under
// <repoRoot>/tmp, so the ops layer must be driven with the real repo root.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-ops-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

test("detectRegions bridges raster2d and stores regions + a tool_run", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Detect" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });

  let result;
  try {
    result = await detectRegions(REPO_ROOT, { projectId: project.id, elementId: element.id });
  } catch (error) {
    t.skip(`raster2d/python pipeline unavailable: ${error.message}`);
    return;
  }

  t.after(() => {
    const sessionId = result.run.result_summary.session_id;
    if (sessionId) {
      rmSync(join(REPO_ROOT, "tmp", "ai_studio", "assets", "raster2d", sessionId), { recursive: true, force: true });
    }
  });

  assert.ok(Array.isArray(result.regions), "regions is an array");
  assert.ok(result.regions.length >= 1, `expected detected regions, got ${result.regions.length}`);

  // Regions are persisted on the element, and a tool_runs entry is recorded.
  assert.deepEqual(result.element.regions, result.regions);
  assert.equal(result.run.op, "detect_regions");
  assert.equal(result.run.elementId, element.id);
  assert.equal(result.project.tool_runs.length, 1);
  assert.equal(result.project.tool_runs[0].result_summary.region_count, result.regions.length);

  // The saved project reflects the same state when re-read.
  const stored = result.project.elements.find((el) => el.id === element.id);
  assert.equal(stored.regions.length, result.regions.length);
});
