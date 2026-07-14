// T0232 increment 3a — rotation + flip: data model (patchElement/patchElements
// whitelist + validation/normalization), the R7 region-op refusals, and the
// canvas<->PIL render/export parity contract (README "Rotation & flip"). Run:
//   node --test ai_studio/assets/canvas/tests/transform.test.mjs
//
// The render-parity test drives the REAL render_group.py through the warm worker
// (renderGroup) and skips cleanly when the studio venv / Pillow is unavailable
// (mirrors the other render smoke tests). Every other test here is pure/store-level and never
// touches Python: the R7 guards all fire BEFORE any disk read/spawn (fail-fast), so the
// refusal tests are deterministic and always run.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import {
  addImage,
  addText,
  alphaCutout,
  assignToGroup,
  createGroup,
  createProject,
  detectRegions,
  getProject,
  patchElement,
  patchElements,
  renderGroup,
  setRegions,
  sliceRegions,
  undoOp,
} from "../ops.mjs";
import { createCanvasApi } from "../api.mjs";
import { decodePng, encodePng, solidPng } from "./png_fixture.mjs";

const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

// Returns REPO_ROOT (not the temp dir) — every ops call below needs the REAL repo root
// for relative lookups (the fonts manifest addText validates against, the python tool
// scripts renderGroup spawns); actual project.json/files/ storage is redirected to the
// temp dir via CANVAS_PROJECTS_ROOT, exactly like ops.test.mjs.
function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-transform-"));
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

test("patchElement rotation normalizes to [0,360) and clears to ABSENT at 0; flip stores as an ABSENT-when-false boolean", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "Normalize" }).id;
  const e = img(root, pid, "sprite", 10, 10);

  patchElement(root, pid, e.id, { rotation: 450 });
  assert.equal(byId(getProject(root, pid), e.id).rotation, 90, "450 wraps to 90");

  patchElement(root, pid, e.id, { rotation: -90 });
  assert.equal(byId(getProject(root, pid), e.id).rotation, 270, "-90 wraps to 270");

  patchElement(root, pid, e.id, { rotation: 0 });
  assert.equal("rotation" in byId(getProject(root, pid), e.id), false, "rotation:0 clears to an absent field");

  patchElement(root, pid, e.id, { flipH: true });
  assert.equal(byId(getProject(root, pid), e.id).flipH, true);
  patchElement(root, pid, e.id, { flipH: false });
  assert.equal("flipH" in byId(getProject(root, pid), e.id), false, "flipH:false clears to an absent field");

  patchElement(root, pid, e.id, { flipV: true });
  assert.equal(byId(getProject(root, pid), e.id).flipV, true);
});

test("patchElement rejects a non-finite rotation and a non-boolean flip flag; flip is image-only, rotation is not", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "Reject" }).id;
  const e = img(root, pid, "sprite", 10, 10);
  const text = addText(root, pid, { content: "Hi" }).element;

  assert.throws(() => patchElement(root, pid, e.id, { rotation: "sideways" }), /finite number of degrees/);
  assert.throws(() => patchElement(root, pid, e.id, { rotation: Infinity }), /finite number of degrees/);
  assert.throws(() => patchElement(root, pid, e.id, { flipH: "yes" }), /flipH must be a boolean/);
  assert.throws(() => patchElement(root, pid, e.id, { flipV: 1 }), /flipV must be a boolean/);

  // Flip is image-only — loud on a text element.
  assert.throws(() => patchElement(root, pid, text.id, { flipH: true }), /flip is image-only/);
  assert.throws(() => patchElement(root, pid, text.id, { flipV: true }), /flip is image-only/);

  // Rotation IS allowed on text ("rotates the box") — no error, just stored + normalized.
  patchElement(root, pid, text.id, { rotation: 30 });
  assert.equal(byId(getProject(root, pid), text.id).rotation, 30);
});

test("patchElements batches rotation across several elements in ONE journal entry; undo restores both", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "Batch" }).id;
  const a = img(root, pid, "a", 10, 10);
  const b = img(root, pid, "b", 10, 10);
  const seq0 = getProject(root, pid).history_seq;

  patchElements(root, {
    projectId: pid,
    patches: [
      { elementId: a.id, rotation: 45 },
      { elementId: b.id, flipH: true },
    ],
  });
  const after = getProject(root, pid);
  assert.equal(after.history_seq, seq0 + 1, "one journal entry for the whole batch");
  assert.equal(byId(after, a.id).rotation, 45);
  assert.equal(byId(after, b.id).flipH, true);

  undoOp(root, { projectId: pid });
  const undone = getProject(root, pid);
  assert.equal("rotation" in byId(undone, a.id), false, "undo restores a's rotation to absent");
  assert.equal("flipH" in byId(undone, b.id), false, "undo restores b's flipH to absent");
});

// ---- R7: region ops refuse loudly on a transformed element (no Python needed — the ----
// ---- guard fires before any read/spawn, so these run deterministically) --------------

test("detectRegions refuses loudly on a rotated element", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "DetectRefuse" }).id;
  const e = img(root, pid, "sprite", 10, 10);
  patchElement(root, pid, e.id, { rotation: 45 });
  return assert.rejects(
    () => detectRegions(root, { projectId: pid, elementId: e.id }),
    /rotated\/flipped — reset rotation\/flip to edit regions or slice/,
  );
});

test("sliceRegions refuses loudly on a flipped element (regions seeded via the pure setRegions op, no detect needed)", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "SliceRefuse" }).id;
  const e = img(root, pid, "sprite", 20, 20);
  setRegions(root, { projectId: pid, elementId: e.id, regions: [{ id: "r1", rect: [0, 0, 10, 10] }] });
  patchElement(root, pid, e.id, { flipV: true });
  return assert.rejects(
    () => sliceRegions(root, { projectId: pid, elementId: e.id }),
    /rotated\/flipped — reset rotation\/flip to edit regions or slice/,
  );
});

test("alphaCutout (single) refuses loudly on a transformed element", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "AlphaRefuse" }).id;
  const e = img(root, pid, "sprite", 10, 10);
  patchElement(root, pid, e.id, { rotation: 90 });
  return assert.rejects(
    () => alphaCutout(root, { projectId: pid, elementId: e.id, method: "matte" }),
    /rotated\/flipped — reset rotation\/flip to edit regions or slice/,
  );
});

test("alphaCutout batch refuses ATOMICALLY when ANY element is transformed — nothing spawns, nothing is mutated", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "AlphaBatchRefuse" }).id;
  const a = img(root, pid, "a", 10, 10);
  const b = img(root, pid, "b", 10, 10);
  // The TRANSFORMED element is listed SECOND: the whole batch must still refuse before
  // element `a` (untouched) is ever spawned through Python.
  patchElement(root, pid, b.id, { rotation: 10 });
  const seq0 = getProject(root, pid).history_seq;
  const srcA0 = getProject(root, pid).elements.find((el) => el.id === a.id).src;

  return assert.rejects(
    () => alphaCutout(root, { projectId: pid, elementIds: [a.id, b.id], method: "matte" }),
    /rotated\/flipped — reset rotation\/flip to edit regions or slice/,
  ).then(() => {
    const after = getProject(root, pid);
    assert.equal(after.history_seq, seq0, "no journal entry — the batch never committed");
    assert.equal(after.elements.find((el) => el.id === a.id).src, srcA0, "a's src is untouched (never spawned)");
  });
});

// ---- HTTP adapter: PATCH .../elements/<eid> accepts rotation/flip (generic passthrough) --

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

test("HTTP PATCH .../elements/<eid> sets rotation/flipH; a non-finite rotation is a loud 400", async (t) => {
  const root = tempProjects(t);
  const handler = createCanvasApi(root);
  const pid = createProject(root, { title: "HttpTransform" }).id;
  const e = img(root, pid, "sprite", 10, 10);

  const ok = await invokeApi(handler, "PATCH", `/api/canvas/projects/${pid}/elements/${e.id}`, {
    rotation: 90,
    flipH: true,
  });
  assert.equal(ok.status, 200);
  const after = getProject(root, pid);
  assert.equal(byId(after, e.id).rotation, 90);
  assert.equal(byId(after, e.id).flipH, true);

  const bad = await invokeApi(handler, "PATCH", `/api/canvas/projects/${pid}/elements/${e.id}`, { rotation: "bad" });
  assert.equal(bad.status, 400);
});

// ---- CLI parity -----------------------------------------------------------------

test("cli element-set --rotation/--flip-h/--flip-v parity", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-transform-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const run = (...args) => {
    const out = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(out.trim().split("\n").filter(Boolean).at(-1));
  };

  const pngPath = join(dir, "sprite.png");
  writeFileSync(pngPath, solidPng(10, 10, [1, 1, 1]));
  const projectId = run("create", "--title", "CLI Transform").project.id;
  const elementId = run("add-image", projectId, "--file", pngPath).element.id;

  const set = run("element-set", projectId, "--element", elementId, "--rotation", "450", "--flip-h", "true");
  assert.equal(set.element.rotation, 90, "450 normalizes to 90 through the CLI too");
  assert.equal(set.element.flipH, true);

  const shown = run("show", projectId).project;
  assert.equal(shown.elements.find((e) => e.id === elementId).rotation, 90);

  assert.throws(() => run("element-set", projectId, "--element", elementId, "--rotation", "abc"));
});

// ---- render/export parity: canvas <-> PIL (the one genuine hazard, T0232 R2) ----------
//
// A rigorously-derived (not hand-waved) expectation: the R2 contract is "world offset
// from the box center after flip (innermost) then a CW rotation by `rotation` degrees",
// applied identically by both renderers. This helper recomputes that SAME transform
// independently of ops.mjs/tree.mjs/render_group.py (it would be a circular, load-bearing-
// nothing test to import the code under test to check the code under test), so a sign or
// axis-order bug in either renderer shows up as a wrong pixel here.
function transformOffset(dx, dy, rotationDeg, flipH, flipV) {
  const x0 = flipH ? -dx : dx;
  const y0 = flipV ? -dy : dy;
  const rad = (rotationDeg * Math.PI) / 180;
  const cos = Math.cos(rad);
  const sin = Math.sin(rad);
  return { x: x0 * cos - y0 * sin, y: x0 * sin + y0 * cos };
}

function closeColor(actual, expected, tol = 40) {
  return Math.abs(actual[0] - expected[0]) <= tol && Math.abs(actual[1] - expected[1]) <= tol && Math.abs(actual[2] - expected[2]) <= tol;
}

test("render parity: a rotated+flipped element renders through render_group.py at the geometrically-predicted pixel (center/angle/size)", async (t) => {
  const root = tempProjects(t);
  const BG = [15, 18, 24];
  const MARKER = [220, 40, 40];
  const W = 24;
  const H = 16;
  const MARK = 8; // marker block size — generous margin against any 1px rounding
  // A solid BG image with a MARKER block anchored at the source's TRUE top-left corner —
  // its center-of-mass (MARK/2, MARK/2) is what we track through the transform.
  const bytes = encodePng(W, H, (x, y) => (x < MARK && y < MARK ? MARKER : BG));

  const pid = createProject(root, { title: "RenderParity" }).id;
  const element = addImage(root, pid, { name: "marker.png", bytes, x: 50, y: 50 }).element;
  assert.equal(element.w, W);
  assert.equal(element.h, H);
  // Explicit, generously-sized group bounds (not fromElements' 24px auto-pad) so the
  // origin (ox,oy) is a known (0,0) and the rotated/expanded footprint never clips.
  const group = createGroup(root, { projectId: pid, name: "Screen", x: 0, y: 0, w: 200, h: 200 }).group;
  assignToGroup(root, { projectId: pid, elementIds: [element.id], groupId: group.id });

  const cx = element.x + element.w / 2; // 62 — element box center, relative to origin (0,0)
  const cy = element.y + element.h / 2; // 58
  const sourceCenterOffset = { x: MARK / 2 - W / 2, y: MARK / 2 - H / 2 }; // marker center - source center

  const cases = [
    { label: "rotation=90", patch: { rotation: 90 }, rotation: 90, flipH: false, flipV: false },
    { label: "rotation=0,flipH=true", patch: { rotation: 0, flipH: true }, rotation: 0, flipH: true, flipV: false },
    { label: "rotation=90,flipH=true (composition order)", patch: { rotation: 90 }, rotation: 90, flipH: true, flipV: false },
  ];

  for (const kase of cases) {
    patchElement(root, pid, element.id, kase.patch);
    let result;
    try {
      result = await renderGroup(root, { projectId: pid, groupId: group.id, scale: 1 });
    } catch (error) {
      t.skip(`render_group.py / PIL unavailable: ${error.message}`);
      return;
    }
    const png = decodePng(readFileSync(result.path));

    const transformed = transformOffset(sourceCenterOffset.x, sourceCenterOffset.y, kase.rotation, kase.flipH, kase.flipV);
    const expected = { x: Math.round(cx + transformed.x), y: Math.round(cy + transformed.y) };
    const actual = png.at(expected.x, expected.y);
    assert.ok(
      closeColor(actual, MARKER),
      `${kase.label}: expected MARKER-ish color at predicted pixel (${expected.x},${expected.y}), got [${actual}]`,
    );

    // Sanity: the UNTRANSFORMED (identity) predicted pixel must NOT show the marker for a
    // case that actually moves it — proves the transform had a real effect, not a no-op.
    const identity = { x: Math.round(cx + sourceCenterOffset.x), y: Math.round(cy + sourceCenterOffset.y) };
    if (identity.x !== expected.x || identity.y !== expected.y) {
      const atIdentity = png.at(identity.x, identity.y);
      assert.ok(
        !closeColor(atIdentity, MARKER),
        `${kase.label}: the marker should have MOVED away from its untransformed position (${identity.x},${identity.y}), but it is still there`,
      );
    }
  }
});
