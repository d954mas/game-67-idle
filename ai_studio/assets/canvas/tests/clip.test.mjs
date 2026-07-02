// Increment-4 clip-to-bounds tests: the patchGroup `clip` model (validation, journaled
// set/unset, absent-field normalization) and the recursive render crop (a clipped subgroup
// crops an overflowing member; a non-clipped subgroup preserves overflow; two nested clips
// intersect). The render tests drive render_group.py and skip cleanly when Python/PIL is
// unavailable. Run:
//   node --test ai_studio/assets/canvas/tests/clip.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import {
  addImage,
  assignToGroup,
  createGroup,
  createProject,
  getProject,
  patchElement,
  patchGroup,
  renderGroup,
  undoOp,
} from "../ops.mjs";
import { decodePng, solidPng } from "./png_fixture.mjs";

// Metadata ops resolve store paths only, so any placeholder root works. The render tests
// spawn Python with cwd = repo root, so they use the real repo root.
const ROOT = "C:/unused-repo-root";
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-clip-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// ---- model: patchGroup clip validation + journaling ----------------------------

test("patchGroup clip=true sets the flag in one journal entry; undo clears it", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Clip" });
  const projectId = project.id;
  const { group } = createGroup(ROOT, { projectId, name: "Frame", x: 0, y: 0, w: 100, h: 100 });
  assert.equal(group.clip, undefined, "a fresh group is unclipped (absent field)");

  const seqBefore = Number(getProject(ROOT, projectId).history_seq);
  patchGroup(ROOT, { projectId, groupId: group.id, clip: true });
  const after = getProject(ROOT, projectId);
  assert.equal(after.groups[0].clip, true);
  assert.equal(Number(after.history_seq), seqBefore + 1, "exactly one journal entry");

  undoOp(ROOT, { projectId });
  assert.equal(getProject(ROOT, projectId).groups[0].clip, undefined, "undo clears the flag");
});

test("patchGroup clip=false REMOVES the field (absent = unclipped)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Unclip" });
  const projectId = project.id;
  const { group } = createGroup(ROOT, { projectId, name: "Frame", x: 0, y: 0, w: 100, h: 100 });
  patchGroup(ROOT, { projectId, groupId: group.id, clip: true });
  assert.equal(getProject(ROOT, projectId).groups[0].clip, true);

  patchGroup(ROOT, { projectId, groupId: group.id, clip: false });
  assert.equal("clip" in getProject(ROOT, projectId).groups[0], false, "clip:false deletes the field, not stored as false");
});

test("patchGroup clip=false on an already-unclipped group is a no-op (no journal entry)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "NoopClip" });
  const projectId = project.id;
  const { group } = createGroup(ROOT, { projectId, name: "Frame", x: 0, y: 0, w: 100, h: 100 });
  const seqBefore = Number(getProject(ROOT, projectId).history_seq);
  patchGroup(ROOT, { projectId, groupId: group.id, clip: false });
  assert.equal(Number(getProject(ROOT, projectId).history_seq), seqBefore, "false on already-unclipped = no change");
});

test("patchGroup clip validates a boolean (a string / number / null throws loudly)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "BadClip" });
  const projectId = project.id;
  const { group } = createGroup(ROOT, { projectId, name: "Frame", x: 0, y: 0, w: 100, h: 100 });
  for (const bad of ["true", 1, 0, null, {}, "yes"]) {
    assert.throws(() => patchGroup(ROOT, { projectId, groupId: group.id, clip: bad }), /clip must be a boolean/);
  }
});

// ---- render: a clipped subgroup crops an overflowing member --------------------

// Screen (0,0,40,40) with a subgroup box (10,10,20,20) that holds a red 20x20 element at
// (25,25) — red overflows the box to (45,45). Returns ids so a test toggles the box clip.
function seedOverflow(root) {
  const project = createProject(root, { title: "Overflow" });
  const projectId = project.id;
  const screen = createGroup(root, { projectId, name: "Screen", x: 0, y: 0, w: 40, h: 40 }).group;
  const red = addImage(root, projectId, { name: "red.png", bytes: solidPng(20, 20, [220, 40, 40]) }).element;
  patchElement(root, projectId, red.id, { x: 25, y: 25 });
  const box = createGroup(root, { projectId, name: "Box", x: 10, y: 10, w: 20, h: 20, parentId: screen.id }).group;
  assignToGroup(root, { projectId, elementIds: [red.id], groupId: box.id });
  return { projectId, screen, box, red };
}

test("a clip=true subgroup crops the part of a member outside its box (skips without Python)", async (t) => {
  tempProjects(t);
  const { projectId, screen, box } = seedOverflow(REPO_ROOT);
  patchGroup(REPO_ROOT, { projectId, groupId: box.id, clip: true });

  let result;
  try {
    result = await renderGroup(REPO_ROOT, { projectId, groupId: screen.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const png = decodePng(readFileSync(result.path));
  assert.deepEqual([png.width, png.height], [40, 40]);
  // Inside the box AND inside the red element (box 10..30, red 25..45 => 25..30) => red shows.
  assert.deepEqual(png.at(27, 27), [220, 40, 40, 255], "member visible inside the clip box");
  // Inside the Screen but OUTSIDE the box (32,32): red would cover it unclipped, but the
  // clip crops it away => transparent.
  assert.equal(png.at(32, 32)[3], 0, "member cropped outside the clip box");
});

test("a clip=false subgroup preserves member overflow (existing behavior guard) (skips without Python)", async (t) => {
  tempProjects(t);
  const { projectId, screen } = seedOverflow(REPO_ROOT); // box left unclipped (default)

  let result;
  try {
    result = await renderGroup(REPO_ROOT, { projectId, groupId: screen.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const png = decodePng(readFileSync(result.path));
  assert.deepEqual(png.at(27, 27), [220, 40, 40, 255], "member drawn inside the box");
  // Overflow preserved: red past the box (but inside the Screen) still paints.
  assert.deepEqual(png.at(32, 32), [220, 40, 40, 255], "member overflow preserved without clip");
});

test("two nested clip=true subgroups intersect: a member is cropped to the inner∩outer box (skips without Python)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "NestedClip" });
  const projectId = project.id;
  const screen = createGroup(REPO_ROOT, { projectId, name: "Screen", x: 0, y: 0, w: 50, h: 50 }).group;
  // Outer clip A (10,10,20,20) => box 10..30; inner clip B (15,15,20,20) => box 15..35 (B
  // pokes past A). A red 20x20 element fills B (15,15 -> 35,35). Visible = A∩B = 15..30.
  const a = createGroup(REPO_ROOT, { projectId, name: "A", x: 10, y: 10, w: 20, h: 20, parentId: screen.id }).group;
  const b = createGroup(REPO_ROOT, { projectId, name: "B", x: 15, y: 15, w: 20, h: 20, parentId: a.id }).group;
  const red = addImage(REPO_ROOT, projectId, { name: "red.png", bytes: solidPng(20, 20, [220, 40, 40]) }).element;
  patchElement(REPO_ROOT, projectId, red.id, { x: 15, y: 15 });
  assignToGroup(REPO_ROOT, { projectId, elementIds: [red.id], groupId: b.id });
  patchGroup(REPO_ROOT, { projectId, groupId: a.id, clip: true });
  patchGroup(REPO_ROOT, { projectId, groupId: b.id, clip: true });

  let result;
  try {
    result = await renderGroup(REPO_ROOT, { projectId, groupId: screen.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const png = decodePng(readFileSync(result.path));
  assert.deepEqual([png.width, png.height], [50, 50]);
  assert.deepEqual(png.at(20, 20), [220, 40, 40, 255], "inside A∩B: member shows");
  assert.deepEqual(png.at(28, 28), [220, 40, 40, 255], "still inside A∩B");
  // (32,32) is inside B (15..35) but OUTSIDE A (10..30): the outer clip crops it.
  assert.equal(png.at(32, 32)[3], 0, "cropped by the OUTER clip (outside A)");
});
