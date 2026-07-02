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
  fitGroup,
  getProject,
  patchElement,
  patchGroup,
  patchGroups,
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

test("patchGroup background sets a color, journals one entry, undo restores; invalid throws", (t) => {
  tempProjects(t);
  const { projectId, red } = seedScreen(ROOT);
  const { group } = createGroup(ROOT, { projectId, name: "Screen", fromElements: [red.id] });

  const seqBefore = Number(getProject(ROOT, projectId).history_seq);
  patchGroup(ROOT, { projectId, groupId: group.id, background: { type: "color", color: "#112233" } });
  const after = getProject(ROOT, projectId);
  assert.deepEqual(after.groups[0].background, { type: "color", color: "#112233" });
  assert.equal(Number(after.history_seq), seqBefore + 1, "exactly one journal entry");

  // undo removes the background (the prior group had none).
  undoOp(ROOT, { projectId });
  assert.equal(getProject(ROOT, projectId).groups[0].background, undefined, "undo clears the fill");

  // Invalid color, shape, and type all throw loudly (no silent fallback).
  assert.throws(() => patchGroup(ROOT, { projectId, groupId: group.id, background: { type: "color", color: "red" } }), /#rrggbb/);
  assert.throws(() => patchGroup(ROOT, { projectId, groupId: group.id, background: { type: "image" } }), /type must be "color"/);
  assert.throws(() => patchGroup(ROOT, { projectId, groupId: group.id, background: "#112233" }), /must be null or/);
});

test("patchGroup background=null on a plain group is a no-op (no journal entry)", (t) => {
  tempProjects(t);
  const { projectId, red } = seedScreen(ROOT);
  const { group } = createGroup(ROOT, { projectId, name: "Screen", fromElements: [red.id] });
  const seqBefore = Number(getProject(ROOT, projectId).history_seq);
  patchGroup(ROOT, { projectId, groupId: group.id, background: null });
  assert.equal(Number(getProject(ROOT, projectId).history_seq), seqBefore, "None on already-none = no change");
});

// ---- patchGroups (batched shared toggles: multi-group inspector) ----------------

test("patchGroups sets visible+clip across N groups in ONE entry; a single undo restores all", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Multi" });
  const g1 = createGroup(ROOT, { projectId: project.id, name: "A", x: 0, y: 0, w: 100, h: 100 }).group;
  const g2 = createGroup(ROOT, { projectId: project.id, name: "B", x: 0, y: 0, w: 100, h: 100 }).group;
  const g3 = createGroup(ROOT, { projectId: project.id, name: "C", x: 0, y: 0, w: 100, h: 100 }).group;
  const seqBefore = Number(getProject(ROOT, project.id).history_seq);

  const result = patchGroups(ROOT, { projectId: project.id, groupIds: [g1.id, g2.id, g3.id], visible: false, clip: true });
  assert.equal(result.count, 3);
  const after = getProject(ROOT, project.id);
  assert.equal(Number(after.history_seq), seqBefore + 1, "exactly one entry for the whole batch");
  for (const id of [g1.id, g2.id, g3.id]) {
    const g = after.groups.find((group) => group.id === id);
    assert.equal(g.visible, false);
    assert.equal(g.clip, true);
  }

  // One undo steps back the whole batch (visible back to true/absent, clip cleared).
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(Number(undone.history_seq), seqBefore);
  for (const id of [g1.id, g2.id, g3.id]) {
    const g = undone.groups.find((group) => group.id === id);
    assert.notEqual(g.visible, false);
    assert.equal(g.clip, undefined, "clip cleared to an absent field on undo");
  }
});

test("patchGroups clip:false clears the flag to an absent field", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Clip" });
  const g1 = createGroup(ROOT, { projectId: project.id, name: "A", x: 0, y: 0, w: 100, h: 100 }).group;
  const g2 = createGroup(ROOT, { projectId: project.id, name: "B", x: 0, y: 0, w: 100, h: 100 }).group;
  patchGroups(ROOT, { projectId: project.id, groupIds: [g1.id, g2.id], clip: true });
  patchGroups(ROOT, { projectId: project.id, groupIds: [g1.id, g2.id], clip: false });
  const after = getProject(ROOT, project.id);
  assert.equal(after.groups.find((g) => g.id === g1.id).clip, undefined);
  assert.equal(after.groups.find((g) => g.id === g2.id).clip, undefined);
});

test("patchGroups rejects bad input atomically (unknown id, empty list, no field)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Bad" });
  const g1 = createGroup(ROOT, { projectId: project.id, name: "A", x: 0, y: 0, w: 100, h: 100 }).group;
  const seqBefore = Number(getProject(ROOT, project.id).history_seq);
  assert.throws(() => patchGroups(ROOT, { projectId: project.id, groupIds: [] }), /non-empty groupIds/);
  assert.throws(() => patchGroups(ROOT, { projectId: project.id, groupIds: [g1.id] }), /at least one of visible, clip/);
  assert.throws(
    () => patchGroups(ROOT, { projectId: project.id, groupIds: [g1.id, "grp_missing"], visible: false }),
    /group not found/,
  );
  assert.throws(() => patchGroups(ROOT, { projectId: project.id, groupIds: [g1.id], clip: "yes" }), /clip must be a boolean/);
  // No write happened on any rejected call.
  const after = getProject(ROOT, project.id);
  assert.equal(Number(after.history_seq), seqBefore);
  assert.notEqual(after.groups.find((g) => g.id === g1.id).visible, false);
});

// ---- fitGroup (resize frame to content) ----------------------------------------

test("fitGroup fits the frame around direct elements + default 24px padding; children never move", (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(ROOT);
  // A group with a deliberately oversized frame so the fit visibly shrinks it.
  const { group } = createGroup(ROOT, { projectId, name: "Loose", x: -100, y: -100, w: 800, h: 800 });
  assignToGroup(ROOT, { projectId, elementIds: [red.id, green.id], groupId: group.id });

  const { group: fitted } = fitGroup(ROOT, { projectId, groupId: group.id });
  // bbox of red@(10,10,8,8) + green@(30,20,6,6): minX=10 minY=10 maxX=36 maxY=26; +24 pad.
  assert.deepEqual({ x: fitted.x, y: fitted.y, w: fitted.w, h: fitted.h }, { x: -14, y: -14, w: 74, h: 64 });
  // Members are untouched by the fit.
  const stored = getProject(ROOT, projectId);
  assert.deepEqual(pos(stored, red.id), { x: 10, y: 10 });
  assert.deepEqual(pos(stored, green.id), { x: 30, y: 20 });
});

test("fitGroup honors a custom padding (0 = tight bbox)", (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(ROOT);
  const { group } = createGroup(ROOT, { projectId, name: "Tight", x: 0, y: 0, w: 500, h: 500 });
  assignToGroup(ROOT, { projectId, elementIds: [red.id, green.id], groupId: group.id });

  const tight = fitGroup(ROOT, { projectId, groupId: group.id, padding: 0 }).group;
  assert.deepEqual({ x: tight.x, y: tight.y, w: tight.w, h: tight.h }, { x: 10, y: 10, w: 26, h: 16 });

  const padded = fitGroup(ROOT, { projectId, groupId: group.id, padding: 10 }).group;
  assert.deepEqual({ x: padded.x, y: padded.y, w: padded.w, h: padded.h }, { x: 0, y: 0, w: 46, h: 36 });
});

test("fitGroup fits around a NESTED subgroup's frame (2 levels)", (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(ROOT);
  const outer = createGroup(ROOT, { projectId, name: "Outer", x: 0, y: 0, w: 1000, h: 1000 }).group;
  assignToGroup(ROOT, { projectId, elementIds: [red.id], groupId: outer.id }); // red is a direct child of outer
  // Inner subgroup nested in outer with a frame that extends BEYOND its own element, so
  // maxX/maxY must come from the subgroup FRAME (200,200,80,60 -> 280,260), not green.
  const inner = createGroup(ROOT, { projectId, name: "Inner", x: 200, y: 200, w: 80, h: 60, parentId: outer.id }).group;
  assignToGroup(ROOT, { projectId, elementIds: [green.id], groupId: inner.id });
  patchElement(ROOT, projectId, green.id, { x: 250, y: 250 }); // green (250,250,6,6) -> 256,256

  const fitted = fitGroup(ROOT, { projectId, groupId: outer.id, padding: 0 }).group;
  // union of red(10,10,8,8)=>18,18, green(250,250,6,6)=>256,256, inner frame=>280,260.
  assert.deepEqual({ x: fitted.x, y: fitted.y, w: fitted.w, h: fitted.h }, { x: 10, y: 10, w: 270, h: 250 });
  // The nested subgroup frame and its element are untouched.
  const stored = getProject(ROOT, projectId);
  const innerStored = stored.groups.find((g) => g.id === inner.id);
  assert.deepEqual({ x: innerStored.x, y: innerStored.y, w: innerStored.w, h: innerStored.h }, { x: 200, y: 200, w: 80, h: 60 });
  assert.deepEqual(pos(stored, green.id), { x: 250, y: 250 });
});

test("fitGroup on an empty group is a loud error; invalid padding throws", (t) => {
  tempProjects(t);
  const { projectId, red } = seedScreen(ROOT);
  const empty = createGroup(ROOT, { projectId, name: "Empty", x: 0, y: 0, w: 100, h: 100 }).group;
  assert.throws(() => fitGroup(ROOT, { projectId, groupId: empty.id }), /nothing to fit/);
  assert.throws(() => fitGroup(ROOT, { projectId, groupId: "nope" }), /group not found/);

  // A group WITH content, so the throw is about padding not emptiness.
  const withContent = createGroup(ROOT, { projectId, name: "Full", fromElements: [red.id] }).group;
  assert.throws(() => fitGroup(ROOT, { projectId, groupId: withContent.id, padding: -1 }), /padding must be/);
  assert.throws(() => fitGroup(ROOT, { projectId, groupId: withContent.id, padding: "abc" }), /padding must be/);
  assert.throws(() => fitGroup(ROOT, { projectId, groupId: withContent.id, padding: Infinity }), /padding must be/);
});

test("fitGroup is one journal entry; undo restores the old frame; works with clip=true", (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(ROOT);
  const { group } = createGroup(ROOT, { projectId, name: "Clip", x: 0, y: 0, w: 500, h: 500 });
  assignToGroup(ROOT, { projectId, elementIds: [red.id, green.id], groupId: group.id });
  patchGroup(ROOT, { projectId, groupId: group.id, clip: true });
  const oldFrame = (() => {
    const g = getProject(ROOT, projectId).groups[0];
    return { x: g.x, y: g.y, w: g.w, h: g.h };
  })();
  const seqBefore = Number(getProject(ROOT, projectId).history_seq);

  const fitted = fitGroup(ROOT, { projectId, groupId: group.id }).group;
  assert.deepEqual({ x: fitted.x, y: fitted.y, w: fitted.w, h: fitted.h }, { x: -14, y: -14, w: 74, h: 64 });
  assert.equal(fitted.clip, true, "clip flag preserved through the fit (frame re-evaluates the clip)");
  assert.equal(Number(getProject(ROOT, projectId).history_seq), seqBefore + 1, "exactly one journal entry");

  const undone = undoOp(ROOT, { projectId }).project;
  const g = undone.groups[0];
  assert.deepEqual({ x: g.x, y: g.y, w: g.w, h: g.h }, oldFrame, "undo restores the old frame");
  assert.equal(g.clip, true, "clip survives undo");
});

test("renderGroup fills group.background and an explicit arg overrides it (skips without Python)", async (t) => {
  tempProjects(t);
  const { projectId, red, green } = seedScreen(REPO_ROOT);
  const { group } = createGroup(REPO_ROOT, { projectId, name: "BG", fromElements: [red.id, green.id] });
  patchGroup(REPO_ROOT, { projectId, groupId: group.id, background: { type: "color", color: "#112233" } });

  let result;
  try {
    result = await renderGroup(REPO_ROOT, { projectId, groupId: group.id, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  // group.background composited as the bottom fill (no explicit arg). (1,1) is a
  // corner of the padded group box, well outside every member.
  let png = decodePng(readFileSync(result.path));
  assert.deepEqual(png.at(1, 1), [17, 34, 51, 255], "group.background fill");

  // An explicit render-time background overrides group.background.
  const overridden = await renderGroup(REPO_ROOT, { projectId, groupId: group.id, scale: 1, background: "#445566" });
  png = decodePng(readFileSync(overridden.path));
  assert.deepEqual(png.at(1, 1), [68, 85, 102, 255], "explicit arg overrides group.background");
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
