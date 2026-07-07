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
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath, URL } from "node:url";

import { addImage, addText, alphaCutout, alphaDualPlate, createProject, getProject, isCorridorKeyGreenKey, isCorridorKeyMagentaKey, setRegions, undoOp, redoOp } from "../ops.mjs";
import { resolveProjectFile } from "../store.mjs";
import { createCanvasApi } from "../api.mjs";
import { darkBgPng, dualPlatePairPng, encodePng, magentaSheetPng, slicedCropPng, softGlowPng } from "./png_fixture.mjs";
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

// ---- batch (T0230: multi-selection, ONE journal entry / undo) -----------------

test("alphaCutout batch (elementIds) keys 2 images in ONE journal entry; one undo restores both byte-exact", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Alpha batch undo" });
  const { element: e1 } = addImage(REPO_ROOT, project.id, { name: "sheet1.png", bytes: magentaSheetPng() });
  const { element: e2 } = addImage(REPO_ROOT, project.id, { name: "sheet2.png", bytes: magentaSheetPng() });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq; // 2 addImage entries
  const originals = getProject(REPO_ROOT, project.id).elements.map((element) => ({ ...element }));

  let result;
  try {
    result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementIds: [e1.id, e2.id], method: "matte" });
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`alpha pipeline unavailable: ${error.message}`);
      return;
    }
    throw error;
  }

  // Exactly ONE new journal entry for the WHOLE batch (not one per element).
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "batch is one journal entry");
  assert.equal(result.count, 2);
  assert.equal(result.elements.length, 2);
  for (const element of result.elements) {
    const original = originals.find((item) => item.id === element.id);
    assert.notEqual(element.src, original.src, `element ${element.id} swapped to a new alpha file`);
    assert.equal(element.meta.alpha.method, "matte");
    assert.equal(element.meta.alpha.parentSrc, original.src);
  }

  // ONE undo restores BOTH elements byte-exact (src + no alpha meta), in one step.
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  for (const original of originals) {
    const restored = undone.elements.find((element) => element.id === original.id);
    assert.deepEqual(restored, original, `undo restores element ${original.id} exactly`);
  }
  assert.equal(undone.history_seq, seqBefore);

  // Redo re-applies both swaps together.
  const redone = redoOp(REPO_ROOT, { projectId: project.id }).project;
  for (const element of result.elements) {
    assert.equal(redone.elements.find((item) => item.id === element.id).src, element.src);
  }
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

test("alphaCutout batch is atomic: one refusing element rejects the WHOLE batch, nothing mutated", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Alpha batch refusal" });
  const { element: good } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  const { element: soft } = addImage(REPO_ROOT, project.id, { name: "glow.png", bytes: softGlowPng() });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const originals = getProject(REPO_ROOT, project.id).elements.map((element) => ({ ...element }));

  try {
    await alphaCutout(REPO_ROOT, { projectId: project.id, elementIds: [good.id, soft.id], method: "auto" });
    assert.fail("a batch containing a soft-art element under auto must refuse (dual-plate guard)");
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`alpha pipeline unavailable: ${error.message}`);
      return;
    }
    // The refusal reaches the operator as the tool's own message (that element's message),
    // never a partial-batch summary or a raw traceback.
    assert.match(error.message, /dual_plate|plate PAIR/, `refusal message expected, got: ${error.message}`);
    assert.ok(!/Traceback|File "/.test(error.message), `traceback leaked to the operator: ${error.message}`);
  }

  // Atomic: NO journal entry, and NEITHER element changed — including `good`, which keyed
  // successfully before `soft` refused (the whole batch is thrown away, not partially kept).
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore, "no journal entry on a rejected batch");
  for (const original of originals) {
    assert.deepEqual(
      after.elements.find((element) => element.id === original.id),
      original,
      `element ${original.id} untouched after the batch refusal`,
    );
  }
});

// ---- batch validation (no Python) ----------------------------------------------

test("alphaCutout batch rejects regions (regions stay single-element) before any python spawn", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Alpha batch regions" });
  const { element: e1 } = addImage(UNUSED_ROOT, project.id, { name: "a.png", bytes: magentaSheetPng() });
  const { element: e2 } = addImage(UNUSED_ROOT, project.id, { name: "b.png", bytes: magentaSheetPng() });
  await assert.rejects(
    () => alphaCutout(UNUSED_ROOT, { projectId: project.id, elementIds: [e1.id, e2.id], regions: ["r1"] }),
    /does not support regions|stay single-element/,
  );
});

test("alphaCutout batch rejects an unknown/non-image element before any write", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Alpha batch bad element" });
  const { element: img } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: magentaSheetPng() });
  const { element: text } = addText(REPO_ROOT, project.id, { content: "hi" });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;

  await assert.rejects(
    () => alphaCutout(REPO_ROOT, { projectId: project.id, elementIds: [img.id, "el_missing"] }),
    /element not found/,
  );
  await assert.rejects(
    () => alphaCutout(REPO_ROOT, { projectId: project.id, elementIds: [img.id, text.id] }),
    /is not an image/,
  );
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore, "no journal entry on rejected batch input");
});

test("alphaCutout rejects elementId and elementIds together, and an empty elementIds array", async () => {
  await assert.rejects(
    () => alphaCutout(UNUSED_ROOT, { projectId: "p", elementId: "e1", elementIds: ["e1", "e2"] }),
    /either elementId or elementIds/,
  );
  await assert.rejects(
    () => alphaCutout(UNUSED_ROOT, { projectId: "p", elementIds: [] }),
    /non-empty elementIds array/,
  );
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

test("alpha API and CLI both drive elementIds batches, one journal entry each (parity)", async (t) => {
  const dir = tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Alpha batch parity" });
  const { element: e1 } = addImage(REPO_ROOT, project.id, { name: "sheet1.png", bytes: magentaSheetPng() });
  const { element: e2 } = addImage(REPO_ROOT, project.id, { name: "sheet2.png", bytes: magentaSheetPng() });
  const seqBeforeApi = getProject(REPO_ROOT, project.id).history_seq; // 2 addImage entries

  // API: POST /alpha { elementIds, method } — mirror how the multi-selection inspector calls it.
  const handler = createCanvasApi(REPO_ROOT);
  const captured = { status: 0, body: null };
  const res = {
    writeHead(status) { captured.status = status; return this; },
    end(payload) { captured.body = payload ? JSON.parse(payload) : null; },
  };
  const req = mockReq("POST", { elementIds: [e1.id, e2.id], method: "matte" });
  await handler(req, res, new URL(`http://x/api/canvas/projects/${project.id}/alpha`));
  if (captured.status === 400 && /venv|Pillow|module/i.test(String(captured.body && captured.body.error))) {
    t.skip(`alpha pipeline unavailable: ${captured.body.error}`);
    return;
  }
  assert.equal(captured.status, 200, `API alpha 200 (got ${captured.status}: ${JSON.stringify(captured.body)})`);
  assert.equal(captured.body.method, "matte");
  assert.equal(captured.body.count, 2);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBeforeApi + 1, "API batch is one journal entry");

  // CLI: alpha <id> --elements e1,e2 --method matte — same ops layer, different transport.
  // (A matte re-run of already-keyed pixels is a valid op — see the single-element parity test.)
  const seqBeforeCli = getProject(REPO_ROOT, project.id).history_seq;
  const out = execFileSync("node", [CLI, "alpha", project.id, "--elements", `${e1.id},${e2.id}`, "--method", "matte"], {
    env: { ...process.env, CANVAS_PROJECTS_ROOT: dir },
    encoding: "utf8",
  });
  const parsed = JSON.parse(out.trim().split("\n").pop());
  assert.equal(parsed.method, "matte");
  assert.equal(parsed.count, 2);
  assert.match(parsed.elements[0].src, /^files\//);
  assert.match(parsed.elements[1].src, /^files\//);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBeforeCli + 1, "CLI batch is one journal entry too");
});

// ---- corridorkey (T0261: neural GREEN-screen matte; T0262: magenta shim + regions) ----
//
// The real path spends a ~13-16s cold GPU model load per call, so the suite NEVER runs it: the
// CK inference is an injectable seam (`corridorKey` on the op — the alphaDualPlateGenerate
// `generator` precedent) that tests fake, and the pure green/magenta classifiers are unit-tested
// directly. ONE live GPU smoke exists OUT of the suite (tests/live/ck_smoke.mjs), covering both
// the native-green and magenta-shim paths.

// A fake CorridorKey invocation: writes a valid RGBA PNG to outAbs and returns a canvas alpha
// report, so the op's src-swap/provenance/undo/journal contract is exercised with NO GPU/venv.
// The key gate lives inside the REAL invoke (route_cutout + isCorridorKeyGreenKey/
// isCorridorKeyMagentaKey); this fake bypasses it, so the source pixels are irrelevant here (the
// gate is unit-tested separately).
function fakeCorridorKey(bytes = softGlowPng()) {
  const calls = { n: 0 };
  const invoke = (root, { outAbs }) => {
    calls.n += 1;
    writeFileSync(outAbs, bytes);
    return {
      schema: "ai_studio.canvas.alpha_cutout_report.v1",
      method: "corridorkey",
      tool: "corridorkey",
      key_color: [0, 255, 0],
      screen_color: "green",
      commit: "test-commit-97e55a4",
      license: "CC-BY-NC-SA-4.0 (test)",
      settings: { image_size: 2048 },
      wall_seconds: 0.01,
      per_frame_seconds: 0.01,
      region_count: 0,
      regions: [{ id: "*element*", method: "corridorkey", routed: "corridorkey", key: [0, 255, 0] }],
    };
  };
  return { invoke, calls };
}

// Same shape, but simulates what the REAL invoke produces on a magenta key: report.shim carries
// "hue180" (the magenta shim ran) and key_color reflects the magenta border. Used to test the
// provenance PLUMBING (report.shim -> element.meta.alpha.shim) without needing a real GPU/venv —
// the real hue-rotation math is unit-tested in ck_pixel_ops_test.py and exercised end-to-end by
// the live GPU smoke (magenta fixture) in tests/live/ck_smoke.mjs.
function fakeCorridorKeyMagentaShim(bytes = softGlowPng()) {
  const calls = { n: 0 };
  const invoke = (root, { outAbs }) => {
    calls.n += 1;
    writeFileSync(outAbs, bytes);
    return {
      schema: "ai_studio.canvas.alpha_cutout_report.v1",
      method: "corridorkey",
      tool: "corridorkey",
      key_color: [255, 0, 255],
      screen_color: "green",
      shim: "hue180",
      commit: "test-commit-97e55a4",
      license: "CC-BY-NC-SA-4.0 (test)",
      settings: { image_size: 2048 },
      wall_seconds: 0.01,
      per_frame_seconds: 0.01,
      region_count: 0,
      regions: [{ id: "*element*", method: "corridorkey", routed: "corridorkey", key: [255, 0, 255] }],
    };
  };
  return { invoke, calls };
}

test("corridorkey green classifier: isCorridorKeyGreenKey accepts green, rejects magenta/neutral/blue/olive", () => {
  assert.equal(isCorridorKeyGreenKey([0, 255, 0]), true, "pure green screen");
  assert.equal(isCorridorKeyGreenKey([0, 200, 40]), true, "green screen with mild tint");
  assert.equal(isCorridorKeyGreenKey([255, 0, 255]), false, "magenta -> not green");
  assert.equal(isCorridorKeyGreenKey([60, 60, 66]), false, "neutral gray -> not green");
  assert.equal(isCorridorKeyGreenKey([0, 0, 255]), false, "blue -> not green");
  assert.equal(isCorridorKeyGreenKey([120, 120, 0]), false, "olive -> g not dominant over r");
  assert.equal(isCorridorKeyGreenKey([]), false, "malformed key -> not green");
});

// Magenta classifier truth table (T0262) — mirrors trick_run.py's magenta_hint dominance rule
// EXACTLY (min(r,b) - g > 40 AND min(r,b) > 110), the mirror image of the green rule above.
test("corridorkey magenta classifier: isCorridorKeyMagentaKey accepts magenta, rejects green/neutral/blue/dim/insufficient-dominance", () => {
  assert.equal(isCorridorKeyMagentaKey([255, 0, 255]), true, "pure magenta screen");
  assert.equal(isCorridorKeyMagentaKey([200, 40, 200]), true, "magenta screen with mild tint");
  assert.equal(isCorridorKeyMagentaKey([0, 255, 0]), false, "green -> not magenta");
  assert.equal(isCorridorKeyMagentaKey([60, 66, 60]), false, "neutral gray -> not magenta");
  assert.equal(isCorridorKeyMagentaKey([0, 0, 255]), false, "blue -> not magenta (r not dominant)");
  assert.equal(isCorridorKeyMagentaKey([100, 0, 100]), false, "dim magenta below the MIN floor");
  assert.equal(isCorridorKeyMagentaKey([130, 100, 130]), false, "insufficient dominance over green");
  assert.equal(isCorridorKeyMagentaKey([]), false, "malformed key -> not magenta");
});

test("corridorkey is in the method table; the CK seam runs ONLY for method=corridorkey (auto/matte never yield it)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "CK method table" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: magentaSheetPng() });
  const ck = fakeCorridorKey();

  // Explicit corridorkey: the CK seam runs exactly once and provenance records method=corridorkey.
  const res = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "corridorkey", corridorKey: ck.invoke });
  assert.equal(ck.calls.n, 1, "CK seam invoked exactly once for method=corridorkey");
  assert.equal(res.method, "corridorkey");
  assert.equal(res.element.meta.alpha.method, "corridorkey");

  // auto/matte must NEVER reach the CK seam (they route to key_matte by construction). We pass the
  // SAME spy and assert it is never called — the op outcome (success or a venv miss) is irrelevant.
  for (const method of ["auto", "matte"]) {
    const p = createProject(REPO_ROOT, { title: `CK not-${method}` });
    const { element: el } = addImage(REPO_ROOT, p.id, { name: "b.png", bytes: magentaSheetPng() });
    try {
      await alphaCutout(REPO_ROOT, { projectId: p.id, elementId: el.id, method, corridorKey: ck.invoke });
    } catch {
      // venv miss / key_matte refusal — does not matter; the point is the spy below.
    }
    assert.equal(ck.calls.n, 1, `CK seam not called for method=${method}`);
  }
});

test("corridorkey (faked GPU run) swaps src + records provenance/timings; ONE undo restores byte-exact, redo re-applies", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "CK provenance" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "green.png", bytes: magentaSheetPng() });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const ck = fakeCorridorKey();

  const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "corridorkey", corridorKey: ck.invoke });
  assert.equal(ck.calls.n, 1);
  assert.notEqual(result.element.src, original.src, "src swapped to a new alpha file");
  assert.equal(result.element.w, original.w, "geometry preserved (w)");
  assert.equal(result.element.h, original.h, "geometry preserved (h)");
  const meta = result.element.meta.alpha;
  assert.equal(meta.method, "corridorkey");
  assert.equal(meta.tool, "corridorkey");
  assert.equal(meta.parentSrc, original.src);
  assert.deepEqual(meta.key_color, [0, 255, 0]);
  assert.equal(meta.screen_color, "green");
  assert.equal(meta.shim, undefined, "no shim recorded for the native-green path");
  assert.ok(meta.commit && meta.license, "commit + licence recorded for provenance");
  assert.equal(typeof meta.timings.wall_seconds, "number", "wall timing recorded");
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore + 1, "corridorkey keying is one journal entry");

  // Original file kept (non-destructive); undo restores byte-exact (src + no alpha meta); redo re-applies.
  assert.ok(existsSync(resolveProjectFile(REPO_ROOT, project.id, original.src)), "original file kept on disk");
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.deepEqual(undone.elements.find((el) => el.id === element.id), original, "undo restores the exact pre-alpha element");
  const redone = redoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((el) => el.id === element.id).src, result.element.src, "redo re-applies the alpha src");
});

test("corridorkey batch (faked) keys 2 images in ONE journal entry; one undo restores both byte-exact", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "CK batch" });
  const { element: e1 } = addImage(REPO_ROOT, project.id, { name: "g1.png", bytes: magentaSheetPng() });
  const { element: e2 } = addImage(REPO_ROOT, project.id, { name: "g2.png", bytes: magentaSheetPng() });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const originals = getProject(REPO_ROOT, project.id).elements.map((el) => ({ ...el }));
  const ck = fakeCorridorKey();

  const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementIds: [e1.id, e2.id], method: "corridorkey", corridorKey: ck.invoke });
  assert.equal(ck.calls.n, 2, "CK seam runs once per element");
  assert.equal(result.count, 2);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore + 1, "batch is one journal entry");
  for (const el of result.elements) assert.equal(el.meta.alpha.method, "corridorkey");
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  for (const original of originals) {
    assert.deepEqual(undone.elements.find((el) => el.id === original.id), original, `undo restores element ${original.id}`);
  }
});

test("corridorkey magenta shim (faked GPU run) records meta.alpha.shim=\"hue180\" provenance; one undo restores byte-exact, redo re-applies", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "CK magenta shim provenance" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "magenta.png", bytes: magentaSheetPng() });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const ck = fakeCorridorKeyMagentaShim();

  const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "corridorkey", corridorKey: ck.invoke });
  assert.equal(ck.calls.n, 1);
  const meta = result.element.meta.alpha;
  assert.equal(meta.method, "corridorkey");
  assert.equal(meta.tool, "corridorkey");
  assert.deepEqual(meta.key_color, [255, 0, 255], "magenta border key recorded");
  assert.equal(meta.screen_color, "green", "CK itself always runs the green checkpoint (the shim did the color work)");
  assert.equal(meta.shim, "hue180", "magenta shim provenance recorded");
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore + 1, "corridorkey keying is one journal entry");

  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.deepEqual(undone.elements.find((el) => el.id === element.id), original, "undo restores the exact pre-alpha element");
  const redone = redoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((el) => el.id === element.id).src, result.element.src, "redo re-applies the shimmed alpha src");
  assert.equal(redone.elements.find((el) => el.id === element.id).meta.alpha.shim, "hue180", "redo keeps the shim provenance");
});

test("corridorkey regions (faked GPU run, real compose_regions python) composes the whole-frame CK result INTO the region; outside the region the source is untouched; one undo", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "CK regions compose" });
  // magentaSheetPng is 64x48; region [40,4,8,8] sits in the flat magenta background, clear of
  // both blobs ([8,8]-[28,28] and [36,16]-[56,40]) — so the untouched-outside check at (2,2) and
  // the composed-inside check at (44,8) both land on unambiguous fixture pixels.
  const { element } = addImage(REPO_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  setRegions(REPO_ROOT, { projectId: project.id, elementId: element.id, regions: [{ id: "r1", rect: [40, 4, 8, 8] }] });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;

  // The FAKED seam stands in for the ~15s GPU call only; compose_regions is the REAL python
  // helper (ck_pixel_ops.py), so this exercises the real region_mask/paste contract end to end.
  const fakeFrame = encodePng(64, 48, () => [10, 20, 30, 128], { alpha: true });
  const ck = fakeCorridorKey(fakeFrame);

  const result = await alphaCutout(REPO_ROOT, {
    projectId: project.id,
    elementId: element.id,
    method: "corridorkey",
    regions: ["r1"],
    corridorKey: ck.invoke,
  });
  assert.equal(ck.calls.n, 1, "CK seam invoked exactly once (whole-frame, never per-region)");
  assert.equal(result.element.meta.alpha.method, "corridorkey");
  assert.deepEqual(result.element.meta.alpha.regions, ["r1"], "provenance names the region");
  assert.deepEqual(result.element.meta.alpha.routing.map((r) => r.id), ["r1"], "routing entry names the region");
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore + 1, "region-scoped corridorkey keying is one journal entry");

  const png = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, result.element.src)));
  assert.deepEqual(png.at(44, 8), [10, 20, 30, 128], "inside the region: the CK result pixel");
  assert.deepEqual(png.at(2, 2), [255, 0, 255, 255], "outside the region: the original magenta pixel, untouched");

  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.deepEqual(undone.elements.find((el) => el.id === element.id), original, "undo restores the exact pre-alpha element");
});

test("corridorkey (real key gate) refuses a NEUTRAL key with a clean message BEFORE any GPU call (skips without the studio venv)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "CK neutral reject" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "neutral.png", bytes: darkBgPng() });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  // No injected seam: the DEFAULT invoke runs the real key gate (route_cutout, repo venv, NO GPU).
  // T0262: green (native) and magenta (hue180 shim) are both supported now — a NEUTRAL border key
  // (neither) must still be rejected before the CorridorKey model is ever loaded.
  try {
    await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "corridorkey" });
    assert.fail("corridorkey on a neutral key must refuse (neither green nor magenta)");
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`corridorkey key gate unavailable (studio venv): ${error.message}`);
      return;
    }
    assert.match(error.message, /neither/i, `neutral refusal expected, got: ${error.message}`);
    assert.ok(!/Traceback|File "/.test(error.message), `traceback leaked to the operator: ${error.message}`);
  }
  assert.deepEqual(getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id), original, "element untouched after a refusal");
});

// ---- vitmatte + birefnet (T0335: two EXPLICIT-only neural methods; bench 2026-07-07) ----
//
// Both real paths spend real GPU/CPU seconds (+ a one-time model download), so the suite NEVER runs
// them: each inference is an injectable seam (`vitmatte` / `birefnet` on the op — the same
// `corridorKey` precedent) that tests fake. The vitmatte key gate (route_cutout, shared repo venv,
// NO GPU) is exercised by the neutral-key refusal test, which skips cleanly without the studio venv.
// No live model runs here.

// A fake ViTMatte invocation: writes a valid RGBA PNG to outAbs and returns a canvas alpha report,
// so the op's src-swap/provenance/undo/journal contract runs with NO GPU/venv. The key gate lives in
// the REAL invoke (route_cutout + classifyCorridorKeyBorder); this fake bypasses it (unit-covered).
function fakeVitmatte(bytes = softGlowPng()) {
  const calls = { n: 0 };
  const invoke = (root, { outAbs }) => {
    calls.n += 1;
    writeFileSync(outAbs, bytes);
    return {
      schema: "ai_studio.canvas.alpha_cutout_report.v1",
      method: "vitmatte",
      tool: "vitmatte",
      model: "hustvl/vitmatte-base-composition-1k",
      device: "cuda",
      despill: true,
      license: "code MIT; weights local-only (Adobe-DIM caveat, T0335 gate)",
      seconds: 1.4,
      key_color: [255, 0, 255],
      region_count: 0,
      regions: [{ id: "*element*", method: "vitmatte", routed: "vitmatte", key: [255, 0, 255] }],
    };
  };
  return { invoke, calls };
}

// A fake BiRefNet invocation: same shape, but NO key_color (birefnet has no chroma key) and the
// CPU-onnxruntime device string the real tool reports.
function fakeBirefnet(bytes = softGlowPng()) {
  const calls = { n: 0 };
  const invoke = (root, { outAbs }) => {
    calls.n += 1;
    writeFileSync(outAbs, bytes);
    return {
      schema: "ai_studio.canvas.alpha_cutout_report.v1",
      method: "birefnet",
      tool: "birefnet",
      model: "birefnet-general",
      device: "cpu-onnxruntime",
      license: "MIT (rembg + BiRefNet-general)",
      seconds: 12.3,
      region_count: 0,
      regions: [{ id: "*element*", method: "birefnet", routed: "birefnet" }],
    };
  };
  return { invoke, calls };
}

test("alphaCutout unknown-method error names all five methods (auto/matte/corridorkey/vitmatte/birefnet)", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Alpha five methods" });
  const { element } = addImage(UNUSED_ROOT, project.id, { name: "s.png", bytes: magentaSheetPng() });
  await assert.rejects(
    () => alphaCutout(UNUSED_ROOT, { projectId: project.id, elementId: element.id, method: "bogus" }),
    (error) => {
      assert.match(error.message, /unknown alpha method/);
      for (const method of ["auto", "matte", "corridorkey", "vitmatte", "birefnet"]) {
        assert.match(error.message, new RegExp(method), `error lists ${method}`);
      }
      return true;
    },
  );
});

test("vitmatte + birefnet are explicit-only: each seam runs ONLY for its own method (auto/matte/corridorkey never yield it)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "vm/bn method table" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: magentaSheetPng() });

  // Explicit vitmatte: its seam runs once; the birefnet seam is never touched.
  const vm = fakeVitmatte();
  const bn = fakeBirefnet();
  const res = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "vitmatte", vitmatte: vm.invoke, birefnet: bn.invoke });
  assert.equal(vm.calls.n, 1, "vitmatte seam invoked once for method=vitmatte");
  assert.equal(bn.calls.n, 0, "birefnet seam untouched by method=vitmatte");
  assert.equal(res.element.meta.alpha.method, "vitmatte");

  // No explicit method reaches either neural seam (auto/matte route to key_matte; corridorkey to CK
  // — a CK fake stands in so the suite never spends the real GPU here). The spies stay put.
  const ck = fakeCorridorKey();
  for (const method of ["auto", "matte", "corridorkey"]) {
    const p = createProject(REPO_ROOT, { title: `vm/bn not-${method}` });
    const { element: el } = addImage(REPO_ROOT, p.id, { name: "b.png", bytes: magentaSheetPng() });
    try {
      await alphaCutout(REPO_ROOT, { projectId: p.id, elementId: el.id, method, corridorKey: ck.invoke, vitmatte: vm.invoke, birefnet: bn.invoke });
    } catch {
      // venv miss / refusal — irrelevant; the point is the spy counts below.
    }
    assert.equal(vm.calls.n, 1, `vitmatte seam not called for method=${method}`);
    assert.equal(bn.calls.n, 0, `birefnet seam not called for method=${method}`);
  }
});

test("vitmatte (faked GPU run) swaps src + records provenance/timings; ONE undo restores byte-exact, redo re-applies", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "vitmatte provenance" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "web.png", bytes: magentaSheetPng() });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const vm = fakeVitmatte();

  const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "vitmatte", vitmatte: vm.invoke });
  assert.equal(vm.calls.n, 1);
  assert.notEqual(result.element.src, original.src, "src swapped to a new alpha file");
  assert.equal(result.element.w, original.w, "geometry preserved (w)");
  assert.equal(result.element.h, original.h, "geometry preserved (h)");
  const meta = result.element.meta.alpha;
  assert.equal(meta.method, "vitmatte");
  assert.equal(meta.tool, "vitmatte");
  assert.equal(meta.parentSrc, original.src);
  assert.equal(meta.model, "hustvl/vitmatte-base-composition-1k");
  assert.equal(meta.despill, true, "despill flag recorded");
  assert.match(meta.license, /MIT/, "license recorded");
  assert.match(meta.license, /local-only/, "weights-local-only caveat recorded");
  assert.deepEqual(meta.key_color, [255, 0, 255], "detected key recorded");
  assert.equal(typeof meta.timings.seconds, "number", "GPU seconds recorded");
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore + 1, "vitmatte keying is one journal entry");

  assert.ok(existsSync(resolveProjectFile(REPO_ROOT, project.id, original.src)), "original file kept on disk");
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.deepEqual(undone.elements.find((el) => el.id === element.id), original, "undo restores the exact pre-alpha element");
  const redone = redoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((el) => el.id === element.id).src, result.element.src, "redo re-applies the alpha src");
});

test("birefnet (faked CPU run) swaps src + records provenance (no key_color); ONE undo restores byte-exact, redo re-applies", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "birefnet provenance" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "busy.png", bytes: magentaSheetPng() });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const bn = fakeBirefnet();

  const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "birefnet", birefnet: bn.invoke });
  assert.equal(bn.calls.n, 1);
  assert.notEqual(result.element.src, original.src, "src swapped to a new alpha file");
  assert.equal(result.element.w, original.w, "geometry preserved (w)");
  assert.equal(result.element.h, original.h, "geometry preserved (h)");
  const meta = result.element.meta.alpha;
  assert.equal(meta.method, "birefnet");
  assert.equal(meta.tool, "birefnet");
  assert.equal(meta.parentSrc, original.src);
  assert.equal(meta.model, "birefnet-general");
  assert.match(meta.license, /MIT/, "license recorded");
  assert.equal(meta.key_color, undefined, "birefnet records no chroma key");
  assert.equal(typeof meta.timings.seconds, "number", "CPU seconds recorded");
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore + 1, "birefnet keying is one journal entry");

  assert.ok(existsSync(resolveProjectFile(REPO_ROOT, project.id, original.src)), "original file kept on disk");
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.deepEqual(undone.elements.find((el) => el.id === element.id), original, "undo restores the exact pre-alpha element");
  const redone = redoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((el) => el.id === element.id).src, result.element.src, "redo re-applies the alpha src");
});

test("vitmatte batch (faked) keys 2 images in ONE journal entry; one undo restores both byte-exact", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "vitmatte batch" });
  const { element: e1 } = addImage(REPO_ROOT, project.id, { name: "v1.png", bytes: magentaSheetPng() });
  const { element: e2 } = addImage(REPO_ROOT, project.id, { name: "v2.png", bytes: magentaSheetPng() });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const originals = getProject(REPO_ROOT, project.id).elements.map((el) => ({ ...el }));
  const vm = fakeVitmatte();

  const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementIds: [e1.id, e2.id], method: "vitmatte", vitmatte: vm.invoke });
  assert.equal(vm.calls.n, 2, "vitmatte seam runs once per element");
  assert.equal(result.count, 2);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore + 1, "batch is one journal entry");
  for (const el of result.elements) assert.equal(el.meta.alpha.method, "vitmatte");
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  for (const original of originals) {
    assert.deepEqual(undone.elements.find((el) => el.id === original.id), original, `undo restores element ${original.id}`);
  }
});

test("birefnet batch (faked) keys 2 images in ONE journal entry; one undo restores both byte-exact", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "birefnet batch" });
  const { element: e1 } = addImage(REPO_ROOT, project.id, { name: "b1.png", bytes: magentaSheetPng() });
  const { element: e2 } = addImage(REPO_ROOT, project.id, { name: "b2.png", bytes: magentaSheetPng() });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const originals = getProject(REPO_ROOT, project.id).elements.map((el) => ({ ...el }));
  const bn = fakeBirefnet();

  const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementIds: [e1.id, e2.id], method: "birefnet", birefnet: bn.invoke });
  assert.equal(bn.calls.n, 2, "birefnet seam runs once per element");
  assert.equal(result.count, 2);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore + 1, "batch is one journal entry");
  for (const el of result.elements) assert.equal(el.meta.alpha.method, "birefnet");
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  for (const original of originals) {
    assert.deepEqual(undone.elements.find((el) => el.id === original.id), original, `undo restores element ${original.id}`);
  }
});

test("vitmatte refuses a region-scoped request (whole-element only in v1) before any GPU/venv touch", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "vitmatte regions reject" });
  const { element } = addImage(UNUSED_ROOT, project.id, { name: "s.png", bytes: magentaSheetPng() });
  setRegions(UNUSED_ROOT, { projectId: project.id, elementId: element.id, regions: [{ id: "r1", rect: [8, 8, 20, 20] }] });
  const vm = fakeVitmatte();
  await assert.rejects(
    () => alphaCutout(UNUSED_ROOT, { projectId: project.id, elementId: element.id, method: "vitmatte", regions: ["r1"], vitmatte: vm.invoke }),
    /whole-element only/,
  );
  assert.equal(vm.calls.n, 0, "the GPU seam is never reached on a region refusal");
});

test("birefnet refuses a region-scoped request (whole-element only in v1) before any spawn", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "birefnet regions reject" });
  const { element } = addImage(UNUSED_ROOT, project.id, { name: "s.png", bytes: magentaSheetPng() });
  setRegions(UNUSED_ROOT, { projectId: project.id, elementId: element.id, regions: [{ id: "r1", rect: [8, 8, 20, 20] }] });
  const bn = fakeBirefnet();
  await assert.rejects(
    () => alphaCutout(UNUSED_ROOT, { projectId: project.id, elementId: element.id, method: "birefnet", regions: ["r1"], birefnet: bn.invoke }),
    /whole-element only/,
  );
  assert.equal(bn.calls.n, 0, "the CPU seam is never reached on a region refusal");
});

test("vitmatte (real key gate) refuses a NEUTRAL key with a clean message BEFORE any GPU call (skips without the studio venv)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "vitmatte neutral reject" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "neutral.png", bytes: darkBgPng() });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  // No injected seam: the DEFAULT invoke runs the real key gate (route_cutout, repo venv, NO GPU) and
  // must refuse a key that is neither green nor magenta BEFORE the tool's own GPU venv is ever touched.
  try {
    await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "vitmatte" });
    assert.fail("vitmatte on a neutral key must refuse (neither green nor magenta)");
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`vitmatte key gate unavailable (studio venv): ${error.message}`);
      return;
    }
    assert.match(error.message, /neither/i, `neutral refusal expected, got: ${error.message}`);
    assert.ok(!/Traceback|File "/.test(error.message), `traceback leaked to the operator: ${error.message}`);
  }
  assert.deepEqual(getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id), original, "element untouched after a refusal");
});

// ---- alphaDualPlate (T0237: white+black plate pair -> ONE new cut element) ----

// ---- validation (no Python) --------------------------------------------------

test("alphaDualPlate validates arity/type before touching disk (fail-fast, no project needed)", async () => {
  await assert.rejects(() => alphaDualPlate(UNUSED_ROOT, {}), /requires projectId/);
  await assert.rejects(() => alphaDualPlate(UNUSED_ROOT, { projectId: "p" }), /requires an elementIds array/);
  await assert.rejects(
    () => alphaDualPlate(UNUSED_ROOT, { projectId: "p", elementIds: ["a"] }),
    /exactly 2 elementIds/,
  );
  await assert.rejects(
    () => alphaDualPlate(UNUSED_ROOT, { projectId: "p", elementIds: ["a", "b", "c"] }),
    /exactly 2 elementIds/,
  );
  await assert.rejects(
    () => alphaDualPlate(UNUSED_ROOT, { projectId: "p", elementIds: ["a", "a"] }),
    /two DIFFERENT elementIds/,
  );
  // A NONEXISTENT project is never touched: the shape check throws before any getProject
  // (disk) read — a bad shape fails fast regardless of whether the project even exists.
  await assert.rejects(
    () => alphaDualPlate(UNUSED_ROOT, { projectId: "nonexistent-project-xyz", elementIds: ["a"] }),
    /exactly 2 elementIds/,
  );
});

test("alphaDualPlate rejects a non-image element and an unknown id, before any python spawn", async (t) => {
  // REPO_ROOT so addText finds the real fonts manifest; alphaDualPlate throws on the type
  // check BEFORE any Python spawn, so this stays a pure-validation test.
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Dual plate validate" });
  const { element: image } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: magentaSheetPng() });
  const { element: text } = addText(REPO_ROOT, project.id, { content: "hi" });
  await assert.rejects(
    () => alphaDualPlate(REPO_ROOT, { projectId: project.id, elementIds: [image.id, text.id] }),
    /is not an image/,
  );
  await assert.rejects(
    () => alphaDualPlate(REPO_ROOT, { projectId: project.id, elementIds: [image.id, "el_missing"] }),
    /element not found/,
  );
});

// ---- pipeline (real warm worker; skips without the studio venv) ---------------

test("alphaDualPlate (happy pair) creates ONE new element in ONE journal entry; plates stay untouched; undo removes it, redo restores it", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Dual plate happy" });
  const { white, black } = dualPlatePairPng();
  const { element: plateWhite } = addImage(REPO_ROOT, project.id, { name: "plate_white.png", bytes: white });
  const { element: plateBlack } = addImage(REPO_ROOT, project.id, { name: "plate_black.png", bytes: black });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq; // 2 addImage entries
  const originals = getProject(REPO_ROOT, project.id).elements.map((element) => ({ ...element }));

  let result;
  try {
    result = await alphaDualPlate(REPO_ROOT, { projectId: project.id, elementIds: [plateWhite.id, plateBlack.id] });
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`dual-plate pipeline unavailable: ${error.message}`);
      return;
    }
    throw error;
  }

  // ONE new journal entry for the whole gesture.
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "dual-plate keying is one journal entry");

  // A brand-new element (never a src-swap of either plate), named off plate A and placed
  // to the RIGHT of both plates' union bbox (gap 16) — never on top of a plate.
  assert.notEqual(result.element.id, plateWhite.id);
  assert.notEqual(result.element.id, plateBlack.id);
  assert.equal(after.elements.length, 3, "2 plates + 1 new cut element");
  assert.equal(result.element.name, `${plateWhite.name} alpha`);
  assert.equal(result.element.x, Math.max(plateWhite.x + plateWhite.w, plateBlack.x + plateBlack.w) + 16);
  assert.equal(result.element.y, Math.min(plateWhite.y, plateBlack.y));
  assert.equal(result.element.meta.alpha.method, "dual_plate");
  assert.deepEqual(result.element.meta.alpha.parents, [plateWhite.src, plateBlack.src]);
  assert.equal(result.element.meta.alpha.pair_gate.verdict, "pass");

  // Both plates stay byte-exact untouched (non-destructive — no src swap, unlike alphaCutout).
  for (const original of originals) {
    assert.deepEqual(after.elements.find((el) => el.id === original.id), original, `plate ${original.id} untouched`);
  }

  // The extracted PNG: transparent background, opaque recovered blob at its original color.
  const png = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, result.element.src)));
  assert.equal(png.channels, 4);
  assert.equal(png.at(0, 0)[3], 0, "background is transparent");
  assert.equal(png.at(18, 14)[3], 255, "blob is opaque");
  assert.deepEqual(png.at(18, 14).slice(0, 3), [200, 60, 60], "blob color recovered");

  // ONE undo removes ONLY the new element; the plates were never mutated in the first
  // place, so they are trivially still there afterward.
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.equal(undone.elements.length, 2, "only the 2 plates remain after undo");
  for (const original of originals) {
    assert.deepEqual(undone.elements.find((el) => el.id === original.id), original);
  }

  // Redo re-creates the exact same element (same id/src).
  const redone = redoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((el) => el.id === result.element.id).src, result.element.src);
});

test("alphaDualPlate refuses a misaligned pair with the pair gate's own message — no Python traceback, nothing mutated", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Dual plate refusal" });
  const { white, black } = dualPlatePairPng({ offset: 10 }); // blob shifted several px on the black plate
  const { element: plateWhite } = addImage(REPO_ROOT, project.id, { name: "plate_white.png", bytes: white });
  const { element: plateBlack } = addImage(REPO_ROOT, project.id, { name: "plate_black.png", bytes: black });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const originals = getProject(REPO_ROOT, project.id).elements.map((element) => ({ ...element }));

  try {
    await alphaDualPlate(REPO_ROOT, { projectId: project.id, elementIds: [plateWhite.id, plateBlack.id] });
    assert.fail("a misaligned dual-plate pair must refuse (pair gate)");
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`dual-plate pipeline unavailable: ${error.message}`);
      return;
    }
    // The gate's own message reaches the operator...
    assert.match(error.message, /consistency gate|regenerate|redraw/i, `refusal message expected, got: ${error.message}`);
    // ...WITHOUT the worker's crash formatting (raw traceback in the UI toast = bug).
    assert.ok(!/Traceback|File "/.test(error.message), `traceback leaked to the operator: ${error.message}`);
  }

  // Nothing mutated: no journal entry, both plates untouched, no stray new element.
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore, "no journal entry on a rejected pair");
  assert.equal(after.elements.length, 2, "no new element created");
  for (const original of originals) {
    assert.deepEqual(after.elements.find((el) => el.id === original.id), original);
  }
});

// ---- API + CLI parity (real pipeline; skips without the venv) -----------------

test("alpha-dual API route and CLI drive the same op", async (t) => {
  const dir = tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Dual plate parity" });
  const { white, black } = dualPlatePairPng();
  const { element: plateWhite } = addImage(REPO_ROOT, project.id, { name: "plate_white.png", bytes: white });
  const { element: plateBlack } = addImage(REPO_ROOT, project.id, { name: "plate_black.png", bytes: black });

  // API: POST /alpha-dual { elementIds } — mirror how the multi-selection inspector calls it.
  const handler = createCanvasApi(REPO_ROOT);
  const captured = { status: 0, body: null };
  const res = {
    writeHead(status) { captured.status = status; return this; },
    end(payload) { captured.body = payload ? JSON.parse(payload) : null; },
  };
  const req = mockReq("POST", { elementIds: [plateWhite.id, plateBlack.id] });
  await handler(req, res, new URL(`http://x/api/canvas/projects/${project.id}/alpha-dual`));
  if (captured.status === 400 && /venv|Pillow|module/i.test(String(captured.body && captured.body.error))) {
    t.skip(`dual-plate pipeline unavailable: ${captured.body.error}`);
    return;
  }
  assert.equal(captured.status, 201, `API alpha-dual 201 (got ${captured.status}: ${JSON.stringify(captured.body)})`);
  assert.equal(captured.body.element.meta.alpha.method, "dual_plate");
  const apiElementId = captured.body.element.id;

  // CLI: alpha-dual <id> --elements a,b — same ops layer, different transport. The plates
  // are still on the canvas (non-destructive), so a second dual-plate run off the SAME
  // pair is a valid op — it mints a SECOND new element.
  const out = execFileSync("node", [CLI, "alpha-dual", project.id, "--elements", `${plateWhite.id},${plateBlack.id}`], {
    env: { ...process.env, CANVAS_PROJECTS_ROOT: dir },
    encoding: "utf8",
  });
  const parsed = JSON.parse(out.trim().split("\n").pop());
  assert.equal(parsed.element.meta.alpha.method, "dual_plate");
  assert.match(parsed.element.src, /^files\//);
  assert.notEqual(parsed.element.id, apiElementId, "CLI run mints its own new element");
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
