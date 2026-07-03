// alphaCutout op + API/CLI parity tests. Run:
//   node --test ai_studio/assets/canvas/tests/alpha.test.mjs
//
// alphaCutout runs an element's CURRENT pixels through the image-tools matte pipeline
// (route + key_matte, reused unmodified) and swaps the element to a NEW content-addressed
// alpha PNG in ONE journaled entry; undo restores the previous src byte-exact. The
// validation tests (non-image, bad method, unknown region) need no Python; the pipeline
// tests run the real warm-worker path and skip cleanly when the studio venv is missing.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath, URL } from "node:url";

import { addImage, addText, alphaCutout, createProject, getProject, setRegions, undoOp, redoOp } from "../ops.mjs";
import { resolveProjectFile } from "../store.mjs";
import { createCanvasApi } from "../api.mjs";
import { magentaSheetPng, slicedCropPng, softGlowPng } from "./png_fixture.mjs";
import { decodePng } from "./png_fixture.mjs";

// Python tools run with cwd = repo root, so the pipeline tests must use the real root.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
// The validation tests never spawn Python, so any unused root works for them.
const UNUSED_ROOT = "C:/unused-repo-root";
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-alpha-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// ---- validation (no Python) --------------------------------------------------

test("alphaCutout rejects a non-image (text) element loudly", async (t) => {
  // REPO_ROOT so addText finds the real fonts manifest; alphaCutout throws on the type
  // check BEFORE any Python spawn, so this stays a pure-validation test.
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Alpha validate" });
  const { element } = addText(REPO_ROOT, project.id, { content: "hi" });
  await assert.rejects(
    () => alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id }),
    /is not an image/,
  );
});

test("alphaCutout rejects an unknown method and dual-plate loudly (no python)", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Alpha method" });
  const { element } = addImage(UNUSED_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  await assert.rejects(
    () => alphaCutout(UNUSED_ROOT, { projectId: project.id, elementId: element.id, method: "bogus" }),
    /unknown alpha method/,
  );
  // dual-plate is out of v1 scope (single element has one image) — a distinct loud message.
  await assert.rejects(
    () => alphaCutout(UNUSED_ROOT, { projectId: project.id, elementId: element.id, method: "dualplate" }),
    /plate PAIR|out of v1 scope/,
  );
});

test("alphaCutout rejects an unknown region id before any python spawn", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Alpha region" });
  const { element } = addImage(UNUSED_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  setRegions(UNUSED_ROOT, { projectId: project.id, elementId: element.id, regions: [{ id: "r1", rect: [8, 8, 20, 20] }] });
  await assert.rejects(
    () => alphaCutout(UNUSED_ROOT, { projectId: project.id, elementId: element.id, regions: ["r1", "ghost"] }),
    /unknown region id\(s\): ghost/,
  );
});

test("alphaCutout requires projectId and elementId", async () => {
  await assert.rejects(() => alphaCutout(UNUSED_ROOT, {}), /requires projectId/);
  await assert.rejects(() => alphaCutout(UNUSED_ROOT, { projectId: "p" }), /requires elementId/);
});

// ---- pipeline (real warm worker; skips without the studio venv) ---------------

// Try one whole-element auto cutout; return {project, element, original} or null on a
// venv/Pillow miss (the caller then t.skip's). Isolates the "does Python run here" gate.
async function tryAlpha(t, method = "auto", regions) {
  const project = createProject(REPO_ROOT, { title: "Alpha run" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  if (regions) setRegions(REPO_ROOT, { projectId: project.id, elementId: element.id, regions });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  try {
    const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method, regions: regions ? regions.map((r) => r.id) : undefined });
    return { projectId: project.id, elementId: element.id, result, original };
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`alpha pipeline unavailable: ${error.message}`);
      return null;
    }
    throw error;
  }
}

test("alphaCutout (auto) keys the whole element: alpha channel, transparent bg, opaque subject", async (t) => {
  tempProjects(t);
  const ran = await tryAlpha(t, "auto");
  if (!ran) return;
  const { projectId, elementId, result, original } = ran;

  // src swapped to a NEW file; geometry preserved.
  assert.notEqual(result.element.src, original.src, "element swapped to a new alpha file");
  assert.equal(result.element.w, original.w);
  assert.equal(result.element.h, original.h);
  // meta records the run like slice provenance.
  assert.equal(result.element.meta.alpha.method, "auto");
  assert.equal(result.element.meta.alpha.parentSrc, original.src);
  // auto ROUTED the hard-edged magenta sheet to key_matte (proves the router ran).
  assert.equal(result.element.meta.alpha.routing[0].routed, "key_matte");

  // The new PNG has an alpha channel; magenta corners transparent, a blob pixel opaque.
  const png = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, projectId, result.element.src)));
  assert.equal(png.channels, 4, "output PNG carries an alpha channel");
  assert.equal(png.at(0, 0)[3], 0, "top-left magenta background is transparent");
  assert.equal(png.at(63, 47)[3], 0, "bottom-right magenta background is transparent");
  assert.equal(png.at(18, 18)[3], 255, "inside the red blob is opaque");
});

test("alphaCutout is ONE journal entry: undo restores the previous src byte-exact, redo re-applies", async (t) => {
  tempProjects(t);
  const ran = await tryAlpha(t, "matte");
  if (!ran) return;
  const { projectId, elementId, result, original } = ran;

  // The original file is immutable — it still exists on disk with identical bytes.
  const originalAbs = resolveProjectFile(REPO_ROOT, projectId, original.src);
  assert.ok(existsSync(originalAbs), "original file kept on disk (non-destructive)");

  // Undo -> the element is restored EXACTLY to its pre-alpha state (src + no alpha meta).
  const undone = undoOp(REPO_ROOT, { projectId }).project;
  const afterUndo = undone.elements.find((el) => el.id === elementId);
  assert.deepEqual(afterUndo, original, "undo restores the exact pre-alpha element");
  assert.equal(afterUndo.src, original.src);
  assert.ok(!afterUndo.meta || !afterUndo.meta.alpha, "alpha meta removed on undo");

  // Redo -> the alpha src comes back.
  const redone = redoOp(REPO_ROOT, { projectId }).project;
  assert.equal(redone.elements.find((el) => el.id === elementId).src, result.element.src);
});

test("alphaCutout (regions) keys ONLY inside the region; outside stays untouched opaque", async (t) => {
  tempProjects(t);
  // Region [4,4,28,28] covers the red blob (8..28) plus a magenta margin; blob2 (36..56)
  // and the far magenta corner sit OUTSIDE the region and must stay original + opaque.
  const ran = await tryAlpha(t, "matte", [{ id: "r1", rect: [4, 4, 28, 28] }]);
  if (!ran) return;
  const { projectId, result } = ran;
  const png = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, projectId, result.element.src)));
  assert.equal(png.channels, 4);
  // Inside the region: magenta margin keyed transparent, blob opaque.
  assert.equal(png.at(5, 5)[3], 0, "in-region magenta margin is transparent");
  assert.equal(png.at(18, 18)[3], 255, "in-region blob is opaque");
  // Outside the region: original pixels, fully opaque (untouched).
  const outside = png.at(60, 4);
  assert.equal(outside[3], 255, "out-of-region pixel stays opaque");
  assert.deepEqual([outside[0], outside[1], outside[2]], [255, 0, 255], "out-of-region magenta is the original color");
});

test("alphaCutout (auto) refuses soft art with a CLEAN message — no Python traceback", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Alpha soft refusal" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "glow.png", bytes: softGlowPng() });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  try {
    await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "auto" });
    assert.fail("auto on soft art must refuse (dual-plate guard)");
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`alpha pipeline unavailable: ${error.message}`);
      return;
    }
    // The deliberate refusal reaches the operator as the tool's own message...
    assert.match(error.message, /dual_plate|plate PAIR/, `refusal message expected, got: ${error.message}`);
    // ...WITHOUT the worker's crash formatting (raw traceback in the UI toast = bug).
    assert.ok(!/Traceback|File "/.test(error.message), `traceback leaked to the operator: ${error.message}`);
  }
  // The refusal changed nothing: same src, no alpha meta, nothing to undo.
  const after = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  assert.deepEqual(after, original, "element untouched after a refusal");
});

test("alphaCutout never resurrects pixels hidden by a polygon slice", async (t) => {
  tempProjects(t);
  // slicedCropPng: hidden orange garbage (alpha 0) in the top-left corner, like the
  // RGB a polygon slice leaves under transparency. After alpha: garbage STAYS hidden,
  // the magenta bg is keyed out, the subject blob stays opaque.
  const project = createProject(REPO_ROOT, { title: "Alpha sliced" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "crop.png", bytes: slicedCropPng() });
  let result;
  try {
    result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "matte" });
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`alpha pipeline unavailable: ${error.message}`);
      return;
    }
    throw error;
  }
  const png = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, result.element.src)));
  assert.equal(png.channels, 4);
  assert.equal(png.at(5, 5)[3], 0, "hidden garbage pixel stays transparent (not resurrected)");
  assert.equal(png.at(19, 15)[3], 0, "hidden garbage corner edge stays transparent");
  assert.equal(png.at(34, 26)[3], 255, "subject blob stays opaque");
  assert.equal(png.at(60, 44)[3], 0, "magenta background is keyed out");
  // The key was estimated from the VISIBLE background, not skewed by hidden orange.
  assert.deepEqual(result.element.meta.alpha.key_color, [255, 0, 255]);
});

// ---- API + CLI parity (real pipeline; skips without the venv) -----------------

test("alpha API route and CLI drive the same op", async (t) => {
  const dir = tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Alpha parity" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });

  // API: POST /alpha { elementId, method } — mirror how the page calls it.
  const handler = createCanvasApi(REPO_ROOT);
  const captured = { status: 0, body: null };
  const res = {
    writeHead(status) { captured.status = status; return this; },
    end(payload) { captured.body = payload ? JSON.parse(payload) : null; },
  };
  const req = mockReq("POST", { elementId: element.id, method: "matte" });
  await handler(req, res, new URL(`http://x/api/canvas/projects/${project.id}/alpha`));
  if (captured.status === 400 && /venv|Pillow|module/i.test(String(captured.body && captured.body.error))) {
    t.skip(`alpha pipeline unavailable: ${captured.body.error}`);
    return;
  }
  assert.equal(captured.status, 200, `API alpha 200 (got ${captured.status}: ${JSON.stringify(captured.body)})`);
  assert.equal(captured.body.method, "matte");

  // CLI: alpha <id> --element <eid> --method matte — same ops layer, different transport.
  const before = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id).src;
  const out = execFileSync("node", [CLI, "alpha", project.id, "--element", element.id, "--method", "matte"], {
    env: { ...process.env, CANVAS_PROJECTS_ROOT: dir },
    encoding: "utf8",
  });
  const parsed = JSON.parse(out.trim().split("\n").pop());
  assert.equal(parsed.method, "matte");
  // A matte re-run of the already-keyed pixels is a valid op; src stays a files/ ref.
  assert.match(parsed.element.src, /^files\//);
  assert.notEqual(parsed.element.src, undefined);
  assert.ok(before, "had a src before the CLI run");
});

// Minimal request mock (a readable-body stub) for the API handler.
function mockReq(method, bodyObject) {
  const body = JSON.stringify(bodyObject);
  return {
    method,
    setEncoding() {},
    on(event, handler) {
      if (event === "data") handler(body);
      if (event === "end") handler();
      return this;
    },
  };
}
