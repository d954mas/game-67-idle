// Animation-card ops tests (T0265 increment 1, video route). The card object + `anim` blob +
// generateAnimFromCard end-to-end behind the tools/anim_generate.mjs seam. ComfyUI/CorridorKey
// NEVER spawn in this suite: every generation test injects a fake `generators.run` that writes
// N synthetic RGBA frames to a temp dir and returns their paths (the T0238 GPU-free contract,
// carried to the video route). Metadata-only ops (no Python for validation/keyframe
// resolution), so the placeholder ROOT works for every direct-ops test; CLI/API parity tests
// drive the real cli.mjs / api.mjs but stay VALIDATION-level for generate (a real generate call
// through the CLI/API always spawns the DEFAULT, real generator, which this suite never
// triggers — only its pre-generation loud refusals).
// Run: node --test ai_studio/assets/canvas/tests/anim_card.test.mjs
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
  addNote,
  alphaCutout,
  assignToGroup,
  bakeFilters,
  cleanupApply,
  createAnimCard,
  createGroup,
  createProject,
  createRecipeCard,
  createStyleCard,
  generateAnimFromCard,
  getProject,
  historyEntryLabel,
  patchAnim,
  redoOp,
  undoOp,
} from "../ops.mjs";
import { createCanvasApi } from "../api.mjs";
import { resolveProjectFile } from "../store.mjs";
import { applyFpsToWorkflow } from "../../tools/video/generate/generate.mjs";
import { encodePng, solidPng } from "./png_fixture.mjs";

// Metadata ops resolve store paths only, so any placeholder root works (no Python).
const ROOT = "C:/unused-repo-root";
// addNote reads the bundled fonts manifest from the repo root, so the "member is not an image"
// refusal (which needs a real non-image element) uses the REAL repo root to mint the note;
// project data still resolves via CANVAS_PROJECTS_ROOT, so createAnimCard stays on ROOT.
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
      this._resolve({ status: this.statusCode, headers: this.headers, buffer, json() { return JSON.parse(buffer.toString("utf8")); } });
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

const DEFAULT_ANIM = {
  v: 1,
  motion: "",
  profile: "draft",
  seed: null,
  matte: "corridorkey",
  gen_fps: null,
  loop: true,
  columns: null,
  trim: false,
  style_ref: null,
  accepted_ref: null,
  last_run: null,
};

// A distinct RGBA frame per index so store.addFile does NOT dedup them to one file (the
// generated matte frames are the flipbook's source of truth — distinct srcs let the test
// assert frame count + order precisely). alpha:true -> a 4-channel PNG like the real matte.
function rgbaFramePng(w, h, seed) {
  return encodePng(w, h, (x, y) => [(seed * 37) % 256, (x * 9) % 256, (y * 11) % 256, 255], { alpha: true });
}

// The seed the fake generator "rolls" when the card left seed=null (mirrors runGenerate's own
// chosenSeed roll). Provenance must freeze this resolved value, never the card's null.
const FAKE_ROLLED_SEED = 987654;

// A fake generator matching tools/anim_generate.mjs's seam: run({...}) -> {framePaths, meta,
// runDir}. Writes `frames` synthetic RGBA PNGs to a temp dir and returns their paths; records
// every call's args for exact assertions. meta.seed echoes the requested seed, or the rolled
// stand-in when the card left seed=null (the F2 provenance contract). NEVER spawns
// ComfyUI/CorridorKey.
function fakeAnimGenerator(t, { frames = 3, frame_w = 12, frame_h = 10, fps = 12, meta } = {}) {
  const calls = [];
  const dir = mkdtempSync(join(tmpdir(), "canvas-anim-frames-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const run = async (args) => {
    calls.push(args);
    const framePaths = [];
    for (let i = 0; i < frames; i += 1) {
      const p = join(dir, `frame_${String(i).padStart(3, "0")}.png`);
      writeFileSync(p, rgbaFramePng(frame_w, frame_h, i + 1));
      framePaths.push(p);
    }
    const resolvedSeed = args.seed == null ? FAKE_ROLLED_SEED : args.seed;
    const defaultMeta = { frame_w, frame_h, fps, seed: resolvedSeed };
    return { framePaths, meta: meta !== undefined ? meta : defaultMeta, runDir: join(dir, "run") };
  };
  return { run, calls };
}

function animCardWithKeyframe(projectId, { name = "Idle", motion = "gentle idle bob", x = 100, y = 50, w = 300, h = 200, loop } = {}) {
  const card = createAnimCard(ROOT, { projectId, name, x, y, w, h }).group;
  const patch = { motion };
  if (loop !== undefined) patch.loop = loop;
  patchAnim(ROOT, { projectId, groupId: card.id, patch });
  const kf = addImage(ROOT, projectId, { name: "kf.png", bytes: solidPng(8, 8, [30, 40, 50]) }).element;
  assignToGroup(ROOT, { projectId, elementIds: [kf.id], groupId: card.id });
  return { card, kf };
}

// ================================================================================
// createAnimCard + defaultAnim
// ================================================================================

test("createAnimCard mints a group with a default anim blob; one entry; undo removes it", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim create" });
  const seqBefore = getProject(ROOT, project.id).history_seq;
  const { project: after, group } = createAnimCard(ROOT, { projectId: project.id, name: "Idle", x: 5, y: 6 });

  assert.equal(group.name, "Idle");
  assert.equal(group.x, 5);
  assert.equal(group.y, 6);
  assert.equal(group.w, 360, "omitted w -> default frame");
  assert.equal(group.h, 280);
  assert.deepEqual(group.anim, DEFAULT_ANIM);
  assert.equal(after.history_seq, seqBefore + 1);

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.groups.length, 0);
  assert.equal(undone.history_seq, seqBefore);
});

test("createAnimCard: cards do not nest inside cards (recipe/style/anim parent all refused); a PLAIN parent is fine", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim nesting" });
  const recipe = createRecipeCard(ROOT, { projectId: project.id, name: "R" }).group;
  const style = createStyleCard(ROOT, { projectId: project.id, name: "S" }).group;
  const anim = createAnimCard(ROOT, { projectId: project.id, name: "A" }).group;
  const plain = createGroup(ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 900, h: 900 }).group;

  assert.throws(() => createAnimCard(ROOT, { projectId: project.id, parentId: recipe.id }), /cards do not nest inside cards/);
  assert.throws(() => createAnimCard(ROOT, { projectId: project.id, parentId: style.id }), /cards do not nest inside cards/);
  assert.throws(() => createAnimCard(ROOT, { projectId: project.id, parentId: anim.id }), /cards do not nest inside cards/);

  const nested = createAnimCard(ROOT, { projectId: project.id, parentId: plain.id }).group;
  assert.equal(nested.parentId, plain.id);
});

// ================================================================================
// patchAnim + normalizeAnimPatch
// ================================================================================

test("patchAnim updates the anim blob; one entry; undo restores byte-exact", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim patch" });
  const card = createAnimCard(ROOT, { projectId: project.id }).group;

  const patched = patchAnim(ROOT, {
    projectId: project.id,
    groupId: card.id,
    patch: { motion: "walk cycle", profile: "final", seed: 701, matte: "key_matte", gen_fps: 16, loop: false, columns: 4, trim: true },
  }).group;
  assert.equal(patched.anim.motion, "walk cycle");
  assert.equal(patched.anim.profile, "final");
  assert.equal(patched.anim.seed, 701);
  assert.equal(patched.anim.matte, "key_matte");
  assert.equal(patched.anim.gen_fps, 16);
  assert.equal(patched.anim.loop, false);
  assert.equal(patched.anim.columns, 4);
  assert.equal(patched.anim.trim, true);
  // untouched fields keep their defaults
  assert.equal(patched.anim.style_ref, null);
  assert.equal(patched.anim.accepted_ref, null);

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.deepEqual(undone.groups.find((g) => g.id === card.id).anim, DEFAULT_ANIM);
});

test("patchAnim is loud on a group that carries no anim blob", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim patch plain" });
  const plain = createGroup(ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;
  const recipe = createRecipeCard(ROOT, { projectId: project.id }).group;
  assert.throws(() => patchAnim(ROOT, { projectId: project.id, groupId: plain.id, patch: { motion: "x" } }), /not an animation card/);
  assert.throws(() => patchAnim(ROOT, { projectId: project.id, groupId: recipe.id, patch: { motion: "x" } }), /not an animation card/);
});

test("normalizeAnimPatch: loud rejections for every field (no silent coercion)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim normalize" });
  const card = createAnimCard(ROOT, { projectId: project.id }).group;
  const p = (patch) => patchAnim(ROOT, { projectId: project.id, groupId: card.id, patch });

  assert.throws(() => p({}), /at least one of/);
  assert.throws(() => p({ motion: 5 }), /motion must be a string/);
  assert.throws(() => p({ profile: "medium" }), /profile must be one of/);
  assert.throws(() => p({ seed: "abc" }), /seed must be null or a number/);
  assert.throws(() => p({ matte: "greenscreen" }), /matte must be one of/);
  assert.throws(() => p({ gen_fps: 0 }), /gen_fps must be null or a positive number/);
  assert.throws(() => p({ gen_fps: -3 }), /gen_fps must be null or a positive number/);
  assert.throws(() => p({ loop: "yes" }), /loop must be a boolean/);
  assert.throws(() => p({ columns: 0 }), /columns must be null or a positive integer/);
  assert.throws(() => p({ columns: 2.5 }), /columns must be null or a positive integer/);
  assert.throws(() => p({ trim: 1 }), /trim must be a boolean/);
  assert.throws(() => p({ style_ref: 42 }), /style_ref must be null or a string/);
  assert.throws(() => p({ accepted_ref: 42 }), /accepted_ref must be null or a string/);
  // null clears are accepted
  assert.doesNotThrow(() => p({ seed: null, gen_fps: null, columns: null, style_ref: null, accepted_ref: null }));
});

test("patchAnim: a non-null style_ref must resolve to a style-card group; accepted_ref to a flipbook element", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim pointers" });
  const card = createAnimCard(ROOT, { projectId: project.id }).group;
  const plainGroup = createGroup(ROOT, { projectId: project.id, name: "G", x: 0, y: 0, w: 10, h: 10 }).group;
  const style = createStyleCard(ROOT, { projectId: project.id, name: "S" }).group;
  const image = addImage(ROOT, project.id, { name: "img.png", bytes: solidPng() }).element;

  assert.throws(() => patchAnim(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: plainGroup.id } }), /style_ref must be null or the id of an existing style-card group/);
  const ok = patchAnim(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: style.id } }).group;
  assert.equal(ok.anim.style_ref, style.id);

  assert.throws(() => patchAnim(ROOT, { projectId: project.id, groupId: card.id, patch: { accepted_ref: image.id } }), /accepted_ref must be null or the id of an existing flipbook element/);
});

// ================================================================================
// generateAnimFromCard — validation (no generator call)
// ================================================================================

test("generateAnimFromCard validates projectId/groupId before touching disk", async () => {
  await assert.rejects(() => generateAnimFromCard(ROOT, {}), /requires projectId/);
  await assert.rejects(() => generateAnimFromCard(ROOT, { projectId: "p" }), /requires groupId/);
});

test("generateAnimFromCard: unknown group, non-anim group, empty motion, 0 keyframes, >1 keyframes are all loud — no generator call", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim gen validate" });
  const plain = createGroup(ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 900, h: 900 }).group;
  const gen = fakeAnimGenerator(t);

  // unknown group
  await assert.rejects(() => generateAnimFromCard(ROOT, { projectId: project.id, groupId: "grp_missing", generators: gen }), /group not found/);
  // non-anim group
  await assert.rejects(() => generateAnimFromCard(ROOT, { projectId: project.id, groupId: plain.id, generators: gen }), /not an animation card/);

  // empty motion
  const card = createAnimCard(ROOT, { projectId: project.id, name: "Blank" }).group;
  await assert.rejects(() => generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: gen }), /empty motion/);

  // motion set but 0 keyframes
  patchAnim(ROOT, { projectId: project.id, groupId: card.id, patch: { motion: "bob" } });
  await assert.rejects(() => generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: gen }), /no keyframes/);

  // >1 keyframes -> increment 3 refusal
  const k1 = addImage(ROOT, project.id, { name: "k1.png", bytes: solidPng(8, 8, [1, 2, 3]) }).element;
  const k2 = addImage(ROOT, project.id, { name: "k2.png", bytes: solidPng(8, 8, [4, 5, 6]) }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [k1.id, k2.id], groupId: card.id });
  await assert.rejects(() => generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: gen }), /increment 3/);

  assert.equal(gen.calls.length, 0, "the generator is never called before validation passes");
});

test("generateAnimFromCard: generator returning no frames / a bad meta.fps is loud", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim gen badmeta" });
  const { card } = animCardWithKeyframe(project.id);

  const noFrames = { run: async () => ({ framePaths: [], meta: { fps: 12 }, runDir: "x" }) };
  await assert.rejects(() => generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: noFrames }), /returned no frames/);

  const badFps = fakeAnimGenerator(t, { meta: { frame_w: 12, frame_h: 10, fps: 0 } });
  await assert.rejects(() => generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: badFps }), /meta\.fps must be a positive number/);
});

// ================================================================================
// generateAnimFromCard — happy path
// ================================================================================

test("generateAnimFromCard (top-level card): ONE flipbook element in the ROOT scope beside the frame; src=frame0; frames/fps/play_mode; last_run; meta.anim_run; ONE journal entry; undo reverts", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim gen happy" });
  const { card, kf } = animCardWithKeyframe(project.id, { name: "Idle", motion: "  gentle idle bob  ", x: 100, y: 50, w: 300, h: 200 });
  patchAnim(ROOT, { projectId: project.id, groupId: card.id, patch: { profile: "final", seed: 701, matte: "key_matte", gen_fps: 16 } });
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const gen = fakeAnimGenerator(t, { frames: 4, frame_w: 12, frame_h: 10, fps: 12 });
  const result = await generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: gen });

  // generator received the resolved keyframe path + card settings (motion trimmed).
  assert.equal(gen.calls.length, 1);
  assert.deepEqual(gen.calls[0].keyframePaths, [resolveProjectFile(ROOT, project.id, kf.src)]);
  assert.equal(gen.calls[0].motion, "gentle idle bob");
  assert.equal(gen.calls[0].profile, "final");
  assert.equal(gen.calls[0].seed, 701);
  assert.equal(gen.calls[0].matte, "key_matte");
  assert.equal(gen.calls[0].gen_fps, 16);

  const el = result.element;
  assert.equal(el.type, "image");
  assert.equal(el.assetStatus, "quarantine", "generated flipbooks enter review in quarantine");
  assert.deepEqual(el.meta.origin, {
    schema: "ai_studio.asset.generation_origin.v1",
    source: "ai",
    mode: "explore",
    game_id: null,
    style_lock_id: null,
    tainted: false,
    taint_reason: null,
  });
  assert.equal(el.x, card.x + card.w + 16, "placed to the RIGHT of the card frame, 16px gap");
  assert.equal(el.y, card.y);
  assert.equal(el.groupId, undefined, "top-level card -> flipbook lands in the ROOT scope, never inside the card");
  assert.equal(el.w, 12, "box = ONE frame");
  assert.equal(el.h, 10);
  assert.equal(el.source_w, 12);
  assert.equal(el.source_h, 10);

  // flipbook blob
  assert.equal(el.flipbook.v, 1);
  assert.equal(el.flipbook.frames.length, 4);
  assert.ok(el.flipbook.frames.every((f) => f.kept === true && typeof f.src === "string"));
  assert.equal(el.src, el.flipbook.frames[0].src, "element.src === frame 0");
  assert.equal(el.flipbook.fps, 12, "fps from generator meta");
  assert.equal(el.flipbook.play_mode, "loop", "loop:true (default) -> play_mode loop");
  assert.equal(el.flipbook.frame_w, 12);
  assert.equal(el.flipbook.frame_h, 10);

  // meta.anim_run provenance
  assert.equal(el.meta.anim_run.cardId, card.id);
  assert.equal(el.meta.anim_run.motion, "gentle idle bob");
  assert.equal(el.meta.anim_run.profile, "final");
  assert.equal(el.meta.anim_run.seed, 701);
  assert.equal(el.meta.anim_run.matte, "key_matte");
  assert.equal(el.meta.anim_run.gen_fps, 16);
  assert.deepEqual(el.meta.anim_run.keyframes, [kf.src]);
  assert.equal(el.meta.anim_run.frame_count, 4);
  assert.ok(el.meta.anim_run.runDir, "runDir provenance recorded");

  // one journal entry
  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1);

  // tool_runs row
  const toolRun = after.tool_runs.at(-1);
  assert.equal(toolRun.op, "generate_anim_from_card");
  assert.equal(toolRun.cardId, card.id);
  assert.equal(toolRun.result_summary.elementId, el.id);
  assert.equal(toolRun.result_summary.frame_count, 4);

  // card.anim.last_run
  const storedCard = after.groups.find((g) => g.id === card.id);
  assert.deepEqual(storedCard.anim.last_run, { at: result.run.at, result_element_id: el.id, verdict: "ok" });

  // ONE undo removes the flipbook element AND reverts anim.last_run to null.
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.ok(!undone.elements.some((e) => e.id === el.id));
  assert.equal(undone.groups.find((g) => g.id === card.id).anim.last_run, null);

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.ok(redone.elements.find((e) => e.id === el.id).flipbook, "redo restores the flipbook element");
});

test("generateAnimFromCard: game-owned production refuses before generation without an accepted lock; noLock stamps tainted explore origin", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Owned anim", gameId: "missing-anim-lock" });
  const { card } = animCardWithKeyframe(project.id);
  const gen = fakeAnimGenerator(t);

  await assert.rejects(
    () => generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: gen }),
    /production generation requires.*style_lock.*--no-lock/s,
  );
  assert.equal(gen.calls.length, 0, "missing production lock refuses before the generator call");

  const handler = createCanvasApi(ROOT);
  const invalidNoLock = await invokeApi(
    handler,
    "POST",
    `/api/canvas/projects/${project.id}/anim-cards/${card.id}/generate`,
    { noLock: "true" },
  );
  assert.equal(invalidNoLock.status, 400);
  assert.match(invalidNoLock.json().error, /noLock must be a boolean/);
  assert.equal(gen.calls.length, 0, "API validation also refuses before the injected generator is called");

  const result = await generateAnimFromCard(ROOT, {
    projectId: project.id,
    groupId: card.id,
    generators: gen,
    noLock: true,
  });
  assert.deepEqual(result.element.meta.origin, {
    schema: "ai_studio.asset.generation_origin.v1",
    source: "ai",
    mode: "explore",
    game_id: "missing-anim-lock",
    style_lock_id: null,
    tainted: true,
    taint_reason: "no-lock",
  });
});

test("generateAnimFromCard (nested card): flipbook lands in the PARENT scope, never inside the card", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim gen nested" });
  const outer = createGroup(ROOT, { projectId: project.id, name: "Outer", x: 0, y: 0, w: 2000, h: 2000 }).group;
  const card = createAnimCard(ROOT, { projectId: project.id, name: "Walk", parentId: outer.id, x: 100, y: 50, w: 300, h: 200 }).group;
  patchAnim(ROOT, { projectId: project.id, groupId: card.id, patch: { motion: "walk" } });
  const kf = addImage(ROOT, project.id, { name: "kf.png", bytes: solidPng() }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [kf.id], groupId: card.id });

  const gen = fakeAnimGenerator(t, { frames: 2 });
  const result = await generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: gen });
  assert.equal(result.element.groupId, outer.id, "PARENT scope, never the card itself");
});

test("generateAnimFromCard: loop:false -> play_mode once", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim gen once" });
  const { card } = animCardWithKeyframe(project.id, { loop: false });
  const gen = fakeAnimGenerator(t, { frames: 3 });
  const result = await generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: gen });
  assert.equal(result.element.flipbook.play_mode, "once");
});

test("generateAnimFromCard refuses when the head moved during generation (HEAD_CONFLICT); no flipbook written, last_run untouched", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim gen headmoved" });
  const { card } = animCardWithKeyframe(project.id);
  const seqBefore = getProject(ROOT, project.id).history_seq;

  // A generator that lands a CONCURRENT edit (bumps the head) mid-run, then returns frames.
  const dir = mkdtempSync(join(tmpdir(), "canvas-anim-hm-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const gen = {
    run: async () => {
      patchAnim(ROOT, { projectId: project.id, groupId: card.id, patch: { profile: "final" } }); // bumps head
      const p = join(dir, "frame_000.png");
      writeFileSync(p, rgbaFramePng(8, 6, 1));
      return { framePaths: [p], meta: { frame_w: 8, frame_h: 6, fps: 12 }, runDir: dir };
    },
  };

  await assert.rejects(
    () => generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: gen }),
    (e) => e.code === "HEAD_CONFLICT" || /changed underneath/.test(e.message),
  );

  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "only the concurrent patch landed");
  assert.ok(!after.elements.some((e) => e.flipbook), "no flipbook element written on refusal");
  assert.equal(after.groups.find((g) => g.id === card.id).anim.last_run, null, "last_run untouched");
});

test("generateAnimFromCard: a card with seed=null freezes the RESOLVED seed into provenance (never null); the card keeps the requested null", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim gen seed" });
  const { card } = animCardWithKeyframe(project.id); // default anim.seed === null
  const gen = fakeAnimGenerator(t, { frames: 2 });
  const result = await generateAnimFromCard(ROOT, { projectId: project.id, groupId: card.id, generators: gen });

  assert.equal(typeof result.element.meta.anim_run.seed, "number", "provenance seed is a real number, not null");
  assert.equal(result.element.meta.anim_run.seed, FAKE_ROLLED_SEED, "the seed the generator actually rolled");
  assert.equal(result.run.params.seed, FAKE_ROLLED_SEED, "tool_runs row records the resolved seed too");

  const after = getProject(ROOT, project.id);
  assert.equal(after.groups.find((g) => g.id === card.id).anim.seed, null, "the card keeps the REQUESTED seed (null) untouched");
});

// ================================================================================
// F4: createAnimCard(memberId) — the "Animate this image" promotion (one gesture, one entry)
// ================================================================================

test("createAnimCard(memberId): ONE journal entry — box fits the image (+24 pad), member moves INSIDE as the first keyframe; ONE undo pops it back out", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim promote" });
  const img = addImage(ROOT, project.id, { name: "hero.png", bytes: solidPng(40, 30, [9, 9, 9]), x: 100, y: 60 }).element;
  assert.equal(img.groupId, undefined, "the image starts at the root scope");
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const { project: after, group: card } = createAnimCard(ROOT, { projectId: project.id, memberId: img.id });

  // box = member box + 24 on every side
  assert.equal(card.x, 76);
  assert.equal(card.y, 36);
  assert.equal(card.w, 88);
  assert.equal(card.h, 78);
  assert.ok(card.anim, "still a real animation card");

  // ONE journal entry (create + move fused)
  assert.equal(after.history_seq, seqBefore + 1);

  // member moved INSIDE the card (world position unchanged) and is the sole (first) keyframe
  const moved = after.elements.find((e) => e.id === img.id);
  assert.equal(moved.groupId, card.id, "member now lives in the card scope");
  assert.equal(moved.x, 100, "member keeps its world position");
  assert.equal(moved.y, 60);
  const members = after.elements.filter((e) => e.groupId === card.id && e.type === "image");
  assert.deepEqual(members.map((e) => e.id), [img.id], "the promoted image is the card's only (first) keyframe");

  // the promotion carries a distinct history label
  assert.equal(historyEntryLabel("createAnimCard", { memberId: img.id, name: card.name }).label, "Anim card from image");

  // ONE undo removes the card AND pops the member back to the root scope
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.ok(!undone.groups.some((g) => g.id === card.id), "card gone");
  assert.equal(undone.elements.find((e) => e.id === img.id).groupId, undefined, "member back at the root scope");
});

test("createAnimCard(memberId) refusals: not found, explicit geometry, not an image, already a card member / claimed style ref", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim promote refuse" });
  const img = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng(10, 10, [1, 2, 3]) }).element;

  // not found
  assert.throws(() => createAnimCard(ROOT, { projectId: project.id, memberId: "el_missing" }), /element not found/);

  // explicit geometry alongside memberId (fit owns the box)
  assert.throws(() => createAnimCard(ROOT, { projectId: project.id, memberId: img.id, x: 5 }), /pass memberId OR explicit/);
  assert.throws(() => createAnimCard(ROOT, { projectId: project.id, memberId: img.id, w: 500 }), /pass memberId OR explicit/);

  // not an image (a note; real repo root so addNote can read the fonts manifest)
  const note = addNote(REPO_ROOT, project.id, { content: "hi", x: 0, y: 0 }).element;
  assert.throws(() => createAnimCard(ROOT, { projectId: project.id, memberId: note.id }), /is not an image/);

  // already a member of another card (recipe) -> duplicate the image first
  const recipe = createRecipeCard(ROOT, { projectId: project.id, name: "R" }).group;
  const inRecipe = addImage(ROOT, project.id, { name: "b.png", bytes: solidPng(10, 10, [4, 5, 6]) }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [inRecipe.id], groupId: recipe.id });
  assert.throws(() => createAnimCard(ROOT, { projectId: project.id, memberId: inRecipe.id }), /duplicate the image first/);

  // a claimed style-card ref (auto-claimed on assign) -> duplicate the image first
  const style = createStyleCard(ROOT, { projectId: project.id, name: "S" }).group;
  const styleRef = addImage(ROOT, project.id, { name: "c.png", bytes: solidPng(10, 10, [7, 8, 9]) }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [styleRef.id], groupId: style.id });
  assert.throws(() => createAnimCard(ROOT, { projectId: project.id, memberId: styleRef.id }), /duplicate the image first/);
});

// ================================================================================
// F3: pixel ops refuse a flipbook element (increment 1 — no frame-level editing yet)
// ================================================================================

async function makeFlipbook(t, projectId, opts) {
  const { card } = animCardWithKeyframe(projectId);
  const gen = fakeAnimGenerator(t, { frames: 3, ...opts });
  const result = await generateAnimFromCard(ROOT, { projectId, groupId: card.id, generators: gen });
  return result.element;
}

test("pixel ops refuse a flipbook element (src-swap would diverge frame 0): alpha cutout single+batch, cleanup, bake — no Python spawn", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Anim flipbook guard" });
  const flip = await makeFlipbook(t, project.id);
  assert.ok(flip.flipbook, "the minted element carries a flipbook");

  const msg = /has a flipbook — frame-level editing lands in increment 2/;
  await assert.rejects(() => alphaCutout(ROOT, { projectId: project.id, elementId: flip.id }), msg);
  await assert.rejects(() => alphaCutout(ROOT, { projectId: project.id, elementIds: [flip.id] }), msg);
  await assert.rejects(() => cleanupApply(ROOT, { projectId: project.id, elementId: flip.id, tool: "quantize", params: { colors: 8 } }), msg);
  await assert.rejects(() => bakeFilters(ROOT, { projectId: project.id, elementId: flip.id }), msg);
});

// ================================================================================
// F6: nesting-guard symmetry — recipe/style cards refuse EVERY card parent
// ================================================================================

test("createRecipeCard / createStyleCard refuse an animation-card parent AND a same-type card parent (F6 holes closed)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Nest symmetry" });
  const recipe = createRecipeCard(ROOT, { projectId: project.id, name: "R" }).group;
  const style = createStyleCard(ROOT, { projectId: project.id, name: "S" }).group;
  const anim = createAnimCard(ROOT, { projectId: project.id, name: "A" }).group;

  // recipe card cannot nest in another recipe card (was a hole) nor an anim card
  assert.throws(() => createRecipeCard(ROOT, { projectId: project.id, parentId: recipe.id }), /cards do not nest inside cards/);
  assert.throws(() => createRecipeCard(ROOT, { projectId: project.id, parentId: anim.id }), /cards do not nest inside cards/);
  // style card cannot nest in another style card (was a hole) nor an anim card
  assert.throws(() => createStyleCard(ROOT, { projectId: project.id, parentId: style.id }), /cards do not nest inside cards/);
  assert.throws(() => createStyleCard(ROOT, { projectId: project.id, parentId: anim.id }), /cards do not nest inside cards/);
});

// ================================================================================
// F1: applyFpsToWorkflow — pure fps injection into a ComfyUI graph (no ComfyUI)
// ================================================================================

test("applyFpsToWorkflow: override sets the fps-bearing node and returns it; no override returns the graph fps; loud on a missing node / non-positive fps", () => {
  const graph = () => ({ "16": { inputs: { fps: 16 } }, "12": { inputs: { width: 512 } } });

  // no override -> the workflow's own fps, graph untouched
  const g1 = graph();
  assert.equal(applyFpsToWorkflow(g1, null), 16);
  assert.equal(g1["16"].inputs.fps, 16);
  assert.equal(applyFpsToWorkflow(g1, undefined), 16);

  // override -> sets the fps node (found by inputs.fps, not a hardcoded id) + returns the value
  const g2 = graph();
  assert.equal(applyFpsToWorkflow(g2, 24), 24);
  assert.equal(g2["16"].inputs.fps, 24);

  // a positive override with NO fps-bearing node is loud (the workflow shape changed)
  assert.throws(() => applyFpsToWorkflow({ "12": { inputs: { width: 512 } } }, 24), /no node carrying inputs\.fps/);
  // a non-positive override is loud
  assert.throws(() => applyFpsToWorkflow(graph(), 0), /positive number/);
  assert.throws(() => applyFpsToWorkflow(graph(), -5), /positive number/);
});

// ================================================================================
// API + CLI parity (VALIDATION-level for generate — no real ComfyUI spawn)
// ================================================================================

test("API POST anim-cards / PATCH anim-cards/<gid> parity; generate on an empty-motion card 400s (pre-generation refusal)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);

  const createdProject = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "API Anim" });
  const projectId = createdProject.json().project.id;

  const created = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/anim-cards`, { name: "API card", x: 1, y: 2 });
  assert.equal(created.status, 201);
  const group = created.json().group;
  assert.equal(group.name, "API card");
  assert.deepEqual(group.anim, DEFAULT_ANIM);

  const patched = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/anim-cards/${group.id}`, { motion: "idle bob", profile: "final", loop: false });
  assert.equal(patched.status, 200);
  assert.equal(patched.json().group.anim.motion, "idle bob");
  assert.equal(patched.json().group.anim.profile, "final");
  assert.equal(patched.json().group.anim.loop, false);
  assert.ok(Number.isFinite(patched.json().duration_ms));
  assert.ok(patched.json().history);

  // A plain group (no anim) 400s through the API too.
  const plainGroup = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, { name: "Plain", x: 0, y: 0, w: 10, h: 10 });
  const plainId = plainGroup.json().group.id;
  const rejected = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/anim-cards/${plainId}`, { motion: "x" });
  assert.equal(rejected.status, 400);
  assert.match(rejected.json().error, /not an animation card/);

  // generate on a card with no keyframes fails BEFORE any generation (400), never spawns.
  const blank = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/anim-cards`, { name: "Blank" });
  const blankId = blank.json().group.id;
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/anim-cards/${blankId}`, { motion: "bob" });
  const genRejected = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/anim-cards/${blankId}/generate`, {});
  assert.equal(genRejected.status, 400);
  assert.match(genRejected.json().error, /no keyframes/);
});

test("CLI anim-card / anim-patch parity; anim-generate on an empty-motion card fails loud (pre-generation refusal)", (t) => {
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  const projectId = run(env, "create", "--title", "CLI Anim").project.id;
  const card = run(env, "anim-card", projectId, "--name", "Idle", "--x", "10", "--y", "20").group;
  assert.equal(card.name, "Idle");
  assert.deepEqual(card.anim, DEFAULT_ANIM);

  const patched = run(env, "anim-patch", projectId, "--group", card.id, "--motion", "gentle bob", "--profile", "final", "--matte", "key_matte", "--loop", "false", "--gen-fps", "16", "--seed", "701");
  assert.equal(patched.group.anim.motion, "gentle bob");
  assert.equal(patched.group.anim.profile, "final");
  assert.equal(patched.group.anim.matte, "key_matte");
  assert.equal(patched.group.anim.loop, false);
  assert.equal(patched.group.anim.gen_fps, 16);
  assert.equal(patched.group.anim.seed, 701);

  const badMatte = runFail(env, "anim-patch", projectId, "--group", card.id, "--matte", "greenscreen");
  assert.match(String(badMatte.stderr || badMatte.message), /matte must be one of/);

  // anim-generate on a card with motion but NO keyframes fails before any generation (no spawn).
  const genFail = runFail(env, "anim-generate", projectId, "--group", card.id);
  assert.match(String(genFail.stderr || genFail.message), /no keyframes/);

  const valuedNoLock = runFail(env, "anim-generate", projectId, "--group", card.id, "--no-lock", "false");
  assert.match(String(valuedNoLock.stderr || valuedNoLock.message), /--no-lock does not take a value/);

  const shown = run(env, "show", projectId).project;
  assert.equal(shown.groups.find((g) => g.id === card.id).anim.motion, "gentle bob");
});

test("API POST anim-cards with memberId promotes the image in ONE journal entry (fit box, member inside); explicit geometry + memberId 400s", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const createdProject = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "API Anim member" });
  const projectId = createdProject.json().project.id;

  const img = addImage(ROOT, projectId, { name: "hero.png", bytes: solidPng(20, 20, [5, 5, 5]), x: 200, y: 100 }).element;
  const seqBefore = getProject(ROOT, projectId).history_seq;

  const created = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/anim-cards`, { memberId: img.id });
  assert.equal(created.status, 201);
  const card = created.json().group;
  assert.equal(card.x, 176, "box fits the image (200 - 24)");
  assert.equal(card.w, 68, "20 + 24*2");
  assert.ok(card.anim);

  const after = getProject(ROOT, projectId);
  assert.equal(after.history_seq, seqBefore + 1, "ONE journal entry");
  assert.equal(after.elements.find((e) => e.id === img.id).groupId, card.id, "member moved inside the card");

  const rejected = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/anim-cards`, { memberId: img.id, x: 5 });
  assert.equal(rejected.status, 400);
  assert.match(rejected.json().error, /pass memberId OR explicit/);
});

test("CLI anim-card --member promotes an image (fit box, member inside); anim-patch bare value flags are loud (F5)", (t) => {
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  const projectId = run(env, "create", "--title", "CLI Anim member").project.id;
  // Seed an image through the ops API (shares CANVAS_PROJECTS_ROOT with the CLI subprocess).
  const img = addImage(ROOT, projectId, { name: "hero.png", bytes: solidPng(30, 30, [2, 2, 2]), x: 50, y: 40 }).element;

  const card = run(env, "anim-card", projectId, "--member", img.id).group;
  assert.equal(card.x, 26, "50 - 24");
  assert.equal(card.w, 78, "30 + 24*2");
  assert.ok(card.anim);
  const shown = run(env, "show", projectId).project;
  assert.equal(shown.elements.find((e) => e.id === img.id).groupId, card.id, "member moved inside the card");

  // bare --member fails loud
  const bareMember = runFail(env, "anim-card", projectId, "--member");
  assert.match(String(bareMember.stderr || bareMember.message), /--member requires an element id/);

  // F5: bare value flags on anim-patch are loud (never a silent clear / NaN)
  const bareSeed = runFail(env, "anim-patch", projectId, "--group", card.id, "--seed");
  assert.match(String(bareSeed.stderr || bareSeed.message), /--seed requires a value or 'none'/);
  const bareStyle = runFail(env, "anim-patch", projectId, "--group", card.id, "--style");
  assert.match(String(bareStyle.stderr || bareStyle.message), /--style requires a value or 'none'/);
  const bareAccepted = runFail(env, "anim-patch", projectId, "--group", card.id, "--accepted");
  assert.match(String(bareAccepted.stderr || bareAccepted.message), /--accepted requires a value or 'none'/);
  const bareGenFps = runFail(env, "anim-patch", projectId, "--group", card.id, "--gen-fps");
  assert.match(String(bareGenFps.stderr || bareGenFps.message), /--gen-fps requires a value or 'none'/);

  // an explicit clear via 'none' still works
  const cleared = run(env, "anim-patch", projectId, "--group", card.id, "--style", "none");
  assert.equal(cleared.group.anim.style_ref, null);
});
