// Recipe card ops tests (T0239 increment 1: the card object + `recipe` meta + inspector
// surface — no generation yet). Metadata-only ops (no Python), so any placeholder root
// works for the direct-ops tests; CLI/API parity tests drive the real cli.mjs / api.mjs
// exactly like tests/cli.test.mjs / tests/api.test.mjs do (duplicated here rather than
// imported, matching every other *.test.mjs file's own tiny helpers).
// Run: node --test ai_studio/assets/canvas/tests/recipe.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import {
  createGroup,
  createProject,
  createRecipeCard,
  duplicateNodes,
  exportProject,
  getProject,
  historyEntryLabel,
  patchRecipe,
  pasteNodes,
  redoOp,
  undoOp,
} from "../ops.mjs";
import { buildNodesSpec } from "../tree.mjs";
import { createCanvasApi } from "../api.mjs";

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

function run(env, ...args) {
  const stdout = execFileSync(process.execPath, [CLI, ...args], {
    env: { ...process.env, ...env },
    encoding: "utf8",
  });
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
    supersample: true,
    n_candidates: 1,
  },
  style_ref: null,
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

  // style_ref: null (default) -> a string id -> back to null.
  const withStyle = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: "grp_style1" } }).group;
  assert.equal(withStyle.recipe.style_ref, "grp_style1");
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
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id, name: "Hero card", x: 10, y: 20 });
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a knight", engine: "gemini", style_ref: "grp_style1" } });
  const seeded = getProject(ROOT, project.id).groups[0];

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

test("exportProject skips top-level recipe-card groups (a card is a workshop object, not a screen)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Export" });
  createGroup(REPO_ROOT, { projectId: project.id, name: "Screen", x: 0, y: 0, w: 20, h: 20 });
  createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Card" });

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

test("CLI recipe-create / recipe-set parity with the ops layer", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-recipe-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const projectId = run(env, "create", "--title", "CLI Recipe").project.id;
  const created = run(env, "recipe-create", projectId, "--name", "CLI card", "--x", "5", "--y", "6", "--w", "300", "--h", "200");
  assert.equal(created.group.name, "CLI card");
  assert.deepEqual(created.group.recipe, DEFAULT_RECIPE);
  assert.deepEqual({ x: created.group.x, y: created.group.y, w: created.group.w, h: created.group.h }, { x: 5, y: 6, w: 300, h: 200 });

  const groupId = created.group.id;
  const patched = run(env, "recipe-set", projectId, "--group", groupId, "--prompt", "a red fox", "--engine", "both");
  assert.equal(patched.group.recipe.prompt, "a red fox");
  assert.equal(patched.group.recipe.engine, "both");

  const withStyle = run(env, "recipe-set", projectId, "--group", groupId, "--style", "grp_style1");
  assert.equal(withStyle.group.recipe.style_ref, "grp_style1");
  const clearedStyle = run(env, "recipe-set", projectId, "--group", groupId, "--style", "none");
  assert.equal(clearedStyle.group.recipe.style_ref, null);

  const failure = runFail(env, "recipe-set", projectId, "--group", groupId, "--engine", "dalle");
  assert.match(String(failure.stderr || failure.message), /engine must be one of/);

  const shown = run(env, "show", projectId).project;
  assert.equal(shown.groups.length, 1);
  assert.equal(shown.groups[0].recipe.prompt, "a red fox");
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
