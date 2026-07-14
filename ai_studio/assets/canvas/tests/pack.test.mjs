// Pack MODE tests (T0332 v2: contracts/recipe-pack.md — lead decision "Слить"/
// "Merge"). Pack mode is NOT a third card type; it is an optional `pack` field on the RECIPE
// card's own blob (`group.recipe.pack`) plus two now-patchable fields inside `recipe.params`
// (`bg_key`/`n_candidates`) — see recipe.test.mjs for the single-image (pack === null) path,
// unchanged by this file. This file covers: patchRecipe's `pack`/`params` validation
// (transplanted from phase A's now-deleted normalizePackPatch — the per-field rules are
// byte-for-byte the same, minus background/candidates, which a later lead decision moved OUT
// of `pack` and into the newly-unfrozen `params`) and packPreview (transplanted from phase A's
// now-deleted expandPack, reading recipe.* instead of a separate card's own `pack.*`). Unlike
// recipe/style patches, `pack` REPLACES wholesale (proven below); `params` MERGES partially
// (mirrors phase A's own `pack.params` precedent) — both are exercised explicitly since they
// are opposite merge strategies on two fields of the SAME patch call.
// packPreview is NOT a codex/agy seam — it runs the REAL expand_jobs.py (offline,
// deterministic, stdlib) through the shared runToolPython warm-worker bridge, so its pipeline
// tests skip cleanly without the studio venv (alpha.test.mjs:99's precedent) rather than
// injecting a fake generator.
// Run: node --test ai_studio/assets/canvas/tests/pack.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import {
  addImage,
  assignToGroup,
  createGroup,
  createProject,
  createRecipeCard,
  createStyleCard,
  detectRegions,
  duplicateNodes,
  generateFromRecipe,
  getProject,
  packPreview,
  packSlice,
  patchRecipe,
  patchStyle,
  readHistory,
  redoOp,
  resolveProjectFile,
  undoOp,
  updateProject,
} from "../ops.mjs";
import { main as runCanvasCli } from "../cli.mjs";
import { decodePng, magentaSheetPng, solidPng } from "./png_fixture.mjs";

// Metadata ops resolve store paths only, so any placeholder root works (no Python) for the
// blob-level tests. packPreview's pipeline tests need the REAL repo root (runToolPython
// resolves the script path + studio config off of it).
const ROOT = "C:/unused-repo-root";
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-pack-"));
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

function runFail(env, ...args) {
  try {
    execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    assert.fail(`expected "${args.join(" ")}" to fail`);
  } catch (error) {
    return error;
  }
}

// A well-formed pack config: one big axis (material, referenced in the prompt/subject_template
// via its {axis} slot) with 2 values, one vary axis (grade) with 4 values fitting a 2x2 grid —
// small enough to stay well under max_jobs/MAX_VARY, shared by every packPreview pipeline test
// below. Patches BOTH `prompt` and `pack` in the SAME call (independent top-level recipe
// fields — proves nothing about merge-vs-replace, that is exercised separately below).
function seedWellFormedRecipe(projectId, groupId) {
  return patchRecipe(REPO_ROOT, {
    projectId,
    groupId,
    patch: {
      prompt: "a {material} generator building",
      pack: {
        axes: { grade: ["rusty", "plain", "gilded", "mythic"], material: ["stone", "wood"] },
        vary: "grade",
        grid: [2, 2],
        max_jobs: 12,
      },
    },
  }).group;
}

// ================================================================================
// patchRecipe: `pack` validation (full-replace) + `params` validation (partial merge).
// ================================================================================

test("patchRecipe: pack full-object round-trip; a SECOND patch REPLACES pack wholesale, not a merge (a dropped axis key stays dropped)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Pack" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id, name: "My card" });
  assert.equal(card.recipe.pack, null, "pack mode is off by default");

  const first = patchRecipe(ROOT, {
    projectId: project.id,
    groupId: card.id,
    patch: { pack: { axes: { grade: ["a", "b"], material: ["stone"] }, vary: "grade", grid: [2, 2], max_jobs: 10 } },
  }).group;
  assert.deepEqual(first.recipe.pack, { v: 1, axes: { grade: ["a", "b"], material: ["stone"] }, vary: "grade", grid: [2, 2], max_jobs: 10 });

  // Second patch drops the "material" axis entirely and changes every other field — a MERGE
  // would leave "material" behind (like patchRecipe's own top-level `{...group.recipe,
  // ...resolved}` would do for any OTHER field); a REPLACE (what pack actually does) does not.
  const second = patchRecipe(ROOT, {
    projectId: project.id,
    groupId: card.id,
    patch: { pack: { axes: { grade: ["x", "y", "z"] }, vary: "grade", grid: [1, 3], max_jobs: 5 } },
  }).group;
  assert.deepEqual(second.recipe.pack, { v: 1, axes: { grade: ["x", "y", "z"] }, vary: "grade", grid: [1, 3], max_jobs: 5 });
  assert.ok(!("material" in second.recipe.pack.axes), "full replace: the old 'material' axis did not survive");

  // pack: null turns pack mode back off.
  const cleared = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: null } }).group;
  assert.equal(cleared.recipe.pack, null);
});

test("patchRecipe: pack requires ALL of axes/vary/grid/max_jobs when non-null — a missing field is loud, not silently carried over from the old blob", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Pack missing" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });

  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { axes: { grade: ["a"] }, vary: "grade", grid: [1, 1] } } }),
    /recipe pack: missing field "max_jobs"/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { vary: "grade", grid: [1, 1], max_jobs: 1 } } }),
    /recipe pack: missing field "axes"/,
  );
});

test("patchRecipe: pack rejects unknown keys — including phase A's now-removed subject_template/style_ref/params/background/candidates", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Pack unknown" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });
  const validPack = { axes: { grade: ["a"] }, vary: "grade", grid: [1, 1], max_jobs: 1 };

  for (const badKey of ["subject_template", "style_ref", "params", "background", "candidates"]) {
    assert.throws(
      () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { ...validPack, [badKey]: "x" } } }),
      new RegExp(`recipe pack: unknown field "${badKey}"`),
      `pack.${badKey} must be rejected (it moved out of pack in the v2 build-spec)`,
    );
  }
});

test("patchRecipe: pack axes must be non-empty arrays of non-empty strings (insertion order preserved); grid must be two ints in 1..3; vary must be a string; max_jobs a positive integer", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Pack fields" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });
  const base = { vary: "grade", grid: [2, 2], max_jobs: 5 };

  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { ...base, axes: { grade: [] } } } }),
    /axes\.grade must be a non-empty array/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { ...base, axes: { grade: ["", "plain"] } } } }),
    /axes\.grade must be a non-empty array/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { ...base, axes: { grade: ["a"] }, vary: 5 } } }),
    /pack vary must be a string/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { ...base, axes: { grade: ["a"] }, grid: [0, 3] } } }),
    /grid must be two integers in 1\.\.3/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { ...base, axes: { grade: ["a"] }, grid: [4, 1] } } }),
    /grid must be two integers in 1\.\.3/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { ...base, axes: { grade: ["a"] }, max_jobs: 0 } } }),
    /max_jobs must be a positive integer/,
  );

  const patched = patchRecipe(ROOT, {
    projectId: project.id,
    groupId: card.id,
    patch: { pack: { ...base, axes: { zeta: ["a", "b"], alpha: ["c"] } } },
  }).group;
  assert.deepEqual(Object.keys(patched.recipe.pack.axes), ["zeta", "alpha"], "axes key order is Object.entries insertion order, not resorted");
});

test("patchRecipe: pack:null clears pack mode; journal/undo byte-exact", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Undo" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });
  const afterCreate = getProject(ROOT, project.id);

  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: { axes: { grade: ["a"] }, vary: "grade", grid: [1, 1], max_jobs: 3 } } });
  const afterSet = getProject(ROOT, project.id);
  assert.ok(afterSet.groups[0].recipe.pack, "pack mode is on after the patch");

  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { pack: null } });
  const afterClear = getProject(ROOT, project.id);
  assert.equal(afterClear.groups[0].recipe.pack, null);

  const undone = undoOp(ROOT, { projectId: project.id }).project; // undo the clear
  assert.equal(JSON.stringify(undone.groups), JSON.stringify(afterSet.groups), "undo restores the prior (non-null) pack byte-exact");

  const undoneAgain = undoOp(ROOT, { projectId: project.id }).project; // undo the set
  assert.equal(JSON.stringify(undoneAgain.groups), JSON.stringify(afterCreate.groups), "undo restores the pre-pack recipe blob byte-exact");

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.ok(redone.groups[0].recipe.pack, "redo re-applies the pack set");
});

test("patchRecipe: params is a PARTIAL patch — bg_key/n_candidates/size/quality merge onto the existing params one level deep (opposite of pack's full-replace)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Params merge" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });
  const originalParams = card.recipe.params;

  const afterBgKey = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { params: { bg_key: "#00ff00" } } }).group;
  assert.equal(afterBgKey.recipe.params.bg_key, "#00ff00");
  // Every OTHER params field survives untouched — a plain spread (like `pack`'s own
  // behavior) would have wiped them, since the given params object only mentions bg_key.
  assert.equal(afterBgKey.recipe.params.size, originalParams.size);
  assert.equal(afterBgKey.recipe.params.quality, originalParams.quality);
  assert.equal(afterBgKey.recipe.params.model, originalParams.model);
  assert.equal(afterBgKey.recipe.params.n_candidates, originalParams.n_candidates);

  const afterCandidates = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { params: { n_candidates: 3, size: "1536x1024", quality: "medium" } } }).group;
  assert.equal(afterCandidates.recipe.params.n_candidates, 3);
  assert.equal(afterCandidates.recipe.params.size, "1536x1024");
  assert.equal(afterCandidates.recipe.params.quality, "medium");
  assert.equal(afterCandidates.recipe.params.bg_key, "#00ff00", "bg_key from the PREVIOUS patch survives this one too");
});

test("patchRecipe: params.model is immutable (loud even at its current value); unknown params keys are loud; bg_key format/n_candidates range are validated; empty params object is loud", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Params validate" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });

  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { params: { model: "gpt-image-2" } } }),
    /params\.model is immutable or unknown/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { params: { supersample: false } } }),
    /params\.supersample is immutable or unknown/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { params: { bg_key: "not-a-hex" } } }),
    /bg_key must be a 6-digit hex color/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { params: { n_candidates: 0 } } }),
    /n_candidates must be a positive integer/,
  );
  assert.throws(
    () => patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { params: {} } }),
    /params patch requires at least one of bg_key, n_candidates, size, quality/,
  );

  // bg_key format is generic hex here (NOT restricted to magenta/green) — the pack-specific
  // pairing is enforced later, at packPreview time, not at patch-time (build-spec: "иной hex —
  // громкая ошибка в packPreview/pack-ветке generate, НЕ на patch-time").
  const afterOddHex = patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { params: { bg_key: "#123456" } } }).group;
  assert.equal(afterOddHex.recipe.params.bg_key, "#123456", "any hex is accepted at patch-time, even one pack mode would reject later");
});

// ================================================================================
// CLI: recipe-set pack/params flags, recipe-pack-preview validation parity.
// ================================================================================

test("CLI recipe-set: pack flags assemble a FULL object (merging the CURRENT pack in the CLI, since the op itself replaces wholesale); --pack none clears; --grid validation", async (t) => {
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  const projectId = (await runInProcess("create", "--title", "CLI Pack")).project.id;
  const created = await runInProcess("recipe-create", projectId, "--name", "CLI recipe");
  const groupId = created.group.id;
  assert.equal(created.group.recipe.pack, null);

  const axesPath = join(dir, "axes.json");
  writeFileSync(axesPath, JSON.stringify({ grade: ["rusty", "plain"], material: ["stone"] }));
  const patched = await runInProcess("recipe-set", projectId, "--group", groupId, "--axes-json", axesPath, "--vary", "grade", "--grid", "2x2", "--max-jobs", "24");
  assert.deepEqual(patched.group.recipe.pack, { v: 1, axes: { grade: ["rusty", "plain"], material: ["stone"] }, vary: "grade", grid: [2, 2], max_jobs: 24 });

  // A SECOND call touching only --vary must NOT lose the axes/grid/max_jobs set above — the
  // CLI reads the CURRENT recipe.pack and merges this one flag on top before sending the FULL
  // object (the op itself never merges).
  const revaried = await runInProcess("recipe-set", projectId, "--group", groupId, "--vary", "material");
  assert.deepEqual(revaried.group.recipe.pack.axes, { grade: ["rusty", "plain"], material: ["stone"] }, "axes carried forward by the CLI merge");
  assert.equal(revaried.group.recipe.pack.grid[0], 2, "grid carried forward too");
  assert.equal(revaried.group.recipe.pack.vary, "material");

  const cleared = await runInProcess("recipe-set", projectId, "--group", groupId, "--pack", "none");
  assert.equal(cleared.group.recipe.pack, null);

  // --pack none takes priority even when other pack flags are given in the SAME call.
  const clearedAgain = await runInProcess("recipe-set", projectId, "--group", groupId, "--vary", "grade", "--pack", "none");
  assert.equal(clearedAgain.group.recipe.pack, null);

  const badGrid = runFail(env, "recipe-set", projectId, "--group", groupId, "--vary", "grade", "--grid", "bogus");
  assert.match(String(badGrid.stderr || badGrid.message), /--grid must look like RxC/);

  const badPack = runFail(env, "recipe-set", projectId, "--group", groupId, "--pack", "something-else");
  assert.match(String(badPack.stderr || badPack.message), /--pack only accepts "none"/);
});

test("CLI recipe-set: params flags (--bg-key/--n-candidates/--size/--quality) patch recipe.params; parity with the ops layer", async (t) => {
  tempProjects(t);

  const projectId = (await runInProcess("create", "--title", "CLI Params")).project.id;
  const created = await runInProcess("recipe-create", projectId, "--name", "CLI recipe");
  const groupId = created.group.id;

  const patched = await runInProcess("recipe-set", projectId, "--group", groupId, "--bg-key", "#00ff00", "--n-candidates", "3", "--size", "1536x1024", "--quality", "medium");
  assert.equal(patched.group.recipe.params.bg_key, "#00ff00");
  assert.equal(patched.group.recipe.params.n_candidates, 3);
  assert.equal(patched.group.recipe.params.size, "1536x1024");
  assert.equal(patched.group.recipe.params.quality, "medium");
  assert.equal(patched.group.recipe.params.model, "gpt-image-2", "model untouched by an unrelated params patch");

  const shown = (await runInProcess("show", projectId)).project;
  assert.equal(shown.groups.find((g) => g.id === groupId).recipe.params.bg_key, "#00ff00");
});

test("CLI recipe-pack-preview: validation parity (no python spawn) — pack:null and a non-recipe group are loud the same way the op is", async (t) => {
  tempProjects(t);

  const projectId = (await runInProcess("create", "--title", "Preview CLI parity")).project.id;
  const plain = (await runInProcess("group-create", projectId, "--name", "Plain", "--x", "0", "--y", "0", "--w", "10", "--h", "10")).group;
  await assert.rejects(runInProcess("recipe-pack-preview", projectId, "--group", plain.id), /not a recipe card/);

  const recipe = (await runInProcess("recipe-create", projectId, "--name", "No pack yet")).group;
  await assert.rejects(runInProcess("recipe-pack-preview", projectId, "--group", recipe.id), /pack mode is off/);
});

test("CLI recipe-pack-generate: validation parity (no python/codex spawn) — missing --group and a non-recipe group are loud the same way the op is", async (t) => {
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  const projectId = (await runInProcess("create", "--title", "Generate CLI parity")).project.id;
  const plain = (await runInProcess("group-create", projectId, "--name", "Plain", "--x", "0", "--y", "0", "--w", "10", "--h", "10")).group;

  // no --group -> the CLI's own local guard.
  const noGroup = runFail(env, "recipe-pack-generate", projectId);
  assert.match(String(noGroup.stderr || noGroup.message), /recipe-pack-generate requires --group/);

  // --group <plain group> -> the OP's own "not a recipe card" (before either branch dispatches).
  await assert.rejects(runInProcess("recipe-pack-generate", projectId, "--group", plain.id), /not a recipe card/);
});

// ================================================================================
// packPreview: validation (no python spawn).
// ================================================================================

test("packPreview requires projectId/groupId; an unknown group, a non-recipe group, and pack:null are all loud before any python spawn; every RECIPE_ENGINES value is accepted (both = 2x cost, the lead's call)", async (t) => {
  tempProjects(t);
  await assert.rejects(() => packPreview(ROOT, {}), /requires projectId/);
  await assert.rejects(() => packPreview(ROOT, { projectId: "p" }), /requires groupId/);

  const project = createProject(ROOT, { title: "Preview validate" });
  const plain = createGroup(ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;
  await assert.rejects(() => packPreview(ROOT, { projectId: project.id, groupId: "grp_missing" }), /group not found/);
  await assert.rejects(() => packPreview(ROOT, { projectId: project.id, groupId: plain.id }), /not a recipe card/);

  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });
  await assert.rejects(() => packPreview(ROOT, { projectId: project.id, groupId: card.id }), /pack mode is off/);

  const withPack = patchRecipe(ROOT, {
    projectId: project.id,
    groupId: card.id,
    patch: { prompt: "a thing", pack: { axes: { grade: ["a"] }, vary: "grade", grid: [1, 1], max_jobs: 1 }, engine: "both" },
  }).group;
  assert.equal(withPack.recipe.engine, "both", "engine 'both' + pack is a LEGAL card state (lead decision 2026-07-07) — no engine gate anywhere in pack mode");
});

test("packPreview: its OWN defensive style_ref re-check fires on a hand-edited blob (patchRecipe already blocks this at write time)", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Preview style missing" });
  const { group: card } = createRecipeCard(ROOT, { projectId: project.id });
  const otherRecipe = createRecipeCard(ROOT, { projectId: project.id }).group;
  patchRecipe(ROOT, {
    projectId: project.id,
    groupId: card.id,
    patch: { prompt: "a thing", pack: { axes: { grade: ["a"] }, vary: "grade", grid: [1, 1], max_jobs: 1 } },
  });

  // patchRecipe refuses a dangling/non-style style_ref at write time (recipe.test.mjs covers
  // that), so the only way to reach packPreview's OWN defensive re-check is a hand-edited
  // project.json — simulated via updateProject directly, bypassing patchRecipe entirely.
  updateProject(ROOT, project.id, {
    groups: getProject(ROOT, project.id).groups.map((group) =>
      group.id === card.id ? { ...group, recipe: { ...group.recipe, style_ref: otherRecipe.id } } : group,
    ),
  });

  await assert.rejects(() => packPreview(ROOT, { projectId: project.id, groupId: card.id }), /has a style_ref that is not a style-card group/);
});

// ---- packPreview pipeline (real expand_jobs.py; skips cleanly without the studio venv) ----

// Try one packPreview call; return the result or null on a venv/interpreter miss (the caller
// then t.skip's) — mirrors alpha.test.mjs's tryAlpha isolation of "does Python run here".
async function tryPreview(t, projectId, groupId) {
  try {
    return await packPreview(REPO_ROOT, { projectId, groupId });
  } catch (error) {
    if (/venv|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`pack preview pipeline unavailable: ${error.message}`);
      return null;
    }
    throw error;
  }
}

test("packPreview (real expand_jobs.py): sheets/jobs/cells shape, no style_ref -> empty style_prefix (not a loud error), no journal entry, blob unchanged", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Preview pipeline" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Generators" });
  seedWellFormedRecipe(project.id, card.id);
  const before = getProject(REPO_ROOT, project.id);

  const result = await tryPreview(t, project.id, card.id);
  if (!result) return;

  assert.equal(result.sheets, 2, "2 material values (big axis) x 1 candidate (default n_candidates) = 2 sheets");
  assert.equal(result.style_ref_image, false, "no style_ref -> no ref image");
  assert.equal(result.jobs.length, 2);
  for (const job of result.jobs) {
    assert.ok(job.name.startsWith("generators"));
    assert.match(job.prompt, /\[TASK\]/);
    assert.match(job.prompt, /\[STYLE\]/);
    assert.equal(job.cells.length, 4, "2x2 grid, vary axis has 4 values -> 4 filled cells");
    assert.deepEqual(Object.keys(job.cells[0]), ["cell", "axes"]);
    // Stripped output: only name/prompt/cells — no out/input_image/pack/size/quality/model.
    assert.deepEqual(Object.keys(job).sort(), ["cells", "name", "prompt"]);
  }

  // EPHEMERAL: no journal entry, blob byte-identical to before the call.
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, before.history_seq, "packPreview never journals");
  assert.equal(JSON.stringify(after.groups), JSON.stringify(before.groups), "packPreview never mutates the blob");
});

test("packPreview (real expand_jobs.py): engine gemini and engine both both pass the gate and expand the SAME sheets — engines don't shape prompts, only who generates (review gap 2026-07-07)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Preview engines" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Generators" });
  seedWellFormedRecipe(project.id, card.id);

  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { engine: "gemini" } });
  const onGemini = await tryPreview(t, project.id, card.id);
  if (!onGemini) return;
  assert.equal(onGemini.sheets, 2, "gemini passes the gate — preview works");

  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { engine: "both" } });
  const onBoth = await tryPreview(t, project.id, card.id);
  if (!onBoth) return;
  assert.equal(onBoth.sheets, 2, "both passes too — the fan-out (2x calls) happens at GENERATE, prompts are engine-independent");
  assert.deepEqual(onBoth.jobs.map((j) => j.prompt), onGemini.jobs.map((j) => j.prompt), "identical prompts across engines");
});

test("packPreview (real expand_jobs.py): background is DERIVED from params.bg_key (magenta/green), candidates from params.n_candidates — an off-pair hex is loud only HERE, not at patch-time", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Preview bg/candidates" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Generators" });
  seedWellFormedRecipe(project.id, card.id);

  const defaultResult = await tryPreview(t, project.id, card.id);
  if (!defaultResult) return;
  // default bg_key (#ff00ff) -> magenta -> expand_jobs.py's own BACKGROUND_HEX["magenta"].
  assert.ok(defaultResult.jobs.every((job) => job.prompt.includes("Solid uniform #FF00FF background")));

  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { params: { bg_key: "#00ff00", n_candidates: 3 } } });
  const greenResult = await tryPreview(t, project.id, card.id);
  if (!greenResult) return;
  assert.ok(greenResult.jobs.every((job) => job.prompt.includes("Solid uniform #00FF00 background")));
  assert.equal(greenResult.sheets, 2, "still 2 sheets (2 material values) — candidates is a per-sheet multiplier, not an extra sheet");
  assert.equal(greenResult.jobs.length, 6, "2 sheets x 3 candidates = 6 flat jobs (expand_jobs.py's own job list)");

  // An off-pair hex patches fine (generic hex validation at patch-time)...
  const oddCard = patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { params: { bg_key: "#123456" } } }).group;
  assert.equal(oddCard.recipe.params.bg_key, "#123456");
  // ...but is loud HERE, at preview-time, before any python spawn (the pack-specific pairing).
  await assert.rejects(() => packPreview(REPO_ROOT, { projectId: project.id, groupId: card.id }), /pack mode requires exactly/);
});

test("packPreview (real expand_jobs.py): subject_template is recipe.prompt VERBATIM — expanded/use_expanded are ignored even when use_expanded is true", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Preview verbatim" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Generators" });
  patchRecipe(REPO_ROOT, {
    projectId: project.id,
    groupId: card.id,
    patch: {
      prompt: "a {material} ZZRAWPROMPT generator building",
      expanded: "a ZZEXPANDEDPROMPT totally different description with no material slot",
      use_expanded: true,
      pack: { axes: { grade: ["rusty", "plain"], material: ["stone", "wood"] }, vary: "grade", grid: [1, 2], max_jobs: 12 },
    },
  });

  const result = await tryPreview(t, project.id, card.id);
  if (!result) return;

  assert.ok(result.jobs.every((job) => job.prompt.includes("ZZRAWPROMPT")), "the RAW prompt reached the expander");
  assert.ok(result.jobs.every((job) => !job.prompt.includes("ZZEXPANDEDPROMPT")), "expanded/use_expanded never leak into pack mode");
});

test("packPreview (real expand_jobs.py): style_ref set with a TEXT-only style card -> style_prefix verbatim in the [STYLE] section, style_ref_image false", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Preview style text" });
  const styleCard = createStyleCard(REPO_ROOT, { projectId: project.id, name: "Painterly" }).group;
  patchStyle(REPO_ROOT, { projectId: project.id, groupId: styleCard.id, patch: { prompt: "  painterly oil, warm gold  " } });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Generators" });
  seedWellFormedRecipe(project.id, card.id);
  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: styleCard.id } });

  const result = await tryPreview(t, project.id, card.id);
  if (!result) return;

  assert.equal(result.style_ref_image, false, "a style card with no ref image -> no ref image flag");
  assert.ok(result.jobs.every((job) => job.prompt.includes("painterly oil, warm gold")), "style prompt travels verbatim into every sheet's [STYLE] section");
});

test("packPreview (real expand_jobs.py): style_ref set with a style card that HAS a ref image -> style_ref_image: true (an INFO flag, never a warning — the image travels in the generate branch), preview still succeeds", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Preview style image" });
  const styleCard = createStyleCard(REPO_ROOT, { projectId: project.id, name: "Painterly" }).group;
  patchStyle(REPO_ROOT, { projectId: project.id, groupId: styleCard.id, patch: { prompt: "painterly oil" } });
  const refImage = addImage(REPO_ROOT, project.id, { name: "ref.png", bytes: solidPng() }).element;
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [refImage.id], groupId: styleCard.id });
  // The FIRST image dropped into an empty style card auto-claims the ref (applyStyleAutoRef) —
  // confirm the precondition before asserting on it.
  const seededStyle = getProject(REPO_ROOT, project.id).groups.find((g) => g.id === styleCard.id);
  assert.equal(seededStyle.style.ref, refImage.id, "precondition: style card has a ref image");

  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Generators" });
  seedWellFormedRecipe(project.id, card.id);
  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: styleCard.id } });

  const result = await tryPreview(t, project.id, card.id);
  if (!result) return;

  assert.equal(result.style_ref_image, true);
  assert.equal(result.sheets, 2, "the info flag does not fail the preview");
});

test("packPreview (real expand_jobs.py): the expander's own SystemExit surfaces verbatim (vary too large for the grid)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Preview overflow" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id });
  patchRecipe(REPO_ROOT, {
    projectId: project.id,
    groupId: card.id,
    patch: {
      prompt: "a generator",
      pack: { axes: { grade: ["a", "b", "c", "d", "e"] }, vary: "grade", grid: [2, 2], max_jobs: 12 }, // 4 cells, 5 vary values
    },
  });

  let error;
  try {
    await packPreview(REPO_ROOT, { projectId: project.id, groupId: card.id });
  } catch (err) {
    error = err;
  }
  if (error && /venv|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
    t.skip(`pack preview pipeline unavailable: ${error.message}`);
    return;
  }
  assert.ok(error, "expected packPreview to reject");
  assert.match(error.message, /only holds 4 cell/);
});

// ================================================================================
// generateFromRecipe pack branch (T0332 v2 build-spec §3): sequential per-sheet codex spawns,
// each minted under its OWN short commit. The generator itself is FAKE (platform seam,
// dual_generate.test.mjs/recipe.test.mjs's own precedent — codex/agy never spawn in the
// suite); expand_jobs.py is REAL (same skip-without-venv pattern as the packPreview pipeline
// tests above, alpha.test.mjs:99's precedent) since it is offline/deterministic/stdlib.
// ================================================================================

// A fake generator that resolves/rejects ONE prepared outcome per call, in order — lets a test
// script "sheet 2 fails, sheets 1 and 3 succeed" deterministically. Mirrors recipe.test.mjs's
// own fakeGen (single fixed outcome) generalized to a per-call sequence.
function fakeGenSequence(outcomes) {
  const calls = [];
  let index = 0;
  const fn = async (args) => {
    calls.push(args);
    const outcome = outcomes[Math.min(index, outcomes.length - 1)];
    index += 1;
    if (outcome instanceof Error) throw outcome;
    return outcome;
  };
  fn.calls = calls;
  return fn;
}

// A 2-sheet pack (material: stone/wood; vary: grade, 2 values, grid 1x2 — 1 candidate each by
// default) OR a 3-sheet pack (+ clay) for the partial-failure test, which needs a THIRD sheet
// to prove the run keeps going past a mid-run failure. Cartesian product order is the given
// array order (expand_jobs.py's own `itertools.product`), so "first/second/third" below always
// means stone/wood/(clay).
function seedPackRecipe(projectId, groupId, materials = ["stone", "wood"]) {
  return patchRecipe(REPO_ROOT, {
    projectId,
    groupId,
    patch: {
      prompt: "a {material} lantern",
      pack: { axes: { grade: ["rusty", "plain"], material: materials }, vary: "grade", grid: [1, 2], max_jobs: 12 },
    },
  }).group;
}

// Try one generateFromRecipe (pack branch) call; return the result or null on a venv/
// interpreter miss (the caller then t.skip's) — mirrors tryPreview above.
async function tryGenerate(t, args) {
  try {
    return await generateFromRecipe(REPO_ROOT, args);
  } catch (error) {
    if (/venv|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`pack generate pipeline unavailable: ${error.message}`);
      return null;
    }
    throw error;
  }
}

test("generateFromRecipe pack branch on engine=both: every job fans out to BOTH engines — two sheets per job with codex/agy-suffixed names, per-engine meta (engine, params_snapshot, job pointer)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack generate both" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id);
  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { engine: "both" } });

  const codex = fakeGenSequence([solidPng(), solidPng()]);
  const gemini = fakeGenSequence([solidPng(), solidPng()]);
  const result = await tryGenerate(t, { projectId: project.id, groupId: card.id, generators: { codex, gemini } });
  if (!result) return;

  assert.equal(codex.calls.length, 2, "one codex call per job (2 material values)");
  assert.equal(gemini.calls.length, 2, "one gemini call per job — 'both' = 2x the paid calls, the lead's explicit choice");
  assert.equal(result.results.length, 4, "one result row per (job, engine) unit");
  assert.ok(result.results.every((r) => r.status === "ok"));
  assert.equal(result.last_run.verdict, "ok");

  const after = getProject(REPO_ROOT, project.id);
  const sheets = after.elements.filter((el) => el.groupId === result.run_group_id);
  assert.equal(sheets.length, 4, "two sheets per job, one per engine");
  for (const engine of ["codex", "gemini"]) {
    const suffix = engine === "gemini" ? "agy" : "codex";
    const engineSheets = sheets.filter((el) => el.meta.pack.engine === engine);
    assert.equal(engineSheets.length, 2);
    for (const el of engineSheets) {
      assert.ok(el.name.endsWith(` ${suffix}`), `both-run sheet display names carry the engine suffix (got ${JSON.stringify(el.name)})`);
      assert.equal(el.meta.pack.job, el.name.slice(0, -(` ${suffix}`.length)), "meta.pack.job = the expander's BARE job name — the stable --sheet key under a suffixed display name");
    }
  }
  const geminiSheet = sheets.find((el) => el.meta.pack.engine === "gemini");
  const codexSheet = sheets.find((el) => el.meta.pack.engine === "codex");
  assert.deepEqual(geminiSheet.meta.pack.params_snapshot, { size: "1024x1024", bg_key: "#ff00ff", n_candidates: 1 }, "per-ENGINE snapshot even inside one both-run");
  assert.deepEqual(codexSheet.meta.pack.params_snapshot, { size: "1024x1024", quality: "high", model: "gpt-image-2", bg_key: "#ff00ff", n_candidates: 1 });

  const packRun = after.tool_runs.find((run) => run.op === "generate_from_recipe_pack" && run.cardId === card.id);
  assert.equal(packRun.params.engine, "both");
  assert.equal(packRun.params.model, "gpt-image-2", "the ONE aggregate row records the codex superset; per-sheet meta is the exact record");
});

test("generateFromRecipe pack branch on engine=both: resume dedups by (sheet_axes, ENGINE) — a failed gemini half regenerates without repaying its codex sibling", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack both resume" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id);
  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { engine: "both" } });

  // First run: codex lands both jobs; gemini fails the FIRST job (stone), lands the second.
  const firstRun = await tryGenerate(t, {
    projectId: project.id, groupId: card.id,
    generators: { codex: fakeGenSequence([solidPng(), solidPng()]), gemini: fakeGenSequence([new Error("agy exploded"), solidPng()]) },
  });
  if (!firstRun) return;
  assert.equal(firstRun.last_run.verdict, "partial");
  assert.deepEqual(firstRun.failed, [{ sheet_axes: { material: "stone" }, engine: "gemini", error: "agy exploded" }], "failed[] names the engine, not just the axes");

  const codex2 = fakeGenSequence([solidPng()]);
  const gemini2 = fakeGenSequence([solidPng()]);
  const resumed = await tryGenerate(t, { projectId: project.id, groupId: card.id, runGroupId: firstRun.run_group_id, generators: { codex: codex2, gemini: gemini2 } });
  if (!resumed) return;

  assert.equal(codex2.calls.length, 0, "codex sheets already landed for BOTH jobs — skipped, not repaid");
  assert.equal(gemini2.calls.length, 1, "only the missing (stone, gemini) unit regenerates");
  assert.equal(resumed.last_run.verdict, "ok");
  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.elements.filter((el) => el.groupId === firstRun.run_group_id).length, 4, "run group converges to two sheets per job");
});

test("generateFromRecipe pack branch: flipping the card's engine between run and resume generates the NEW engine's sheets beside the old ones — (axes, engine) identity, and a legacy engineless sheet counts as codex", async (t) => {
  const projectsDir = tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack engine flip resume" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id);

  const firstRun = await tryGenerate(t, { projectId: project.id, groupId: card.id, generators: { codex: fakeGenSequence([solidPng(), solidPng()]) } });
  if (!firstRun) return;

  // Simulate a LEGACY sheet (minted before engines were recorded): strip engine/job from one
  // sheet's meta.pack directly in the store — no op can produce this state anymore.
  const stored = getProject(REPO_ROOT, project.id);
  const oneSheet = stored.elements.find((el) => el.groupId === firstRun.run_group_id);
  const storeFile = join(projectsDir, project.id, "project.json");
  const raw = JSON.parse(readFileSync(storeFile, "utf8"));
  for (const el of raw.elements) {
    if (el.id === oneSheet.id) {
      delete el.meta.pack.engine;
      delete el.meta.pack.job;
    }
  }
  writeFileSync(storeFile, `${JSON.stringify(raw, null, 2)}\n`, "utf8");

  // Resume on codex: BOTH sheets (one modern, one legacy engineless) count as codex — skipped.
  const codex2 = fakeGenSequence([solidPng()]);
  const resumedSame = await tryGenerate(t, { projectId: project.id, groupId: card.id, runGroupId: firstRun.run_group_id, generators: { codex: codex2 } });
  if (!resumedSame) return;
  assert.equal(codex2.calls.length, 0, "legacy engineless sheet counts as codex — nothing repaid");

  // Flip to gemini and resume: no (axes, gemini) sheets exist -> ALL jobs generate on gemini,
  // codex sheets stay put — the cheap "agy versions side by side" gesture.
  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { engine: "gemini" } });
  const gemini2 = fakeGenSequence([solidPng(), solidPng()]);
  const resumedFlipped = await tryGenerate(t, { projectId: project.id, groupId: card.id, runGroupId: firstRun.run_group_id, generators: { gemini: gemini2 } });
  if (!resumedFlipped) return;
  assert.equal(gemini2.calls.length, 2, "every job generates on the NEW engine");
  const after = getProject(REPO_ROOT, project.id);
  const sheets = after.elements.filter((el) => el.groupId === firstRun.run_group_id);
  assert.equal(sheets.length, 4, "codex sheets survive beside the fresh gemini ones");
  assert.equal(sheets.filter((el) => el.meta.pack.engine === "gemini").length, 2);
});

test("generateFromRecipe pack branch on engine=gemini (real expand_jobs.py, fake agy): dispatches EVERY sheet to the gemini generator, never codex, and stamps meta.pack.engine per sheet", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack generate agy" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id);
  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { engine: "gemini" } });

  const codex = fakeGenSequence([solidPng()]);
  const gemini = fakeGenSequence([solidPng(), solidPng()]);
  const result = await tryGenerate(t, { projectId: project.id, groupId: card.id, generators: { codex, gemini } });
  if (!result) return;

  assert.equal(gemini.calls.length, 2, "one gemini call per sheet (2 material values)");
  assert.equal(codex.calls.length, 0, "codex never runs on a gemini card");
  assert.ok(result.results.every((r) => r.status === "ok"));

  const after = getProject(REPO_ROOT, project.id);
  const sheetElements = after.elements.filter((el) => el.groupId === result.run_group_id);
  assert.equal(sheetElements.length, 2);
  for (const el of sheetElements) {
    assert.equal(el.meta.pack.engine, "gemini", "each sheet records the engine that ACTUALLY generated it");
    assert.deepEqual(
      el.meta.pack.params_snapshot,
      { size: "1024x1024", bg_key: "#ff00ff", n_candidates: 1 },
      "engine-filtered snapshot: agy consumed only size; bg_key/n_candidates are canvas-level — NO gpt-image model/quality on a gemini sheet",
    );
  }
  const packRun = after.tool_runs.find((run) => run.op === "generate_from_recipe_pack" && run.cardId === card.id);
  assert.ok(packRun, "the one-per-run tool_runs entry landed");
  assert.equal(packRun.params.engine, "gemini", "tool_runs records the run's engine, not a hardcoded codex");
  assert.equal(packRun.params.model, undefined, "tool_runs no longer claims a hardcoded gpt-image model on a gemini run");
  assert.equal(packRun.params.size, "1024x1024", "what agy DID consume is still recorded");
});

test("generateFromRecipe pack branch: an unknown --run group id, or one belonging to a different card, is loud before any generation or python spawn", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack generate bad run" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id);
  const codex = fakeGenSequence([solidPng()]);

  await assert.rejects(
    () => generateFromRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, runGroupId: "grp_missing", generators: { codex } }),
    /run group not found or does not carry a pack_run marker/,
  );
  assert.equal(codex.calls.length, 0);

  const plain = createGroup(REPO_ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;
  await assert.rejects(
    () => generateFromRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, runGroupId: plain.id, generators: { codex } }),
    /run group not found or does not carry a pack_run marker/,
    "a plain group (no pack_run at all) is loud the same way",
  );
  assert.equal(codex.calls.length, 0);
});

test("generateFromRecipe pack branch (real expand_jobs.py, fake codex): mints a run group beside the card (name + pack_run marker), one sheet element per job, ONE journal entry PER SHEET, meta.pack with refs_snapshot including the style card's ref image", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack generate happy" });
  const styleCard = createStyleCard(REPO_ROOT, { projectId: project.id, name: "Painterly" }).group;
  patchStyle(REPO_ROOT, { projectId: project.id, groupId: styleCard.id, patch: { prompt: "painterly oil" } });
  const styleRefImage = addImage(REPO_ROOT, project.id, { name: "style-ref.png", bytes: solidPng() }).element;
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [styleRefImage.id], groupId: styleCard.id });

  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns", x: 100, y: 50, w: 300, h: 200 });
  seedPackRecipe(project.id, card.id);
  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { style_ref: styleCard.id } });

  const seqBefore = getProject(REPO_ROOT, project.id).history_seq;
  const codex = fakeGenSequence([solidPng(), solidPng()]);
  const result = await tryGenerate(t, { projectId: project.id, groupId: card.id, generators: { codex } });
  if (!result) return;

  assert.equal(codex.calls.length, 2, "one generation call per sheet (2 material values)");
  assert.equal(result.results.length, 2);
  assert.ok(result.results.every((r) => r.status === "ok"));
  assert.equal(result.failed.length, 0);
  assert.equal(result.last_run.verdict, "ok");
  assert.equal(result.last_run.run_group_id, result.run_group_id);

  const after = getProject(REPO_ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 2, "one journal entry PER SHEET, not one for the whole call");

  const runGroup = after.groups.find((g) => g.id === result.run_group_id);
  assert.ok(runGroup, "run group minted");
  assert.deepEqual(runGroup.pack_run, { v: 1, cardId: card.id, at: result.last_run.at });
  assert.equal(runGroup.name, `Painterly/grade ${result.last_run.at}`, "name = <style card name>/<vary> <ts>");

  const sheetElements = after.elements.filter((el) => el.groupId === result.run_group_id);
  assert.equal(sheetElements.length, 2);
  for (const el of sheetElements) {
    assert.ok(el.meta.pack, "meta.pack present");
    assert.equal(el.meta.pack.cardId, card.id);
    assert.ok(Array.isArray(el.meta.pack.cells) && el.meta.pack.cells.length === 2, "full cell manifest carried");
    assert.ok(el.meta.pack.refs_snapshot.includes(styleRefImage.src), "style card ref image travels in refs_snapshot (the lead's requirement)");
    assert.equal(el.meta.pack.style_snapshot.name, "Painterly");
    assert.ok(["stone", "wood"].includes(el.meta.pack.sheet_axes.material));
    assert.equal(el.meta.pack.sheet_axes.grade, undefined, "the vary axis is excluded from sheet_axes");
  }
  for (const call of codex.calls) {
    assert.ok(call.refPaths.length >= 1 && call.refPaths.every((p) => p.endsWith(".png")), "the style ref image reaches EVERY sheet's generation call");
  }
});

test("generateFromRecipe pack branch: a mid-run generator failure keeps the earlier sheet, records failed[] naming the sheet_axes, and STILL attempts the next sheet — last_run.verdict stays partial", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack generate partial" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id, ["stone", "wood", "clay"]);

  const codex = fakeGenSequence([solidPng(), new Error("codex exploded"), solidPng()]);
  const result = await tryGenerate(t, { projectId: project.id, groupId: card.id, generators: { codex } });
  if (!result) return;

  assert.equal(codex.calls.length, 3, "the run does not stop after a failure — the third sheet is still attempted");
  assert.equal(result.results.length, 3);
  assert.equal(result.results[0].status, "ok");
  assert.equal(result.results[1].status, "failed");
  assert.equal(result.results[1].error, "codex exploded");
  assert.equal(result.results[2].status, "ok");

  assert.equal(result.failed.length, 1);
  assert.deepEqual(result.failed[0].sheet_axes, { material: "wood" }, "failed[] names the sheet_axes of the failed sheet");
  assert.equal(result.last_run.verdict, "partial", "one failure -> partial, even though the run finished");
  assert.equal(result.last_run.failed.length, 1);
  assert.deepEqual(result.last_run.failed[0].sheet_axes, { material: "wood" });

  const after = getProject(REPO_ROOT, project.id);
  const sheetElements = after.elements.filter((el) => el.groupId === result.run_group_id);
  assert.equal(sheetElements.length, 2, "the first (stone) and third (clay) sheets landed; the failed second sheet minted nothing");
  const survivingMaterials = sheetElements.map((el) => el.meta.pack.sheet_axes.material).sort();
  assert.deepEqual(survivingMaterials, ["clay", "stone"]);
});

test("generateFromRecipe pack branch: --run resumes into an existing group and SKIPS sheets whose sheet_axes already landed (a killed/timed-out run is not repaid)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack generate resume" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id);

  const firstRun = await tryGenerate(t, { projectId: project.id, groupId: card.id, generators: { codex: fakeGenSequence([solidPng(), new Error("boom")]) } });
  if (!firstRun) return;
  assert.equal(firstRun.results[0].status, "ok");
  assert.equal(firstRun.results[1].status, "failed");
  assert.equal(firstRun.last_run.verdict, "partial");
  const runGroupId = firstRun.run_group_id;
  assert.ok(runGroupId, "the first (successful) sheet minted a run group even though the run finished partial");

  const codex2 = fakeGenSequence([solidPng()]);
  const resumed = await tryGenerate(t, { projectId: project.id, groupId: card.id, runGroupId, generators: { codex: codex2 } });
  if (!resumed) return;

  assert.equal(codex2.calls.length, 1, "only the missing (wood) sheet regenerates — the stone sheet is SKIPPED, not repaid");
  assert.equal(resumed.results.length, 2);
  const stoneRow = resumed.results.find((r) => r.sheet_axes.material === "stone");
  assert.equal(stoneRow.status, "skipped");
  const woodRow = resumed.results.find((r) => r.sheet_axes.material === "wood");
  assert.equal(woodRow.status, "ok");
  assert.equal(resumed.run_group_id, runGroupId, "resume lands in the SAME group — no second group minted");
  assert.equal(resumed.last_run.verdict, "ok", "every requested sheet has now landed");

  const after = getProject(REPO_ROOT, project.id);
  const sheetElements = after.elements.filter((el) => el.groupId === runGroupId);
  assert.equal(sheetElements.length, 2, "stone (from the first run) + wood (from the resume) — no duplicate stone element");
});

test("generateFromRecipe pack branch: --sheet REPLACES the old sheet (and its slice subgroup) in the SAME commit — a promoted copy of a cut survives, --sheet resolves the run from last_run without --run (deep-review поправка)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack generate replace" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id);

  // Real magenta-sheet bytes (not solidPng) — distinct tags so the two sheets never collide
  // in the content-addressed store — so the freshly-minted stone/wood sheets carry REAL
  // detectable regions: this test slices them for real (packSlice) to prove the forced regen
  // also removes the old sheet's slice SUBGROUP, not just the sheet element itself.
  const firstRun = await tryGenerate(t, {
    projectId: project.id,
    groupId: card.id,
    generators: { codex: fakeGenSequence([magentaSheetPng(1), magentaSheetPng(2)]) },
  });
  if (!firstRun) return;
  const runGroupId = firstRun.run_group_id;
  const stoneRow = firstRun.results.find((r) => r.sheet_axes.material === "stone");
  const woodRow = firstRun.results.find((r) => r.sheet_axes.material === "wood");
  const stoneJobName = stoneRow.name;
  const oldStoneSheetId = stoneRow.elementId;
  const oldWoodSheetId = woodRow.elementId;

  const sliced = await tryPackSlice(t, { projectId: project.id, groupId: card.id, runGroupId });
  if (!sliced) return;
  const stoneCutIds = sliced.contract.find((row) => row.sheet_element_id === oldStoneSheetId).cut_ids;
  const woodCutIds = sliced.contract.find((row) => row.sheet_element_id === oldWoodSheetId).cut_ids;
  assert.equal(stoneCutIds.length, 2, "precondition: the stone sheet's two magenta blobs both sliced");
  assert.equal(woodCutIds.length, 2, "precondition: the wood sheet's two magenta blobs both sliced");

  const beforeForce = getProject(REPO_ROOT, project.id);
  const stoneSliceGroupId = beforeForce.elements.find((el) => el.id === stoneCutIds[0]).groupId;
  assert.ok(stoneSliceGroupId && stoneSliceGroupId !== runGroupId, "precondition: the stone cuts landed in their own wrapper slices-group");
  assert.equal(beforeForce.groups.find((g) => g.id === stoneSliceGroupId).parentId, runGroupId);

  // Promote a COPY of one stone cut OUTSIDE the run group (root scope) before forcing the
  // regen — build-spec: a copy already promoted outside the run group is never touched.
  const promotedId = duplicateNodes(REPO_ROOT, { projectId: project.id, nodeIds: [stoneCutIds[0]], scopeId: null }).elementIds[0];

  // Force-regen the stone sheet — NO --run passed (FIX 2: resolves recipe.last_run.run_group_id).
  const codex2 = fakeGenSequence([solidPng()]);
  const forced = await tryGenerate(t, { projectId: project.id, groupId: card.id, sheetSlug: stoneJobName, generators: { codex: codex2 } });
  if (!forced) return;

  assert.equal(codex2.calls.length, 1, "the forced sheet regenerates even though its axes already existed in the group");
  assert.equal(forced.results.length, 1);
  assert.equal(forced.results[0].status, "ok");
  assert.equal(forced.run_group_id, runGroupId, "--sheet alone resolved the SAME group via last_run — no second group minted");

  const after = getProject(REPO_ROOT, project.id);
  const stoneSheets = after.elements.filter(
    (el) => el.groupId === runGroupId && el.meta.pack && Array.isArray(el.meta.pack.cells) && el.meta.pack.sheet_axes.material === "stone",
  );
  assert.equal(stoneSheets.length, 1, "REPLACE, not duplicate — exactly one stone sheet remains in the run group");
  assert.notEqual(stoneSheets[0].id, oldStoneSheetId, "the surviving stone sheet is the freshly forced one");
  assert.ok(!after.elements.some((el) => el.id === oldStoneSheetId), "the OLD stone sheet element is gone");
  for (const cutId of stoneCutIds) {
    assert.ok(!after.elements.some((el) => el.id === cutId), "the old stone sheet's own cuts are gone");
  }
  assert.ok(!after.groups.some((g) => g.id === stoneSliceGroupId), "the old stone sheet's slice subgroup is gone");

  const promotedEl = after.elements.find((el) => el.id === promotedId);
  assert.ok(promotedEl, "a copy of a cut promoted OUTSIDE the run group survives the replace");
  assert.notEqual(promotedEl.groupId, runGroupId, "the surviving copy is still outside the run group");

  // The wood sheet + its own cuts/subgroup are completely untouched by the stone replace.
  assert.ok(after.elements.some((el) => el.id === oldWoodSheetId), "the wood sheet is untouched");
  for (const cutId of woodCutIds) {
    assert.ok(after.elements.some((el) => el.id === cutId), "the wood sheet's cuts are untouched");
  }
});

test("generateFromRecipe pack branch: --sheet with no prior pack run at all is loud before any generation call — no silent new-group fork", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack generate force no run" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id);
  const codex = fakeGenSequence([solidPng()]);

  let error;
  try {
    await generateFromRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, sheetSlug: "whatever", generators: { codex } });
  } catch (err) {
    error = err;
  }
  if (error && /venv|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
    t.skip(`pack generate pipeline unavailable: ${error.message}`);
    return;
  }
  assert.ok(error, "expected generateFromRecipe to reject");
  assert.match(error.message, /--sheet requires an existing pack run/);
  assert.equal(codex.calls.length, 0, "no generation, no python spawn either");
});

test("generateFromRecipe pack branch: an unknown --sheet slug is loud, no generation call", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack generate bad sheet" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Lanterns" });
  seedPackRecipe(project.id, card.id);

  // An unknown slug is only meaningfully distinguishable from "--sheet with no run" once a
  // run actually exists (FIX 2's own gate would otherwise fire first) — establish one.
  const firstRun = await tryGenerate(t, { projectId: project.id, groupId: card.id, generators: { codex: fakeGenSequence([solidPng(), solidPng()]) } });
  if (!firstRun) return;

  const codex = fakeGenSequence([solidPng()]);
  let error;
  try {
    await generateFromRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, sheetSlug: "no-such-sheet", generators: { codex } });
  } catch (err) {
    error = err;
  }
  if (error && /venv|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
    t.skip(`pack generate pipeline unavailable: ${error.message}`);
    return;
  }
  assert.ok(error, "expected generateFromRecipe to reject");
  assert.match(error.message, /does not match any expanded job name/);
  assert.equal(codex.calls.length, 0, "the expander ran (to check the slug) but no sheet was ever generated");
});

test("generateFromRecipe: the single-image branch (recipe.pack === null) is UNCHANGED by this increment — pack-only opts are simply ignored", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Single path non-regression" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Hero card", x: 100, y: 50, w: 300, h: 200 });
  patchRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a red fox" } });
  assert.equal(card.recipe.pack, null, "precondition: pack mode is off");

  const codex = fakeGenSequence([solidPng(10, 8, [200, 40, 40])]);
  // Pass pack-only opts anyway — the single-image branch must never read them.
  const result = await generateFromRecipe(REPO_ROOT, { projectId: project.id, groupId: card.id, runGroupId: "grp_bogus", sheetSlug: "bogus", generators: { codex } });

  assert.equal(codex.calls.length, 1);
  assert.equal(result.elements.length, 1, "single-image shape: `elements`, not `results`/`run_group_id`");
  assert.equal(result.elements[0].name, "Hero card codex");
  assert.equal(result.group.recipe.last_run.result_element_id, result.elements[0].id, "single-image last_run shape unchanged (result_element_id, not run_group_id)");
  assert.equal(result.run_group_id, undefined, "the pack branch's own return fields are absent from the single-image result");
});

// ================================================================================
// packSlice (T0332 B3, build-spec §4): detectRegions -> hard gate region_count ===
// cells.length -> sliceRegions per sheet, minimal per-cut meta, reparented into the run
// group. detectRegions/sliceRegions are REAL (region-detector + crop_regions.py — same
// skip-without-venv precedent as ops.test.mjs's own sliceRegions tests); no fake seam,
// since neither is a paid codex/agy call. Fixtures are built directly (createRecipeCard +
// a hand-built run group + sheet elements carrying meta.pack) rather than through the full
// generateFromRecipe pack branch, so these tests do not ALSO depend on the expander
// pipeline — packSlice itself is what is under test here.
// ================================================================================

// A hand-built pack run: a recipe card + a run group carrying pack_run, with one sheet
// element per entry in `sheets` ({ bytesTag, cells }). Mirrors commitPackSheetOutcome's
// own shape (meta.pack = {cardId, at, sheet_axes, cells, ...}) closely enough for
// packSlice to resolve everything it needs, without spawning the real expander/codex.
// `bytesTag` feeds magentaSheetPng(tag) so sibling sheets never collide in the
// content-addressed file store (identical pixels would dedup to ONE file, and corrupting
// one sheet's bytes for a MISSING-path fixture would then corrupt every sibling sharing
// that file too — a real trap this suite fell into once during development).
function seedPackRun(projectId, cardId, sheets) {
  const at = new Date().toISOString();
  const before = getProject(REPO_ROOT, projectId);
  const runGroup = {
    id: `grp_run_${Math.random().toString(36).slice(2, 8)}`,
    name: "Run",
    x: 0,
    y: 0,
    w: 100,
    h: 100,
    visible: true,
    pack_run: { v: 1, cardId, at },
  };
  updateProject(REPO_ROOT, projectId, { groups: [...(before.groups || []), runGroup] });

  const sheetIds = [];
  for (const sheet of sheets) {
    const img = addImage(REPO_ROOT, projectId, { name: sheet.name, bytes: magentaSheetPng(sheet.bytesTag) }).element;
    updateProject(REPO_ROOT, projectId, {
      elements: getProject(REPO_ROOT, projectId).elements.map((el) =>
        el.id === img.id
          ? {
              ...el,
              groupId: runGroup.id,
              meta: {
                pack: {
                  cardId,
                  at,
                  sheet_axes: sheet.sheet_axes || {},
                  cells: sheet.cells,
                  prompt_snapshot: "a thing",
                  refs_snapshot: [],
                  params_snapshot: {},
                },
              },
            }
          : el,
      ),
    });
    sheetIds.push(img.id);
  }
  return { runGroup, sheetIds };
}

// Try one detectRegions call (used ONLY to learn the detector's own region order/geometry
// for the row-major-zip proof below, a throwaway probe upload — never wired into a pack
// run itself); returns null on a venv/interpreter miss (the caller then t.skip's).
async function tryDetectForOrder(t, projectId, elementId) {
  try {
    return await detectRegions(REPO_ROOT, { projectId, elementId });
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`region detector pipeline unavailable: ${error.message}`);
      return null;
    }
    throw error;
  }
}

// Try one packSlice call; return the result or null on a venv/interpreter miss (the
// caller then t.skip's) — mirrors tryPreview/tryGenerate above.
async function tryPackSlice(t, args) {
  try {
    return await packSlice(REPO_ROOT, args);
  } catch (error) {
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(error.message)) {
      t.skip(`region detector/crop pipeline unavailable: ${error.message}`);
      return null;
    }
    throw error;
  }
}

test("packSlice OK path: region_count === cells.length -> sliced, minimal meta.pack per cut, row-major zip verified by pixel, reparented into the run group", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack slice OK" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Card" });

  // Learn the detector's OWN region order/geometry FIRST (a throwaway upload, same bytes),
  // so `cells` is built in the SAME row-major order detectRegions will actually return —
  // proving the zip is correct empirically, not by assuming which blob comes "first".
  // magentaSheetPng's two blobs have DIFFERENT TIGHT heights (red 20, green 24 —
  // region.content_bbox, not the padded region.rect sliceRegions actually crops from), so
  // content_bbox height alone identifies which source blob a region came from.
  const probe = addImage(REPO_ROOT, project.id, { name: "probe.png", bytes: magentaSheetPng(1) }).element;
  const detected = await tryDetectForOrder(t, project.id, probe.id);
  if (!detected) return;
  assert.equal(detected.regions.length, 2, "precondition: magentaSheetPng's two blobs both detect");
  const identityOf = (region) => (region.content_bbox[3] >= 22 ? "green" : "red"); // content_bbox = [x,y,w,h], tight
  const cells = detected.regions.map((region, index) => ({ cell: [0, index], axes: { grade: identityOf(region) } }));
  assert.deepEqual(cells.map((c) => c.axes.grade).sort(), ["green", "red"], "sanity: one of each");

  const { runGroup, sheetIds } = seedPackRun(project.id, card.id, [{ name: "sheet-ok.png", bytesTag: 1, cells }]);

  const result = await tryPackSlice(t, { projectId: project.id, groupId: card.id, runGroupId: runGroup.id });
  if (!result) return;

  assert.equal(result.run_group_id, runGroup.id);
  assert.equal(result.contract.length, 1);
  const row = result.contract[0];
  assert.equal(row.sheet_element_id, sheetIds[0]);
  assert.equal(row.verdict, "OK");
  assert.equal(row.region_count, 2);
  assert.equal(row.cells_len, 2);
  assert.equal(row.cut_ids.length, 2);

  const after = getProject(REPO_ROOT, project.id);
  const cuts = row.cut_ids.map((id) => after.elements.find((el) => el.id === id));
  for (const cut of cuts) {
    assert.deepEqual(Object.keys(cut.meta.pack).sort(), ["axes", "cardId", "cell", "sheet_element_id"], "minimal per-cut meta — no prompt/manifest duplication");
    assert.equal(cut.meta.pack.cardId, card.id);
    assert.equal(cut.meta.pack.sheet_element_id, sheetIds[0]);
    // meta.parent (sliceRegions' own provenance) survives ALONGSIDE meta.pack — additive.
    assert.equal(cut.meta.parent.elementId, sheetIds[0]);
  }

  // Row-major zip verified by PIXEL: whichever cut carries axes.grade "red" must be the
  // red blob's own pixels (and "green" the green blob's) — proving cells[i] (built above
  // from the detector's OWN order) lines up with created[i] (this specific cut), not just
  // with SOME cut.
  const cutRed = cuts.find((c) => c.meta.pack.axes.grade === "red");
  const cutGreen = cuts.find((c) => c.meta.pack.axes.grade === "green");
  assert.ok(cutRed && cutGreen, "one cut of each grade");
  const pngRed = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, cutRed.src)));
  const pngGreen = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, cutGreen.src)));
  // sliceRegions crops the (slightly padded) region.rect, not the tight content_bbox used
  // for identityOf above, so sample each crop's OWN center rather than assuming an exact
  // pixel size — the blob is centered within its own detected rect either way.
  const centerRed = pngRed.at(Math.floor(pngRed.width / 2), Math.floor(pngRed.height / 2)).slice(0, 3);
  const centerGreen = pngGreen.at(Math.floor(pngGreen.width / 2), Math.floor(pngGreen.height / 2)).slice(0, 3);
  assert.deepEqual(centerRed, [220, 40, 41], "'red'-labeled cut's own pixels are the red blob (tag=1 -> blue channel 41)");
  assert.deepEqual(centerGreen, [40, 180, 60], "'green'-labeled cut's own pixels are the green blob");

  // Reparented: the fresh "<sheet> slices" group nests under the run group.
  const sliceGroup = after.groups.find((g) => g.id === cuts[0].groupId);
  assert.equal(sliceGroup.parentId, runGroup.id);

  // Journal: one detect + one slice landed for this sheet (build-spec: "один detect + один
  // slice на лист").
  const journalOps = readHistory(REPO_ROOT, { projectId: project.id }).entries.map((line) => line.op);
  assert.ok(journalOps.includes("detectRegions"));
  assert.ok(journalOps.includes("slice"));
});

test("packSlice REJECT path: region count mismatch rejects that sheet (got/expected in the contract), siblings still slice", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack slice reject" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Card" });
  const okCells = [{ cell: [0, 0], axes: { grade: "a" } }, { cell: [0, 1], axes: { grade: "b" } }];
  // 3 expected cells but magentaSheetPng only ever has 2 detectable blobs -> mismatch.
  const rejectCells = [{ cell: [0, 0], axes: {} }, { cell: [0, 1], axes: {} }, { cell: [0, 2], axes: {} }];
  const { runGroup, sheetIds } = seedPackRun(project.id, card.id, [
    { name: "sheet-ok.png", bytesTag: 1, cells: okCells },
    { name: "sheet-reject.png", bytesTag: 2, cells: rejectCells },
  ]);

  const result = await tryPackSlice(t, { projectId: project.id, groupId: card.id, runGroupId: runGroup.id });
  if (!result) return;

  assert.equal(result.contract.length, 2, "both sheets get a verdict — the mismatch does not stop the run");
  const okRow = result.contract.find((r) => r.sheet_element_id === sheetIds[0]);
  const rejectRow = result.contract.find((r) => r.sheet_element_id === sheetIds[1]);
  assert.equal(okRow.verdict, "OK");
  assert.equal(okRow.cut_ids.length, 2, "the sibling sheet still slices despite the other's rejection");
  assert.equal(rejectRow.verdict, "REJECT");
  assert.equal(rejectRow.region_count, 2, "got — the build-spec's own got/expected pair");
  assert.equal(rejectRow.cells_len, 3, "expected");
  assert.deepEqual(rejectRow.cut_ids, [], "a rejected sheet mints no cuts");

  // The rejected sheet's detectRegions still ran (it populated region_count) and is
  // journaled — only the slice half is skipped.
  const after = getProject(REPO_ROOT, project.id);
  const rejectedSheet = after.elements.find((el) => el.id === sheetIds[1]);
  assert.ok(Array.isArray(rejectedSheet.regions) && rejectedSheet.regions.length === 2, "detection persisted regions on the rejected sheet");
});

test("packSlice MISSING path: a sheet whose image cannot even be read (corrupt bytes) lands as MISSING, siblings unaffected", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack slice missing" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Card" });
  const okCells = [{ cell: [0, 0], axes: { grade: "a" } }, { cell: [0, 1], axes: { grade: "b" } }];
  const missingCells = [{ cell: [0, 0], axes: {} }];
  const { runGroup, sheetIds } = seedPackRun(project.id, card.id, [
    { name: "sheet-ok.png", bytesTag: 1, cells: okCells },
    { name: "sheet-missing.png", bytesTag: 2, cells: missingCells },
  ]);
  // Corrupt ONLY the second sheet's own file (distinct content-addressed path, tag=2 —
  // never shared with the first sheet's tag=1 file) so imageSize() throws before any
  // python spawn — a MISSING verdict that does not depend on the studio venv at all.
  const missingSheet = getProject(REPO_ROOT, project.id).elements.find((el) => el.id === sheetIds[1]);
  writeFileSync(resolveProjectFile(REPO_ROOT, project.id, missingSheet.src), Buffer.from("not a png"));

  const result = await tryPackSlice(t, { projectId: project.id, groupId: card.id, runGroupId: runGroup.id });
  if (!result) return;

  const okRow = result.contract.find((r) => r.sheet_element_id === sheetIds[0]);
  const missingRow = result.contract.find((r) => r.sheet_element_id === sheetIds[1]);
  assert.equal(okRow.verdict, "OK", "the sibling sheet is unaffected by the corrupt one");
  assert.equal(okRow.cut_ids.length, 2);
  assert.equal(missingRow.verdict, "MISSING");
  assert.equal(missingRow.region_count, 0);
  assert.equal(missingRow.cells_len, 1);
  assert.deepEqual(missingRow.cut_ids, []);
});

test("packSlice: --run selector validation — an unknown run group, one belonging to a different card, and no run group at all (no --run, no last_run) are all loud", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack slice run selector" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Card" });
  const otherCard = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Other" }).group;

  // No --run and no last_run.run_group_id at all.
  await assert.rejects(
    () => packSlice(REPO_ROOT, { projectId: project.id, groupId: card.id }),
    /has no last_run\.run_group_id/,
  );

  // An unknown --run id.
  await assert.rejects(
    () => packSlice(REPO_ROOT, { projectId: project.id, groupId: card.id, runGroupId: "grp_missing" }),
    /run group not found or does not carry a pack_run marker/,
  );

  // A plain group (no pack_run at all) passed as --run.
  const plain = createGroup(REPO_ROOT, { projectId: project.id, name: "Plain", x: 0, y: 0, w: 10, h: 10 }).group;
  await assert.rejects(
    () => packSlice(REPO_ROOT, { projectId: project.id, groupId: card.id, runGroupId: plain.id }),
    /run group not found or does not carry a pack_run marker/,
  );

  // A run group that DOES carry pack_run, but for a DIFFERENT card.
  const { runGroup: otherRunGroup } = seedPackRun(project.id, otherCard.id, [{ name: "s.png", bytesTag: 1, cells: [{ cell: [0, 0], axes: {} }] }]);
  await assert.rejects(
    () => packSlice(REPO_ROOT, { projectId: project.id, groupId: card.id, runGroupId: otherRunGroup.id }),
    /belongs to a different recipe card/,
  );

  // Not a recipe card at all.
  await assert.rejects(
    () => packSlice(REPO_ROOT, { projectId: project.id, groupId: plain.id }),
    /not a recipe card/,
  );

  // A run group with pack_run for the RIGHT card, but no qualifying sheet elements inside it.
  const emptyRunGroup = createGroup(REPO_ROOT, { projectId: project.id, name: "EmptyRun", x: 0, y: 0, w: 10, h: 10 }).group;
  updateProject(REPO_ROOT, project.id, {
    groups: getProject(REPO_ROOT, project.id).groups.map((g) => (g.id === emptyRunGroup.id ? { ...g, pack_run: { v: 1, cardId: card.id, at: "x" } } : g)),
  });
  await assert.rejects(
    () => packSlice(REPO_ROOT, { projectId: project.id, groupId: card.id, runGroupId: emptyRunGroup.id }),
    /has no sheet elements \(meta\.pack with a cells manifest\) to slice/,
  );
});

test("CLI recipe-pack-slice: prints one name/verdict/got-expected line per sheet + the final JSON contract; --run selects an explicit group", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Pack slice CLI" });
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId: project.id, name: "Card" });
  const cells = [{ cell: [0, 0], axes: { grade: "a" } }, { cell: [0, 1], axes: { grade: "b" } }];
  const { runGroup } = seedPackRun(project.id, card.id, [{ name: "sheet-ok.png", bytesTag: 1, cells }]);

  let stdout;
  try {
    stdout = execFileSync(process.execPath, [CLI, "recipe-pack-slice", project.id, "--group", card.id, "--run", runGroup.id], { encoding: "utf8" });
  } catch (error) {
    const message = String((error && error.stderr) || (error && error.message) || "");
    if (/venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i.test(message)) {
      t.skip(`region detector/crop pipeline unavailable: ${message}`);
      return;
    }
    throw error;
  }
  const lines = stdout.trim().split("\n").filter(Boolean);
  const printedLine = lines.find((line) => line.includes("OK ("));
  assert.ok(printedLine, `expected an "OK (got/expected)" line, got: ${stdout}`);
  assert.match(printedLine, /sheet-ok\.png: OK \(2\/2\)/);

  const jsonLine = lines.at(-1);
  const parsed = JSON.parse(jsonLine);
  assert.equal(parsed.run_group_id, runGroup.id);
  assert.equal(parsed.contract.length, 1);
  assert.equal(parsed.contract[0].verdict, "OK");
});
