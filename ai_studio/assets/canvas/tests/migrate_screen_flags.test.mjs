// Migration tests for the T0332 B1 export opt-in inversion (build-spec "ЭКСПОРТ —
// ИНВЕРСИЯ НА OPT-IN"). Two surfaces: `ops.migrateScreenFlags` (the per-project op that
// does the actual flagging, one journal entry) and the standalone one-shot script
// tools/migrate_screen_flags.mjs (a thin lister/summarizer over that op — dry-run by
// default, --apply to write). NEVER run the script against the real canvasProjectsRoot
// from a test; every case here drives a tmp CANVAS_PROJECTS_ROOT fixture, same as every
// other *.test.mjs file.
// Run: node --test ai_studio/assets/canvas/tests/migrate_screen_flags.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { createGroup, createProject, createRecipeCard, createStyleCard, getProject, migrateScreenFlags, undoOp, updateProject } from "../ops.mjs";

const ROOT = "C:/unused-repo-root";
const MIGRATE_SCRIPT = fileURLToPath(new URL("../tools/migrate_screen_flags.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-migrate-screen-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// A mixed project per the build-spec's own migration test case: a plain top-level visible
// group (should be flagged), a recipe card + a style card + a pack_run "run" group (all
// carry recipe/style/pack_run — should be left untouched), a nested plain group (parentId
// set — never a top-level candidate), and a hidden top-level plain group (visible:false —
// the old filter excluded it too).
function seedMixedProject(root) {
  const project = createProject(root, { title: "Mixed" });
  const plain = createGroup(root, { projectId: project.id, name: "Plain screen", x: 0, y: 0, w: 20, h: 20 }).group;
  const { group: recipe } = createRecipeCard(root, { projectId: project.id, name: "Recipe card" });
  const { group: style } = createStyleCard(root, { projectId: project.id, name: "Style card" });
  const nested = createGroup(root, { projectId: project.id, name: "Nested", x: 0, y: 0, w: 10, h: 10, parentId: plain.id }).group;
  const hidden = createGroup(root, { projectId: project.id, name: "Hidden", x: 0, y: 0, w: 10, h: 10 }).group;
  updateProject(root, project.id, {
    groups: getProject(root, project.id).groups.map((g) => (g.id === hidden.id ? { ...g, visible: false } : g)),
  });
  // A pack_run "run" group — top-level, visible, but carries pack_run (never auto-flagged;
  // see migrateScreenFlags' own doc for why this is deliberate, not an accidental gap).
  const before = getProject(root, project.id);
  const runGroup = { id: "grp_run_mixed", name: "Run", x: 0, y: 0, w: 10, h: 10, visible: true, pack_run: { v: 1, cardId: recipe.id, at: "x" } };
  updateProject(root, project.id, { groups: [...(before.groups || []), runGroup] });
  return { projectId: project.id, plain, recipe, style, nested, hidden, runGroup };
}

// ================================================================================
// ops.migrateScreenFlags
// ================================================================================

test("migrateScreenFlags: plain top group -> flagged; recipe/style/pack_run groups -> untouched; nested/hidden -> untouched", (t) => {
  tempProjects(t);
  const { projectId, plain, recipe, style, nested, hidden, runGroup } = seedMixedProject(ROOT);

  const { flagged } = migrateScreenFlags(ROOT, { projectId });
  assert.deepEqual(flagged, [plain.id], "only the plain top-level visible group is flagged");

  const after = getProject(ROOT, projectId);
  const byId = Object.fromEntries(after.groups.map((g) => [g.id, g]));
  assert.equal(byId[plain.id].screen, true);
  assert.equal(byId[recipe.id].screen, undefined, "recipe card untouched");
  assert.equal(byId[style.id].screen, undefined, "style card untouched");
  assert.equal(byId[runGroup.id].screen, undefined, "pack_run group untouched (provenance-only, never a screen)");
  assert.equal(byId[nested.id].screen, undefined, "nested group untouched (never a top-level candidate)");
  assert.equal(byId[hidden.id].screen, undefined, "hidden top-level group untouched");
});

test("migrateScreenFlags: ONE journal entry per project even with multiple flagged groups; undo restores byte-exact", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Multi flag" });
  const g1 = createGroup(ROOT, { projectId: project.id, name: "A", x: 0, y: 0, w: 10, h: 10 }).group;
  const g2 = createGroup(ROOT, { projectId: project.id, name: "B", x: 0, y: 0, w: 10, h: 10 }).group;
  const before = getProject(ROOT, project.id);
  const seqBefore = Number(before.history_seq);

  const { flagged } = migrateScreenFlags(ROOT, { projectId: project.id });
  assert.equal(flagged.length, 2);
  const after = getProject(ROOT, project.id);
  assert.equal(Number(after.history_seq), seqBefore + 1, "exactly one journal entry for the whole project, not one per group");

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(JSON.stringify(undone.groups), JSON.stringify(before.groups), "undo restores byte-exact");
});

test("migrateScreenFlags: idempotent — a second run flags nothing further, no journal entry; a group already carrying screen:false (an explicit opt-out) is left alone too", (t) => {
  tempProjects(t);
  const { projectId, plain } = seedMixedProject(ROOT);

  migrateScreenFlags(ROOT, { projectId });
  const seqAfterFirst = Number(getProject(ROOT, projectId).history_seq);

  const second = migrateScreenFlags(ROOT, { projectId });
  assert.deepEqual(second.flagged, [], "re-running the migration flags nothing further");
  assert.equal(Number(getProject(ROOT, projectId).history_seq), seqAfterFirst, "no journal entry for a no-op migration");

  // A hand-edited project where a top-level plain group was explicitly opted OUT
  // (screen:false stored — simulating a rare pre-existing hand edit, since patchGroup
  // itself never stores a literal false) must not be silently re-flagged by a later run.
  const opted = createGroup(ROOT, { projectId, name: "Opted out", x: 0, y: 0, w: 10, h: 10 }).group;
  updateProject(ROOT, projectId, {
    groups: getProject(ROOT, projectId).groups.map((g) => (g.id === opted.id ? { ...g, screen: false } : g)),
  });
  const third = migrateScreenFlags(ROOT, { projectId });
  assert.ok(!third.flagged.includes(opted.id), "a group already carrying an explicit screen key (even false) is left alone");
});

test("migrateScreenFlags: a project with zero qualifying groups is a clean no-op (no journal entry)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Empty" });
  const seqBefore = Number(getProject(ROOT, project.id).history_seq);
  const { flagged } = migrateScreenFlags(ROOT, { projectId: project.id });
  assert.deepEqual(flagged, []);
  assert.equal(Number(getProject(ROOT, project.id).history_seq), seqBefore);
});

test("migrateScreenFlags requires projectId", (t) => {
  tempProjects(t);
  assert.throws(() => migrateScreenFlags(ROOT, {}), /requires projectId/);
});

// ================================================================================
// tools/migrate_screen_flags.mjs (the standalone one-shot script) — dry-run default,
// --apply to write. Spawned for real (it is a standalone entry point, not a pure module),
// against a tmp CANVAS_PROJECTS_ROOT fixture only.
// ================================================================================

function runScript(env, ...args) {
  return execFileSync(process.execPath, [MIGRATE_SCRIPT, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
}

test("migrate_screen_flags.mjs: dry-run (default) prints the plan but writes NOTHING; --apply writes screen:true and is idempotent on a second run", (t) => {
  const dir = tempProjects(t); // sets process.env.CANVAS_PROJECTS_ROOT for the fixture calls below
  const env = { CANVAS_PROJECTS_ROOT: dir }; // the SAME dir, passed to the child script process

  const { projectId, plain, recipe } = seedMixedProject(ROOT);

  const dryRunOut = runScript(env);
  assert.match(dryRunOut, /DRY-RUN/);
  assert.match(dryRunOut, /would flag 1 group/);
  assert.match(dryRunOut, /dry-run only - pass --apply to write/);
  // No write happened: the plain group is still unflagged.
  assert.equal(getProject(ROOT, projectId).groups.find((g) => g.id === plain.id).screen, undefined, "dry-run writes nothing");

  const applyOut = runScript(env, "--apply");
  assert.match(applyOut, /APPLY/);
  assert.match(applyOut, /flagged 1 group/);
  assert.equal(getProject(ROOT, projectId).groups.find((g) => g.id === plain.id).screen, true, "--apply actually writes screen:true");
  assert.equal(getProject(ROOT, projectId).groups.find((g) => g.id === recipe.id).screen, undefined, "the recipe card is still untouched");

  // Idempotent: a second --apply run touches nothing further.
  const secondApplyOut = runScript(env, "--apply");
  assert.match(secondApplyOut, /flagged 0 group\(s\) across 0\/1 project\(s\)/);
});

test("migrate_screen_flags.mjs: a project with nothing to flag is silently skipped (not listed, zero touched)", (t) => {
  const dir = tempProjects(t);
  const env = { CANVAS_PROJECTS_ROOT: dir };

  createProject(ROOT, { title: "Nothing to do" });
  const out = runScript(env, "--apply");
  assert.match(out, /flagged 0 group\(s\) across 0\/1 project\(s\)/);
});
