// bakeFilters ("Apply" — T0274, Photoshop-rasterize semantics) op + API/CLI parity
// tests. Run:
//   node --test ai_studio/assets/canvas/tests/filters_bake.test.mjs
//
// bakeFilters burns an element's CURRENT non-destructive filters+opacity (T0273/T0260)
// into a NEW content-addressed source file in ONE journaled entry, then clears both
// fields ("принял -> получил новый арт -> ползунки снова в 0"); undo restores the
// previous src + filters + opacity byte-exact, like cleanupApply (both keep the in-place
// src-swap; alphaCutout since T0336 instead mints a NEW element beside the source). The
// validation tests (non-image, nothing-to-bake, bad elementId/elementIds shape) need no
// Python; the pipeline tests run the real warm-worker path and skip cleanly when the
// studio venv is missing (mirrors alpha.test.mjs/filters.test.mjs).
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath, URL } from "node:url";

import {
  addImage,
  addText,
  assignToGroup,
  bakeFilters,
  createGroup,
  createProject,
  getProject,
  hasBakeableFilters,
  patchElement,
  redoOp,
  renderGroup,
  undoOp,
} from "../ops.mjs";
import { resolveProjectFile } from "../store.mjs";
import { createCanvasApi } from "../api.mjs";
import { decodePng, encodePng, solidPng } from "./png_fixture.mjs";

// Python tools run with cwd = repo root, so the pipeline tests must use the real root.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
// The validation tests never spawn Python, so any unused root works for them.
const UNUSED_ROOT = "C:/unused-repo-root";
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-bake-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

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

// ---- pure helper (no disk) ----------------------------------------------------

test("hasBakeableFilters: true only when filters is non-default or opacity != 1", () => {
  assert.equal(hasBakeableFilters(null), false);
  assert.equal(hasBakeableFilters({}), false);
  assert.equal(hasBakeableFilters({ filters: { brightness: 1.2 } }), true);
  assert.equal(hasBakeableFilters({ opacity: 0.5 }), true);
  assert.equal(hasBakeableFilters({ opacity: 1 }), false);
});

// ---- validation (no Python) ---------------------------------------------------

test("bakeFilters rejects a non-image (text) element loudly", async (t) => {
  // REPO_ROOT so addText finds the real fonts manifest; bakeFilters throws on the type
  // check BEFORE any Python spawn, so this stays a pure-validation test.
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Bake validate" });
  const { element } = addText(REPO_ROOT, project.id, { content: "hi" });
  await assert.rejects(
    () => bakeFilters(REPO_ROOT, { projectId: project.id, elementId: element.id }),
    /is not an image/,
  );
});

test("bakeFilters rejects an element with nothing to bake (filters/opacity at defaults)", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Bake nothing" });
  const { element } = addImage(UNUSED_ROOT, project.id, { name: "flat.png", bytes: solidPng(10, 10, [10, 20, 30]) });
  await assert.rejects(
    () => bakeFilters(UNUSED_ROOT, { projectId: project.id, elementId: element.id }),
    /nothing to apply|at defaults/,
  );
  // The refusal changed nothing.
  const after = getProject(UNUSED_ROOT, project.id).elements.find((el) => el.id === element.id);
  assert.equal(after.src, element.src);
});

test("bakeFilters requires projectId and elementId", async () => {
  await assert.rejects(() => bakeFilters(UNUSED_ROOT, {}), /requires projectId/);
  await assert.rejects(() => bakeFilters(UNUSED_ROOT, { projectId: "p" }), /requires elementId/);
});

test("bakeFilters rejects elementId and elementIds together, and an empty elementIds array", async () => {
  await assert.rejects(
    () => bakeFilters(UNUSED_ROOT, { projectId: "p", elementId: "e1", elementIds: ["e1", "e2"] }),
    /either elementId or elementIds/,
  );
  await assert.rejects(
    () => bakeFilters(UNUSED_ROOT, { projectId: "p", elementIds: [] }),
    /non-empty elementIds array/,
  );
});

// ---- batch validation (no Python) — atomicity never depends on the venv -------

test("bakeFilters batch is atomic: one element with nothing to bake rejects the WHOLE batch, nothing mutated", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Bake batch refusal" });
  const { element: hasFilters } = addImage(UNUSED_ROOT, project.id, { name: "a.png", bytes: solidPng(10, 10, [200, 60, 60]) });
  const { element: plain } = addImage(UNUSED_ROOT, project.id, { name: "b.png", bytes: solidPng(10, 10, [60, 200, 60]) });
  patchElement(UNUSED_ROOT, project.id, hasFilters.id, { filters: { brightness: 1.2 } });
  // `plain` has nothing to bake — filters/opacity both at default. Validation runs for
  // EVERY element up front, before any tool spawn, so this never touches Python.
  const seqBefore = getProject(UNUSED_ROOT, project.id).history_seq;
  const originals = getProject(UNUSED_ROOT, project.id).elements.map((el) => ({ ...el }));

  await assert.rejects(
    () => bakeFilters(UNUSED_ROOT, { projectId: project.id, elementIds: [hasFilters.id, plain.id] }),
    /nothing to apply|at defaults/,
  );

  const after = getProject(UNUSED_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore, "no journal entry on a rejected batch");
  for (const original of originals) {
    assert.deepEqual(
      after.elements.find((el) => el.id === original.id),
      original,
      `element ${original.id} untouched after the batch refusal`,
    );
  }
});

test("bakeFilters batch rejects an unknown/non-image element before any write", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Bake batch bad element" });
  const { element: img } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: solidPng(10, 10, [1, 2, 3]) });
  patchElement(REPO_ROOT, project.id, img.id, { opacity: 0.5 });
  const { element: text } = addText(REPO_ROOT, project.id, { content: "hi" });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;

  await assert.rejects(
    () => bakeFilters(REPO_ROOT, { projectId: project.id, elementIds: [img.id, "el_missing"] }),
    /element not found/,
  );
  await assert.rejects(
    () => bakeFilters(REPO_ROOT, { projectId: project.id, elementIds: [img.id, text.id] }),
    /is not an image/,
  );
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore, "no journal entry on rejected batch input");
});

// ---- pipeline (real warm worker; skips without the studio venv) ---------------

// Independently-derived (not imported) reimplementation of the SAME canonical formulas
// filters_math.py's apply_filters implements (README "Image filters") — importing it
// would make the parity assertion below circular, load-bearing-nothing. Mirrors
// filters.test.mjs's applyFiltersJs verbatim.
function applyFiltersJs([r, g, b, a], filters) {
  let R = r / 255;
  let G = g / 255;
  let B = b / 255;
  const brightness = filters.brightness ?? 1;
  const saturation = filters.saturation ?? 1;
  const contrast = filters.contrast ?? 1;
  if (brightness !== 1) {
    R *= brightness;
    G *= brightness;
    B *= brightness;
  }
  if (saturation !== 1) {
    const s = saturation;
    const R2 = (0.2126 + 0.7874 * s) * R + (0.7152 - 0.7152 * s) * G + (0.0722 - 0.0722 * s) * B;
    const G2 = (0.2126 - 0.2126 * s) * R + (0.7152 + 0.2848 * s) * G + (0.0722 - 0.0722 * s) * B;
    const B2 = (0.2126 - 0.2126 * s) * R + (0.7152 - 0.7152 * s) * G + (0.0722 + 0.9278 * s) * B;
    R = R2;
    G = G2;
    B = B2;
  }
  if (contrast !== 1) {
    R = (R - 0.5) * contrast + 0.5;
    G = (G - 0.5) * contrast + 0.5;
    B = (B - 0.5) * contrast + 0.5;
  }
  R = Math.min(1, Math.max(0, R));
  G = Math.min(1, Math.max(0, G));
  B = Math.min(1, Math.max(0, B));
  const tint = filters.tint;
  if (tint && tint.strength > 0) {
    const strength = tint.strength;
    const hex = tint.color.replace("#", "");
    const tr = parseInt(hex.slice(0, 2), 16) / 255;
    const tg = parseInt(hex.slice(2, 4), 16) / 255;
    const tb = parseInt(hex.slice(4, 6), 16) / 255;
    R = R * (1 - strength) + tr * strength;
    G = G * (1 - strength) + tg * strength;
    B = B * (1 - strength) + tb * strength;
    R = Math.min(1, Math.max(0, R));
    G = Math.min(1, Math.max(0, G));
    B = Math.min(1, Math.max(0, B));
  }
  return [Math.round(R * 255), Math.round(G * 255), Math.round(B * 255), a];
}

function closeChannel(actual, expected, tol = 1) {
  return Math.abs(actual - expected) <= tol;
}

const VENV_MISSING = /venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i;

// Create a project with ONE image element carrying the given filters/opacity, and return
// {projectId, elementId, original, seqBeforeBake} — the shared setup for the pipeline
// tests below. `original` is a snapshot BEFORE bakeFilters runs.
function setupBakeElement(root, rgba, { filters, opacity } = {}) {
  const project = createProject(root, { title: "Bake run" });
  const bytes = encodePng(16, 16, () => rgba, { alpha: true });
  const { element } = addImage(root, project.id, { name: "sprite.png", bytes });
  const patch = {};
  if (filters !== undefined) patch.filters = filters;
  if (opacity !== undefined) patch.opacity = opacity;
  if (Object.keys(patch).length) patchElement(root, project.id, element.id, patch);
  const original = getProject(root, project.id).elements.find((el) => el.id === element.id);
  const seqBeforeBake = getProject(root, project.id).history_seq;
  return { projectId: project.id, elementId: element.id, original, seqBeforeBake };
}

test(
  "bakeFilters writes a new src: canonical filters baked + alpha multiplied by opacity; filters/opacity cleared; ONE journal entry",
  async (t) => {
    tempProjects(t);
    const RGBA = [180, 90, 40, 200];
    const filters = { brightness: 1.2, saturation: 0.4, contrast: 1.3, tint: { color: "#0000ff", strength: 0.4 } };
    const opacity = 0.5; // 200 * 0.5 = 100 exactly — no rounding-mode ambiguity between JS/Python
    const { projectId, elementId, original, seqBeforeBake } = setupBakeElement(REPO_ROOT, RGBA, { filters, opacity });

    let result;
    try {
      result = await bakeFilters(REPO_ROOT, { projectId, elementId });
    } catch (error) {
      if (VENV_MISSING.test(error.message)) {
        t.skip(`bake pipeline unavailable: ${error.message}`);
        return;
      }
      throw error;
    }

    // src swapped to a NEW file; source dims + box preserved (geometry unchanged, regions
    // stay valid — README note).
    assert.notEqual(result.element.src, original.src, "element swapped to a new baked file");
    assert.equal(result.element.source_w, original.source_w);
    assert.equal(result.element.source_h, original.source_h);
    assert.equal(result.element.w, original.w);
    assert.equal(result.element.h, original.h);

    // Sliders reset — filters/opacity cleared to absent (the whole point of "Apply").
    assert.equal("filters" in result.element, false, "filters cleared after bake");
    assert.equal("opacity" in result.element, false, "opacity cleared after bake");

    // Provenance: meta.filters_bake records what was actually burned in + how to undo.
    assert.deepEqual(result.element.meta.filters_bake.baked.filters, filters);
    assert.equal(result.element.meta.filters_bake.baked.opacity, opacity);
    assert.equal(result.element.meta.filters_bake.prev_src, original.src);

    // ONE journal entry for the whole bake.
    const after = getProject(REPO_ROOT, projectId);
    assert.equal(after.history_seq, seqBeforeBake + 1, "bake is one journal entry");

    // Pixel math: RGB from the canonical filter chain (±1), alpha = round(200 * 0.5).
    const png = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, projectId, result.element.src)));
    assert.equal(png.channels, 4);
    const expected = applyFiltersJs(RGBA, filters);
    const actual = png.at(8, 8);
    for (let c = 0; c < 3; c += 1) {
      assert.ok(closeChannel(actual[c], expected[c]), `channel ${c}: got ${actual[c]}, expected ~${expected[c]}`);
    }
    assert.equal(actual[3], 100, "alpha multiplied by opacity (200 * 0.5)");
  },
);

test("bakeFilters is ONE journal entry: undo restores previous src + filters + opacity byte-exact; redo re-applies", async (t) => {
  tempProjects(t);
  const { projectId, elementId, original } = setupBakeElement(REPO_ROOT, [200, 60, 60, 255], {
    filters: { brightness: 1.3 },
    opacity: 0.6,
  });

  let result;
  try {
    result = await bakeFilters(REPO_ROOT, { projectId, elementId });
  } catch (error) {
    if (VENV_MISSING.test(error.message)) {
      t.skip(`bake pipeline unavailable: ${error.message}`);
      return;
    }
    throw error;
  }

  // The original file is immutable — it still exists on disk with identical bytes.
  const originalAbs = resolveProjectFile(REPO_ROOT, projectId, original.src);
  assert.ok(existsSync(originalAbs), "original file kept on disk (non-destructive)");
  const originalBytes = readFileSync(originalAbs);

  // Undo -> the element is restored EXACTLY to its pre-bake state (src + filters + opacity).
  const undone = undoOp(REPO_ROOT, { projectId }).project;
  const afterUndo = undone.elements.find((el) => el.id === elementId);
  assert.deepEqual(afterUndo, original, "undo restores the exact pre-bake element");
  assert.deepEqual(
    readFileSync(resolveProjectFile(REPO_ROOT, projectId, afterUndo.src)),
    originalBytes,
    "undo restores byte-exact previous pixels",
  );

  // Redo -> the baked src (and cleared filters/opacity) come back.
  const redone = redoOp(REPO_ROOT, { projectId }).project;
  const afterRedo = redone.elements.find((el) => el.id === elementId);
  assert.equal(afterRedo.src, result.element.src);
  assert.equal("filters" in afterRedo, false);
  assert.equal("opacity" in afterRedo, false);
});

// ---- batch (multi-selection "Apply filters on N images", ONE journal entry) ---

test("bakeFilters batch (elementIds) bakes 2 images in ONE journal entry; one undo restores both byte-exact", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Bake batch undo" });
  const { element: e1 } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: solidPng(10, 10, [200, 60, 60]) });
  const { element: e2 } = addImage(REPO_ROOT, project.id, { name: "b.png", bytes: solidPng(10, 10, [60, 200, 60]) });
  patchElement(REPO_ROOT, project.id, e1.id, { filters: { brightness: 1.2 } });
  patchElement(REPO_ROOT, project.id, e2.id, { opacity: 0.6 });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const originals = getProject(REPO_ROOT, project.id).elements.map((el) => ({ ...el }));

  let result;
  try {
    result = await bakeFilters(REPO_ROOT, { projectId: project.id, elementIds: [e1.id, e2.id] });
  } catch (error) {
    if (VENV_MISSING.test(error.message)) {
      t.skip(`bake pipeline unavailable: ${error.message}`);
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
    assert.notEqual(element.src, original.src, `element ${element.id} swapped to a new baked file`);
    assert.equal("filters" in element, false);
    assert.equal("opacity" in element, false);
  }

  // ONE undo restores BOTH elements byte-exact (src + filters + opacity), in one step.
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  for (const original of originals) {
    const restored = undone.elements.find((element) => element.id === original.id);
    assert.deepEqual(restored, original, `undo restores element ${original.id} exactly`);
  }
  assert.equal(undone.history_seq, seqBefore);

  // Redo re-applies both bakes together.
  const redone = redoOp(REPO_ROOT, { projectId: project.id }).project;
  for (const element of result.elements) {
    assert.equal(redone.elements.find((item) => item.id === element.id).src, element.src);
  }
});

// ---- render parity: renderGroup before vs after bake match (the whole point) --

test("render parity: after bake, renderGroup output matches the pre-bake render (tolerance ±1)", async (t) => {
  // Every ops call here needs the REAL repo root for relative lookups (fonts manifest,
  // python tool paths); actual project.json/files/ storage is redirected to the temp dir
  // via CANVAS_PROJECTS_ROOT (tempProjects' side effect), exactly like filters.test.mjs.
  tempProjects(t);
  const W = 16;
  const H = 16;
  // scale:1 and element box == source dims (addImage defaults w/h to the source size), so
  // render_group.py's resize step is a no-op both before and after — the ONLY thing that
  // changes is whether the filter/opacity math ran at render time (before) or was already
  // baked into the source (after); pixels must land identically (±1 rounding).
  const RGBA = [180, 90, 40, 200];
  const bytes = encodePng(W, H, () => RGBA, { alpha: true });
  const pid = createProject(REPO_ROOT, { title: "BakeRenderParity" }).id;
  const element = addImage(REPO_ROOT, pid, { name: "sprite.png", bytes, x: 0, y: 0 }).element;
  assert.equal(element.w, W);
  assert.equal(element.h, H);
  const group = createGroup(REPO_ROOT, { projectId: pid, name: "Screen", x: 0, y: 0, w: 60, h: 60 }).group;
  assignToGroup(REPO_ROOT, { projectId: pid, elementIds: [element.id], groupId: group.id });
  const filters = { brightness: 1.2, saturation: 0.4, contrast: 1.3, tint: { color: "#0000ff", strength: 0.4 } };
  patchElement(REPO_ROOT, pid, element.id, { filters, opacity: 0.6 });

  let before;
  try {
    before = await renderGroup(REPO_ROOT, { projectId: pid, groupId: group.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const beforePng = decodePng(readFileSync(before.path));

  let baked;
  try {
    baked = await bakeFilters(REPO_ROOT, { projectId: pid, elementId: element.id });
  } catch (error) {
    if (VENV_MISSING.test(error.message)) {
      t.skip(`bake pipeline unavailable: ${error.message}`);
      return;
    }
    throw error;
  }
  assert.equal("filters" in baked.element, false);
  assert.equal("opacity" in baked.element, false);

  const after = await renderGroup(REPO_ROOT, { projectId: pid, groupId: group.id, scale: 1 });
  const afterPng = decodePng(readFileSync(after.path));

  assert.equal(beforePng.width, afterPng.width);
  assert.equal(beforePng.height, afterPng.height);
  for (let y = 0; y < H; y += 1) {
    for (let x = 0; x < W; x += 1) {
      const b = beforePng.at(x, y);
      const a = afterPng.at(x, y);
      for (let c = 0; c < 4; c += 1) {
        assert.ok(closeChannel(a[c], b[c]), `pixel (${x},${y}) channel ${c}: before=${b[c]} after=${a[c]}`);
      }
    }
  }
});

// ---- API + CLI parity (real pipeline; skips without the venv) -----------------

test("filters-bake API route and CLI drive the same op (single)", async (t) => {
  const dir = tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Bake parity" });
  const bytes = encodePng(10, 10, () => [200, 60, 60, 255], { alpha: true });
  const { element } = addImage(REPO_ROOT, project.id, { name: "sprite.png", bytes });
  patchElement(REPO_ROOT, project.id, element.id, { filters: { brightness: 1.2 }, opacity: 0.5 });

  // API: POST /elements/<eid>/filters-bake {} — mirror how the page calls it.
  const handler = createCanvasApi(REPO_ROOT);
  const captured = { status: 0, body: null };
  const res = {
    writeHead(status) { captured.status = status; return this; },
    end(payload) { captured.body = payload ? JSON.parse(payload) : null; },
  };
  const req = mockReq("POST", {});
  await handler(req, res, new URL(`http://x/api/canvas/projects/${project.id}/elements/${element.id}/filters-bake`));
  if (captured.status === 400 && VENV_MISSING.test(String(captured.body && captured.body.error))) {
    t.skip(`bake pipeline unavailable: ${captured.body.error}`);
    return;
  }
  assert.equal(captured.status, 200, `API filters-bake 200 (got ${captured.status}: ${JSON.stringify(captured.body)})`);
  assert.equal("filters" in captured.body.element, false);
  assert.equal("opacity" in captured.body.element, false);
  assert.match(captured.body.element.src, /^files\//);

  // CLI: filters-bake <id> --element <eid> — same ops layer, different transport.
  const project2 = createProject(REPO_ROOT, { title: "Bake CLI" });
  const { element: element2 } = addImage(REPO_ROOT, project2.id, { name: "sprite2.png", bytes });
  patchElement(REPO_ROOT, project2.id, element2.id, { opacity: 0.4 });
  const out = execFileSync("node", [CLI, "filters-bake", project2.id, "--element", element2.id], {
    env: { ...process.env, CANVAS_PROJECTS_ROOT: dir },
    encoding: "utf8",
  });
  const parsed = JSON.parse(out.trim().split("\n").pop());
  assert.match(parsed.element.src, /^files\//);
  assert.equal("opacity" in parsed.element, false);
});

test("filters-bake API and CLI both drive elementIds batches, one journal entry each (parity)", async (t) => {
  const dir = tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Bake batch parity" });
  const bytes1 = encodePng(10, 10, () => [200, 60, 60, 255], { alpha: true });
  const bytes2 = encodePng(10, 10, () => [60, 200, 60, 255], { alpha: true });
  const { element: e1 } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: bytes1 });
  const { element: e2 } = addImage(REPO_ROOT, project.id, { name: "b.png", bytes: bytes2 });
  patchElement(REPO_ROOT, project.id, e1.id, { filters: { brightness: 1.2 } });
  patchElement(REPO_ROOT, project.id, e2.id, { opacity: 0.5 });
  const seqBeforeApi = getProject(REPO_ROOT, project.id).history_seq;

  // API: POST /filters-bake { elementIds } — mirror how the multi-selection inspector calls it.
  const handler = createCanvasApi(REPO_ROOT);
  const captured = { status: 0, body: null };
  const res = {
    writeHead(status) { captured.status = status; return this; },
    end(payload) { captured.body = payload ? JSON.parse(payload) : null; },
  };
  const req = mockReq("POST", { elementIds: [e1.id, e2.id] });
  await handler(req, res, new URL(`http://x/api/canvas/projects/${project.id}/filters-bake`));
  if (captured.status === 400 && VENV_MISSING.test(String(captured.body && captured.body.error))) {
    t.skip(`bake pipeline unavailable: ${captured.body.error}`);
    return;
  }
  assert.equal(captured.status, 200, `API filters-bake batch 200 (got ${captured.status}: ${JSON.stringify(captured.body)})`);
  assert.equal(captured.body.count, 2);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBeforeApi + 1, "API batch is one journal entry");

  // CLI: filters-bake <id> --elements e1,e2 — same ops layer, different transport.
  const project2 = createProject(REPO_ROOT, { title: "Bake batch CLI" });
  const { element: c1 } = addImage(REPO_ROOT, project2.id, { name: "a.png", bytes: bytes1 });
  const { element: c2 } = addImage(REPO_ROOT, project2.id, { name: "b.png", bytes: bytes2 });
  patchElement(REPO_ROOT, project2.id, c1.id, { opacity: 0.6 });
  patchElement(REPO_ROOT, project2.id, c2.id, { filters: { contrast: 1.3 } });
  const seqBeforeCli = getProject(REPO_ROOT, project2.id).history_seq;
  const out = execFileSync("node", [CLI, "filters-bake", project2.id, "--elements", `${c1.id},${c2.id}`], {
    env: { ...process.env, CANVAS_PROJECTS_ROOT: dir },
    encoding: "utf8",
  });
  const parsed = JSON.parse(out.trim().split("\n").pop());
  assert.equal(parsed.count, 2);
  assert.match(parsed.elements[0].src, /^files\//);
  assert.match(parsed.elements[1].src, /^files\//);
  assert.equal(getProject(REPO_ROOT, project2.id).history_seq, seqBeforeCli + 1, "CLI batch is one journal entry too");
});
