// Text -> animation bridge tests (T0264). Two layers, mirroring tests/prompt_assist.test.mjs +
// the codex-seam block of tests/recipe.test.mjs:
//   1. The PURE instruction builder (buildAnimateInstruction) — no spawn.
//   2. runAnimateFromText + ops.animateElementFromText with an INJECTED fake runner — codex
//      NEVER spawns in this suite (the T0238 contract). The fake runner stands in for the codex
//      call (the {instruction, imagePath} -> raw-reply seam), so the op exercises the REAL
//      instruction-build + strict JSON parse + validateAnimation gate end to end.
// CLI/API parity tests stay VALIDATION-level only (recipe-generate/extract precedent): the CLI/
// API have no fake-runner injection seam, and a real animate call would spawn the DEFAULT codex
// runner, which this suite never triggers.
// Run: node --test ai_studio/assets/canvas/tests/animation_assist.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import {
  addImage,
  addText,
  animateElementFromText,
  createProject,
  getProject,
  historyEntryLabel,
  redoOp,
  undoOp,
} from "../ops.mjs";
import { resolveProjectFile, withProjectLock } from "../store.mjs";
import { createCanvasApi } from "../api.mjs";
import { buildAnimateInstruction, runAnimateFromText } from "../tools/animation_assist.mjs";
import { solidPng } from "./png_fixture.mjs";

// Metadata + fake-runner ops resolve store paths only (no python, no codex), so a placeholder
// root works for the image-element tests. The one TEXT-element test needs the real repo root
// because addText reads the fonts manifest — mirrors tests/recipe.test.mjs.
const ROOT = "C:/unused-repo-root";
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
  return dir;
}

function run(env, ...args) {
  const stdout = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
  const line = stdout.trim().split("\n").filter(Boolean).at(-1);
  return JSON.parse(line);
}

function runFail(env, ...args) {
  try {
    execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    assert.fail(`expected "${args.join(" ")}" to fail`);
  } catch (error) {
    return error;
  }
}

// Minimal req/res doubles (mirrors tests/recipe.test.mjs's own invokeApi).
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
      this._resolve({
        status: this.statusCode,
        json() {
          return JSON.parse(buffer.toString("utf8"));
        },
      });
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

// A fake codex runner: the {instruction, imagePath} -> raw-reply seam runAnimateFromText/the op
// inject in place of the real codex spawn. Returns a fixed reply (or throws, if given an Error),
// recording every call for argument assertions — the same shape recipe.test.mjs's fakeGen uses.
function fakeRunner(replyOrError) {
  const calls = [];
  const fn = async (args) => {
    calls.push(args);
    if (replyOrError instanceof Error) throw replyOrError;
    return replyOrError;
  };
  fn.calls = calls;
  return fn;
}

const SPEC = { v: 1, channels: [{ prop: "off_y", kind: "osc", amplitude: 40, period_ms: 1500 }] };
const SPEC_JSON = JSON.stringify(SPEC);

// ================================================================================
// buildAnimateInstruction (pure, no spawn)
// ================================================================================

test("buildAnimateInstruction: pins the schema id, every property, and its UNITS", () => {
  const instruction = buildAnimateInstruction({ element: { name: "Wing", w: 1254, h: 800 }, text: "flap" });
  assert.match(instruction, /ai_studio\.canvas\.animation\.v1/);
  for (const prop of ["off_x", "off_y", "rot", "scale", "opacity"]) {
    assert.ok(instruction.includes(prop), `missing property ${prop}`);
  }
  // Units are load-bearing (a wrong unit = a wrong amplitude): pixels, degrees, multiplier, clamp.
  assert.match(instruction, /world-PIXEL offset ADDED/);
  assert.match(instruction, /DEGREES added/);
  assert.match(instruction, /MULTIPLIER on the element scale/);
  assert.match(instruction, /clamped to 0\.\.1/);
  // Both channel kinds, with the seams that make a spec valid: period_ms > 0, keyframes t_ms 0,
  // seamless-loop last==first.
  assert.match(instruction, /"kind":"osc"/);
  assert.match(instruction, /period_ms MUST be > 0/);
  assert.match(instruction, /"kind":"keyframes"/);
  assert.match(instruction, /FIRST point MUST be t_ms 0/);
  assert.match(instruction, /last[\s\S]*point's v MUST equal the first/);
  assert.match(instruction, /AT MOST ONE channel per property/);
});

test("buildAnimateInstruction: embeds the element name + w/h and demands PROPORTIONAL amplitude", () => {
  const instruction = buildAnimateInstruction({ element: { name: "Wing", w: 1254, h: 800 }, text: "flap" });
  assert.match(instruction, /"Wing"/);
  assert.match(instruction, /1254x800 world units/);
  assert.match(instruction, /PROPORTIONAL/);
  // The proportional guidance names a concrete ~5%-of-dimension magnitude for THIS element
  // (a 4px bob on an 800px element is invisible — the lead's wing example).
  assert.match(instruction, /a 4px bob on a 800px-tall element is invisible/);
  assert.ok(instruction.includes(`${Math.round(800 * 0.05)}px (off_y)`), "names ~5% of the height as the visible bob");
});

test("buildAnimateInstruction: includes the user's description VERBATIM (incl. non-ASCII)", () => {
  const instruction = buildAnimateInstruction({ element: { name: "Wing", w: 1254, h: 800 }, text: "  крылья медленно машут  " });
  assert.match(instruction, /\[REQUEST\]\nкрылья медленно машут/);
});

test("buildAnimateInstruction: currentSpec present -> MINIMAL-PATCH directive + the current spec JSON", () => {
  const withSpec = buildAnimateInstruction({ element: { name: "Wing", w: 1254, h: 800 }, currentSpec: SPEC, text: "make it slower" });
  assert.match(withSpec, /ALREADY has this animation/);
  assert.match(withSpec, /MODIFY IT MINIMALLY/);
  assert.ok(withSpec.includes(SPEC_JSON), "the current spec JSON is embedded verbatim for a minimal patch");
  assert.match(withSpec, /медленнее.*increase period_ms/);

  const fresh = buildAnimateInstruction({ element: { name: "Wing", w: 1254, h: 800 }, text: "make it slower" });
  assert.match(fresh, /no animation yet — author one from scratch/);
  assert.doesNotMatch(fresh, /MODIFY IT MINIMALLY/);
});

test("buildAnimateInstruction: output contract is ONLY the JSON object, no prose/fence", () => {
  const instruction = buildAnimateInstruction({ element: { name: "Wing", w: 100, h: 100 }, text: "flap" });
  assert.match(instruction, /Output ONLY the animation JSON object/);
  assert.match(instruction, /no markdown code fence/);
});

test("buildAnimateInstruction requires element + text", () => {
  assert.throws(() => buildAnimateInstruction({ text: "flap" }), /requires element/);
  assert.throws(() => buildAnimateInstruction({ element: { name: "x", w: 1, h: 1 } }), /requires text/);
  assert.throws(() => buildAnimateInstruction({ element: { name: "x", w: 1, h: 1 }, text: "   " }), /requires text/);
});

// ================================================================================
// runAnimateFromText (fake runner — codex never spawns)
// ================================================================================

test("runAnimateFromText: builds the instruction, calls the runner with {instruction, imagePath}, returns the PARSED object", async () => {
  const runner = fakeRunner(SPEC_JSON);
  const out = await runAnimateFromText({ element: { name: "Wing", w: 1254, h: 800 }, imagePath: "C:/proj/files/wing.png", text: "gently flap", runner });

  assert.equal(runner.calls.length, 1);
  assert.equal(runner.calls[0].imagePath, "C:/proj/files/wing.png", "the source image path is forwarded for the vision call");
  assert.match(runner.calls[0].instruction, /gently flap/, "the built instruction reaches the runner");
  assert.deepEqual(out, SPEC, "the strict-parsed animation object is returned verbatim");
});

test("runAnimateFromText: text-only path forwards imagePath null", async () => {
  const runner = fakeRunner(SPEC_JSON);
  await runAnimateFromText({ element: { name: "Label", w: 300, h: 80 }, imagePath: null, text: "pulse", runner });
  assert.equal(runner.calls[0].imagePath, null);
});

test("runAnimateFromText: empty / non-JSON / non-object replies are all loud (no fence stripping)", async () => {
  await assert.rejects(() => runAnimateFromText({ element: { name: "x", w: 1, h: 1 }, text: "flap", runner: fakeRunner("   ") }), /empty result/);
  await assert.rejects(
    () => runAnimateFromText({ element: { name: "x", w: 1, h: 1 }, text: "flap", runner: fakeRunner("not json at all") }),
    /was not valid JSON/,
  );
  // A fenced reply is NOT stripped — it stays invalid JSON and throws loudly (loud-not-lenient).
  await assert.rejects(
    () => runAnimateFromText({ element: { name: "x", w: 1, h: 1 }, text: "flap", runner: fakeRunner("```json\n{\"v\":1}\n```") }),
    /was not valid JSON/,
  );
  await assert.rejects(
    () => runAnimateFromText({ element: { name: "x", w: 1, h: 1 }, text: "flap", runner: fakeRunner("42") }),
    /non-object JSON value/,
  );
});

test("runAnimateFromText requires element + text (before the runner is ever called)", async () => {
  const runner = fakeRunner(SPEC_JSON);
  await assert.rejects(() => runAnimateFromText({ text: "flap", runner }), /requires element/);
  await assert.rejects(() => runAnimateFromText({ element: { name: "x", w: 1, h: 1 }, runner }), /requires text/);
  assert.equal(runner.calls.length, 0);
});

// ================================================================================
// ops.animateElementFromText (fake runner) — the happy path + the loud paths
// ================================================================================

test("animateElementFromText (image): spec lands + meta.animation_request + ONE journal entry; undo/redo; the runner gets the source imagePath", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Animate" });
  const image = addImage(ROOT, project.id, { name: "Wing", bytes: solidPng(20, 16, [10, 20, 30]) }).element;
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const runner = fakeRunner(SPEC_JSON);
  const result = await animateElementFromText(ROOT, { projectId: project.id, elementId: image.id, text: "  gently bobs up and down  ", runner });

  // The codex seam saw the resolved source path (vision) + the trimmed description in the instruction.
  assert.equal(runner.calls.length, 1);
  assert.equal(runner.calls[0].imagePath, resolveProjectFile(ROOT, project.id, image.src));
  assert.match(runner.calls[0].instruction, /gently bobs up and down/);

  // The validated spec landed on element.animation; provenance rides in meta.animation_request.
  assert.deepEqual(result.animation, SPEC);
  assert.deepEqual(result.element.animation, SPEC);
  assert.equal(result.element.meta.animation_request.text, "gently bobs up and down", "text is trimmed + recorded");
  assert.ok(result.element.meta.animation_request.at, "an ISO timestamp is recorded");

  // ONE journal entry; undo removes BOTH the spec and the provenance, redo restores.
  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1);

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  const undoneEl = undone.elements.find((el) => el.id === image.id);
  assert.equal(undoneEl.animation, undefined, "undo removes element.animation");
  assert.equal(undoneEl.meta && undoneEl.meta.animation_request, undefined, "undo removes the animation_request provenance");

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.deepEqual(redone.elements.find((el) => el.id === image.id).animation, SPEC);
});

test("animateElementFromText (text element): text-only call, imagePath null", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Animate text" });
  const textEl = addText(REPO_ROOT, project.id, { content: "Hello" }).element;

  const runner = fakeRunner(SPEC_JSON);
  const result = await animateElementFromText(REPO_ROOT, { projectId: project.id, elementId: textEl.id, text: "pulse gently", runner });

  assert.equal(runner.calls[0].imagePath, null, "a text element has no source image -> text-only codex call");
  assert.deepEqual(result.element.animation, SPEC);
});

test("animateElementFromText: a SECOND call minimally patches — the runner receives the CURRENT spec", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Animate patch" });
  const image = addImage(ROOT, project.id, { name: "Wing", bytes: solidPng() }).element;

  await animateElementFromText(ROOT, { projectId: project.id, elementId: image.id, text: "bob up and down", runner: fakeRunner(SPEC_JSON) });

  // Second call: the instruction handed to codex must carry the stored spec + the minimal-patch directive.
  const slower = { v: 1, channels: [{ prop: "off_y", kind: "osc", amplitude: 40, period_ms: 3000 }] };
  const runner2 = fakeRunner(JSON.stringify(slower));
  const result = await animateElementFromText(ROOT, { projectId: project.id, elementId: image.id, text: "make it slower", runner: runner2 });

  assert.match(runner2.calls[0].instruction, /MODIFY IT MINIMALLY/);
  assert.ok(runner2.calls[0].instruction.includes(SPEC_JSON), "the current spec JSON is embedded so codex can patch just the period");
  assert.deepEqual(result.element.animation, slower);
});

test("animateElementFromText: an invalid-JSON reply writes NOTHING (journal clean)", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Animate bad json" });
  const image = addImage(ROOT, project.id, { name: "Wing", bytes: solidPng() }).element;
  const seqBefore = getProject(ROOT, project.id).history_seq;

  await assert.rejects(
    () => animateElementFromText(ROOT, { projectId: project.id, elementId: image.id, text: "flap", runner: fakeRunner("sorry, here is your animation!") }),
    /was not valid JSON/,
  );
  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore, "no journal entry");
  assert.equal(after.elements.find((el) => el.id === image.id).animation, undefined, "no spec written");
});

test("animateElementFromText: a valid-JSON but SCHEMA-invalid spec writes NOTHING (journal clean)", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Animate bad spec" });
  const image = addImage(ROOT, project.id, { name: "Wing", bytes: solidPng() }).element;
  const seqBefore = getProject(ROOT, project.id).history_seq;

  // Parses fine, but "bogus" is not a known prop -> validateAnimation throws in the op.
  const badSpec = JSON.stringify({ v: 1, channels: [{ prop: "bogus", kind: "osc", amplitude: 1, period_ms: 100 }] });
  await assert.rejects(
    () => animateElementFromText(ROOT, { projectId: project.id, elementId: image.id, text: "flap", runner: fakeRunner(badSpec) }),
    /prop must be one of/,
  );
  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore, "no journal entry");
  assert.equal(after.elements.find((el) => el.id === image.id).animation, undefined, "no spec written");
});

test("animateElementFromText: empty text is loud, runner NEVER called; a missing element is loud", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Animate validate" });
  const image = addImage(ROOT, project.id, { name: "Wing", bytes: solidPng() }).element;
  const runner = fakeRunner(SPEC_JSON);

  await assert.rejects(() => animateElementFromText(ROOT, { projectId: project.id, elementId: image.id, text: "   ", runner }), /non-empty text/);
  await assert.rejects(() => animateElementFromText(ROOT, { projectId: project.id, elementId: "el_missing", text: "flap", runner }), /element not found/);
  assert.equal(runner.calls.length, 0, "no codex call before validation passes");
});

test("historyEntryLabel maps animateElementFromText to a readable label", () => {
  assert.deepEqual(historyEntryLabel("animateElementFromText", { text: "flap wings" }), { label: "Animate", summary: "flap wings" });
  assert.deepEqual(historyEntryLabel("animateElementFromText", {}), { label: "Animate", summary: "" });
});

// ================================================================================
// CLI + API validation parity (no codex spawn — no fake-runner seam at these layers)
// ================================================================================

test("animate: CLI rejects missing --element / missing --text / not-found element (validation parity, no codex spawn)", (t) => {
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  const projectId = run(env, "create", "--title", "Animate CLI").project.id;
  const group = run(env, "group-create", projectId, "--name", "Plain", "--x", "0", "--y", "0", "--w", "10", "--h", "10").group;

  // No --element / no --text -> the CLI's own local guards (never reach codex).
  assert.throws(() => execFileSync(process.execPath, [CLI, "animate", projectId], { env: { ...process.env, ...env }, encoding: "utf8" }));
  const noText = runFail(env, "animate", projectId, "--element", group.id);
  assert.match(String(noText.stderr || noText.message), /requires --text/);

  // A GROUP id is never an element id -> the op's own "element not found" (before any codex call).
  const notFound = runFail(env, "animate", projectId, "--element", group.id, "--text", "flap");
  assert.match(String(notFound.stderr || notFound.message), /element not found/);
});

test("animate API: 404 not-found element / 400 empty text (statusForError parity)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Animate API" })).json().project.id;
  const image = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, { name: "Wing", bytes_base64: solidPng().toString("base64") })).json().element;

  const missing = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/elements/el_missing/animate`, { text: "flap" });
  assert.equal(missing.status, 404);
  assert.match(missing.json().error, /element not found/);

  const emptyText = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/elements/${image.id}/animate`, { text: "   " });
  assert.equal(emptyText.status, 400);
  assert.match(emptyText.json().error, /non-empty text/);
});

test("animate API route is EXCLUDED from the lock wrapper (self-locking slow codex route)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Animate lock" })).json().project.id;

  // Hold the project lock, then fire an animate request that fails validation (empty text) BEFORE
  // the op ever reaches its OWN withProjectLock. If the route wrapped the op in `locked` (it must
  // NOT — a slow codex route), the request would queue behind our held lock and never resolve; it
  // resolving (400) while the lock is still held proves the exclusion.
  let release;
  const held = new Promise((r) => (release = r));
  let acquired;
  const acquiredP = new Promise((r) => (acquired = r));
  const lockDone = withProjectLock(ROOT, projectId, async () => {
    acquired();
    return held;
  });
  await acquiredP; // the lock is now held in-process

  const rejected = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/elements/el_missing/animate`, { text: "" });
  assert.equal(rejected.status, 400);
  assert.match(rejected.json().error, /non-empty text/);

  release();
  await lockDone;
});
