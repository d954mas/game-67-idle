// T0273 — non-destructive image filters (brightness/saturation/contrast/tint) + the
// element.opacity whitelist gap on patchElements: data model (patchElement/patchElements
// validation/normalization), buildRenderNodes forwarding, and the canvas<->PIL render
// parity contract (README "Image filters"). Run:
//   node --test ai_studio/assets/canvas/tests/filters.test.mjs
//
// The render-parity tests drive the REAL render_group.py through the warm worker
// (renderGroup) and skip cleanly when the studio venv / Pillow is unavailable (mirrors
// transform.test.mjs). Every other test here is pure/store-level
// and never touches Python — validation/normalization/undo all fire before any disk
// read/spawn, so those tests are deterministic and always run.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import {
  addImage,
  addText,
  assignToGroup,
  createGroup,
  createProject,
  getProject,
  patchElement,
  patchElements,
  renderGroup,
  undoOp,
} from "../ops.mjs";
import { createCanvasApi } from "../api.mjs";
import { canvasProjectsRoot } from "../config.mjs";
import { decodePng, encodePng, solidPng } from "./png_fixture.mjs";

const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

// Returns REPO_ROOT (not the temp dir) — every ops call below needs the REAL repo root
// for relative lookups; actual project.json/files/ storage is redirected to the temp dir
// via CANVAS_PROJECTS_ROOT, exactly like transform.test.mjs.
function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-filters-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return REPO_ROOT;
}

function img(root, pid, name, w, h, x = 0, y = 0, rgb = [10, 20, 30]) {
  return addImage(root, pid, { name, bytes: solidPng(w, h, rgb), x, y }).element;
}

function byId(project, id) {
  return (project.elements || []).find((item) => item.id === id);
}

// ---- data model: normalize / validate (pure, no Python) -----------------------

test("patchElement filters: image-only, loud validation before any write", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "FiltersReject" }).id;
  const image = img(root, pid, "bg", 20, 20);
  const text = addText(root, pid, { content: "Hi" }).element;

  assert.throws(() => patchElement(root, pid, text.id, { filters: { brightness: 1.2 } }), /filters are image-only/);

  assert.throws(() => patchElement(root, pid, image.id, { filters: { brightness: 3 } }), /brightness must be a finite number in \[0,2\]/);
  assert.throws(() => patchElement(root, pid, image.id, { filters: { saturation: -1 } }), /saturation must be a finite number in \[0,2\]/);
  assert.throws(() => patchElement(root, pid, image.id, { filters: { contrast: NaN } }), /contrast must be a finite number in \[0,2\]/);
  assert.throws(() => patchElement(root, pid, image.id, { filters: { brightness: "sideways" } }), /brightness must be a finite number/);
  assert.throws(() => patchElement(root, pid, image.id, { filters: { tint: { color: "red", strength: 0.5 } } }), /tint\.color must be #rrggbb/);
  assert.throws(
    () => patchElement(root, pid, image.id, { filters: { tint: { color: "#112233", strength: 2 } } }),
    /tint\.strength must be a finite number in \[0,1\]/,
  );
  assert.throws(() => patchElement(root, pid, image.id, { filters: "nope" }), /filters must be an object/);

  // Nothing from any of the throws above landed on the element (atomic).
  assert.equal(byId(getProject(root, pid), image.id).filters, undefined);
});

test("patchElement filters normalize: defaults omitted, whole-object REPLACE (not merge), {}/null clears", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "FiltersNorm" }).id;
  const image = img(root, pid, "bg", 20, 20);

  patchElement(root, pid, image.id, { filters: { brightness: 1, saturation: 1, contrast: 1 } });
  assert.equal("filters" in byId(getProject(root, pid), image.id), false, "all-default filters normalize to absent");

  patchElement(root, pid, image.id, { filters: { brightness: 1.5, saturation: 1, contrast: 0.8 } });
  assert.deepEqual(byId(getProject(root, pid), image.id).filters, { brightness: 1.5, contrast: 0.8 }, "only non-default keys are stored");

  // Whole-object replace: patching just contrast drops the previously-set brightness.
  patchElement(root, pid, image.id, { filters: { contrast: 1.3 } });
  assert.deepEqual(byId(getProject(root, pid), image.id).filters, { contrast: 1.3 });

  // tint is stored only when strength > 0 (validated loudly either way).
  patchElement(root, pid, image.id, { filters: { tint: { color: "#112233", strength: 0 } } });
  assert.equal("filters" in byId(getProject(root, pid), image.id), false, "tint at strength 0 normalizes to absent");
  patchElement(root, pid, image.id, { filters: { tint: { color: "#112233", strength: 0.4 } } });
  assert.deepEqual(byId(getProject(root, pid), image.id).filters, { tint: { color: "#112233", strength: 0.4 } });

  // {} and null both clear to an absent field.
  patchElement(root, pid, image.id, { filters: {} });
  assert.equal("filters" in byId(getProject(root, pid), image.id), false);
  patchElement(root, pid, image.id, { filters: { brightness: 1.5 } });
  patchElement(root, pid, image.id, { filters: null });
  assert.equal("filters" in byId(getProject(root, pid), image.id), false);
});

test("patchElement filters is ONE journal entry; undo restores the prior filters exactly", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "FiltersUndo" }).id;
  const image = img(root, pid, "bg", 20, 20);
  patchElement(root, pid, image.id, { filters: { brightness: 1.4 } });
  const headBefore = getProject(root, pid).history_seq;

  const after = patchElement(root, pid, image.id, {
    filters: { saturation: 0.5, tint: { color: "#ff0000", strength: 0.3 } },
  }).project;
  assert.equal(after.history_seq, headBefore + 1, "one journal entry for the whole patch");
  assert.deepEqual(byId(after, image.id).filters, { saturation: 0.5, tint: { color: "#ff0000", strength: 0.3 } });

  const undone = undoOp(root, { projectId: pid }).project;
  assert.deepEqual(byId(undone, image.id).filters, { brightness: 1.4 }, "undo restores the prior filters exactly");
});

test("patchElements batches filters AND opacity across several elements in ONE journal entry; undo restores both", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "FiltersBatch" }).id;
  const a = img(root, pid, "a", 10, 10);
  const b = img(root, pid, "b", 10, 10);
  const headBefore = getProject(root, pid).history_seq;

  const result = patchElements(root, {
    projectId: pid,
    patches: [
      { elementId: a.id, filters: { brightness: 0.7, saturation: 0.6 }, opacity: 0.7 },
      { elementId: b.id, opacity: 0.5 },
    ],
  });
  assert.equal(result.project.history_seq, headBefore + 1, "one journal entry for the whole batch");
  assert.deepEqual(byId(result.project, a.id).filters, { brightness: 0.7, saturation: 0.6 });
  assert.equal(byId(result.project, a.id).opacity, 0.7);
  assert.equal(byId(result.project, b.id).opacity, 0.5);
  assert.equal("filters" in byId(result.project, b.id), false);

  const undone = undoOp(root, { projectId: pid }).project;
  assert.equal("filters" in byId(undone, a.id), false, "undo restores a's filters to absent");
  assert.equal("opacity" in byId(undone, a.id), false, "undo restores a's opacity to absent");
  assert.equal("opacity" in byId(undone, b.id), false, "undo restores b's opacity to absent");
});

test("patchElements validates opacity/filters per element atomically — a bad patch anywhere in the batch writes nothing", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "FiltersBatchBad" }).id;
  const a = img(root, pid, "a", 10, 10);
  const text = addText(root, pid, { content: "hi" }).element;

  assert.throws(
    () => patchElements(root, { projectId: pid, patches: [{ elementId: a.id, opacity: 2 }] }),
    /opacity must be a finite number in \[0,1\]/,
  );
  assert.throws(
    () => patchElements(root, { projectId: pid, patches: [{ elementId: a.id, opacity: 0.5 }, { elementId: text.id, filters: { brightness: 1.2 } }] }),
    /filters are image-only/,
  );
  assert.equal(byId(getProject(root, pid), a.id).opacity, undefined, "nothing written after either throw");
});

// ---- buildRenderNodes forwards filters (spec JSON, independent of Python availability) --
//
// ops.compositeGroup writes the render spec JSON to disk BEFORE spawning render_group.py,
// so reading it back proves buildRenderNodes forwarded `filters` regardless of whether the
// studio venv/Pillow is actually available on this machine.

test("buildRenderNodes forwards filters verbatim on an image paint node", async (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "ForwardFilters" }).id;
  const element = img(root, pid, "bg", 10, 10);
  const group = createGroup(root, { projectId: pid, name: "Screen", x: 0, y: 0, w: 100, h: 100 }).group;
  assignToGroup(root, { projectId: pid, elementIds: [element.id], groupId: group.id });
  patchElement(root, pid, element.id, { filters: { brightness: 1.4, tint: { color: "#112233", strength: 0.5 } } });

  try {
    await renderGroup(root, { projectId: pid, groupId: group.id, scale: 1 });
  } catch {
    // Python/Pillow may be unavailable here — the spec file below is written BEFORE the
    // spawn (ops.compositeGroup), so this assertion never depends on Python succeeding.
  }
  const exportRoot = join(canvasProjectsRoot(root), pid, "export");
  const stamp = readdirSync(exportRoot)[0];
  const spec = JSON.parse(readFileSync(join(exportRoot, stamp, "render_spec.json"), "utf8"));
  const node = spec.children.find((child) => child.kind === "element");
  assert.deepEqual(node.filters, { brightness: 1.4, tint: { color: "#112233", strength: 0.5 } });
});

// ---- render/export parity: canvas <-> PIL (the genuine hazard) ----------------------
//
// An independently-derived (not imported) reimplementation of the SAME canonical formulas
// the packet specifies (README "Image filters") — importing apply_filters/render_group.py
// would make this a circular, load-bearing-nothing test.
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

// One 32x24 fixture, quartered, so ONE render exercises: a saturated color (general
// brightness/saturation/contrast/tint math), a mid-gray (contrast-pivot sanity), a fully
// transparent region, and a semi-transparent region (tint-vs-alpha) — all through a
// single Python spawn. scale:1 and box == source dims (addImage defaults element.w/h to
// the source size), so render_group.py's resize step is a no-op: pixels map 1:1, no
// resample blur at the quadrant seams.
async function renderFilteredQuadrants(t, filters) {
  const root = tempProjects(t);
  const W = 32;
  const H = 24;
  const TL = [200, 50, 50, 255]; // saturated red — general filter math
  const TR = [128, 128, 128, 255]; // mid-gray — contrast-pivot sanity
  const BL = [40, 60, 80, 0]; // fully transparent — tint must not resurrect it
  const BR = [100, 150, 200, 128]; // semi-transparent — alpha must survive exactly
  const bytes = encodePng(
    W,
    H,
    (x, y) => {
      const left = x < W / 2;
      const top = y < H / 2;
      if (top) return left ? TL : TR;
      return left ? BL : BR;
    },
    { alpha: true },
  );

  const pid = createProject(root, { title: "FilterParity" }).id;
  const element = addImage(root, pid, { name: "quad.png", bytes, x: 0, y: 0 }).element;
  assert.equal(element.w, W);
  assert.equal(element.h, H);
  const group = createGroup(root, { projectId: pid, name: "Screen", x: 0, y: 0, w: 100, h: 100 }).group;
  assignToGroup(root, { projectId: pid, elementIds: [element.id], groupId: group.id });
  patchElement(root, pid, element.id, { filters });

  let result;
  try {
    result = await renderGroup(root, { projectId: pid, groupId: group.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return null;
  }
  const png = decodePng(readFileSync(result.path));
  return { png, TL, TR, BL, BR, W, H };
}

test("render parity: brightness+saturation+contrast+tint match the JS-computed expectation channel-exact within ±1", async (t) => {
  const filters = { brightness: 1.2, saturation: 0.4, contrast: 1.3, tint: { color: "#0000ff", strength: 0.4 } };
  const fixture = await renderFilteredQuadrants(t, filters);
  if (!fixture) return;
  const { png, TL, TR, W, H } = fixture;

  const expectedTL = applyFiltersJs(TL, filters);
  const actualTL = png.at(W / 4, H / 4);
  for (let c = 0; c < 3; c += 1) {
    assert.ok(closeChannel(actualTL[c], expectedTL[c]), `TL channel ${c}: got ${actualTL[c]}, expected ${expectedTL[c]}`);
  }
  assert.equal(actualTL[3], 255, "TL stays fully opaque");

  const expectedTR = applyFiltersJs(TR, filters);
  const actualTR = png.at((3 * W) / 4, H / 4);
  for (let c = 0; c < 3; c += 1) {
    assert.ok(closeChannel(actualTR[c], expectedTR[c]), `TR channel ${c}: got ${actualTR[c]}, expected ${expectedTR[c]}`);
  }
});

test("render parity: tint respects alpha — a fully transparent pixel stays transparent, a semi-transparent one keeps its exact alpha", async (t) => {
  const filters = { tint: { color: "#00ff00", strength: 0.6 } };
  const fixture = await renderFilteredQuadrants(t, filters);
  if (!fixture) return;
  const { png, W, H } = fixture;

  const actualBL = png.at(W / 4, (3 * H) / 4);
  assert.equal(actualBL[3], 0, "the fully transparent quadrant stays alpha 0 — tint never resurrects it");

  const actualBR = png.at((3 * W) / 4, (3 * H) / 4);
  assert.equal(actualBR[3], 128, "the semi-transparent quadrant keeps its EXACT original alpha");
});

test("render parity: saturation=0 desaturates via the 0.2126/0.7152/0.0722 luma, NOT PIL's default 0.299/0.587/0.114", async (t) => {
  const root = tempProjects(t);
  const RED = [200, 50, 50];
  const bytes = solidPng(16, 16, RED);
  const pid = createProject(root, { title: "SaturationZero" }).id;
  const element = addImage(root, pid, { name: "flat.png", bytes, x: 0, y: 0 }).element;
  const group = createGroup(root, { projectId: pid, name: "Screen", x: 0, y: 0, w: 60, h: 60 }).group;
  assignToGroup(root, { projectId: pid, elementIds: [element.id], groupId: group.id });
  patchElement(root, pid, element.id, { filters: { saturation: 0 } });

  let result;
  try {
    result = await renderGroup(root, { projectId: pid, groupId: group.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const png = decodePng(readFileSync(result.path));
  const actual = png.at(8, 8);

  const luma709 = Math.round(0.2126 * RED[0] + 0.7152 * RED[1] + 0.0722 * RED[2]); // ~82
  const luma601 = Math.round(0.299 * RED[0] + 0.587 * RED[1] + 0.114 * RED[2]); // ~95 (PIL's default — the trap)
  assert.ok(Math.abs(luma709 - luma601) > 5, "sanity: the two luma conventions must actually differ on this fixture");

  for (let c = 0; c < 3; c += 1) {
    assert.ok(closeChannel(actual[c], luma709), `channel ${c}: got ${actual[c]}, expected the 0.2126/0.7152/0.0722 gray ${luma709}`);
    assert.ok(!closeChannel(actual[c], luma601, 3), `channel ${c}: ${actual[c]} must NOT match PIL's default 0.299/0.587/0.114 gray ${luma601}`);
  }
});

test("render parity: contrast pivots at 0.5 — a mid-gray pixel stays ~unchanged for any contrast", async (t) => {
  const root = tempProjects(t);
  const GRAY = [128, 128, 128];
  const bytes = solidPng(16, 16, GRAY);
  const pid = createProject(root, { title: "ContrastPivot" }).id;
  const element = addImage(root, pid, { name: "gray.png", bytes, x: 0, y: 0 }).element;
  const group = createGroup(root, { projectId: pid, name: "Screen", x: 0, y: 0, w: 60, h: 60 }).group;
  assignToGroup(root, { projectId: pid, elementIds: [element.id], groupId: group.id });
  patchElement(root, pid, element.id, { filters: { contrast: 1.8 } });

  let result;
  try {
    result = await renderGroup(root, { projectId: pid, groupId: group.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const png = decodePng(readFileSync(result.path));
  const actual = png.at(8, 8);
  for (let c = 0; c < 3; c += 1) {
    assert.ok(closeChannel(actual[c], GRAY[c], 2), `channel ${c}: got ${actual[c]}, expected ~${GRAY[c]} (contrast pivots at mid-gray)`);
  }
});

// ---- HTTP adapter: PATCH .../elements/<eid> accepts filters (generic passthrough) -----

function invokeApi(handler, method, path, body) {
  const req = new EventEmitter();
  req.method = method;
  req.setEncoding = () => {};
  req.destroy = () => {};
  const chunks = [];
  const res = {
    statusCode: 0,
    headers: {},
    writeHead(status, headers) {
      this.statusCode = status;
      this.headers = headers || {};
    },
    end(chunk) {
      if (chunk !== undefined && chunk !== null && chunk !== "") chunks.push(Buffer.from(chunk));
      const buffer = Buffer.concat(chunks);
      this._resolve({ status: this.statusCode, json: () => JSON.parse(buffer.toString("utf8")) });
    },
  };
  const done = new Promise((r) => {
    res._resolve = r;
  });
  handler(req, res, new URL(path, "http://canvas.local"));
  queueMicrotask(() => {
    if (body !== undefined) req.emit("data", Buffer.from(JSON.stringify(body)));
    req.emit("end");
  });
  return done;
}

test("HTTP PATCH .../elements/<eid> sets filters + opacity; an out-of-range value is a loud 400", async (t) => {
  const root = tempProjects(t);
  const handler = createCanvasApi(root);
  const pid = createProject(root, { title: "HttpFilters" }).id;
  const e = img(root, pid, "sprite", 10, 10);

  const ok = await invokeApi(handler, "PATCH", `/api/canvas/projects/${pid}/elements/${e.id}`, {
    opacity: 0.7,
    filters: { brightness: 0.7, saturation: 0.6 },
  });
  assert.equal(ok.status, 200);
  const after = getProject(root, pid);
  assert.equal(byId(after, e.id).opacity, 0.7);
  assert.deepEqual(byId(after, e.id).filters, { brightness: 0.7, saturation: 0.6 });

  const bad = await invokeApi(handler, "PATCH", `/api/canvas/projects/${pid}/elements/${e.id}`, {
    filters: { brightness: 5 },
  });
  assert.equal(bad.status, 400);
});

// ---- CLI parity -----------------------------------------------------------------

test("cli element-set --filters-json / --opacity parity, incl. null clearing", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-filters-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const run = (...args) => {
    const out = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(out.trim().split("\n").filter(Boolean).at(-1));
  };

  const pngPath = join(dir, "sprite.png");
  writeFileSync(pngPath, solidPng(10, 10, [1, 1, 1]));
  const projectId = run("create", "--title", "CLI Filters").project.id;
  const elementId = run("add-image", projectId, "--file", pngPath).element.id;

  const set = run(
    "element-set",
    projectId,
    "--element",
    elementId,
    "--opacity",
    "0.7",
    "--filters-json",
    '{"brightness":0.7,"saturation":0.6}',
  );
  assert.equal(set.element.opacity, 0.7);
  assert.deepEqual(set.element.filters, { brightness: 0.7, saturation: 0.6 });

  const shown = run("show", projectId).project;
  assert.deepEqual(shown.elements.find((e) => e.id === elementId).filters, { brightness: 0.7, saturation: 0.6 });

  const cleared = run("element-set", projectId, "--element", elementId, "--filters-json", "null");
  assert.equal("filters" in cleared.element, false, "--filters-json null clears to absent");

  assert.throws(() => run("element-set", projectId, "--element", elementId, "--filters-json", '{"brightness":9}'));
});
