// T0260 Track A (increment 1) — procedural animation: the shared pure module
// (animation.mjs: validateAnimation + sampleAnimation), the setElementAnimation op,
// static element.opacity on patchElement, the API PUT animation / PATCH opacity routes,
// the CLI animation-set / element-set --opacity, and render-pixel parity for a
// translucent element through the REAL render_group.py. Run:
//   node --test ai_studio/assets/canvas/tests/animation.test.mjs
//
// The ONE render-pixel test drives the real render_group.py through the warm worker
// (renderGroup — same harness pattern as slice9.test.mjs / export.test.mjs) and skips
// cleanly when the studio venv / Pillow is unavailable. Every other test is pure/store/
// API/CLI level and never touches Python.
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
  assignToGroup,
  createGroup,
  createProject,
  getProject,
  patchElement,
  renderGroup,
  setElementAnimation,
  undoOp,
} from "../ops.mjs";
import { sampleAnimation, validateAnimation } from "../animation.mjs";
import { createCanvasApi } from "../api.mjs";
import { decodePng, solidPng } from "./png_fixture.mjs";

// REPO_ROOT (not the temp dir): renderGroup + the CLI need the real repo root to find
// tools/render_group.py + cli.mjs; project storage is redirected via CANVAS_PROJECTS_ROOT.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-anim-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return REPO_ROOT;
}

function img(root, pid, name, w, h, color = [10, 20, 30], x = 0, y = 0) {
  return addImage(root, pid, { name, bytes: solidPng(w, h, color), x, y }).element;
}

function byId(project, id) {
  return (project.elements || []).find((item) => item.id === id);
}

// ---- validateAnimation: loud cases (pure, no Python) ---------------------------

test("validateAnimation accepts an osc channel and normalizes it, dropping default phase/center", () => {
  const out = validateAnimation({ channels: [{ prop: "off_x", kind: "osc", amplitude: 5, period_ms: 100, phase: 0, center: 0 }] });
  assert.deepEqual(out, { v: 1, channels: [{ prop: "off_x", kind: "osc", amplitude: 5, period_ms: 100 }] });
  // A non-default phase + center (center != the property identity 1 for scale) are kept.
  const scale = validateAnimation({ channels: [{ prop: "scale", kind: "osc", amplitude: 0.2, period_ms: 800, phase: 0.25, center: 1.5 }] });
  assert.deepEqual(scale.channels[0], { prop: "scale", kind: "osc", amplitude: 0.2, period_ms: 800, phase: 0.25, center: 1.5 });
  // center == the property identity (1 for scale) is dropped like a default.
  const idCenter = validateAnimation({ channels: [{ prop: "scale", kind: "osc", amplitude: 0.2, period_ms: 800, center: 1 }] });
  assert.equal("center" in idCenter.channels[0], false);
});

test("validateAnimation accepts an opacity osc channel (opacity is a valid animatable property)", () => {
  const out = validateAnimation({ channels: [{ prop: "opacity", kind: "osc", amplitude: 0.3, period_ms: 500 }] });
  assert.equal(out.channels[0].prop, "opacity");
  assert.equal(out.channels[0].kind, "osc");
});

test("validateAnimation accepts a keyframes channel starting at 0 with strictly increasing t_ms", () => {
  const out = validateAnimation({ channels: [{ prop: "off_y", kind: "keyframes", points: [{ t_ms: 0, v: 0 }, { t_ms: 400, v: 80 }, { t_ms: 1000, v: 0 }] }] });
  assert.deepEqual(out.channels[0].points, [{ t_ms: 0, v: 0 }, { t_ms: 400, v: 80 }, { t_ms: 1000, v: 0 }]);
});

test("validateAnimation rejects an unknown kind", () => {
  assert.throws(() => validateAnimation({ channels: [{ prop: "off_x", kind: "wiggle", amplitude: 1, period_ms: 100 }] }), /kind must be osc or keyframes/);
});

test("validateAnimation rejects an unknown / duplicate target property", () => {
  assert.throws(() => validateAnimation({ channels: [{ prop: "color", kind: "osc", amplitude: 1, period_ms: 100 }] }), /prop must be one of/);
  assert.throws(
    () => validateAnimation({ channels: [
      { prop: "off_x", kind: "osc", amplitude: 1, period_ms: 100 },
      { prop: "off_x", kind: "osc", amplitude: 2, period_ms: 100 },
    ] }),
    /more than one channel targeting "off_x"/,
  );
});

test("validateAnimation rejects non-increasing keyframes and a first point not at 0", () => {
  assert.throws(
    () => validateAnimation({ channels: [{ prop: "off_y", kind: "keyframes", points: [{ t_ms: 0, v: 0 }, { t_ms: 0, v: 5 }] }] }),
    /strictly increasing/,
  );
  assert.throws(
    () => validateAnimation({ channels: [{ prop: "off_y", kind: "keyframes", points: [{ t_ms: 100, v: 0 }, { t_ms: 200, v: 5 }] }] }),
    /must start at t_ms 0/,
  );
  assert.throws(
    () => validateAnimation({ channels: [{ prop: "off_y", kind: "keyframes", points: [{ t_ms: 0, v: 0 }] }] }),
    /points must be an array of >= 2/,
  );
});

test("validateAnimation rejects a period_ms <= 0 and a non-finite amplitude", () => {
  assert.throws(() => validateAnimation({ channels: [{ prop: "off_x", kind: "osc", amplitude: 1, period_ms: 0 }] }), /period_ms must be a finite number > 0/);
  assert.throws(() => validateAnimation({ channels: [{ prop: "off_x", kind: "osc", amplitude: 1, period_ms: -5 }] }), /period_ms must be a finite number > 0/);
  assert.throws(() => validateAnimation({ channels: [{ prop: "off_x", kind: "osc", amplitude: NaN, period_ms: 100 }] }), /amplitude must be a finite number/);
});

test("validateAnimation rejects an out-of-range phase and a non-object / empty spec", () => {
  assert.throws(() => validateAnimation({ channels: [{ prop: "off_x", kind: "osc", amplitude: 1, period_ms: 100, phase: 1 }] }), /phase must be a number in \[0,1\)/);
  assert.throws(() => validateAnimation({ channels: [{ prop: "off_x", kind: "osc", amplitude: 1, period_ms: 100, phase: -0.1 }] }), /phase must be a number in \[0,1\)/);
  assert.throws(() => validateAnimation(null), /animation must be an object/);
  assert.throws(() => validateAnimation({ channels: [] }), /non-empty array/);
});

// ---- sampleAnimation: math, determinism, loop stability (pure, no Python) -------

test("sampleAnimation osc math: t=0 -> center, quarter -> +amplitude, half -> center, three-quarter -> -amplitude", () => {
  const spec = validateAnimation({ channels: [{ prop: "off_x", kind: "osc", amplitude: 10, period_ms: 1000 }] });
  assert.equal(sampleAnimation(spec, 0).offX, 0); // sin 0
  assert.equal(sampleAnimation(spec, 250).offX, 10); // sin(pi/2) = 1 exactly
  assert.ok(Math.abs(sampleAnimation(spec, 500).offX) < 1e-9); // sin(pi) ~ 0
  assert.equal(sampleAnimation(spec, 750).offX, -10); // sin(3pi/2) = -1 exactly
  // Unanimated properties yield their identity (0 for offsets/rot, 1 for scale/opacity).
  const at250 = sampleAnimation(spec, 250);
  assert.deepEqual({ offY: at250.offY, rot: at250.rot, scale: at250.scale, opacity: at250.opacity }, { offY: 0, rot: 0, scale: 1, opacity: 1 });
});

test("sampleAnimation osc center defaults to the property identity (scale wobbles around 1)", () => {
  const spec = validateAnimation({ channels: [{ prop: "scale", kind: "osc", amplitude: 0.2, period_ms: 1000 }] });
  assert.equal(sampleAnimation(spec, 0).scale, 1); // center = identity 1
  assert.ok(Math.abs(sampleAnimation(spec, 250).scale - 1.2) < 1e-9);
  assert.ok(Math.abs(sampleAnimation(spec, 750).scale - 0.8) < 1e-9);
});

test("sampleAnimation clamps the composed opacity multiplier to [0,1] after composition", () => {
  // center defaults to 1; +0.5 at the quarter -> 1.5 clamps to 1, -0.5 at 3/4 -> 0.5 stays.
  const spec = validateAnimation({ channels: [{ prop: "opacity", kind: "osc", amplitude: 0.5, period_ms: 1000 }] });
  assert.equal(sampleAnimation(spec, 250).opacity, 1); // 1.5 clamped
  assert.ok(Math.abs(sampleAnimation(spec, 750).opacity - 0.5) < 1e-9);
});

test("sampleAnimation is deterministic (same spec + tMs -> deep-equal result)", () => {
  const spec = validateAnimation({ channels: [{ prop: "rot", kind: "osc", amplitude: 30, period_ms: 640 }] });
  assert.deepEqual(sampleAnimation(spec, 123.4), sampleAnimation(spec, 123.4));
});

test("sampleAnimation osc is loop-stable: sample at t and t + N*period are identical (no drift for large tMs)", () => {
  const spec = validateAnimation({ channels: [{ prop: "off_x", kind: "osc", amplitude: 7, period_ms: 1000 }] });
  const base = sampleAnimation(spec, 137).offX;
  assert.equal(sampleAnimation(spec, 137 + 1000 * 3).offX, base);
  assert.equal(sampleAnimation(spec, 137 + 1000 * 1e9).offX, base); // arbitrarily large tMs, no drift
});

test("sampleAnimation keyframes interpolate linearly and wrap over the last point's t_ms", () => {
  const spec = validateAnimation({ channels: [{ prop: "off_y", kind: "keyframes", points: [{ t_ms: 0, v: 0 }, { t_ms: 1000, v: 100 }] }] });
  assert.equal(sampleAnimation(spec, 0).offY, 0);
  assert.equal(sampleAnimation(spec, 250).offY, 25);
  assert.equal(sampleAnimation(spec, 500).offY, 50);
  assert.equal(sampleAnimation(spec, 1000).offY, 0); // wrap: tMod = 0
  assert.equal(sampleAnimation(spec, 1500).offY, 50); // wrap: tMod = 500
});

test("sampleAnimation keyframes are loop-stable: t and t + N*loop identical", () => {
  const spec = validateAnimation({ channels: [{ prop: "off_y", kind: "keyframes", points: [{ t_ms: 0, v: 0 }, { t_ms: 400, v: 80 }, { t_ms: 1000, v: 0 }] }] });
  const base = sampleAnimation(spec, 137).offY;
  assert.equal(sampleAnimation(spec, 137 + 1000 * 4).offY, base);
  assert.equal(sampleAnimation(spec, 137 + 1000 * 1e6).offY, base);
});

// ---- setElementAnimation op: set / clear / undo (pure, no Python) ---------------

test("setElementAnimation sets a spec, journals one entry, and undo restores the previous (absent) animation", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "AnimOp" }).id;
  const element = img(root, pid, "sprite", 8, 8);
  const seq0 = getProject(root, pid).history_seq;

  const result = setElementAnimation(root, { projectId: pid, elementId: element.id, animation: { channels: [{ prop: "off_x", kind: "osc", amplitude: 5, period_ms: 500 }] } });
  assert.equal(result.element.animation.v, 1);
  assert.equal(result.element.animation.channels.length, 1);
  assert.equal(getProject(root, pid).history_seq, seq0 + 1, "one journal entry");

  const undone = undoOp(root, { projectId: pid }).project;
  assert.equal("animation" in byId(undone, element.id), false, "undo restores the pre-set (absent) animation");
});

test("setElementAnimation with animation:null clears a previously-set animation (undoable)", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "AnimClear" }).id;
  const element = img(root, pid, "sprite", 8, 8);
  setElementAnimation(root, { projectId: pid, elementId: element.id, animation: { channels: [{ prop: "rot", kind: "osc", amplitude: 15, period_ms: 500 }] } });
  const seq1 = getProject(root, pid).history_seq;

  const cleared = setElementAnimation(root, { projectId: pid, elementId: element.id, animation: null });
  assert.equal("animation" in cleared.element, false);
  assert.equal(getProject(root, pid).history_seq, seq1 + 1);

  const undone = undoOp(root, { projectId: pid }).project;
  assert.equal(byId(undone, element.id).animation.channels[0].prop, "rot", "undo restores the cleared animation");
});

test("setElementAnimation allows BOTH image and text elements (geometry/opacity-level, not pixel-level)", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "AnimTypes" }).id;
  const image = img(root, pid, "sprite", 8, 8);
  const text = addText(root, pid, { content: "Hi" }).element;
  const spec = { channels: [{ prop: "off_y", kind: "keyframes", points: [{ t_ms: 0, v: 0 }, { t_ms: 500, v: 10 }] }] };
  assert.equal(setElementAnimation(root, { projectId: pid, elementId: image.id, animation: spec }).element.animation.channels.length, 1);
  assert.equal(setElementAnimation(root, { projectId: pid, elementId: text.id, animation: spec }).element.animation.channels.length, 1);
});

test("setElementAnimation surfaces validateAnimation's loud error and writes nothing", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "AnimLoud" }).id;
  const element = img(root, pid, "sprite", 8, 8);
  const seq0 = getProject(root, pid).history_seq;
  assert.throws(
    () => setElementAnimation(root, { projectId: pid, elementId: element.id, animation: { channels: [{ prop: "off_x", kind: "osc", amplitude: 1, period_ms: 0 }] } }),
    /period_ms must be a finite number > 0/,
  );
  assert.equal(getProject(root, pid).history_seq, seq0, "no journal entry on a rejected set");
});

test("setElementAnimation requires projectId/elementId and 404s on an unknown element", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "AnimReq" }).id;
  assert.throws(() => setElementAnimation(root, { elementId: "x", animation: null }), /requires projectId/);
  assert.throws(() => setElementAnimation(root, { projectId: pid, animation: null }), /requires elementId/);
  assert.throws(() => setElementAnimation(root, { projectId: pid, elementId: "nope", animation: null }), /element not found/);
});

// ---- static element.opacity on patchElement (pure, no Python) -------------------

test("patchElement stores opacity only when != 1 (opacity 1 deletes the field, like slice9.scale / rotation:0)", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "Opacity" }).id;
  const element = img(root, pid, "sprite", 8, 8);
  assert.equal("opacity" in byId(getProject(root, pid), element.id), false, "a fresh element has no opacity field");

  const set = patchElement(root, pid, element.id, { opacity: 0.5 });
  assert.equal(set.element.opacity, 0.5);
  assert.equal(byId(getProject(root, pid), element.id).opacity, 0.5);

  // opacity 0 is valid (fully transparent) and IS stored (not confused with "default").
  assert.equal(patchElement(root, pid, element.id, { opacity: 0 }).element.opacity, 0);

  const reset = patchElement(root, pid, element.id, { opacity: 1 });
  assert.equal("opacity" in reset.element, false, "opacity 1 deletes the field");
  assert.equal("opacity" in byId(getProject(root, pid), element.id), false);
});

test("patchElement rejects an out-of-range / non-finite opacity loudly and writes nothing", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "OpacityLoud" }).id;
  const element = img(root, pid, "sprite", 8, 8);
  const seq0 = getProject(root, pid).history_seq;
  assert.throws(() => patchElement(root, pid, element.id, { opacity: 2 }), /opacity must be a finite number in \[0,1\]/);
  assert.throws(() => patchElement(root, pid, element.id, { opacity: -0.1 }), /opacity must be a finite number in \[0,1\]/);
  assert.throws(() => patchElement(root, pid, element.id, { opacity: "x" }), /opacity must be a finite number in \[0,1\]/);
  assert.equal(getProject(root, pid).history_seq, seq0, "no journal entry on a rejected opacity");
  assert.equal("opacity" in byId(getProject(root, pid), element.id), false);
});

test("patchElement opacity is journaled and undo restores the prior value", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "OpacityUndo" }).id;
  const element = img(root, pid, "sprite", 8, 8);
  patchElement(root, pid, element.id, { opacity: 0.4 });
  patchElement(root, pid, element.id, { opacity: 0.9 });
  const undone = undoOp(root, { projectId: pid }).project;
  assert.equal(byId(undone, element.id).opacity, 0.4, "undo restores the prior opacity");
});

// ---- API: PATCH opacity + PUT animation routes ---------------------------------

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
    write(chunk) {
      chunks.push(Buffer.from(chunk));
      return true;
    },
    end(chunk) {
      if (chunk !== undefined && chunk !== null && chunk !== "") chunks.push(Buffer.from(chunk));
      const buffer = Buffer.concat(chunks);
      this._resolve({ status: this.statusCode, headers: this.headers, buffer, json: () => JSON.parse(buffer.toString("utf8")) });
    },
  };
  const done = new Promise((r) => {
    res._resolve = r;
  });
  handler(req, res, new URL(path, "http://canvas.local"));
  queueMicrotask(() => {
    if (body !== undefined) req.emit("data", Buffer.from(typeof body === "string" ? body : JSON.stringify(body)));
    req.emit("end");
  });
  return done;
}

test("canvas API: PATCH element opacity (200 stored / 400 out-of-range / 404 unknown element)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "OpacityAPI" })).json().project.id;
  const elementId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, { name: "s.png", bytes_base64: solidPng(6, 6).toString("base64") })).json().element.id;

  const set = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elementId}`, { opacity: 0.5 });
  assert.equal(set.status, 200);
  assert.equal(set.json().element.opacity, 0.5);

  const bad = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elementId}`, { opacity: 2 });
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /opacity must be a finite number in \[0,1\]/);

  const missing = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/nope`, { opacity: 0.5 });
  assert.equal(missing.status, 404);
});

test("canvas API: PUT element animation (200 set / 200 clear / 400 bad spec / 404 unknown element)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "AnimAPI" })).json().project.id;
  const elementId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, { name: "s.png", bytes_base64: solidPng(6, 6).toString("base64") })).json().element.id;

  const put = await invokeApi(handler, "PUT", `/api/canvas/projects/${projectId}/elements/${elementId}/animation`, {
    animation: { channels: [{ prop: "off_x", kind: "osc", amplitude: 5, period_ms: 500 }] },
  });
  assert.equal(put.status, 200);
  assert.equal(put.json().element.animation.channels[0].prop, "off_x");

  const clear = await invokeApi(handler, "PUT", `/api/canvas/projects/${projectId}/elements/${elementId}/animation`, { animation: null });
  assert.equal(clear.status, 200);
  assert.equal("animation" in clear.json().element, false);

  const bad = await invokeApi(handler, "PUT", `/api/canvas/projects/${projectId}/elements/${elementId}/animation`, {
    animation: { channels: [{ prop: "off_x", kind: "osc", amplitude: 5, period_ms: 0 }] },
  });
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /period_ms must be a finite number > 0/);

  const missing = await invokeApi(handler, "PUT", `/api/canvas/projects/${projectId}/elements/nope/animation`, { animation: null });
  assert.equal(missing.status, 404);
});

// ---- CLI: element-set --opacity + animation-set / --clear ----------------------

test("cli element-set --opacity and animation-set / --clear round-trip through the store", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-anim-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(9, 6, [12, 34, 56]));
  const specPath = join(dir, "anim.json");
  writeFileSync(specPath, JSON.stringify({ channels: [{ prop: "scale", kind: "osc", amplitude: 0.1, period_ms: 900 }] }));

  const run = (...args) => {
    const stdout = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    return JSON.parse(stdout.trim().split("\n").filter(Boolean).at(-1));
  };

  const projectId = run("create", "--title", "CLI Anim").project.id;
  const elementId = run("add-image", projectId, "--file", pngPath).element.id;

  // element-set --opacity: stored at != 1, deleted at exactly 1.
  assert.equal(run("element-set", projectId, "--element", elementId, "--opacity", "0.5").element.opacity, 0.5);
  assert.equal("opacity" in run("element-set", projectId, "--element", elementId, "--opacity", "1").element, false);

  // animation-set --json: reads + validates the spec; --clear removes it.
  assert.equal(run("animation-set", projectId, "--element", elementId, "--json", specPath).element.animation.channels[0].prop, "scale");
  assert.equal("animation" in run("animation-set", projectId, "--element", elementId, "--clear").element, false);

  // An out-of-range --opacity fails loudly (exit 1, ops validation message on stderr).
  let failed;
  try {
    execFileSync(process.execPath, [CLI, "element-set", projectId, "--element", elementId, "--opacity", "5"], { env: { ...process.env, ...env }, encoding: "utf8" });
    assert.fail("expected --opacity 5 to fail");
  } catch (error) {
    failed = error;
  }
  assert.match(failed.stderr, /opacity must be a finite number in \[0,1\]/);
});

// ---- render-pixel parity: a translucent element through the REAL render_group.py -
//
// element.opacity multiplies the element alpha before compositing. On the canvas that is
// ctx.globalAlpha; in render_group.py it is the alpha-channel multiply. Both are the
// Porter-Duff source-over of the element (alpha = opacity) OVER the opaque group
// background, so the expected pixel is computed HERE independently (EL*opacity +
// BG*(1-opacity)) — the same "don't test the code under test with itself" stance
// slice9.test.mjs takes. This pins render_group.py to the canvas's ctx.globalAlpha result.

const BG = [32, 64, 96]; // #204060, opaque group background
const EL = [220, 40, 40]; // opaque source pixels

function closeColor(actual, expected, tol = 10) {
  return Math.abs(actual[0] - expected[0]) <= tol && Math.abs(actual[1] - expected[1]) <= tol && Math.abs(actual[2] - expected[2]) <= tol;
}

function buildOpacityFixture(root, opacity) {
  const pid = createProject(root, { title: "OpacityParity" }).id;
  const element = img(root, pid, "sprite.png", 40, 40, EL, 20, 20);
  if (opacity !== undefined) patchElement(root, pid, element.id, { opacity });
  const group = createGroup(root, { projectId: pid, name: "Screen", x: 0, y: 0, w: 100, h: 100 }).group;
  assignToGroup(root, { projectId: pid, elementIds: [element.id], groupId: group.id });
  return { pid, groupId: group.id };
}

test("render-pixel parity: a 0.5-opacity element composites as EL*0.5 + BG*0.5 over the background (an opaque element is unblended)", async (t) => {
  const root = tempProjects(t);
  const translucent = buildOpacityFixture(root, 0.5);
  const expectedBlend = EL.map((c, i) => Math.round(c * 0.5 + BG[i] * 0.5)); // independently derived

  let png;
  try {
    const result = await renderGroup(root, { projectId: translucent.pid, groupId: translucent.groupId, scale: 1, background: "#204060" });
    png = decodePng(readFileSync(result.path));
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  // Element center (world 40,40 -> canvas 40,40): the translucent blend, NOT pure EL.
  assert.ok(closeColor(png.at(40, 40), expectedBlend), `expected blend [${expectedBlend}] at the element center, got [${png.at(40, 40)}]`);
  assert.ok(!closeColor(png.at(40, 40), EL), "the translucent center must have moved off the opaque element color");
  // Outside the element: pure background (opacity never touches the bg).
  assert.ok(closeColor(png.at(5, 5), BG), `expected pure background [${BG}] outside the element, got [${png.at(5, 5)}]`);

  // Control: the SAME element with no opacity (absent = 1) renders the opaque element color.
  const opaque = buildOpacityFixture(root, undefined);
  const opaquePng = decodePng(readFileSync((await renderGroup(root, { projectId: opaque.pid, groupId: opaque.groupId, scale: 1, background: "#204060" })).path));
  assert.ok(closeColor(opaquePng.at(40, 40), EL), `expected the opaque element color [${EL}] with no opacity, got [${opaquePng.at(40, 40)}]`);
});
