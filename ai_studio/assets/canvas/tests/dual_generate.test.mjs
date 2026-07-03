// alphaDualPlateGenerate (T0238: AUTOMATIC dual-plate alpha) + addImageFromFile tests. Run:
//   node --test ai_studio/assets/canvas/tests/dual_generate.test.mjs
//
// alphaDualPlateGenerate is an action on ONE existing image element: its CURRENT pixels
// are the dual-plate LIGHT plate (loud refusal unless the border is flat + light), the
// DARK plate is generated as a codex edit of it, the pair runs through the SAME
// alphaDualPlate tool (T0237/T0243, unmodified), with one automatic retry on a gate
// refusal. Codex NEVER runs in this suite — every pipeline test injects a fake
// `generator` (the {lightPngPath, prompt} -> Buffer|path seam); only the pure prompt/
// command builders in tools/dual_plate_generate.mjs are exercised for the DEFAULT
// generator, never spawned. The flat-bg check and the reused alphaDualPlate tool both
// need the studio venv (numpy/Pillow) — those tests skip cleanly when it is missing.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath, URL } from "node:url";

import { addImage, addImageFromFile, addText, alphaDualPlateGenerate, createProject, getProject, undoOp, redoOp } from "../ops.mjs";
import { resolveProjectFile } from "../store.mjs";
import { createCanvasApi } from "../api.mjs";
import { buildBlackPlatePrompt, buildGenerateDarkPlateCommand } from "../tools/dual_plate_generate.mjs";
import { darkBgPng, decodePng, dualPlatePairPng, magentaSheetPng } from "./png_fixture.mjs";

// Python tools run with cwd = repo root, so the pipeline tests must use the real root.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
// The pure/validation tests never spawn Python, so any unused root works for them.
const UNUSED_ROOT = "C:/unused-repo-root";
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));
const VENV_MISSING = /venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i;

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-dualgen-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// A fake generator that always returns the SAME bytes (Buffer), counting calls so tests
// can assert retry behavior precisely.
function fakeGenerator(bytes) {
  const calls = [];
  const fn = async (args) => {
    calls.push(args);
    return bytes;
  };
  fn.calls = calls;
  return fn;
}

// ---- pure builders (no spawn — T0238 packet: "unit-test the command line it builds") ---

test("buildBlackPlatePrompt: subject-lock + background-swap clauses, extra text appended", () => {
  const base = buildBlackPlatePrompt();
  assert.match(base, /BACKGROUND-COLOR SWAP/);
  assert.match(base, /solid black #000000/);
  assert.match(base, /Keep the subject EXACTLY as in the input image/);
  assert.ok(!base.endsWith(" "), "no trailing space with no extra text");

  const withExtra = buildBlackPlatePrompt("a pair of angel wings");
  assert.ok(withExtra.startsWith(base), "extra text is APPENDED, base clauses are never reordered");
  assert.match(withExtra, /a pair of angel wings$/);
});

test("buildGenerateDarkPlateCommand: builds the exact argv, never spawns anything", () => {
  const { command, args } = buildGenerateDarkPlateCommand({
    lightPngPath: "C:/tmp/light.png",
    prompt: "test prompt",
    outPath: "C:/tmp/out.png",
  });
  assert.equal(command, "python");
  assert.match(args[0], /generate_image\.py$/, "argv[0] is the codex image-generation skill script");
  assert.deepEqual(args.slice(1), [
    "--input-image", "C:/tmp/light.png",
    "--prompt", "test prompt",
    "--out", "C:/tmp/out.png",
    "--size", "1024x1024",
    "--quality", "high",
  ]);
});

test("buildGenerateDarkPlateCommand requires lightPngPath/prompt/outPath", () => {
  assert.throws(() => buildGenerateDarkPlateCommand({ prompt: "p", outPath: "o" }), /requires lightPngPath/);
  assert.throws(() => buildGenerateDarkPlateCommand({ lightPngPath: "l", outPath: "o" }), /requires prompt/);
  assert.throws(() => buildGenerateDarkPlateCommand({ lightPngPath: "l", prompt: "p" }), /requires outPath/);
});

// ---- alphaDualPlateGenerate validation (no Python) -----------------------------

test("alphaDualPlateGenerate validates projectId/elementId before touching disk", async () => {
  await assert.rejects(() => alphaDualPlateGenerate(UNUSED_ROOT, {}), /requires projectId/);
  await assert.rejects(() => alphaDualPlateGenerate(UNUSED_ROOT, { projectId: "p" }), /requires elementId/);
});

test("alphaDualPlateGenerate rejects an unknown element id and a non-image element, before any generator call", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Dual generate validate" });
  const { element: image } = addImage(REPO_ROOT, project.id, { name: "a.png", bytes: magentaSheetPng() });
  const { element: text } = addText(REPO_ROOT, project.id, { content: "hi" });
  const generator = fakeGenerator(Buffer.from("unused"));

  await assert.rejects(
    () => alphaDualPlateGenerate(REPO_ROOT, { projectId: project.id, elementId: "el_missing", generator }),
    /element not found/,
  );
  await assert.rejects(
    () => alphaDualPlateGenerate(REPO_ROOT, { projectId: project.id, elementId: text.id, generator }),
    /is not an image/,
  );
  assert.equal(generator.calls.length, 0, "the generator is never called before validation passes");
});

// ---- pipeline (real warm worker; skips without the studio venv) ---------------

// Try alphaDualPlateGenerate with a fake generator; return null (after t.skip) on a
// venv/Pillow miss so the caller returns early. Isolates the "does Python run here" gate.
async function tryGenerate(t, args) {
  try {
    return await alphaDualPlateGenerate(REPO_ROOT, args);
  } catch (error) {
    if (VENV_MISSING.test(error.message)) {
      t.skip(`dual-plate pipeline unavailable: ${error.message}`);
      return undefined;
    }
    throw error;
  }
}

test("alphaDualPlateGenerate (happy path): ONE new element, ONE journal entry, placement/meta/prompt/gate/align recorded; source untouched; undo/redo", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Dual generate happy" });
  const { white, black } = dualPlatePairPng();
  const { element: source } = addImage(REPO_ROOT, project.id, { name: "wings.png", bytes: white });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const original = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === source.id);

  const generator = fakeGenerator(black);
  const result = await tryGenerate(t, { projectId: project.id, elementId: source.id, prompt: "angel wings", generator });
  if (result === undefined) return; // skipped

  // ONE new journal entry for the whole gesture (generation itself is outside it).
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "the whole gesture is one journal entry");
  assert.equal(after.elements.length, 2, "1 source + 1 new cut element");
  assert.equal(generator.calls.length, 1, "no retry needed on a clean pair");
  assert.equal(generator.calls[0].prompt.includes("angel wings"), true, "the extra prompt text reached the generator");

  // A brand-new element, named off the source, placed to its RIGHT with a 16px gap.
  assert.notEqual(result.element.id, source.id);
  assert.equal(result.element.name, `${source.name} alpha`);
  assert.equal(result.element.x, source.x + source.w + 16);
  assert.equal(result.element.y, source.y);

  // meta.alpha: method, fixed light/dark plate roles, prompt, pair gate verdict, align.
  const alphaMeta = result.element.meta.alpha;
  assert.equal(alphaMeta.method, "dual_plate");
  assert.equal(alphaMeta.plates.length, 2);
  assert.deepEqual(alphaMeta.plates[0], { src: source.src, role: "light" });
  assert.equal(alphaMeta.plates[1].role, "dark");
  assert.match(alphaMeta.plates[1].src, /^files\//);
  assert.match(alphaMeta.prompt, /angel wings$/);
  assert.equal(alphaMeta.pair_gate.verdict, "pass");
  assert.ok(alphaMeta.align && typeof alphaMeta.align.dx === "number", "align delta recorded");
  assert.ok("fraction_before" in alphaMeta.align && "fraction_after" in alphaMeta.align);

  // The extracted PNG: transparent background, opaque recovered blob at its original color.
  const png = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, result.element.src)));
  assert.equal(png.channels, 4);
  assert.equal(png.at(0, 0)[3], 0, "background is transparent");
  assert.equal(png.at(18, 14)[3], 255, "blob is opaque");

  // The SOURCE element is byte-exact untouched (non-destructive).
  assert.deepEqual(after.elements.find((el) => el.id === source.id), original);

  // ONE undo removes ONLY the new element.
  const undone = undoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.equal(undone.elements.length, 1);
  assert.deepEqual(undone.elements[0], original);

  // Redo re-creates the exact same element.
  const redone = redoOp(REPO_ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((el) => el.id === result.element.id).src, result.element.src);
});

test("alphaDualPlateGenerate refuses a non-flat/dark background LOUDLY, before any generation, nothing written", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Dual generate flat-bg refusal" });
  const { element: source } = addImage(REPO_ROOT, project.id, { name: "dark.png", bytes: darkBgPng() });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const original = getProject(REPO_ROOT, project.id).elements.map((el) => ({ ...el }));
  const generator = fakeGenerator(Buffer.from("unused"));

  try {
    await alphaDualPlateGenerate(REPO_ROOT, { projectId: project.id, elementId: source.id, generator });
    assert.fail("a dark/non-flat background must refuse before generating");
  } catch (error) {
    if (VENV_MISSING.test(error.message)) {
      t.skip(`dual-plate pipeline unavailable: ${error.message}`);
      return;
    }
    assert.match(error.message, /flat light background/i);
    assert.ok(!/Traceback|File "/.test(error.message), `traceback leaked to the operator: ${error.message}`);
  }

  assert.equal(generator.calls.length, 0, "the generator is never invoked on a flat-bg refusal");
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore, "nothing written: no journal entry");
  assert.deepEqual(after.elements, original, "nothing written: source element unchanged, no new element");
});

test("alphaDualPlateGenerate: a gate refusal retries EXACTLY once, then a loud error names both preserved dark-plate attempts + the manual path", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Dual generate gate-fail retry" });
  const { white, black } = dualPlatePairPng({ offset: 10 }); // misaligned -> gate "regenerate" even after T0243 align
  const { element: source } = addImage(REPO_ROOT, project.id, { name: "wings.png", bytes: white });
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const original = getProject(REPO_ROOT, project.id).elements.map((el) => ({ ...el }));

  const generator = fakeGenerator(black); // ALWAYS the misaligned dark plate — both attempts fail
  let venvMissing = false;
  try {
    await alphaDualPlateGenerate(REPO_ROOT, { projectId: project.id, elementId: source.id, generator });
    assert.fail("a pair that never passes the gate must refuse after the retry");
  } catch (error) {
    if (VENV_MISSING.test(error.message)) {
      venvMissing = true;
    } else {
      // Retry counted exactly 1: the generator ran twice (1 initial + 1 retry), never more.
      assert.equal(generator.calls.length, 2, "exactly one automatic retry");
      assert.match(error.message, /dark_attempt_1=/);
      assert.match(error.message, /dark_attempt_2=/);
      assert.match(error.message, new RegExp(`light=${source.src.replace(/[/.]/g, "\\$&")};`));
      assert.match(error.message, /alphaDualPlate|alpha-dual/, "names the manual pair-op retry path");
      assert.ok(!/Traceback|File "/.test(error.message), `traceback leaked to the operator: ${error.message}`);

      // Both preserved dark-plate attempts actually exist on disk (content-addressed;
      // semicolon-delimited so a ".png" src is never mis-split by the parser).
      const attemptSrcs = [...error.message.matchAll(/dark_attempt_\d+=([^;]+);/g)].map((m) => m[1]);
      assert.equal(attemptSrcs.length, 2);
      for (const src of attemptSrcs) {
        assert.ok(existsSync(resolveProjectFile(REPO_ROOT, project.id, src)), `preserved dark plate exists: ${src}`);
      }
    }
  }
  if (venvMissing) {
    t.skip("dual-plate pipeline unavailable (studio venv)");
    return;
  }

  // Nothing mutated on project.json: no journal entry, source unchanged, no new element.
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore, "no journal entry on a rejected pair");
  assert.equal(after.elements.length, 1, "no new cut element created");
  assert.deepEqual(after.elements, original, "source element untouched");
});

// ---- alpha-dual-generate CLI + API validation parity (no Python) --------------

test("alpha-dual-generate: CLI and API reject a missing element the same way (validation parity, no python)", async (t) => {
  const dir = tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Dual generate parity validation" });

  // API: POST /alpha-dual-generate with no elementId -> the op's own "requires elementId".
  const handler = createCanvasApi(REPO_ROOT);
  const captured = { status: 0, body: null };
  const res = {
    writeHead(status) { captured.status = status; return this; },
    end(payload) { captured.body = payload ? JSON.parse(payload) : null; },
  };
  const req = mockReq("POST", {});
  await handler(req, res, new URL(`http://x/api/canvas/projects/${project.id}/alpha-dual-generate`));
  assert.equal(captured.status, 400);
  assert.match(captured.body.error, /requires elementId/);

  // CLI: alpha-dual-generate <id> with no --element -> the same guard, via fail().
  assert.throws(() => execFileSync("node", [CLI, "alpha-dual-generate", project.id], {
    env: { ...process.env, CANVAS_PROJECTS_ROOT: dir },
    encoding: "utf8",
  }));
});

// ---- addImageFromFile ("Add to canvas" op, T0238) ------------------------------

test("addImageFromFile mints a normal journaled element from an EXISTING file src (no re-upload); undo removes it", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Add from file" });
  const { element: original } = addImage(UNUSED_ROOT, project.id, { name: "plate.png", bytes: magentaSheetPng() });
  const seqBefore = getProject(UNUSED_ROOT, project.id).history_seq;

  const result = addImageFromFile(UNUSED_ROOT, project.id, { src: original.src, name: "plate copy", x: 100, y: 40 });

  // ONE new journal entry (it goes through the SAME addImage op).
  const after = getProject(UNUSED_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1);
  assert.equal(after.elements.length, 2);
  assert.notEqual(result.element.id, original.id, "a NEW element, not a src-swap of the original");
  assert.equal(result.element.src, original.src, "SAME content-addressed file — no duplicate bytes");
  assert.equal(result.element.name, "plate copy");
  assert.equal(result.element.x, 100);
  assert.equal(result.element.y, 40);

  // Undo removes only the minted element.
  const undone = undoOp(UNUSED_ROOT, { projectId: project.id }).project;
  assert.equal(undone.elements.length, 1);
  assert.equal(undone.elements[0].id, original.id);
});

test("addImageFromFile is loud on a missing file, and on missing projectId/src", async (t) => {
  tempProjects(t);
  const project = createProject(UNUSED_ROOT, { title: "Add from file missing" });
  await assert.rejects(async () => addImageFromFile(UNUSED_ROOT, project.id, { src: "files/does-not-exist.png" }), /file not found/);
  assert.throws(() => addImageFromFile(UNUSED_ROOT, undefined, { src: "files/x.png" }), /requires projectId/);
  assert.throws(() => addImageFromFile(UNUSED_ROOT, project.id, {}), /requires src/);
});

// ---- images-from-file CLI + API parity (no Python) -----------------------------

test("images-from-file: API route and CLI drive the same op (full round trip, no python needed)", async (t) => {
  const dir = tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Add from file parity" });
  const { element: original } = addImage(REPO_ROOT, project.id, { name: "plate.png", bytes: magentaSheetPng() });

  // API: POST /images-from-file {src, name?, x?, y?}.
  const handler = createCanvasApi(REPO_ROOT);
  const captured = { status: 0, body: null };
  const res = {
    writeHead(status) { captured.status = status; return this; },
    end(payload) { captured.body = payload ? JSON.parse(payload) : null; },
  };
  const req = mockReq("POST", { src: original.src, name: "via api" });
  await handler(req, res, new URL(`http://x/api/canvas/projects/${project.id}/images-from-file`));
  assert.equal(captured.status, 201, `API images-from-file 201 (got ${captured.status}: ${JSON.stringify(captured.body)})`);
  assert.equal(captured.body.element.src, original.src);
  assert.equal(captured.body.element.name, "via api");

  // CLI: add-image-from-file <id> --src <src> --name "via cli".
  const out = execFileSync("node", [CLI, "add-image-from-file", project.id, "--src", original.src, "--name", "via cli"], {
    env: { ...process.env, CANVAS_PROJECTS_ROOT: dir },
    encoding: "utf8",
  });
  const parsed = JSON.parse(out.trim().split("\n").pop());
  assert.equal(parsed.element.src, original.src);
  assert.equal(parsed.element.name, "via cli");
  assert.notEqual(parsed.element.id, captured.body.element.id, "CLI run mints its own new element");
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
