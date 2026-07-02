// Group (screen) ops tests. The metadata-only ops need no Python; the two render
// tests drive our render_group.py compositor and skip cleanly when Python/PIL is
// unavailable. Run:
//   node --test ai_studio/assets/canvas/tests/groups.test.mjs
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
  deleteGroup,
  getProject,
  patchElement,
  patchGroup,
  redoOp,
  renderGroup,
  undoOp,
} from "../ops.mjs";
import { decodePng, encodePng } from "./png_fixture.mjs";

// Metadata ops resolve store paths only, so any placeholder root works. The two
// render tests spawn Python with cwd = repo root, so they use the real repo root.
const ROOT = "C:/unused-repo-root";
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-groups-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// Two solid PNGs placed at known offsets inside one group.
function seedScreen(root) {
  const project = createProject(root, { title: "Screen" });
  const red = addImage(root, project.id, { name: "red.png", bytes: encodePng(8, 8, () => [220, 40, 40]) }).element;
  patchElement(root, project.id, red.id, { x: 10, y: 10 });
  const green = addImage(root, project.id, { name: "green.png", bytes: encodePng(6, 6, () => [40, 180, 60]) }).element;
  patchElement(root, project.id, green.id, { x: 30, y: 20 });
  return { projectId: project.id, red, green };
}

test("createGroup fromElements uses the members' bbox + 24px padding and assigns groupId", (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(ROOT);
  const { group } = createGroup(ROOT, { projectId, name: "Main Menu", fromElements: [red.id, green.id] });

  // bbox: minX=10 minY=10 maxX=36 maxY=26 -> padded by 24 on every side.
  assert.deepEqual({ x: group.x, y: group.y, w: group.w, h: group.h }, { x: -14, y: -14, w: 74, h: 64 });
  assert.equal(group.visible, true);
  const stored = getProject(ROOT, projectId);
  assert.equal(stored.elements.find((e) => e.id === red.id).groupId, group.id);
  assert.equal(stored.elements.find((e) => e.id === green.id).groupId, group.id);
});

test("createGroup with explicit bounds requires positive w/h", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Explicit" });
  const { group } = createGroup(ROOT, { projectId: project.id, name: "HUD", x: 5, y: 6, w: 100, h: 80 });
  assert.deepEqual({ x: group.x, y: group.y, w: group.w, h: group.h }, { x: 5, y: 6, w: 100, h: 80 });
  assert.throws(() => createGroup(ROOT, { projectId: project.id, name: "Bad" }), /fromElements or positive w\/h/);
});

test("patchGroup move translates every member; undo restores group + all members", (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(ROOT);
  const { group } = createGroup(ROOT, { projectId, name: "Screen", fromElements: [red.id, green.id] });

  patchGroup(ROOT, { projectId, groupId: group.id, x: group.x + 100, y: group.y + 50 });
  const moved = getProject(ROOT, projectId);
  assert.deepEqual([moved.groups[0].x, moved.groups[0].y], [86, 36]);
  assert.deepEqual(pos(moved, red.id), { x: 110, y: 60 }); // 10+100, 10+50
  assert.deepEqual(pos(moved, green.id), { x: 130, y: 70 }); // 30+100, 20+50

  // One journal entry: undo restores the group frame AND both members together.
  const undone = undoOp(ROOT, { projectId }).project;
  assert.deepEqual([undone.groups[0].x, undone.groups[0].y], [-14, -14]);
  assert.deepEqual(pos(undone, red.id), { x: 10, y: 10 });
  assert.deepEqual(pos(undone, green.id), { x: 30, y: 20 });

  const redone = redoOp(ROOT, { projectId }).project;
  assert.deepEqual(pos(redone, red.id), { x: 110, y: 60 });
});

test("patchGroup resize does not move members; visible toggle is journaled", (t) => {
  tempProjects(t);
  const { projectId, red } = seedScreen(ROOT);
  const { group } = createGroup(ROOT, { projectId, name: "Screen", fromElements: [red.id] });
  const before = pos(getProject(ROOT, projectId), red.id);

  patchGroup(ROOT, { projectId, groupId: group.id, w: 500, h: 400, visible: false });
  const after = getProject(ROOT, projectId);
  assert.deepEqual([after.groups[0].w, after.groups[0].h], [500, 400]);
  assert.equal(after.groups[0].visible, false);
  assert.deepEqual(pos(after, red.id), before, "resize leaves members in place");

  undoOp(ROOT, { projectId });
  assert.equal(getProject(ROOT, projectId).groups[0].visible, true);
});

test("deleteGroup deletes member elements (undo restores); ungroup keeps them", (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(ROOT);
  const { group } = createGroup(ROOT, { projectId, name: "Screen", x: 0, y: 0, w: 200, h: 200 });

  assignToGroup(ROOT, { projectId, elementIds: [red.id, green.id], groupId: group.id });
  assert.equal(getProject(ROOT, projectId).elements.every((e) => e.groupId === group.id), true);

  // Delete = group AND its content go together (dissolving is Ungroup's job).
  const result = deleteGroup(ROOT, { projectId, groupId: group.id });
  const afterDelete = getProject(ROOT, projectId);
  assert.equal(afterDelete.groups.length, 0);
  assert.equal(afterDelete.elements.length, 0, "member elements deleted with the group");
  assert.deepEqual(result.removedElements.sort(), [red.id, green.id].sort());

  // ONE undo restores the group and every member.
  undoOp(ROOT, { projectId });
  const restored = getProject(ROOT, projectId);
  assert.equal(restored.groups.length, 1);
  assert.equal(restored.elements.length, 2, "undo restores members with the group");
  assert.equal(restored.elements.every((e) => e.groupId === group.id), true);

  // Ungroup (assignToGroup null) is the non-destructive path: members survive.
  assignToGroup(ROOT, { projectId, elementIds: [red.id, green.id], groupId: null });
  const ungrouped = getProject(ROOT, projectId);
  assert.equal(ungrouped.elements.length, 2, "ungroup keeps the elements");
  assert.equal(ungrouped.elements.every((e) => e.groupId === null), true);
});

test("renderGroup composites members at the right pixels (skips without Python)", async (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(REPO_ROOT);
  const { group } = createGroup(REPO_ROOT, { projectId, name: "Main Menu", fromElements: [red.id, green.id] });

  let result;
  try {
    result = await renderGroup(REPO_ROOT, { projectId, groupId: group.id, scale: 2, background: "#1a1f2b" });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  assert.equal(result.manifest.kind, "screen");
  assert.deepEqual([result.manifest.width, result.manifest.height], [148, 128]);

  const png = decodePng(readFileSync(result.path));
  assert.deepEqual([png.width, png.height], [148, 128]);
  // group origin (-14,-14), scale 2. red@(10,10) -> (48,48); green@(30,20) -> (88,68).
  assert.deepEqual(png.at(2, 2), [26, 31, 43, 255], "solid background fill");
  assert.deepEqual(png.at(52, 52), [220, 40, 40, 255], "red member");
  assert.deepEqual(png.at(92, 72), [40, 180, 60, 255], "green member");
});

test("renderGroup omits hidden members and honors transparent background (skips without Python)", async (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(REPO_ROOT);
  const { group } = createGroup(REPO_ROOT, { projectId, name: "Screen", fromElements: [red.id, green.id] });

  // Hide the green element, render with a transparent background.
  patchElement(REPO_ROOT, projectId, green.id, { visible: false });
  let result;
  try {
    result = await renderGroup(REPO_ROOT, { projectId, groupId: group.id, scale: 2 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  assert.equal(result.members, 1, "only the visible member is composited");

  const png = decodePng(readFileSync(result.path));
  assert.equal(png.at(2, 2)[3], 0, "transparent background (alpha 0)");
  assert.deepEqual(png.at(52, 52), [220, 40, 40, 255], "visible red member still drawn");
  assert.equal(png.at(92, 72)[3], 0, "hidden green member absent (transparent)");
});

function pos(project, elementId) {
  const element = project.elements.find((e) => e.id === elementId);
  return { x: element.x, y: element.y };
}
