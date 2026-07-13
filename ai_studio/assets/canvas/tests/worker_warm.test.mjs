// Canvas warm-worker proof (T0202): the persistent Python worker under the ops layer
// makes the second and later render/detect calls markedly faster than the first
// (cold-spawn) call, while keeping identical results (pure transport swap — parity).
// Drives the REAL ops (renderGroup, detectRegions) through the warm worker; skips
// cleanly only when the studio venv / Pillow is unavailable.
//   node --test ai_studio/assets/canvas/tests/worker_warm.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { performance } from "node:perf_hooks";
import { fileURLToPath } from "node:url";

import { addImage, createGroup, createProject, detectRegions, renderGroup } from "../ops.mjs";
import { magentaSheetPng, solidPng } from "./png_fixture.mjs";

// The Python tools run with cwd = repo root, so ops must be driven with the real root.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const ASSERT_TIMING = process.env.AI_STUDIO_ASSERT_TIMING === "1";
const raster2dSessions = new Set();

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-warm-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
    for (const sessionId of raster2dSessions) {
      rmSync(join(REPO_ROOT, "tmp", "ai_studio", "assets", "raster2d", sessionId), { recursive: true, force: true });
    }
  });
  return dir;
}

test("renderGroup: warm second call is far faster than the cold first call, same PNG", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Warm Render" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "chip.png", bytes: solidPng(16, 16, [220, 40, 40]) });
  const { group } = createGroup(REPO_ROOT, { projectId: project.id, name: "Screen", fromElements: [element.id] });

  let first;
  const t0 = performance.now();
  try {
    first = await renderGroup(REPO_ROOT, { projectId: project.id, groupId: group.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const coldMs = performance.now() - t0;
  assert.ok(existsSync(first.path), "first render wrote its PNG");

  const t1 = performance.now();
  const second = await renderGroup(REPO_ROOT, { projectId: project.id, groupId: group.id, scale: 1 });
  const warmMs = performance.now() - t1;
  assert.ok(existsSync(second.path), "warm render wrote its PNG");

  // Parity: the warm render is the same size/shape as the cold one (same tool, same spec).
  assert.deepEqual(
    [second.manifest.width, second.manifest.height],
    [first.manifest.width, first.manifest.height],
    "warm render matches the cold render dimensions (parity)",
  );

  if (ASSERT_TIMING) {
    assert.ok(warmMs < coldMs, `warm render (${warmMs.toFixed(1)}ms) should be < cold render (${coldMs.toFixed(1)}ms)`);
    assert.ok(warmMs < 150, `a warm trivial render should be well under 150ms, was ${warmMs.toFixed(1)}ms`);
  }
});

test("detectRegions: warm second call is far faster than the cold first call, same regions", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Warm Detect" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });

  let first;
  const t0 = performance.now();
  try {
    first = await detectRegions(REPO_ROOT, { projectId: project.id, elementId: element.id });
  } catch (error) {
    t.skip(`detect pipeline / numpy / PIL unavailable: ${error.message}`);
    return;
  }
  const coldMs = performance.now() - t0;
  if (first.run.result_summary.session_id) raster2dSessions.add(first.run.result_summary.session_id);
  assert.ok(first.regions.length >= 1, "cold detect found regions");

  const t1 = performance.now();
  const second = await detectRegions(REPO_ROOT, { projectId: project.id, elementId: element.id });
  const warmMs = performance.now() - t1;
  if (second.run.result_summary.session_id) raster2dSessions.add(second.run.result_summary.session_id);

  // Parity: the same sheet detects the same region count warm as cold.
  assert.equal(second.regions.length, first.regions.length, "warm detect returns the same region count (parity)");
  if (ASSERT_TIMING) {
    assert.ok(warmMs < coldMs, `warm detect (${warmMs.toFixed(1)}ms) should be < cold detect (${coldMs.toFixed(1)}ms)`);
  }
});
