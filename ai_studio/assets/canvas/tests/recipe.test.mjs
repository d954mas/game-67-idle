// Recipe card ops tests. Increment 1: the card object + `recipe` meta + inspector surface
// (no generation). Increment 2 (T0239): generateFromRecipe end-to-end — codex/gemini/both
// engines behind the tools/recipe_generate.mjs seam. Codex/agy NEVER spawn in this suite:
// every generation test injects a fake `generators` map (the T0238 contract, extended to two
// engines); only the pure argv/instruction builders exercise the DEFAULT generators' shape,
// never spawned. Metadata-only ops (no Python for validation/refs-resolution), so the
// placeholder ROOT works for every direct-ops test, including ones that add real ref image
// bytes (addImage's imageSize parsing is pure JS). CLI parity uses cli.mjs's production
// dispatcher in-process for domain results, while local fail()/exit guards remain real child
// processes; API parity uses api.mjs directly. All stay VALIDATION-level only (T0238's
// dual_generate.test.mjs precedent) — a real generate call through the CLI/API always spawns
// the DEFAULT generators, which this suite never triggers.
// Run: node --test ai_studio/assets/canvas/tests/recipe.test.mjs
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
  createRecipeCard,
  createStyleCard,
  duplicateNodes,
  expandRecipePrompt,
  exportProject,
  extractFromElement,
  generateFromRecipe,
  getProject,
  historyEntryLabel,
  patchElement,
  patchGroup,
  patchRecipe,
  patchStyle,
  pasteNodes,
  promoteExtractedRecipe,
  promoteExtractedStyle,
  redoOp,
  undoOp,
} from "../ops.mjs";
import { buildNodesSpec } from "../tree.mjs";
import { createCanvasApi } from "../api.mjs";
import { main as runCanvasCli } from "../cli.mjs";
import { resolveProjectFile } from "../store.mjs";
import { buildAgyCommand, buildAgyInstruction, buildGenerateCommand, GENERATE_IMAGE_SCRIPT, verifyAgyRefProof } from "../tools/recipe_generate.mjs";
import { solidPng } from "./png_fixture.mjs";

// Metadata ops resolve store paths only, so any placeholder root works (no Python).
const ROOT = "C:/unused-repo-root";
// exportProject spawns render_group.py's Python compositor + reads the repo-relative
// fonts manifest, so that one test drives a handler bound to the REAL repo root (store
// paths stay redirected by CANVAS_PROJECTS_ROOT) — mirrors tests/export.test.mjs.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-recipe-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

function runInProcess(...args) {
  return runCanvasCli(args, { repoRoot: REPO_ROOT, print: (value) => value });
}

// Minimal req/res doubles (mirrors tests/api.test.mjs's own invokeApi).
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
        headers: this.headers,
        buffer,
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

const DEFAULT_RECIPE = {
  v: 1,
  prompt: "",
  expanded: null,
  use_expanded: true,
  engine: "codex",
  params: {
    size: "1024x1024",
    quality: "high",
    model: "gpt-image-2",
    bg_key: "#ff00ff",
    n_candidates: 1,
  },
  style_ref: null,
  pack: null, // T0332 v2: pack mode is off by default — see pack.test.mjs
  last_run: null,
};

test("createRecipeCard mints a group with a default recipe blob; one entry; undo removes it", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Recipe" });
  const before = getProject(ROOT, project.id);

  const { group } = createRecipeCard(ROOT, { projectId: project.id, name: "My card" });
  assert.equal(group.name, "My card");
  assert.deepEqual(group.recipe, DEFAULT_RECIPE);
  assert.equal(group.visible, true);
  assert.equal(group.parentId, undefined, "no parentId given -> top level");

  const stored = getProject(ROOT, project.id);
  assert.equal(stored.groups.length, 1);
  assert.deepEqual(stored.groups[0].recipe, DEFAULT_RECIPE);

  // One journal entry; a byte-exact undo removes the card and restores the project.
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal((undone.groups || []).length, 0);
  assert.equal(JSON.stringify(undone.elements), JSON.stringify(before.elements));
  assert.equal(JSON.stringify(undone.groups || []), JSON.stringify(before.groups || []));

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.deepEqual(redone.groups[0].recipe, DEFAULT_RECIPE);
});

test("createRecipeCard: explicit bounds/parentId honored; default size falls back; unknown parent is loud", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Bounds" });

  // No x/y/w/h -> default frame at the origin (a workshop widget, not a screen size).
  const { group: defaultCard } = createRecipeCard(ROOT, { projectId: project.id });
  assert.deepEqual({ x: defaultCard.x, y: defaultCard.y, w: defaultCard.w, h: defaultCard.h }, { x: 0, y: 0, w: 360, h: 280 });

  // Explicit bounds win.
  const { group: placed } = createRecipeCard(ROOT, { projectId: project.id, x: 50, y: 60, w: 400, h: 300 });
  assert.deepEqual({ x: placed.x, y: placed.y, w: placed.w, h: placed.h }, { x: 50, y: 60, w: 400, h: 300 });

  // parentId nests the card like any group.
  const outer = createGroup(ROOT, { projectId: project.id, name: "Outer", x: 0, y: 0, w: 1000, h: 1000 }).group;
  const { group: nested } = createRecipeCard(ROOT, { projectId: project.id, parentId: outer.id });
  assert.equal(nested.parentId, outer.id);

  assert.throws(
    () => createRecipeCard(ROOT, { projectId: project.id, parentId: "grp_missing" }),
    /group not found/,
  );
});

test("patchRecipe: prompt/engine/style_ref roundtrip; bad engine is loud; a plain group is loud", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Patch" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });

  const afterPrompt = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a red fox" } }).group;
  assert.equal(afterPrompt.recipe.prompt, "a red fox");
  // Untouched fields survive a partial patch.
  assert.equal(afterPrompt.recipe.engine, "codex");
  assert.deepEqual(afterPrompt.recipe.params, DEFAULT_RECIPE.params);

  for (const engine of ["codex", "gemini", "both"]) {
    const after = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { engine } }).group;
    assert.equal(after.recipe.engine, engine);
  }

  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { engine: "dalle" } }),
    /engine must be one of/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: 5 } }),
    /prompt must be a string/,
  );

  // style_ref: null (default) -> an existing style-card group id -> back to null. (T0239
  // increment 3 tightened this from a free-form pointer to a validated one; see
  // style.test.mjs for the "must resolve to a style card" coverage.)
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "A style" }).group;
  const withStyle = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: styleCard.id } }).group;
  assert.equal(withStyle.recipe.style_ref, styleCard.id);
  const clearedStyle = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: null } }).group;
  assert.equal(clearedStyle.recipe.style_ref, null);
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: 5 } }),
    /style_ref must be null or a string id/,
  );

  // An empty patch is loud (nothing to do).
  assert.throws(() => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: {} }), /requires at least one of/);

  // A PLAIN group (no recipe) is a loud error — it is not a card.
  const plain = createGroup(ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: plain.id, patch: { prompt: "x" } }),
    /not a recipe card/,
  );

  // An unknown group id is loud too.
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: "grp_missing", patch: { prompt: "x" } }),
    /group not found/,
  );
});

test("patchRecipe is one journal entry per call; undo restores the prior recipe blob byte-exact", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Undo" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });
  const afterCreate = getProject(ROOT, project.id);

  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a dragon", engine: "both" } });
  const afterPatch = getProject(ROOT, project.id);
  assert.equal(afterPatch.groups[0].recipe.prompt, "a dragon");
  assert.equal(afterPatch.groups[0].recipe.engine, "both");

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(JSON.stringify(undone.groups), JSON.stringify(afterCreate.groups), "undo restores the recipe blob byte-exact");

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.groups[0].recipe.prompt, "a dragon");
});

test("copy/paste and duplicate carry the recipe blob through with a fresh group id", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Copy" });
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "A style" }).group;
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id, name: "Hero card", x: 10, y: 20 });
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a knight", engine: "gemini", style_ref: styleCard.id } });
  const seeded = getProject(ROOT, project.id).groups.find((g) => g.id === card.id);

  // buildNodesSpec (tree.mjs) is a deep clone of the group record minus id/parentId/order,
  // so `recipe` survives with no schema-aware carve-out needed.
  const spec = buildNodesSpec(getProject(ROOT, project.id), [card.id]);
  assert.equal(spec.nodes.length, 1);
  assert.deepEqual(spec.nodes[0].group.recipe, seeded.recipe);

  const pasted = pasteNodes(ROOT, { projectId: project.id, spec, dx: 100, dy: 0 });
  const pastedGroup = pasted.project.groups.find((g) => g.id === pasted.groupIds[0]);
  assert.notEqual(pastedGroup.id, card.id, "paste mints a fresh id");
  assert.deepEqual(pastedGroup.recipe, seeded.recipe, "recipe blob carried verbatim");

  // duplicateNodes (live nodes, +offset) goes through the same buildNodesSpec/pasteNodes path.
  const duped = duplicateNodes(ROOT, { projectId: project.id, nodeIds: [card.id] });
  const dupedGroup = duped.project.groups.find((g) => g.id === duped.groupIds[0]);
  assert.notEqual(dupedGroup.id, card.id);
  assert.deepEqual(dupedGroup.recipe, seeded.recipe);
});

test("exportProject never exports a top-level recipe-card group (a card is a workshop object, not a screen) — T0332 B1: the card simply never carries screen:true, no special-case skip needed", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Export" });
  const screen = createGroup(REPO_ROOT, { projectId: project.id, name: "Screen", x: 0, y: 0, w: 20, h: 20 }).group;
  patchGroup(REPO_ROOT, { projectId: project.id, groupId: screen.id, screen: true });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Card" });
  assert.equal(card.screen, undefined, "createRecipeCard never sets screen — unflagged by construction");
  // Flagging the card's group as screen:true anyway (a caller bug or a hand-edit) is NOT
  // blocked by exportProject — the flag is the only gate, no recipe-aware special-casing —
  // but that is a deliberate lead choice to leave alone, not this test's concern.

  let result;
  try {
    result = await exportProject(REPO_ROOT, { projectId: project.id });
  } catch (error) {
    t.skip(`render_group.py / python pipeline unavailable: ${error.message}`);
    return;
  }
  assert.equal(result.screens.length, 1, "only the plain group is exported as a screen");
  assert.equal(result.screens[0].name, "Screen");
});

test("historyEntryLabel maps createRecipeCard/patchRecipe to readable labels", () => {
  assert.deepEqual(historyEntryLabel("createRecipeCard", { name: "Hero" }), { label: "Recipe card", summary: "Hero" });
  assert.deepEqual(historyEntryLabel("patchRecipe", {}), { label: "Edit recipe", summary: "" });
});

test("CLI recipe-create / recipe-set parity with the ops layer", async (t) => {
  tempProjects(t);

  const projectId = (await runInProcess("create", "--title", "CLI Recipe")).project.id;
  const created = await runInProcess("recipe-create", projectId, "--name", "CLI card", "--x", "5", "--y", "6", "--w", "300", "--h", "200");
  assert.equal(created.group.name, "CLI card");
  assert.deepEqual(created.group.recipe, DEFAULT_RECIPE);
  assert.deepEqual({ x: created.group.x, y: created.group.y, w: created.group.w, h: created.group.h }, { x: 5, y: 6, w: 300, h: 200 });

  const groupId = created.group.id;
  const patched = await runInProcess("recipe-set", projectId, "--group", groupId, "--prompt", "a red fox", "--engine", "both");
  assert.equal(patched.group.recipe.prompt, "a red fox");
  assert.equal(patched.group.recipe.engine, "both");

  // T0239 increment 3: --style must now resolve to a REAL style-card group id (a style-create
  // smoke here; dedicated style-create/style-set CLI parity lives in style.test.mjs).
  const styleCard = await runInProcess("style-create", projectId, "--name", "CLI style");
  assert.equal(styleCard.group.name, "CLI style");
  const withStyle = await runInProcess("recipe-set", projectId, "--group", groupId, "--style", styleCard.group.id);
  assert.equal(withStyle.group.recipe.style_ref, styleCard.group.id);
  const clearedStyle = await runInProcess("recipe-set", projectId, "--group", groupId, "--style", "none");
  assert.equal(clearedStyle.group.recipe.style_ref, null);

  await assert.rejects(
    runInProcess("recipe-set", projectId, "--group", groupId, "--engine", "dalle"),
    /engine must be one of/,
  );

  const shown = (await runInProcess("show", projectId)).project;
  assert.equal(shown.groups.length, 2);
  assert.equal(shown.groups.find((g) => g.id === groupId).recipe.prompt, "a red fox");
});

test("API POST recipe-cards / PATCH recipe-cards/<gid> parity with the ops layer", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);

  const createdProject = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "API Recipe" });
  const projectId = createdProject.json().project.id;

  const created = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards`, {
    name: "API card",
    x: 1,
    y: 2,
  });
  assert.equal(created.status, 201);
  const group = created.json().group;
  assert.equal(group.name, "API card");
  assert.deepEqual(group.recipe, DEFAULT_RECIPE);

  const patched = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/recipe-cards/${group.id}`, {
    prompt: "a castle",
    engine: "gemini",
  });
  assert.equal(patched.status, 200);
  assert.equal(patched.json().group.recipe.prompt, "a castle");
  assert.equal(patched.json().group.recipe.engine, "gemini");
  // sendMutation folds duration_ms + history flags onto every mutating response.
  assert.ok(Number.isFinite(patched.json().duration_ms));
  assert.ok(patched.json().history);

  // A plain group (no recipe) 400s through the API too — the op's loud error surfaces
  // as a 400, not a silent 200.
  const plainGroup = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "Plain",
    x: 0,
    y: 0,
    w: 10,
    h: 10,
  });
  const plainId = plainGroup.json().group.id;
  const rejected = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/recipe-cards/${plainId}`, { prompt: "x" });
  assert.equal(rejected.status, 400);
  assert.match(rejected.json().error, /not a recipe card/);
});

// ================================================================================
// T0239 increment 2: generateFromRecipe — end-to-end generation, engine seam, placement.
// ================================================================================

// A fake generator that returns fixed bytes (or throws, if given an Error), counting every
// call for exact argument/order assertions — the SAME shape tests/dual_generate.test.mjs's
// fakeGenerator uses, adapted to the recipe seam's {prompt, refPaths, params} args.
function fakeGen(bytesOrError) {
  const calls = [];
  const fn = async (args) => {
    calls.push(args);
    if (bytesOrError instanceof Error) throw bytesOrError;
    return bytesOrError;
  };
  fn.calls = calls;
  return fn;
}

// ---- pure builders (no spawn) --------------------------------------------------

test("buildGenerateCommand: prompt, one --input-image per ref, size/quality/model, out — never spawns", () => {
  const { command, args } = buildGenerateCommand({
    prompt: "a red fox",
    refPaths: ["C:/tmp/ref1.png", "C:/tmp/ref2.png"],
    size: "1536x1024",
    quality: "medium",
    model: "gpt-image-2",
    outPath: "C:/tmp/out.png",
    pythonPath: "test-python",
  });
  assert.equal(command, "test-python");
  assert.equal(args[0], GENERATE_IMAGE_SCRIPT);
  assert.match(args[0], /generate_image\.py$/);
  assert.deepEqual(args.slice(1), [
    "--prompt", "a red fox",
    "--input-image", "C:/tmp/ref1.png",
    "--input-image", "C:/tmp/ref2.png",
    "--size", "1536x1024",
    "--quality", "medium",
    "--model", "gpt-image-2",
    "--out", "C:/tmp/out.png",
  ]);
});

test("buildGenerateCommand: no refs -> no --input-image flags; defaults fill size/quality/model", () => {
  const { args } = buildGenerateCommand({ prompt: "p", outPath: "o.png", pythonPath: "test-python" });
  assert.deepEqual(args.slice(1), ["--prompt", "p", "--size", "1024x1024", "--quality", "high", "--model", "gpt-image-2", "--out", "o.png"]);
});

test("buildGenerateCommand requires prompt/outPath", () => {
  assert.throws(() => buildGenerateCommand({ outPath: "o" }), /requires prompt/);
  assert.throws(() => buildGenerateCommand({ prompt: "p" }), /requires outPath/);
});

test("buildAgyInstruction: square/non-square aspect label, prompt + out path embedded, no-drawing-code clause", () => {
  const square = buildAgyInstruction({ prompt: "a red fox", size: "1024x1024", outPath: "C:/tmp/a.png" });
  assert.match(square, /square 1:1/);
  assert.match(square, /a red fox/);
  assert.match(square, /Save the PNG to C:\/tmp\/a\.png/);
  assert.match(square, /Do not write or run any drawing code/);

  const wide = buildAgyInstruction({ prompt: "a red fox", size: "1536x1024", outPath: "C:/tmp/b.png" });
  assert.match(wide, /1536:1024 aspect ratio/);

  // An unparseable size falls back to the common square default rather than a malformed
  // instruction.
  const fallback = buildAgyInstruction({ prompt: "a red fox", size: "not-a-size", outPath: "C:/tmp/c.png" });
  assert.match(fallback, /square 1:1/);
});

test("buildAgyInstruction requires prompt/outPath", () => {
  assert.throws(() => buildAgyInstruction({ outPath: "o" }), /requires prompt/);
  assert.throws(() => buildAgyInstruction({ prompt: "p" }), /requires outPath/);
});

// T0251: refs-empty stays BYTE-IDENTICAL to the pre-T0251 gen_both.sh-verbatim template — no
// ref clause perturbs the no-refs path, whether refPaths is omitted or explicitly [].
test("buildAgyInstruction: no refs (omitted or []) is byte-identical to the verbatim gen_both.sh template", () => {
  const omitted = buildAgyInstruction({ prompt: "a red fox", size: "1024x1024", outPath: "C:/tmp/a.png" });
  const explicitEmpty = buildAgyInstruction({ prompt: "a red fox", size: "1024x1024", outPath: "C:/tmp/a.png", refPaths: [] });
  const expected =
    "Use your built-in image generation to create one real raster image (not code-drawn), square 1:1: a red fox. " +
    "Save the PNG to C:/tmp/a.png . Do not write or run any drawing code.";
  assert.equal(omitted, expected);
  assert.equal(explicitEmpty, expected);
});

// T0251: refs present prepends the "open and
// view + write one sentence to <outPath>.seen.txt" proof clause, names every ref path, and
// still carries the generation clause (match subject/style/palette).
test("buildAgyInstruction: refs present prepend the open-and-view + .seen.txt proof clause, list every ref path", () => {
  const withRefs = buildAgyInstruction({
    prompt: "a dragon",
    size: "1024x1024",
    outPath: "C:/tmp/out.png",
    refPaths: ["C:/refs/ref1.png", "C:/refs/ref2.png"],
  });
  assert.match(withRefs, /2 reference image/);
  assert.match(withRefs, /FIRST open and view each with your tools/);
  assert.match(withRefs, /C:\/refs\/ref1\.png/);
  assert.match(withRefs, /C:\/refs\/ref2\.png/);
  assert.match(withRefs, /write ONE sentence describing what you saw to C:\/tmp\/out\.png\.seen\.txt/);
  assert.match(withRefs, /Match the subject\/style\/palette of the reference image\(s\)/);
  assert.match(withRefs, /a dragon/);
  assert.match(withRefs, /Save the PNG to C:\/tmp\/out\.png/);
});

test("buildAgyCommand: shells the agy binary with --dangerously-skip-permissions -p <instruction>", () => {
  const { command, args } = buildAgyCommand({ prompt: "a red fox", size: "1024x1024", outPath: "C:/tmp/a.png" });
  assert.match(command, /agy(\.exe)?$/);
  assert.deepEqual(args.slice(0, 2), ["--dangerously-skip-permissions", "-p"]);
  assert.match(args[2], /a red fox/);
});

// T0251: one --add-dir per DISTINCT ref parent dir, BEFORE --dangerously-skip-permissions;
// two refs sharing a dir collapse to one --add-dir (dedupe).
test("buildAgyCommand: one --add-dir per unique ref dir, deduped, before --dangerously-skip-permissions", () => {
  const { args } = buildAgyCommand({
    prompt: "a dragon",
    size: "1024x1024",
    outPath: "C:/tmp/out.png",
    refPaths: ["C:/refs/a/ref1.png", "C:/refs/a/ref2.png", "C:/refs/b/ref3.png"],
  });
  assert.deepEqual(args.slice(0, 4), ["--add-dir", "C:/refs/a", "--add-dir", "C:/refs/b"], "deduped, one per distinct dir");
  assert.deepEqual(args.slice(4, 6), ["--dangerously-skip-permissions", "-p"]);
});

test("buildAgyCommand: no refs -> no --add-dir args at all", () => {
  const { args } = buildAgyCommand({ prompt: "a red fox", size: "1024x1024", outPath: "C:/tmp/a.png" });
  assert.deepEqual(args.slice(0, 2), ["--dangerously-skip-permissions", "-p"]);
  assert.ok(!args.includes("--add-dir"));
});

// ---- verifyAgyRefProof (pure I/O, no spawn): the silent-divergence guard on its own --------

test("verifyAgyRefProof: no refs -> always passes, .seen.txt not even checked", () => {
  assert.doesNotThrow(() => verifyAgyRefProof("C:/nonexistent/out.png", []));
  assert.doesNotThrow(() => verifyAgyRefProof("C:/nonexistent/out.png", undefined));
});

test("verifyAgyRefProof: refs present but .seen.txt missing or empty -> throws loud (silent-divergence guard)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-recipe-seenproof-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const outPath = join(dir, "out.png");

  // .seen.txt never written.
  assert.throws(() => verifyAgyRefProof(outPath, ["C:/refs/ref1.png"]), /no non-empty .*\.seen\.txt/);

  // .seen.txt written but blank/whitespace-only.
  writeFileSync(`${outPath}.seen.txt`, "   \n", "utf8");
  assert.throws(() => verifyAgyRefProof(outPath, ["C:/refs/ref1.png"]), /no non-empty .*\.seen\.txt/);
});

test("verifyAgyRefProof: refs present and .seen.txt non-empty -> passes", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-recipe-seenproof-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const outPath = join(dir, "out.png");
  writeFileSync(`${outPath}.seen.txt`, "A pair of gold and white angel wings.", "utf8");

  assert.doesNotThrow(() => verifyAgyRefProof(outPath, ["C:/refs/ref1.png"]));
});

// ---- generateFromRecipe validation (no generation attempted) -------------------

test("generateFromRecipe validates projectId/groupId before touching disk", async () => {
  await assert.rejects(() => generateFromRecipe(ROOT, {}), /requires projectId/);
  await assert.rejects(() => generateFromRecipe(ROOT, { projectId: "p" }), /requires groupId/);
});

test("generateFromRecipe: an unknown group, a plain (non-card) group, and an empty prompt are all loud — no generator call", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate validate" });
  const plain = createGroup(ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Blank card" }).group;
  const codex = fakeGen(solidPng());

  await assert.rejects(
    () => generateFromRecipe(ROOT, { projectId: project.id, groupId: "grp_missing", generators: { codex } }),
    /group not found/,
  );
  await assert.rejects(
    () => generateFromRecipe(ROOT, { projectId: project.id, groupId: plain.id, generators: { codex } }),
    /not a recipe card/,
  );
  await assert.rejects(
    () => generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } }),
    /has an empty prompt/,
  );
  assert.equal(codex.calls.length, 0, "the generator is never called before validation passes");
});

test("generateFromRecipe: more than 5 member-image refs is a loud error before any generation", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate refs cap" });
  const card = createRecipeCard(ROOT, { projectId: project.id }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a red fox" } });
  const imageIds = [];
  for (let i = 0; i < 6; i += 1) {
    imageIds.push(addImage(ROOT, project.id, { name: `ref${i}.png`, bytes: solidPng() }).element.id);
  }
  assignToGroup(ROOT, { projectId: project.id, elementIds: imageIds, groupId: card.id });
  const codex = fakeGen(solidPng());

  await assert.rejects(
    () => generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } }),
    /at most 5/,
  );
  assert.equal(codex.calls.length, 0);
});

// ---- generateFromRecipe happy paths (fake generators; codex/agy never spawn) ---

test("generateFromRecipe (codex, top-level card): ONE new element in the ROOT scope beside the frame, meta.recipe frozen snapshot, tool_runs row, card.last_run set, ONE journal entry; undo reverts everything", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate happy" });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Hero card", x: 100, y: 50, w: 300, h: 200 }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "  a red fox riding a dragon  " } });
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const codex = fakeGen(solidPng(10, 8, [200, 40, 40]));
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } });

  assert.equal(codex.calls.length, 1);
  assert.equal(codex.calls[0].prompt, "a red fox riding a dragon", "prompt is trimmed before it reaches the generator");
  assert.deepEqual(codex.calls[0].refPaths, [], "prompt-only card: no refs");

  assert.equal(result.elements.length, 1);
  const el = result.elements[0];
  assert.equal(el.name, "Hero card codex");
  assert.equal(el.x, card.x + card.w + 16, "placed to the RIGHT of the card frame, 16px gap");
  assert.equal(el.y, card.y);
  assert.equal(el.groupId, undefined, "top-level card -> result lands in the ROOT scope, not inside the card");
  assert.equal(result.failed.length, 0);

  // meta.recipe: frozen per-run snapshot; meta.alpha absent (raw, no alpha — decision 5).
  // params_snapshot is engine-FILTERED (snapshotParamsForEngine): only what codex actually
  // consumed (size/quality/model) + the canvas-level bg_key — never n_candidates (pack-only).
  assert.deepEqual(el.meta.recipe, {
    cardId: card.id,
    engine: "codex",
    at: result.run.at,
    prompt_snapshot: "a red fox riding a dragon",
    refs_snapshot: [],
    params_snapshot: {
      size: DEFAULT_RECIPE.params.size,
      quality: DEFAULT_RECIPE.params.quality,
      model: DEFAULT_RECIPE.params.model,
      bg_key: DEFAULT_RECIPE.params.bg_key,
    },
  });
  assert.equal(el.meta.alpha, undefined);

  // ONE journal entry for the whole gesture (generation itself is outside it).
  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1);

  // tool_runs row.
  const run = after.tool_runs.at(-1);
  assert.equal(run.op, "generate_from_recipe");
  assert.equal(run.cardId, card.id);
  assert.equal(run.result_summary.results.length, 1);
  assert.equal(run.result_summary.results[0].elementId, el.id);
  assert.equal(run.result_summary.failed.length, 0);

  // card.recipe.last_run set; the card itself otherwise unchanged.
  const storedCard = after.groups.find((g) => g.id === card.id);
  assert.deepEqual(storedCard.recipe.last_run, { at: result.run.at, result_element_id: el.id, verdict: "ok" });

  // ONE undo removes the new element AND reverts recipe.last_run to null (byte-exact,
  // free via the group snapshot).
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.equal(undone.elements.length, 0);
  assert.equal(undone.groups.find((g) => g.id === card.id).recipe.last_run, null);

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((item) => item.id === el.id).name, "Hero card codex");
});

test("generateFromRecipe resolves refs from VISIBLE member IMAGE elements only, and passes their abs paths to the generator", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate refs" });
  const card = createRecipeCard(ROOT, { projectId: project.id }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a knight" } });

  const ref1 = addImage(ROOT, project.id, { name: "ref1.png", bytes: solidPng(4, 3, [1, 2, 3]) }).element;
  const ref2 = addImage(ROOT, project.id, { name: "ref2.png", bytes: solidPng(4, 3, [4, 5, 6]) }).element;
  const hidden = addImage(ROOT, project.id, { name: "hidden.png", bytes: solidPng(4, 3, [7, 8, 9]) }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [ref1.id, ref2.id, hidden.id], groupId: card.id });
  patchElement(ROOT, project.id, hidden.id, { visible: false });

  const codex = fakeGen(solidPng());
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } });

  assert.equal(codex.calls[0].refPaths.length, 2, "the hidden member is excluded");
  // Assert against the SAME resolveProjectFile the op itself calls, not a hand-rolled path,
  // so this test can't silently drift from the real path-join rules.
  assert.deepEqual(codex.calls[0].refPaths, [
    resolveProjectFile(ROOT, project.id, ref1.src),
    resolveProjectFile(ROOT, project.id, ref2.src),
  ]);
  assert.deepEqual(result.elements[0].meta.recipe.refs_snapshot, [ref1.src, ref2.src]);
});

test("generateFromRecipe (engine=both, nested card): mints TWO elements in the PARENT scope, second stacked BELOW the first, ONE journal entry", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate both nested" });
  const outer = createGroup(ROOT, { projectId: project.id, name: "Outer", x: 0, y: 0, w: 1000, h: 1000 }).group;
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Dragon", parentId: outer.id, x: 100, y: 50, w: 300, h: 200 }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a dragon", engine: "both" } });
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const codex = fakeGen(solidPng(10, 8, [200, 40, 40]));
  const gemini = fakeGen(solidPng(6, 5, [40, 200, 40]));
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex, gemini } });

  assert.equal(result.elements.length, 2);
  assert.equal(result.failed.length, 0);
  const [first, second] = result.elements;
  assert.equal(first.name, "Dragon codex");
  assert.equal(second.name, "Dragon agy");
  assert.equal(first.groupId, outer.id, "PARENT scope, never the card itself (decision 8)");
  assert.equal(second.groupId, outer.id);
  assert.equal(first.x, card.x + card.w + 16);
  assert.equal(first.y, card.y);
  assert.equal(second.x, first.x, "second result stacks BELOW the first, same x");
  assert.equal(second.y, first.y + first.h + 16);
  assert.equal(first.meta.recipe.engine, "codex");
  assert.equal(second.meta.recipe.engine, "gemini");
  // Per-ELEMENT engine-filtered snapshots even inside ONE both-run (review gap 2026-07-07):
  // the codex element records what codex consumed; its agy sibling records only size+bg_key —
  // never the gpt-image model/quality it had no knob for.
  assert.deepEqual(first.meta.recipe.params_snapshot, {
    size: DEFAULT_RECIPE.params.size,
    quality: DEFAULT_RECIPE.params.quality,
    model: DEFAULT_RECIPE.params.model,
    bg_key: DEFAULT_RECIPE.params.bg_key,
  });
  assert.deepEqual(second.meta.recipe.params_snapshot, {
    size: DEFAULT_RECIPE.params.size,
    bg_key: DEFAULT_RECIPE.params.bg_key,
  });

  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "one journal entry for the pair");
  const run = after.tool_runs.at(-1);
  assert.equal(run.result_summary.results.length, 2);

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.equal((undone.elements || []).length, 0, "one undo removes BOTH minted elements");
});

test("generateFromRecipe (engine=both, partial success): one engine failing still lands the other; failed[] names it; op does not throw", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate both partial" });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Castle" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a castle", engine: "both" } });

  const codex = fakeGen(solidPng());
  const gemini = fakeGen(new Error("agy timed out"));
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex, gemini } });

  assert.equal(result.elements.length, 1);
  assert.equal(result.elements[0].name, "Castle codex");
  assert.equal(result.failed.length, 1);
  assert.equal(result.failed[0].engine, "gemini");
  assert.match(result.failed[0].error, /agy timed out/);

  const stored = getProject(ROOT, project.id);
  assert.equal(stored.groups.find((g) => g.id === card.id).recipe.last_run.verdict, "partial");
});

test("legacy params.supersample (pre-2026-07-07 blob): survives an unrelated params patch inert AND never leaks into a run's params_snapshot", async (t) => {
  const projectsDir = tempProjects(t);
  const project = createProject(ROOT, { title: "Legacy supersample" });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Old card" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "an old thing" } });

  // Seed the legacy state directly in the store — no op can produce it anymore (supersample
  // left the defaults 2026-07-07; patching it is a loud unknown-key error, pinned elsewhere).
  const storeFile = join(projectsDir, project.id, "project.json");
  const raw = JSON.parse(readFileSync(storeFile, "utf8"));
  raw.groups.find((g) => g.id === card.id).recipe.params.supersample = true;
  writeFileSync(storeFile, `${JSON.stringify(raw, null, 2)}\n`, "utf8");

  // An unrelated params patch merges ONTO the stored params — the legacy key survives, inert.
  const patched = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { params: { bg_key: "#00ff00" } } }).group;
  assert.equal(patched.recipe.params.supersample, true, "legacy key survives a shallow params merge");
  assert.equal(patched.recipe.params.bg_key, "#00ff00");

  // A run's snapshot is allow-listed per engine — the legacy key can never reach provenance.
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex: fakeGen(solidPng()) } });
  assert.equal(result.elements[0].meta.recipe.params_snapshot.supersample, undefined, "allow-listed snapshot: no leak");
  assert.equal(result.elements[0].meta.recipe.params_snapshot.bg_key, "#00ff00");
});

// T0251: agy ref support is now VERIFIED — engine="both" with refs runs BOTH engines (no more
// skip-gemini-when-refs), same partial-success semantics as any other both/two-engine run.
test("generateFromRecipe (engine=both, refs present): BOTH engines are called with the SAME refPaths; two elements land", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate both refs" });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Griffin" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a griffin", engine: "both" } });
  const ref = addImage(ROOT, project.id, { name: "ref.png", bytes: solidPng() }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [ref.id], groupId: card.id });

  const codex = fakeGen(solidPng());
  const gemini = fakeGen(solidPng());
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex, gemini } });

  assert.equal(codex.calls.length, 1, "gemini having refs no longer skips codex's own attempt");
  assert.equal(gemini.calls.length, 1, "gemini IS attempted now that agy ref support is verified (T0251)");
  assert.equal(codex.calls[0].refPaths.length, 1);
  assert.deepEqual(gemini.calls[0].refPaths, codex.calls[0].refPaths, "both engines receive the SAME refPaths");
  assert.equal(result.elements.length, 2);
  assert.equal(result.failed.length, 0);
  assert.equal(result.elements[0].name, "Griffin codex");
  assert.equal(result.elements[1].name, "Griffin agy");
});

// T0251: engine="gemini" with refs now PROCEEDS (no loud refusal); the fake gemini generator
// receives the resolved refPaths through the SAME seam shape codex has always used.
test("generateFromRecipe (engine=gemini, refs present): proceeds — the gemini generator RECEIVES refPaths, no refusal", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate gemini refs" });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Phoenix" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a phoenix", engine: "gemini" } });
  const ref = addImage(ROOT, project.id, { name: "ref.png", bytes: solidPng() }).element;
  assignToGroup(ROOT, { projectId: project.id, elementIds: [ref.id], groupId: card.id });

  const gemini = fakeGen(solidPng());
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { gemini } });

  assert.equal(gemini.calls.length, 1);
  assert.equal(gemini.calls[0].refPaths.length, 1, "the gemini generator receives the resolved ref abs path");
  assert.deepEqual(gemini.calls[0].refPaths, [resolveProjectFile(ROOT, project.id, ref.src)]);
  assert.equal(result.elements.length, 1);
  assert.equal(result.elements[0].name, "Phoenix agy");
  assert.equal(result.failed.length, 0);
  assert.deepEqual(result.elements[0].meta.recipe.refs_snapshot, [ref.src]);
});

test("generateFromRecipe (engine=gemini, no refs): text-only generation is OK", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate gemini text only" });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Phoenix" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a phoenix", engine: "gemini" } });

  const gemini = fakeGen(solidPng());
  const result = await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { gemini } });

  assert.equal(gemini.calls.length, 1);
  assert.equal(result.elements.length, 1);
  assert.equal(result.elements[0].name, "Phoenix agy");
  assert.equal(result.elements[0].meta.recipe.engine, "gemini");
});

test("generateFromRecipe: every attempted engine failing throws loud; nothing is written (single engine and both)", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Generate all fail" });
  const card = createRecipeCard(ROOT, { projectId: project.id }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a griffin" } });
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const codex = fakeGen(new Error("codex network error"));
  await assert.rejects(
    () => generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } }),
    /codex: codex network error/,
  );
  assert.equal(getProject(ROOT, project.id).history_seq, seqBefore, "no journal entry — single-engine failure stays loud-fail");
  assert.equal(getProject(ROOT, project.id).elements.length, 0);

  // patchRecipe itself IS a journal entry — re-baseline the expected seq off of it before
  // the both-engines-fail assertion below (a bug in the test, not the op, would otherwise
  // be off by one here).
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { engine: "both" } });
  const seqBeforeBoth = getProject(ROOT, project.id).history_seq;
  const codex2 = fakeGen(new Error("codex down"));
  const gemini2 = fakeGen(new Error("agy down"));
  await assert.rejects(
    () => generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex: codex2, gemini: gemini2 } }),
    /codex: codex down; gemini: agy down/,
  );
  assert.equal(getProject(ROOT, project.id).history_seq, seqBeforeBoth, "still no journal entry after the both-engines-fail case");
  assert.equal(getProject(ROOT, project.id).elements.length, 0);
});

test("historyEntryLabel maps generateFromRecipe to a readable label", () => {
  assert.deepEqual(historyEntryLabel("generateFromRecipe", { engine: "codex" }), { label: "Generate from recipe", summary: "codex" });
  assert.deepEqual(historyEntryLabel("generateFromRecipe", {}), { label: "Generate from recipe", summary: "" });
});

// ---- recipe-generate CLI + API validation parity (no python; no real generation) -----

test("recipe-generate: CLI and API reject a non-card group the same way (validation parity, no generation)", async (t) => {
  // tempProjects sets process.env.CANVAS_PROJECTS_ROOT = dir (for the in-process API
  // handler below) AND returns `dir` so the CLI subprocess calls can pass it explicitly.
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  const projectId = (await runInProcess("create", "--title", "Generate CLI parity")).project.id;
  const plain = (await runInProcess("group-create", projectId, "--name", "Plain", "--x", "0", "--y", "0", "--w", "10", "--h", "10")).group;

  // CLI: recipe-generate <id> with no --group -> the CLI's own local guard.
  assert.throws(() => execFileSync("node", [CLI, "recipe-generate", projectId], { env: { ...process.env, ...env }, encoding: "utf8" }));

  // CLI: recipe-generate <id> --group <plain group> -> the OP's own "not a recipe card".
  await assert.rejects(runInProcess("recipe-generate", projectId, "--group", plain.id), /not a recipe card/);

  // API: POST .../recipe-cards/<plain group>/generate -> 400 with the SAME op error.
  const handler = createCanvasApi(ROOT);
  const rejected = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards/${plain.id}/generate`, {});
  assert.equal(rejected.status, 400);
  assert.match(rejected.json().error, /not a recipe card/);

  // API: POST .../recipe-cards/<unknown group>/generate -> 404 "group not found"
  // (T0254 Tier 1 #2: statusForError maps "not found" to 404, not the old catch-all 400).
  const missing = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards/grp_missing/generate`, {});
  assert.equal(missing.status, 404);
  assert.match(missing.json().error, /group not found/);
});

// ================================================================================
// T0239 increment 4: expandRecipePrompt / extractFromElement / promoteExtracted* — the
// codex TEXT/VISION seam (tools/prompt_assist.mjs). Codex NEVER spawns in this suite —
// every direct-ops test injects a fake `assistant` (the T0238 contract, extended to
// text/vision); CLI/API parity tests stay VALIDATION-only (recipe-generate's own
// precedent above) since the CLI has no fake-assistant injection seam.
// ================================================================================

// A fake assistant that returns a fixed value (or throws, if given an Error), counting
// every call — the SAME shape fakeGen uses above, adapted to the prompt-assist seam's
// single-arg calls ({prompt,styleBlock} for expand, {imagePath} for extract).
function fakeAssistant(valueOrError) {
  const calls = [];
  const fn = async (args) => {
    calls.push(args);
    if (valueOrError instanceof Error) throw valueOrError;
    return valueOrError;
  };
  fn.calls = calls;
  return fn;
}

// A well-formed extract result (the assistant.extract contract) with any field overridable.
function extractResult(overrides = {}) {
  return {
    prompt_full: "a red fox on a rock, painterly oil style",
    prompt_subject: "a red fox on a rock",
    style_block: "painterly oil, warm palette",
    palette: ["warm red", "brown"],
    materials: "fur, stone",
    lighting: "soft morning light",
    composition: "centered, medium shot",
    constraints_block: "no text, no watermark",
    description: "A painterly red fox on a rock.",
    ...overrides,
  };
}

// Seed element.meta.extracted through the REAL op (a fake assistant, never codex) rather
// than poking store state directly — exercises extractFromElement itself and keeps the
// promotion tests black-box.
async function seedExtracted(projectId, elementId, overrides = {}) {
  const extract = fakeAssistant(extractResult(overrides));
  return extractFromElement(ROOT, { projectId, elementId, assistant: { extract } });
}

// ---- expandRecipePrompt ---------------------------------------------------------

test("expandRecipePrompt: writes recipe.expanded via a fake assistant, ONE history entry, undo restores the prior blob byte-exact", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Expand" });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Fox card" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "  a red fox  " } });
  const afterPatch = getProject(ROOT, project.id);
  const seqBefore = afterPatch.history_seq;

  const expand = fakeAssistant("[TASK]\nGenerate...\n[SUBJECT]\na red fox\n...");
  const result = await expandRecipePrompt(ROOT, { projectId: project.id, groupId: card.id, assistant: { expand } });

  assert.equal(expand.calls.length, 1);
  assert.equal(expand.calls[0].prompt, "a red fox", "prompt is trimmed before it reaches the assistant");
  assert.equal(expand.calls[0].styleBlock, "", "no style_ref -> empty styleBlock");
  assert.equal(result.expanded, "[TASK]\nGenerate...\n[SUBJECT]\na red fox\n...");
  assert.equal(result.group.recipe.expanded, result.expanded);

  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "one journal entry");

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.equal(JSON.stringify(undone.groups), JSON.stringify(afterPatch.groups), "undo restores the recipe blob byte-exact");

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.groups.find((g) => g.id === card.id).recipe.expanded, result.expanded);
});

test("expandRecipePrompt: style_ref set -> the assistant receives the style card's prompt as styleBlock", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Expand style" });
  const styleCard = createStyleCard(ROOT, { projectId: project.id, name: "Painterly" }).group;
  patchStyle(ROOT, { projectId: project.id, groupId: styleCard.id, patch: { prompt: "  painterly oil, warm gold  " } });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Knight card" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a knight", style_ref: styleCard.id } });

  const expand = fakeAssistant("expanded text");
  await expandRecipePrompt(ROOT, { projectId: project.id, groupId: card.id, assistant: { expand } });

  assert.equal(expand.calls[0].styleBlock, "painterly oil, warm gold");
});

test("expandRecipePrompt: empty prompt is loud, no entry, assistant never called", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Expand empty" });
  const card = createRecipeCard(ROOT, { projectId: project.id }).group;
  const seqBefore = getProject(ROOT, project.id).history_seq;
  const expand = fakeAssistant("x");

  await assert.rejects(
    () => expandRecipePrompt(ROOT, { projectId: project.id, groupId: card.id, assistant: { expand } }),
    /has an empty prompt/,
  );
  assert.equal(expand.calls.length, 0);
  assert.equal(getProject(ROOT, project.id).history_seq, seqBefore);
});

test("expandRecipePrompt: an unknown group and a plain (non-card) group are loud, no entry, assistant never called", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Expand validate" });
  const plain = createGroup(ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;
  const seqBefore = getProject(ROOT, project.id).history_seq;
  const expand = fakeAssistant("x");

  await assert.rejects(
    () => expandRecipePrompt(ROOT, { projectId: project.id, groupId: "grp_missing", assistant: { expand } }),
    /group not found/,
  );
  await assert.rejects(
    () => expandRecipePrompt(ROOT, { projectId: project.id, groupId: plain.id, assistant: { expand } }),
    /not a recipe card/,
  );
  assert.equal(expand.calls.length, 0);
  assert.equal(getProject(ROOT, project.id).history_seq, seqBefore);
});

test("expandRecipePrompt: a fake assistant returning an empty string is loud, no entry", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Expand blank result" });
  const card = createRecipeCard(ROOT, { projectId: project.id }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a griffin" } });
  const seqBefore = getProject(ROOT, project.id).history_seq;
  const expand = fakeAssistant("   ");

  await assert.rejects(
    () => expandRecipePrompt(ROOT, { projectId: project.id, groupId: card.id, assistant: { expand } }),
    /empty result/,
  );
  assert.equal(getProject(ROOT, project.id).history_seq, seqBefore);
});

// ---- patchRecipe: expanded/use_expanded (T0239 increment 4) --------------------

test("patchRecipe: expanded/use_expanded accepted + validated; a fresh card defaults use_expanded true", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Patch expanded" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });
  assert.equal(card.recipe.use_expanded, true, "fresh card defaults to sending the expansion once one exists");

  const withExpanded = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { expanded: "a fine template" } }).group;
  assert.equal(withExpanded.recipe.expanded, "a fine template");

  const cleared = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { expanded: null } }).group;
  assert.equal(cleared.recipe.expanded, null);

  const toggledOff = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { use_expanded: false } }).group;
  assert.equal(toggledOff.recipe.use_expanded, false);
  const toggledOn = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { use_expanded: true } }).group;
  assert.equal(toggledOn.recipe.use_expanded, true);

  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { expanded: 5 } }),
    /expanded must be null or a string/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { use_expanded: "yes" } }),
    /use_expanded must be a boolean/,
  );
});

// ---- generateFromRecipe honoring the use_expanded toggle (lead, 2026-07-03 final spec) --

test("generateFromRecipe: sends expanded/short prompt exactly per resolveRecipePromptText's rule (use_expanded && expanded ? expanded : prompt)", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Toggle" });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Toggle card" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "short prompt" } });

  // No expansion yet -> always the short prompt, regardless of use_expanded (still true by default).
  {
    const codex = fakeGen(solidPng());
    await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } });
    assert.equal(codex.calls[0].prompt, "short prompt");
  }

  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { expanded: "long expanded text" } });

  // use_expanded true (the default) + expanded set -> Generate sends the expanded text.
  {
    const codex = fakeGen(solidPng());
    await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } });
    assert.equal(codex.calls[0].prompt, "long expanded text");
  }

  // Checkbox off -> Generate falls back to the short prompt even though expanded exists.
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { use_expanded: false } });
  {
    const codex = fakeGen(solidPng());
    await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } });
    assert.equal(codex.calls[0].prompt, "short prompt");
  }

  // Discard (expanded -> null) -> Generate sends the short prompt regardless of the flag.
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { use_expanded: true, expanded: null } });
  {
    const codex = fakeGen(solidPng());
    await generateFromRecipe(ROOT, { projectId: project.id, groupId: card.id, generators: { codex } });
    assert.equal(codex.calls[0].prompt, "short prompt");
  }
});

// ---- extractFromElement ---------------------------------------------------------

test("extractFromElement: writes element.meta.extracted via a fake assistant, ONE history entry, undo restores, re-run overwrites (two independent entries)", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Extract" });
  const image = addImage(ROOT, project.id, { name: "art.png", bytes: solidPng(10, 8, [10, 20, 30]) }).element;
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const fixture = extractResult();
  const extract1 = fakeAssistant(fixture);
  const result1 = await extractFromElement(ROOT, { projectId: project.id, elementId: image.id, assistant: { extract: extract1 } });

  assert.equal(extract1.calls.length, 1);
  assert.deepEqual(extract1.calls[0], { imagePath: resolveProjectFile(ROOT, project.id, image.src) });
  assert.equal(result1.element.meta.extracted.prompt_full, fixture.prompt_full);
  assert.equal(result1.element.meta.extracted.prompt_subject, fixture.prompt_subject);
  assert.deepEqual(result1.element.meta.extracted.style, {
    style_block: fixture.style_block,
    palette: fixture.palette,
    materials: fixture.materials,
    lighting: fixture.lighting,
    composition: fixture.composition,
    constraints_block: fixture.constraints_block,
  });
  assert.equal(result1.element.meta.extracted.description, fixture.description);
  assert.ok(result1.element.meta.extracted.at);
  assert.deepEqual(result1.extracted, result1.element.meta.extracted);

  const afterFirst = getProject(ROOT, project.id);
  assert.equal(afterFirst.history_seq, seqBefore + 1, "one journal entry");

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.equal(undone.elements.find((el) => el.id === image.id).meta.extracted, undefined, "undo removes meta.extracted");

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((el) => el.id === image.id).meta.extracted.description, fixture.description, "redo restores it");

  // Re-run OVERWRITES with a SECOND independent journal entry — asserted by comparing
  // seqs BEFORE/AFTER this call directly (not seqBefore+N: undo/redo consume their own
  // slots in the monotonic journal counter even though the DISPLAYED history_seq snaps
  // back to the logical undo-chain position, so only a same-call delta is reliable here).
  const seqBeforeRerun = getProject(ROOT, project.id).history_seq;
  const extract2 = fakeAssistant(extractResult({ description: "A second look." }));
  const result2 = await extractFromElement(ROOT, { projectId: project.id, elementId: image.id, assistant: { extract: extract2 } });
  assert.equal(result2.element.meta.extracted.description, "A second look.");
  const seqAfterRerun = getProject(ROOT, project.id).history_seq;
  assert.ok(seqAfterRerun > seqBeforeRerun, "re-run is its own NEW journal entry, on top of the current head");
});

test("extractFromElement: a non-image (text) element is loud, no entry, assistant never called", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Extract non-image" });
  const text = addText(REPO_ROOT, project.id, { content: "Hello" }).element;
  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const extract = fakeAssistant(extractResult());

  await assert.rejects(
    () => extractFromElement(REPO_ROOT, { projectId: project.id, elementId: text.id, assistant: { extract } }),
    /is not an image/,
  );
  assert.equal(extract.calls.length, 0);
  assert.equal(getProject(REPO_ROOT, project.id).history_seq, seqBefore);
});

test("extractFromElement: an assistant result missing a non-empty required key is loud, no entry", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Extract missing key" });
  const image = addImage(ROOT, project.id, { name: "art.png", bytes: solidPng() }).element;
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const extract = fakeAssistant(extractResult({ style_block: "" }));
  await assert.rejects(
    () => extractFromElement(ROOT, { projectId: project.id, elementId: image.id, assistant: { extract } }),
    /missing a non-empty "style_block"/,
  );
  assert.equal(getProject(ROOT, project.id).history_seq, seqBefore);
});

// ---- promoteExtractedRecipe / promoteExtractedStyle ------------------------------

test("promoteExtractedRecipe: loud without meta.extracted (run Extract first)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Promote recipe validate" });
  const image = addImage(ROOT, project.id, { name: "art.png", bytes: solidPng() }).element;

  assert.throws(
    () => promoteExtractedRecipe(ROOT, { projectId: project.id, elementId: image.id }),
    /has no extracted data — run Extract first/,
  );
});

test("promoteExtractedRecipe: mints a RECIPE card BELOW the element, frame FIT to the copy + padding; ONE entry; undo removes card+copy; promoting twice mints two independent cards", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Promote recipe" });
  const image = addImage(ROOT, project.id, { name: "art.png", bytes: solidPng(20, 16, [1, 2, 3]) }).element;
  patchElement(ROOT, project.id, image.id, { x: 100, y: 50 });
  await seedExtracted(project.id, image.id);
  const seeded = getProject(ROOT, project.id).elements.find((el) => el.id === image.id);
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const result = promoteExtractedRecipe(ROOT, { projectId: project.id, elementId: image.id });

  assert.equal(result.card.recipe.prompt, "a red fox on a rock");
  assert.equal(result.card.name, "a red fox on a rock");
  assert.equal(result.card.x, seeded.x, "below the element: same x");
  assert.equal(result.card.y, seeded.y + seeded.h + 16, "below the element: y+h+16");
  // Fit-to-content (lead 2026-07-03): frame = copy + 16px padding on every side, never the
  // fixed default card size.
  assert.deepEqual({ w: result.card.w, h: result.card.h }, { w: seeded.w + 32, h: seeded.h + 32 });

  assert.equal(result.refElement.src, seeded.src);
  assert.equal(result.refElement.groupId, result.card.id);
  assert.notEqual(result.refElement.id, image.id, "a fresh copy, not the original element");
  assert.equal(result.refElement.x, result.card.x + 16, "copy sits at the padding inset");
  assert.equal(result.refElement.y, result.card.y + 16);

  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "one journal entry");

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.equal((undone.groups || []).length, 0, "undo removes the minted card");
  assert.ok(!undone.elements.some((el) => el.id === result.refElement.id), "undo removes the ref copy");

  redoOp(ROOT, { projectId: project.id });
  const second = promoteExtractedRecipe(ROOT, { projectId: project.id, elementId: image.id });
  assert.notEqual(second.card.id, result.card.id, "promoting twice mints a SECOND independent card");
  assert.equal(getProject(ROOT, project.id).groups.length, 2);
});

test("promoteExtractedStyle: loud without meta.extracted (run Extract first)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Promote style validate" });
  const image = addImage(ROOT, project.id, { name: "art.png", bytes: solidPng() }).element;

  assert.throws(
    () => promoteExtractedStyle(ROOT, { projectId: project.id, elementId: image.id }),
    /has no extracted data — run Extract first/,
  );
});

test("promoteExtractedStyle: mints a STYLE card RIGHT of the element with style_block+constraints as the prompt and the copy as style.ref; ONE entry; undo removes card+copy; promoting twice mints two independent cards", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Promote style" });
  const image = addImage(ROOT, project.id, { name: "art.png", bytes: solidPng(20, 16, [4, 5, 6]) }).element;
  patchElement(ROOT, project.id, image.id, { x: 100, y: 50 });
  await seedExtracted(project.id, image.id);
  const seeded = getProject(ROOT, project.id).elements.find((el) => el.id === image.id);
  const seqBefore = getProject(ROOT, project.id).history_seq;

  const result = promoteExtractedStyle(ROOT, { projectId: project.id, elementId: image.id });

  assert.equal(result.card.style.prompt, "painterly oil, warm palette\n\nno text, no watermark");
  assert.equal(result.card.name, "A painterly red fox on a rock.");
  assert.equal(result.card.x, seeded.x + seeded.w + 16, "right of the element: x+w+16");
  assert.equal(result.card.y, seeded.y, "right of the element: same y");
  // Fit-to-content: frame = copy + 16px padding, same rule as promoteExtractedRecipe.
  assert.deepEqual({ w: result.card.w, h: result.card.h }, { w: seeded.w + 32, h: seeded.h + 32 });
  assert.equal(result.card.style.ref, result.refElement.id);

  assert.equal(result.refElement.src, seeded.src);
  assert.equal(result.refElement.groupId, result.card.id);

  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "one journal entry");

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.history_seq, seqBefore);
  assert.equal((undone.groups || []).length, 0);
  assert.ok(!undone.elements.some((el) => el.id === result.refElement.id));

  redoOp(ROOT, { projectId: project.id });
  const second = promoteExtractedStyle(ROOT, { projectId: project.id, elementId: image.id });
  assert.notEqual(second.card.id, result.card.id, "promoting twice mints a SECOND independent card");
});

test("promoteExtractedStyle: an empty constraints_block never adds a stray blank-line join", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Promote style no constraints" });
  const image = addImage(ROOT, project.id, { name: "art.png", bytes: solidPng() }).element;
  await seedExtracted(project.id, image.id, { constraints_block: "" });

  const result = promoteExtractedStyle(ROOT, { projectId: project.id, elementId: image.id });
  assert.equal(result.card.style.prompt, "painterly oil, warm palette");
});

// ---- historyEntryLabel -----------------------------------------------------------

test("historyEntryLabel maps expandRecipePrompt/extractFromElement/promoteExtractedRecipe/promoteExtractedStyle to readable labels", () => {
  assert.deepEqual(historyEntryLabel("expandRecipePrompt", {}), { label: "Expand prompt", summary: "" });
  assert.deepEqual(historyEntryLabel("extractFromElement", {}), { label: "Extract", summary: "" });
  assert.deepEqual(historyEntryLabel("promoteExtractedRecipe", {}), { label: "Recipe from extract", summary: "" });
  assert.deepEqual(historyEntryLabel("promoteExtractedStyle", {}), { label: "Style from extract", summary: "" });
});

// ---- CLI + API validation parity (no codex spawn — the CLI has no fake-assistant seam,
// mirroring recipe-generate's own established precedent above) -------------------------

test("recipe-expand: CLI and API reject a missing --group / non-card group / empty prompt the same way (validation parity, no codex spawn)", async (t) => {
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  const projectId = (await runInProcess("create", "--title", "Expand CLI parity")).project.id;
  const plain = (await runInProcess("group-create", projectId, "--name", "Plain", "--x", "0", "--y", "0", "--w", "10", "--h", "10")).group;
  const card = (await runInProcess("recipe-create", projectId, "--name", "Blank card")).group;

  // CLI: recipe-expand <id> with no --group -> the CLI's own local guard.
  assert.throws(() => execFileSync("node", [CLI, "recipe-expand", projectId], { env: { ...process.env, ...env }, encoding: "utf8" }));

  await assert.rejects(runInProcess("recipe-expand", projectId, "--group", plain.id), /not a recipe card/);
  await assert.rejects(runInProcess("recipe-expand", projectId, "--group", card.id), /has an empty prompt/);

  const handler = createCanvasApi(ROOT);
  const rejectedPlain = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards/${plain.id}/expand`, {});
  assert.equal(rejectedPlain.status, 400);
  assert.match(rejectedPlain.json().error, /not a recipe card/);

  // T0254 Tier 1 #2: statusForError maps "not found" to 404, not the old catch-all 400.
  const rejectedMissing = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards/grp_missing/expand`, {});
  assert.equal(rejectedMissing.status, 404);
  assert.match(rejectedMissing.json().error, /group not found/);
});

test("extract: CLI and API reject a missing --element / not-found element the same way (validation parity, no codex spawn)", async (t) => {
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  const projectId = (await runInProcess("create", "--title", "Extract CLI parity")).project.id;
  const plain = (await runInProcess("group-create", projectId, "--name", "Plain", "--x", "0", "--y", "0", "--w", "10", "--h", "10")).group;

  // CLI: extract <id> with no --element -> the CLI's own local guard.
  assert.throws(() => execFileSync("node", [CLI, "extract", projectId], { env: { ...process.env, ...env }, encoding: "utf8" }));

  // A GROUP id is never an element id -> the op's own "element not found".
  await assert.rejects(runInProcess("extract", projectId, "--element", plain.id), /element not found/);

  // T0254 Tier 1 #2: statusForError maps "not found" to 404, not the old catch-all 400.
  const handler = createCanvasApi(ROOT);
  const rejected = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/elements/${plain.id}/extract`, {});
  assert.equal(rejected.status, 404);
  assert.match(rejected.json().error, /element not found/);
});

test("promote-recipe / promote-style: CLI and API reject an element with no meta.extracted the same way (validation parity)", async (t) => {
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  const projectId = (await runInProcess("create", "--title", "Promote CLI parity")).project.id;
  const filePath = join(dir, "art.png");
  writeFileSync(filePath, solidPng());
  const image = (await runInProcess("add-image", projectId, "--file", filePath)).element;

  assert.throws(() => execFileSync("node", [CLI, "promote-recipe", projectId], { env: { ...process.env, ...env }, encoding: "utf8" }));
  assert.throws(() => execFileSync("node", [CLI, "promote-style", projectId], { env: { ...process.env, ...env }, encoding: "utf8" }));

  await assert.rejects(runInProcess("promote-recipe", projectId, "--element", image.id), /run Extract first/);
  await assert.rejects(runInProcess("promote-style", projectId, "--element", image.id), /run Extract first/);

  const handler = createCanvasApi(ROOT);
  const rejectedRecipe = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/elements/${image.id}/promote-recipe`, {});
  assert.equal(rejectedRecipe.status, 400);
  assert.match(rejectedRecipe.json().error, /run Extract first/);
  const rejectedStyle = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/elements/${image.id}/promote-style`, {});
  assert.equal(rejectedStyle.status, 400);
  assert.match(rejectedStyle.json().error, /run Extract first/);
});
