// Canvas NOTE element tests (T0268). A note (`type:"note"`) is a Miro/FigJam sticky card:
// plain text on a colored, FULLY-fixed (w+h user-set), browser-wrapped + clipped box. Notes
// are work annotations, NOT render content — renderGroup/exportProject skip them and
// exportElements refuses them. These mirror text.test.mjs: the metadata ops (defaults +
// validation, patch + undo, export exclusion, CLI parity, copy/paste) need no Python; the
// render-exclusion test drives render_group.py / PIL and skips cleanly when it is unavailable.
// addNote reads the bundled fonts.json from the repo root, so all tests use the REAL repo
// root as the ops root and only redirect the PROJECTS dir.
// Run: node --test ai_studio/assets/canvas/tests/note.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import {
  addImage,
  addNote,
  addText,
  assignToGroup,
  createGroup,
  createProject,
  duplicateNodes,
  exportElements,
  getProject,
  patchElement,
  renderGroup,
  undoOp,
} from "../ops.mjs";
import { decodePng, encodePng } from "./png_fixture.mjs";

const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-note-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

test("addNote applies defaults + derives the layer name from the first content line", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Note" });
  const { element } = addNote(REPO_ROOT, project.id, { x: 40, y: 50, content: "Todo list\nbuy milk" });

  assert.equal(element.type, "note");
  assert.equal(element.x, 40);
  assert.equal(element.y, 50);
  assert.equal(element.content, "Todo list\nbuy milk");
  assert.equal(element.name, "Todo list"); // first non-empty line
  // Fully-fixed default box (both dimensions user-set; DEFAULT_NOTE_SIZE 220x180).
  assert.equal(element.w, 220);
  assert.equal(element.h, 180);
  // Defaults from fonts.mjs DEFAULT_NOTE_STYLE (font SUBSET — no stroke/shadow/autoResize).
  assert.equal(element.style.fontFamily, "Inter");
  assert.equal(element.style.fontWeight, 400);
  assert.equal(element.style.fontSize, 18);
  assert.equal(element.style.lineHeight, 1.35);
  assert.equal(element.style.align, "left");
  assert.equal(element.style.color, "#1a1a1a");
  assert.equal(element.style.stroke, undefined);
  assert.equal(element.style.shadow, undefined);
  // Default background = the first sticky preset (yellow).
  assert.deepEqual(element.background, { type: "color", color: "#fff9b1" });
});

test("addNote honors an explicit fixed box, custom background, and empty default content", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Note" });
  const { element } = addNote(REPO_ROOT, project.id, { w: 320, h: 90, background: { type: "color", color: "#ABCDEF" } });
  assert.equal(element.w, 320);
  assert.equal(element.h, 90);
  assert.equal(element.content, ""); // notes default to empty (no "Text" placeholder)
  assert.equal(element.name, "Note");
  assert.deepEqual(element.background, { type: "color", color: "#abcdef" }); // normalized lowercase

  // background: null => a note with no fill (absent field, like group.background).
  const noFill = addNote(REPO_ROOT, project.id, { background: null }).element;
  assert.equal(noFill.background, undefined);
});

test("addNote validates style + background LOUDLY", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Note" });

  assert.throws(() => addNote(REPO_ROOT, project.id, { style: { fontFamily: "Comic Sans" } }), /unknown font family/);
  assert.throws(() => addNote(REPO_ROOT, project.id, { style: { fontFamily: "Inter", fontWeight: 500 } }), /no 500\/normal/);
  assert.throws(() => addNote(REPO_ROOT, project.id, { style: { align: "middle" } }), /align must be/);
  assert.throws(() => addNote(REPO_ROOT, project.id, { style: { color: "red" } }), /color must be #rrggbb/);
  assert.throws(() => addNote(REPO_ROOT, project.id, { style: { fontSize: -5 } }), /fontSize must be/);
  assert.throws(() => addNote(REPO_ROOT, project.id, { style: "big" }), /note style must be an object/);
  // Background validation (loud, no silent fallback).
  assert.throws(() => addNote(REPO_ROOT, project.id, { background: { type: "color", color: "green" } }), /note background color must be #rrggbb/);
  assert.throws(() => addNote(REPO_ROOT, project.id, { background: { type: "image" } }), /note background type must be "color"/);
  assert.throws(() => addNote(REPO_ROOT, project.id, { background: "yellow" }), /note background must be/);
});

test("patchElement merges note content + style + background and undo restores each step", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Note" });
  const { element } = addNote(REPO_ROOT, project.id, { content: "hi", w: 200, h: 150 });

  // Full patch: content + style + background + geometry (all in one journaled entry).
  const patched = patchElement(REPO_ROOT, project.id, element.id, {
    content: "HELLO",
    style: { fontFamily: "Rubik", fontWeight: 700, fontSize: 22, color: "#223344" },
    background: { type: "color", color: "#d5f6b1" },
    w: 280,
    h: 210,
  }).element;
  assert.equal(patched.content, "HELLO");
  assert.equal(patched.style.fontFamily, "Rubik");
  assert.equal(patched.style.fontWeight, 700);
  assert.equal(patched.style.fontSize, 22);
  assert.equal(patched.style.color, "#223344");
  assert.deepEqual(patched.background, { type: "color", color: "#d5f6b1" });
  assert.equal(patched.w, 280);
  assert.equal(patched.h, 210);

  // A PARTIAL style patch shallow-merges over the current style (untouched fields survive).
  const partial = patchElement(REPO_ROOT, project.id, element.id, { style: { fontSize: 30 } }).element;
  assert.equal(partial.style.fontSize, 30);
  assert.equal(partial.style.fontFamily, "Rubik");

  // Clearing the background (null) drops it to an absent field.
  const cleared = patchElement(REPO_ROOT, project.id, element.id, { background: null }).element;
  assert.equal(cleared.background, undefined);

  // Undo steps back the clear, then the partial, then the full patch (three journal entries).
  undoOp(REPO_ROOT, { projectId: project.id });
  const afterUndo1 = getProject(REPO_ROOT, project.id).elements[0];
  assert.deepEqual(afterUndo1.background, { type: "color", color: "#d5f6b1" });
  undoOp(REPO_ROOT, { projectId: project.id });
  const afterUndo2 = getProject(REPO_ROOT, project.id).elements[0];
  assert.equal(afterUndo2.style.fontSize, 22);
  undoOp(REPO_ROOT, { projectId: project.id });
  const afterUndo3 = getProject(REPO_ROOT, project.id).elements[0];
  assert.equal(afterUndo3.content, "hi");
  assert.equal(afterUndo3.style.fontFamily, "Inter");
  assert.equal(afterUndo3.w, 200);
  assert.equal(afterUndo3.h, 150);
});

test("content/style/background patches respect element type (loud on the wrong type)", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Mixed" });
  const image = addImage(REPO_ROOT, project.id, { name: "p.png", bytes: encodePng(4, 4, () => [10, 20, 30]) }).element;
  const text = addText(REPO_ROOT, project.id, { content: "t" }).element;
  const note = addNote(REPO_ROOT, project.id, { content: "n" }).element;

  // content/style only apply to text/note — an image refuses.
  assert.throws(() => patchElement(REPO_ROOT, project.id, image.id, { content: "no" }), /not a text element/);
  assert.throws(() => patchElement(REPO_ROOT, project.id, image.id, { style: { fontSize: 10 } }), /not a text element/);
  // background is NOTE-only — an image AND a text element refuse it loudly.
  assert.throws(() => patchElement(REPO_ROOT, project.id, image.id, { background: { type: "color", color: "#ffffff" } }), /not a note element/);
  assert.throws(() => patchElement(REPO_ROOT, project.id, text.id, { background: { type: "color", color: "#ffffff" } }), /not a note element/);
  // A note accepts all three.
  const patched = patchElement(REPO_ROOT, project.id, note.id, {
    content: "yes",
    style: { fontSize: 20 },
    background: { type: "color", color: "#cfe6ff" },
  }).element;
  assert.equal(patched.content, "yes");
  assert.equal(patched.style.fontSize, 20);
  assert.deepEqual(patched.background, { type: "color", color: "#cfe6ff" });
});

test("exportElements refuses a note element loudly (never reaches a PNG)", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Note" });
  const { element } = addNote(REPO_ROOT, project.id, { content: "no export" });
  await assert.rejects(
    () => exportElements(REPO_ROOT, { projectId: project.id, elementIds: [element.id] }),
    /note element/,
  );
});

test("copy/paste (duplicateNodes) keeps a note's content + style + background", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Note" });
  const { element } = addNote(REPO_ROOT, project.id, {
    content: "keep me",
    w: 260,
    h: 140,
    style: { fontFamily: "Rubik", fontWeight: 700, fontSize: 21 },
    background: { type: "color", color: "#ffd6e0" },
  });
  const result = duplicateNodes(REPO_ROOT, { projectId: project.id, nodeIds: [element.id], dx: 20, dy: 20 });
  const copyId = (result.elementIds || []).find((id) => id !== element.id);
  assert.ok(copyId, "a fresh note id was minted");
  const copy = getProject(REPO_ROOT, project.id).elements.find((e) => e.id === copyId);
  assert.equal(copy.type, "note");
  assert.equal(copy.content, "keep me");
  assert.equal(copy.w, 260);
  assert.equal(copy.h, 140);
  assert.equal(copy.style.fontFamily, "Rubik");
  assert.equal(copy.style.fontSize, 21);
  assert.deepEqual(copy.background, { type: "color", color: "#ffd6e0" });
});

// ---- render exclusion (PIL; skips cleanly without Python) --------------------

function seedImageScreen(root) {
  const project = createProject(root, { title: "Screen" });
  const image = addImage(root, project.id, { name: "p.png", bytes: encodePng(40, 30, (x, y) => [(x * 6) % 256, (y * 8) % 256, 120]) }).element;
  patchElement(root, project.id, image.id, { x: 10, y: 10 });
  const { group } = createGroup(root, { projectId: project.id, name: "S", x: 0, y: 0, w: 120, h: 100 });
  assignToGroup(root, { projectId: project.id, elementIds: [image.id], groupId: group.id });
  return { projectId: project.id, groupId: group.id };
}

test("renderGroup excludes notes: a group WITH a note renders pixel-equal to WITHOUT it", async (t) => {
  tempProjects(t);
  const { projectId, groupId } = seedImageScreen(REPO_ROOT);

  let without;
  try {
    without = await renderGroup(REPO_ROOT, { projectId, groupId, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const pngWithout = decodePng(readFileSync(without.path));

  // Add a fully-overlapping note INSIDE the group, then render again.
  addNote(REPO_ROOT, projectId, { x: 12, y: 12, w: 90, h: 70, content: "hidden annotation — never rendered", groupId });
  const withNote = await renderGroup(REPO_ROOT, { projectId, groupId, scale: 1 });
  const pngWith = decodePng(readFileSync(withNote.path));

  assert.equal(pngWith.width, pngWithout.width);
  assert.equal(pngWith.height, pngWithout.height);
  let diff = 0;
  for (let y = 0; y < pngWith.height; y += 1) {
    for (let x = 0; x < pngWith.width; x += 1) {
      const a = pngWith.at(x, y);
      const b = pngWithout.at(x, y);
      if (a[0] !== b[0] || a[1] !== b[1] || a[2] !== b[2] || a[3] !== b[3]) diff += 1;
    }
  }
  assert.equal(diff, 0, "the note contributed zero pixels to the render (excluded from the spec)");
});

// ---- CLI parity --------------------------------------------------------------

function runCli(env, ...args) {
  const stdout = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
  return JSON.parse(stdout.trim().split("\n").filter(Boolean).at(-1));
}

test("cli add-note + element-set --content/--style-json/--background parity", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-note-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const projectId = runCli(env, "create", "--title", "CLI Note").project.id;
  const added = runCli(env, "add-note", projectId, "--x", "5", "--y", "6", "--w", "300", "--h", "120", "--content", "Note here", "--background", "#d5f6b1");
  assert.equal(added.element.type, "note");
  assert.equal(added.element.x, 5);
  assert.equal(added.element.w, 300);
  assert.equal(added.element.h, 120);
  assert.equal(added.element.name, "Note here");
  assert.deepEqual(added.element.background, { type: "color", color: "#d5f6b1" });
  const elementId = added.element.id;

  // element-set --content updates the string.
  const recontent = runCli(env, "element-set", projectId, "--element", elementId, "--content", "Changed");
  assert.equal(recontent.element.content, "Changed");

  // element-set --style-json shallow-merges + validates a partial note style.
  const stylePath = join(dir, "style.json");
  writeFileSync(stylePath, JSON.stringify({ fontFamily: "JetBrains Mono", fontWeight: 700, fontSize: 16 }));
  const restyled = runCli(env, "element-set", projectId, "--element", elementId, "--style-json", stylePath);
  assert.equal(restyled.element.style.fontFamily, "JetBrains Mono");
  assert.equal(restyled.element.style.fontSize, 16);

  // element-set --background changes the fill; 'none' clears it.
  const rebg = runCli(env, "element-set", projectId, "--element", elementId, "--background", "#cfe6ff");
  assert.deepEqual(rebg.element.background, { type: "color", color: "#cfe6ff" });
  const cleared = runCli(env, "element-set", projectId, "--element", elementId, "--background", "none");
  assert.equal(cleared.element.background, undefined);

  // A bad background from the CLI surfaces the loud op error (non-zero exit).
  assert.throws(() => runCli(env, "element-set", projectId, "--element", elementId, "--background", "notacolor"));
});
