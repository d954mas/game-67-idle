// Batched multi-element ops (T0200 fix H): patchElements / removeElements.
// Each is ONE journal entry for the whole gesture, so a single undo restores it;
// bad input is rejected atomically (no partial write). Run:
//   node --test ai_studio/assets/canvas/tests/batched.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import {
  addImage,
  addImages,
  createProject,
  getProject,
  patchElements,
  readHistory,
  redoOp,
  removeElements,
  undoOp,
} from "../ops.mjs";
import { solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-batched-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// Three distinct-content images at known origins.
function seed(t) {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Batch" });
  const ids = [];
  for (let i = 0; i < 3; i += 1) {
    const { element } = addImage(ROOT, project.id, { name: `e${i}.png`, bytes: solidPng(4, 4, [i * 20, 0, 0]), x: i, y: i });
    ids.push(element.id);
  }
  return { projectId: project.id, ids };
}

test("patchElements moves N elements in ONE journal entry; a single undo restores all", (t) => {
  const { projectId, ids } = seed(t);
  const seqBefore = getProject(ROOT, projectId).history_seq; // 3 addImage entries

  const result = patchElements(ROOT, {
    projectId,
    patches: [
      { elementId: ids[0], x: 100, y: 10 },
      { elementId: ids[1], x: 200, y: 20 },
      { elementId: ids[2], x: 300, y: 30 },
    ],
  });
  assert.equal(result.count, 3);
  assert.equal(result.elements.length, 3);

  // Exactly one new mutation entry for the whole gesture.
  const after = getProject(ROOT, projectId);
  assert.equal(after.history_seq, seqBefore + 1);
  const byId = new Map(after.elements.map((e) => [e.id, e]));
  assert.deepEqual([byId.get(ids[0]).x, byId.get(ids[1]).x, byId.get(ids[2]).x], [100, 200, 300]);

  // One undo steps back the entire batch.
  const undone = undoOp(ROOT, { projectId }).project;
  const u = new Map(undone.elements.map((e) => [e.id, e]));
  assert.deepEqual([u.get(ids[0]).x, u.get(ids[1]).x, u.get(ids[2]).x], [0, 1, 2]);
  assert.equal(undone.history_seq, seqBefore);

  // Redo re-applies the whole batch.
  const redone = redoOp(ROOT, { projectId }).project;
  const r = new Map(redone.elements.map((e) => [e.id, e]));
  assert.deepEqual([r.get(ids[0]).x, r.get(ids[1]).x, r.get(ids[2]).x], [100, 200, 300]);
});

test("removeElements deletes N in ONE journal entry; a single undo restores all", (t) => {
  const { projectId, ids } = seed(t);
  const seqBefore = getProject(ROOT, projectId).history_seq;

  const result = removeElements(ROOT, { projectId, elementIds: [ids[0], ids[2]] });
  assert.deepEqual(result.removed.sort(), [ids[0], ids[2]].sort());

  const after = getProject(ROOT, projectId);
  assert.equal(after.history_seq, seqBefore + 1);
  assert.deepEqual(after.elements.map((e) => e.id), [ids[1]]);

  const undone = undoOp(ROOT, { projectId }).project;
  assert.deepEqual(undone.elements.map((e) => e.id).sort(), ids.slice().sort());
  assert.equal(undone.history_seq, seqBefore);
});

test("removeElements de-duplicates ids and is one entry", (t) => {
  const { projectId, ids } = seed(t);
  const seqBefore = getProject(ROOT, projectId).history_seq;
  const result = removeElements(ROOT, { projectId, elementIds: [ids[0], ids[0]] });
  assert.deepEqual(result.removed, [ids[0]]);
  assert.equal(getProject(ROOT, projectId).history_seq, seqBefore + 1);
});

test("patchElements rejects bad input atomically (no partial write)", (t) => {
  const { projectId, ids } = seed(t);
  const seqBefore = getProject(ROOT, projectId).history_seq;

  assert.throws(() => patchElements(ROOT, { projectId }), /requires a patches array/);
  assert.throws(() => patchElements(ROOT, { projectId, patches: [{ x: 1 }] }), /missing an elementId/);
  // A good patch followed by an unknown id must NOT partially apply the good one.
  assert.throws(
    () => patchElements(ROOT, { projectId, patches: [{ elementId: ids[0], x: 999 }, { elementId: "el_missing", x: 1 }] }),
    /element not found/,
  );
  const after = getProject(ROOT, projectId);
  assert.equal(after.history_seq, seqBefore, "no journal entry written on a rejected batch");
  assert.equal(after.elements.find((e) => e.id === ids[0]).x, 0, "the good element was not mutated");
});

test("removeElements rejects an unknown id atomically (no partial delete)", (t) => {
  const { projectId, ids } = seed(t);
  const seqBefore = getProject(ROOT, projectId).history_seq;
  assert.throws(
    () => removeElements(ROOT, { projectId, elementIds: [ids[0], "el_missing"] }),
    /element not found/,
  );
  const after = getProject(ROOT, projectId);
  assert.equal(after.history_seq, seqBefore);
  assert.equal(after.elements.length, 3, "no element was deleted");
});

test("an empty batch is a no-op (no journal entry)", (t) => {
  const { projectId } = seed(t);
  const seqBefore = getProject(ROOT, projectId).history_seq;
  patchElements(ROOT, { projectId, patches: [] });
  removeElements(ROOT, { projectId, elementIds: [] });
  assert.equal(getProject(ROOT, projectId).history_seq, seqBefore);
  assert.equal(readHistory(ROOT, { projectId }).history_seq, seqBefore);
});

// ---- addImages (batched multi-image add: multi-file drop/paste) ------------------

test("addImages adds N images in ONE journal entry; a single undo removes all", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Drop" });
  const seqBefore = getProject(ROOT, project.id).history_seq; // 0 (fresh)

  const result = addImages(ROOT, project.id, {
    images: [
      { name: "a.png", bytes: solidPng(4, 4, [10, 0, 0]), x: 1, y: 2 },
      { name: "b.png", bytes: solidPng(4, 4, [0, 10, 0]), x: 3, y: 4 },
      { name: "c.png", bytes: solidPng(4, 4, [0, 0, 10]), x: 5, y: 6 },
    ],
  });
  assert.equal(result.count, 3);
  assert.equal(result.elements.length, 3);
  assert.deepEqual(result.elements.map((e) => [e.x, e.y]), [[1, 2], [3, 4], [5, 6]]);

  const after = getProject(ROOT, project.id);
  assert.equal(after.history_seq, seqBefore + 1, "exactly one entry for the whole drop");
  assert.equal(after.elements.length, 3);

  // One undo removes every added image; redo re-adds all three.
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.elements.length, 0);
  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.length, 3);
});

test("addImages rejects a bad image atomically (no partial add) and an empty list", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Bad drop" });
  assert.throws(() => addImages(ROOT, project.id, { images: [] }), /non-empty images array/);
  // A good image followed by an empty-bytes one must NOT half-add the good one.
  assert.throws(
    () => addImages(ROOT, project.id, { images: [{ name: "ok.png", bytes: solidPng(4, 4, [1, 2, 3]) }, { name: "bad.png", bytes: Buffer.alloc(0) }] }),
    /empty bytes/,
  );
  assert.equal(getProject(ROOT, project.id).elements.length, 0, "no element added on a rejected batch");
  assert.equal(getProject(ROOT, project.id).history_seq, 0);
});
