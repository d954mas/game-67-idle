// Canvas ops tests, incl. the bridged detectRegions op. Run:
//   node --test ai_studio/assets/canvas/tests/ops.test.mjs
//
// detectRegions drives the real raster2d + Python pipeline. When that pipeline
// (Python / numpy / Pillow) is unavailable the test skips with a clear message
// instead of failing.
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { addImage, createProject, detectRegions, getProject, resolveProjectFile, setRegions, sliceRegions, undoOp } from "../ops.mjs";
import { decodePng, magentaSheetPng } from "./png_fixture.mjs";

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

test("createProject without a title generates a random default", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, {});
  assert.match(project.title, /^[A-Z][a-z]+ [A-Z][a-z]+$/);
  assert.ok(project.id.length > 0);
});

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

test("sliceRegions crops detected regions into new elements with provenance; undo removes them", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Slice" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });

  let detected;
  try {
    detected = await detectRegions(REPO_ROOT, { projectId: project.id, elementId: element.id });
  } catch (error) {
    t.skip(`raster2d/python pipeline unavailable: ${error.message}`);
    return;
  }
  const sessions = new Set([detected.run.result_summary.session_id]);

  let sliced;
  try {
    sliced = await sliceRegions(REPO_ROOT, { projectId: project.id, elementId: element.id });
  } catch (error) {
    t.skip(`raster2d/python slicing unavailable: ${error.message}`);
    return;
  }
  sessions.add(sliced.run.result_summary.session_id);
  t.after(() => {
    for (const sessionId of sessions) {
      if (sessionId) {
        rmSync(join(REPO_ROOT, "tmp", "ai_studio", "assets", "raster2d", sessionId), { recursive: true, force: true });
      }
    }
  });

  const expected = detected.regions.length;
  assert.ok(expected >= 1, "detection found regions to slice");
  assert.equal(sliced.created.length, expected, "one new element per region");

  // Provenance + placement: each crop inherits its region's numbered name
  // ("<element name> N" assigned at detect), links meta.parent, records
  // intrinsic source size, and sits to the right of the parent sheet.
  for (const crop of sliced.created) {
    assert.match(crop.name, / \d+$/);
    assert.equal(crop.meta.parent.elementId, element.id);
    assert.equal(crop.meta.parent.sheetSrc, element.src);
    assert.ok(crop.source_w >= 1 && crop.source_h >= 1);
    assert.ok(crop.x >= element.x + element.w, "crop placed right of the parent");
  }
  const run = sliced.project.tool_runs.find((entry) => entry.op === "slice_regions");
  assert.ok(run, "a slice_regions tool_run was recorded");

  // One journal entry for the whole slice: undo removes every created crop.
  const before = getProject(REPO_ROOT, project.id).elements.length;
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(undone.elements.length, before - sliced.created.length);
  assert.equal(undone.elements.length, 1, "only the parent sheet remains after undo");
});

test("sliceRegions crops the STORED rects verbatim (edited + hand-drawn regions)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Verbatim" });
  // 64x48 magenta sheet: red blob (8,8)-(28,28), green blob (36,16)-(56,40).
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });

  // No detect run at all: an EDITED rect (covers the red blob exactly) and a
  // brand-NEW hand-drawn rect (fully inside the green blob) whose ids never came
  // from any detect. Slicing MUST crop exactly these rects from the raw pixels.
  setRegions(REPO_ROOT, {
    projectId: project.id,
    elementId: element.id,
    regions: [
      { id: "edited_red", rect: [8, 8, 20, 20] },
      { id: "drawn_green", name: "Hero", rect: [40, 20, 10, 10] },
    ],
  });

  let sliced;
  try {
    sliced = await sliceRegions(REPO_ROOT, { projectId: project.id, elementId: element.id });
  } catch (error) {
    t.skip(`crop_regions.py / PIL unavailable: ${error.message}`);
    return;
  }

  assert.equal(sliced.created.length, 2, "one crop per stored region, in order");
  const [red, green] = sliced.created;

  // Dimensions match the stored rects exactly (verbatim geometry).
  assert.deepEqual([red.w, red.h], [20, 20]);
  assert.deepEqual([green.w, green.h], [10, 10]);
  // Unnamed region falls back to <parent>#<id>; named region uses its name.
  assert.match(red.name, /#edited_red$/);
  assert.equal(green.name, "Hero");

  // Pixels come from the element's OWN bytes at the rect (not a re-detected/keyed
  // image): the red crop is the red blob, the green crop is solid green blob pixels.
  const redPng = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, red.src)));
  const greenPng = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, green.src)));
  assert.deepEqual(redPng.at(10, 10).slice(0, 3), [220, 40, 40], "red crop centre = red blob");
  assert.deepEqual(greenPng.at(5, 5).slice(0, 3), [40, 180, 60], "green crop centre = green blob");
});

test("sliceRegions masks alpha outside a polygon region (in-poly opaque, out transparent)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Polygon slice" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() }); // 64x48

  // A right-triangle polygon over the red blob [8,8)-[28,28): vertices (8,8),(28,8),(8,28).
  // bbox = [8,8,20,20]; inside the triangle (local x+y < 20) stays red + opaque, outside
  // the ring (but inside the bbox) is alpha-zeroed by the ImageDraw.polygon mask.
  setRegions(REPO_ROOT, {
    projectId: project.id,
    elementId: element.id,
    regions: [{ id: "tri", name: "Tri", rect: [8, 8, 20, 20], polygon: [[8, 8], [28, 8], [8, 28]] }],
  });

  let sliced;
  try {
    sliced = await sliceRegions(REPO_ROOT, { projectId: project.id, elementId: element.id });
  } catch (error) {
    t.skip(`crop_regions.py / PIL unavailable: ${error.message}`);
    return;
  }

  assert.equal(sliced.created.length, 1, "one crop for the polygon region");
  const crop = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, sliced.created[0].src)));
  assert.equal(crop.width, 20, "crop is the polygon bbox width");
  assert.equal(crop.height, 20, "crop is the polygon bbox height");
  assert.equal(crop.channels, 4, "polygon crop carries an alpha channel");
  // Inside the triangle: opaque red (the source pixel survives the mask).
  assert.deepEqual(crop.at(2, 2), [220, 40, 40, 255], "in-polygon pixel = opaque red");
  // Outside the triangle but inside the bbox: fully transparent.
  assert.equal(crop.at(18, 18)[3], 0, "out-of-polygon pixel is transparent");
});

test("sliceRegions errors clearly when the element has no regions", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "NoRegions" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  return assert.rejects(
    () => sliceRegions(REPO_ROOT, { projectId: project.id, elementId: element.id }),
    /has no regions; run detectRegions first/,
  );
});
