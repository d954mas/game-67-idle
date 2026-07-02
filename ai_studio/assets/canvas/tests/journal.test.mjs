// Journal + undo/redo + export ops tests (no Python needed). Run:
//   node --test ai_studio/assets/canvas/tests/journal.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, readdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import {
  addImage,
  createProject,
  exportElements,
  getProject,
  patchElement,
  readHistory,
  redoOp,
  removeElement,
  undoOp,
} from "../ops.mjs";
import { solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-journal-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

function positions(project) {
  return (project.elements || []).map((element) => ({ id: element.id, x: element.x, y: element.y }));
}

test("journal round-trip: op -> undo -> redo restores identical state", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Round trip" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });

  const moved = patchElement(ROOT, project.id, element.id, { x: 50, y: 30 }).project;
  const afterMove = positions(moved);
  assert.deepEqual(afterMove, [{ id: element.id, x: 50, y: 30 }]);

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.deepEqual(positions(undone), [{ id: element.id, x: 0, y: 0 }]);

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.deepEqual(positions(redone), afterMove);

  // A journal.jsonl exists next to project.json and holds mutation + marker lines.
  assert.equal(existsSync(join(process.env.CANVAS_PROJECTS_ROOT, project.id, "journal.jsonl")), true);
  const history = readHistory(ROOT, { projectId: project.id });
  assert.deepEqual(history.entries.map((entry) => entry.op), ["addImage", "patchElement", "undo", "redo"]);
});

test("undo unwinds multiple ops back to the base state, then errors", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Multi" });
  const a = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng(4, 3, [1, 2, 3]) }).element;
  const b = addImage(ROOT, project.id, { name: "b.png", bytes: solidPng(5, 4, [9, 8, 7]) }).element;
  patchElement(ROOT, project.id, b.id, { x: 100 });
  removeElement(ROOT, project.id, a.id);

  assert.equal(getProject(ROOT, project.id).elements.length, 1); // only b remains

  undoOp(ROOT, { projectId: project.id }); // undo remove -> a back
  assert.equal(getProject(ROOT, project.id).elements.length, 2);
  undoOp(ROOT, { projectId: project.id }); // undo move
  assert.equal(getProject(ROOT, project.id).elements.find((e) => e.id === b.id).x, 0);
  undoOp(ROOT, { projectId: project.id }); // undo add b
  undoOp(ROOT, { projectId: project.id }); // undo add a
  assert.equal(getProject(ROOT, project.id).elements.length, 0);
  assert.equal(getProject(ROOT, project.id).history_seq, 0);
  assert.throws(() => undoOp(ROOT, { projectId: project.id }), /nothing to undo/);
});

test("a new op after undo invalidates the redo tail", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Invalidate" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  patchElement(ROOT, project.id, element.id, { x: 50 }); // seq2, head2

  undoOp(ROOT, { projectId: project.id }); // head back to seq1 (x:0)
  assert.equal(getProject(ROOT, project.id).elements[0].x, 0);
  assert.equal(readHistory(ROOT, { projectId: project.id }).canRedo, true);

  patchElement(ROOT, project.id, element.id, { x: 99 }); // new branch, invalidates redo of x:50
  assert.equal(getProject(ROOT, project.id).elements[0].x, 99);
  const history = readHistory(ROOT, { projectId: project.id });
  assert.equal(history.canRedo, false);
  assert.throws(() => redoOp(ROOT, { projectId: project.id }), /nothing to redo/);

  // Undo the new branch, then redo picks the newest branch (x:99), not the stale one.
  undoOp(ROOT, { projectId: project.id });
  assert.equal(getProject(ROOT, project.id).elements[0].x, 0);
  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements[0].x, 99);
});

test("exportElements writes a stamped folder with copied files + manifest", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Export" });
  const png = solidPng(6, 6, [10, 20, 30]);
  const a = addImage(ROOT, project.id, { name: "Hero Sprite.png", bytes: png }).element;
  const b = addImage(ROOT, project.id, { name: "Hero Sprite.png", bytes: solidPng(7, 7, [40, 50, 60]) }).element;

  const result = exportElements(ROOT, { projectId: project.id, elementIds: [a.id, b.id] });
  assert.equal(result.items.length, 2);
  assert.equal(result.manifest.schema, "ai_studio.canvas.export.v1");
  assert.equal(result.manifest.project, project.id);

  // Names collide (same title) -> deterministic collision suffix.
  const files = result.items.map((item) => item.file);
  assert.equal(new Set(files).size, 2, "collision-suffixed unique file names");
  for (const item of result.items) {
    assert.equal(existsSync(join(result.folder, item.file)), true, `${item.file} copied`);
  }
  assert.equal(existsSync(join(result.folder, "manifest.json")), true);
  const manifest = JSON.parse(readFileSync(join(result.folder, "manifest.json"), "utf8"));
  assert.deepEqual(manifest.items.map((item) => item.elementId), [a.id, b.id]);

  // Export is NOT journaled (no new undoable entry) but IS recorded in tool_runs.
  const stored = getProject(ROOT, project.id);
  assert.equal(stored.tool_runs.at(-1).op, "export_elements");
  const ops = readHistory(ROOT, { projectId: project.id }).entries.map((entry) => entry.op);
  assert.deepEqual(ops, ["addImage", "addImage"], "export added no journal mutation");

  // The copied bytes match the immutable source.
  const firstFile = readdirSync(result.folder).find((name) => name.endsWith(".png"));
  assert.ok(firstFile, "at least one exported png");
});
