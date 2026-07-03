// cleanupPreview/cleanupApply op + API/CLI parity tests (T0207). Run:
//   node --test ai_studio/assets/canvas/tests/cleanup.test.mjs
//
// Cleanup is TWO separate interactive tools — Quantize (color-count + optional dither)
// and Denoise (strength) — never one monolithic "Clean up" (bg-solidify is CUT as a
// standalone tool; it stays an internal pre-pass of the alpha keyer only). Preview runs
// the tool against the element's CURRENT pixels and hands back base64 bytes + a report
// WITHOUT touching the store at all (no files/ write, no journal entry); Apply commits a
// fresh deterministic run of the SAME tool+params as ONE journal entry (new
// content-addressed file + element.src swap + element.meta.cleanup) — undo restores the
// previous src byte-exact, exactly like alphaCutout. The validation tests need no Python;
// the pipeline tests run the real warm-worker path and skip cleanly when the studio venv
// is missing.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readdirSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath, URL } from "node:url";

import { addImage, addText, cleanupApply, cleanupPreview, createProject, getProject, patchElement, undoOp, redoOp } from "../ops.mjs";
import { resolveProjectFile, resolveProjectPath, updateProject } from "../store.mjs";
import { createCanvasApi } from "../api.mjs";
import { encodePng, decodePng, magentaSheetPng } from "./png_fixture.mjs";

// Python tools run with cwd = repo root, so the pipeline tests must use the real root.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
// The validation tests never spawn Python, so any unused root works for them.
const UNUSED_ROOT = "C:/unused-repo-root";
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cleanup-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// A 24x16 RGBA gradient (lots of unique colors, so quantize has real work to do) with a
// transparent right strip, so tests can prove alpha never moves under either tool.
function gradientRgbaPng() {
  const width = 24;
  const height = 16;
  return encodePng(
    width,
    height,
    (x, y) => [
      Math.round((x * 255) / (width - 1)),
      Math.round((y * 255) / (height - 1)),
      128,
      x < width - 4 ? 255 : 0,
    ],
    { alpha: true },
  );
}

function filesListing(root, projectId) {
  const dir = resolveProjectPath(root, projectId, "files");
  return existsSync(dir) ? readdirSync(dir).sort() : [];
}

// Try a whole-element preview or apply; return the op's result or null on a venv/Pillow
// miss (the caller then t.skip's). Isolates the "does Python run here" gate.
async function tryCleanup(t, fn, args) {
  try {
    return await fn(REPO_ROOT, args);
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`cleanup pipeline unavailable: ${error.message}`);
      return null;
    }
    throw error;
  }
}

// ---- validation (no Python) --------------------------------------------------

test("cleanupPreview/cleanupApply require projectId and elementId", async () => {
  await assert.rejects(() => cleanupPreview(UNUSED_ROOT, {}), /requires projectId/);
  await assert.rejects(() => cleanupPreview(UNUSED_ROOT, { projectId: "p" }), /requires elementId/);
  await assert.rejects(() => cleanupApply(UNUSED_ROOT, {}), /requires projectId/);
  await assert.rejects(() => cleanupApply(UNUSED_ROOT, { projectId: "p" }), /requires elementId/);
});

test("cleanup rejects an unknown tool loudly, before any project/disk read", async () => {
  await assert.rejects(
    () => cleanupPreview(UNUSED_ROOT, { projectId: "nonexistent-project-xyz", elementId: "e1", tool: "sharpen" }),
    /unknown cleanup tool/,
  );
  await assert.rejects(
    () => cleanupApply(UNUSED_ROOT, { projectId: "nonexistent-project-xyz", elementId: "e1", tool: "" }),
    /unknown cleanup tool/,
  );
});

test("quantize rejects out-of-range colors loudly (no python)", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Quantize validate" });
  const { element } = addImage(UNUSED_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  await assert.rejects(
    () => cleanupPreview(UNUSED_ROOT, { projectId: project.id, elementId: element.id, tool: "quantize", params: { colors: 1 } }),
    /colors between 2 and 256/,
  );
  await assert.rejects(
    () => cleanupApply(UNUSED_ROOT, { projectId: project.id, elementId: element.id, tool: "quantize", params: { colors: 257 } }),
    /colors between 2 and 256/,
  );
});

test("denoise rejects an unknown strength loudly (no python)", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Denoise validate" });
  const { element } = addImage(UNUSED_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  await assert.rejects(
    () => cleanupPreview(UNUSED_ROOT, { projectId: project.id, elementId: element.id, tool: "denoise", params: { strength: 4 } }),
    /strength 1, 2, or 3/,
  );
});

test("cleanup rejects a non-image (text) element loudly", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Cleanup validate text" });
  const { element } = addText(REPO_ROOT, project.id, { content: "hi" });
  await assert.rejects(
    () => cleanupPreview(REPO_ROOT, { projectId: project.id, elementId: element.id, tool: "quantize", params: { colors: 8 } }),
    /is not an image/,
  );
});

test("cleanup refuses a rotated/flipped element with the SAME message alphaCutout uses (R7 parity)", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Cleanup transform guard" });
  const { element } = addImage(UNUSED_ROOT, project.id, { name: "sheet.png", bytes: magentaSheetPng() });
  patchElement(UNUSED_ROOT, project.id, element.id, { rotation: 45 });
  await assert.rejects(
    () => cleanupPreview(UNUSED_ROOT, { projectId: project.id, elementId: element.id, tool: "quantize", params: { colors: 8 } }),
    /rotated\/flipped/,
  );
  await assert.rejects(
    () => cleanupApply(UNUSED_ROOT, { projectId: project.id, elementId: element.id, tool: "denoise", params: { strength: 1 } }),
    /rotated\/flipped/,
  );
});

// ---- pipeline: preview (real warm worker; skips without the studio venv) ------

test("cleanupPreview (quantize) returns bytes + report and writes NOTHING to the store", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Quantize preview" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "gradient.png", bytes: gradientRgbaPng() });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const filesBefore = filesListing(REPO_ROOT, project.id);

  const result = await tryCleanup(t, cleanupPreview, {
    projectId: project.id,
    elementId: element.id,
    tool: "quantize",
    params: { colors: 6 },
  });
  if (!result) return;

  assert.equal(result.tool, "quantize");
  assert.equal(result.params.colors, 6);
  assert.ok(result.previewBase64 && result.previewBase64.length > 0, "preview carries base64 bytes");
  assert.equal(result.report.colors_requested, 6);
  assert.ok(result.report.palette_size_after <= 6, "quantized palette respects the requested cap");

  const png = decodePng(Buffer.from(result.previewBase64, "base64"));
  assert.equal(png.channels, 4, "preview PNG carries an alpha channel");
  assert.equal(png.width, element.w);
  assert.equal(png.height, element.h);
  // Alpha preserved exactly: the transparent strip stays fully transparent.
  assert.equal(png.at(element.w - 1, 0)[3], 0);
  assert.equal(png.at(0, 0)[3], 255);

  // NOTHING written to the store: no journal entry, no new files/ entry.
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore, "preview writes no journal entry");
  assert.deepEqual(filesListing(REPO_ROOT, project.id), filesBefore, "preview writes no files/ entry");
  assert.equal(getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id).src, element.src, "element src untouched by preview");
});

test("cleanupPreview (denoise) returns bytes + report and writes NOTHING to the store", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Denoise preview" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "gradient.png", bytes: gradientRgbaPng() });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const filesBefore = filesListing(REPO_ROOT, project.id);

  const result = await tryCleanup(t, cleanupPreview, {
    projectId: project.id,
    elementId: element.id,
    tool: "denoise",
    params: { strength: 2 },
  });
  if (!result) return;

  assert.equal(result.tool, "denoise");
  assert.equal(result.params.strength, 2);
  assert.equal(result.report.strength, 2);
  assert.ok(Number.isFinite(result.report.changed_pixel_pct));

  const png = decodePng(Buffer.from(result.previewBase64, "base64"));
  assert.equal(png.channels, 4);
  assert.equal(png.at(element.w - 1, 0)[3], 0, "alpha preserved exactly (never filtered)");

  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore, "preview writes no journal entry");
  assert.deepEqual(filesListing(REPO_ROOT, project.id), filesBefore, "preview writes no files/ entry");
});

// ---- pipeline: apply (real warm worker; skips without the studio venv) --------

test("cleanupApply (quantize) is ONE journal entry: new file, src swap, meta.cleanup shape, undo restores byte-exact", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Quantize apply" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "gradient.png", bytes: gradientRgbaPng() });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;

  const result = await tryCleanup(t, cleanupApply, {
    projectId: project.id,
    elementId: element.id,
    tool: "quantize",
    params: { colors: 8, dither: true },
  });
  if (!result) return;

  // New content-addressed file, src swapped, geometry preserved.
  assert.notEqual(result.element.src, original.src, "element swapped to a new cleaned file");
  assert.equal(result.element.w, original.w);
  assert.equal(result.element.h, original.h);
  assert.ok(result.element.src.startsWith("files/"));

  // meta.cleanup shape (additive; alpha's meta, if any, would sit alongside it untouched).
  assert.equal(result.element.meta.cleanup.tool, "quantize");
  assert.deepEqual(result.element.meta.cleanup.params, { colors: 8, dither: true });
  assert.equal(result.element.meta.cleanup.prev_src, original.src);
  assert.ok(result.element.meta.cleanup.report);
  assert.ok(result.element.meta.cleanup.at);

  // ONE new journal entry for the whole apply.
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "apply is one journal entry");

  // The original file is immutable — still on disk with identical bytes.
  const originalAbs = resolveProjectFile(REPO_ROOT, project.id, original.src);
  assert.ok(existsSync(originalAbs), "original file kept on disk (non-destructive)");

  // Undo restores the element EXACTLY (src + no cleanup meta); redo re-applies the swap.
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  const afterUndo = undone.elements.find((el) => el.id === element.id);
  assert.deepEqual(afterUndo, original, "undo restores the exact pre-cleanup element");
  assert.ok(!afterUndo.meta || !afterUndo.meta.cleanup, "cleanup meta removed on undo");
  assert.ok(existsSync(originalAbs), "original file still present after undo");

  const redone = redoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((el) => el.id === element.id).src, result.element.src);
});

test("cleanupApply (denoise) is ONE journal entry: new file, src swap, meta.cleanup shape, undo restores byte-exact", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Denoise apply" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "gradient.png", bytes: gradientRgbaPng() });
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === element.id);
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;

  const result = await tryCleanup(t, cleanupApply, {
    projectId: project.id,
    elementId: element.id,
    tool: "denoise",
    params: { strength: 3 },
  });
  if (!result) return;

  assert.notEqual(result.element.src, original.src);
  assert.equal(result.element.meta.cleanup.tool, "denoise");
  assert.deepEqual(result.element.meta.cleanup.params, { strength: 3 });
  assert.equal(result.element.meta.cleanup.prev_src, original.src);

  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "apply is one journal entry");

  const originalAbs = resolveProjectFile(REPO_ROOT, project.id, original.src);
  assert.ok(existsSync(originalAbs), "original file kept on disk (non-destructive)");

  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.deepEqual(undone.elements.find((el) => el.id === element.id), original, "undo restores the exact pre-cleanup element");
});

test("cleanupApply preserves an existing alpha meta alongside the new cleanup meta (additive)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Cleanup after alpha" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "gradient.png", bytes: gradientRgbaPng() });
  // Fake a pre-existing meta.alpha (as alphaCutout would have left) without running the
  // real alpha pipeline — this test only cares that cleanup's meta write is additive.
  const before = getProject(REPO_ROOT, project.id);
  const withFakeAlphaMeta = before.elements.map((item) =>
    item.id === element.id ? { ...item, meta: { ...(item.meta || {}), alpha: { method: "matte" } } } : item,
  );
  // Direct project write (not journaled) just to seed the fixture state.
  updateProject(REPO_ROOT, project.id, { elements: withFakeAlphaMeta });

  const result = await tryCleanup(t, cleanupApply, {
    projectId: project.id,
    elementId: element.id,
    tool: "quantize",
    params: { colors: 16 },
  });
  if (!result) return;

  assert.equal(result.element.meta.alpha.method, "matte", "pre-existing alpha meta survives");
  assert.equal(result.element.meta.cleanup.tool, "quantize", "cleanup meta added alongside it");
});

// ---- API + CLI parity (real pipeline; skips without the venv) -----------------

test("cleanup-preview API route matches the op (no journal entry)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Cleanup preview API" });
  const { element } = addImage(REPO_ROOT, project.id, { name: "gradient.png", bytes: gradientRgbaPng() });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;

  const handler = createCanvasApi(REPO_ROOT);
  const captured = { status: 0, body: null };
  const res = {
    writeHead(status) { captured.status = status; return this; },
    end(payload) { captured.body = payload ? JSON.parse(payload) : null; },
  };
  const req = mockReq("POST", { tool: "quantize", params: { colors: 6 } });
  await handler(req, res, new URL(`http://x/api/canvas/projects/${project.id}/elements/${element.id}/cleanup-preview`));
  if (captured.status === 400 && /venv|Pillow|module/i.test(String(captured.body && captured.body.error))) {
    t.skip(`cleanup pipeline unavailable: ${captured.body.error}`);
    return;
  }
  assert.equal(captured.status, 200, `cleanup-preview 200 (got ${captured.status}: ${JSON.stringify(captured.body)})`);
  assert.ok(captured.body.preview_base64 && captured.body.preview_base64.length > 0);
  assert.equal(captured.body.tool, "quantize");
  assert.equal(captured.body.report.colors_requested, 6);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore, "API preview writes no journal entry");
});

test("cleanup API route and CLI drive the same apply op (one journal entry each)", async (t) => {
  const dir = tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Cleanup apply parity" });
  const { element: e1 } = addImage(REPO_ROOT, project.id, { name: "gradient1.png", bytes: gradientRgbaPng() });
  const { element: e2 } = addImage(REPO_ROOT, project.id, { name: "gradient2.png", bytes: gradientRgbaPng() });

  // API: POST /elements/<eid>/cleanup { tool, params } — mirror how the inspector's Apply
  // button calls it.
  const handler = createCanvasApi(REPO_ROOT);
  const captured = { status: 0, body: null };
  const res = {
    writeHead(status) { captured.status = status; return this; },
    end(payload) { captured.body = payload ? JSON.parse(payload) : null; },
  };
  const seqBeforeApi = getProject(REPO_ROOT, project.id).history_seq;
  const req = mockReq("POST", { tool: "quantize", params: { colors: 8 } });
  await handler(req, res, new URL(`http://x/api/canvas/projects/${project.id}/elements/${e1.id}/cleanup`));
  if (captured.status === 400 && /venv|Pillow|module/i.test(String(captured.body && captured.body.error))) {
    t.skip(`cleanup pipeline unavailable: ${captured.body.error}`);
    return;
  }
  assert.equal(captured.status, 200, `cleanup 200 (got ${captured.status}: ${JSON.stringify(captured.body)})`);
  assert.equal(captured.body.tool, "quantize");
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBeforeApi + 1, "API apply is one journal entry");

  // CLI preview: quantize <id> --element <eid> --colors 6 --preview <out.png> — writes the
  // preview PNG + prints the report, no journal entry.
  const previewOut = join(dir, "preview.png");
  const seqBeforeCliPreview = getProject(REPO_ROOT, project.id).history_seq;
  const previewStdout = execFileSync(
    "node",
    [CLI, "quantize", project.id, "--element", e2.id, "--colors", "6", "--preview", previewOut],
    { env: { ...process.env, CANVAS_PROJECTS_ROOT: dir }, encoding: "utf8" },
  );
  const previewParsed = JSON.parse(previewStdout.trim().split("\n").pop());
  assert.equal(previewParsed.tool, "quantize");
  assert.ok(existsSync(previewOut), "CLI --preview wrote the PNG");
  const previewPng = decodePng(readFileSync(previewOut));
  assert.equal(previewPng.channels, 4);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBeforeCliPreview, "CLI --preview writes no journal entry");

  // CLI apply: denoise <id> --element <eid> --strength 1 — same ops layer, one undo step.
  const seqBeforeCliApply = getProject(REPO_ROOT, project.id).history_seq;
  const applyStdout = execFileSync(
    "node",
    [CLI, "denoise", project.id, "--element", e2.id, "--strength", "1"],
    { env: { ...process.env, CANVAS_PROJECTS_ROOT: dir }, encoding: "utf8" },
  );
  const applyParsed = JSON.parse(applyStdout.trim().split("\n").pop());
  assert.equal(applyParsed.tool, "denoise");
  assert.match(applyParsed.element.src, /^files\//);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBeforeCliApply + 1, "CLI apply is one journal entry too");
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
