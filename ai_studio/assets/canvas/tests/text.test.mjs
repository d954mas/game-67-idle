// Canvas TEXT element tests (T0222 increment 1). The metadata ops (addText defaults +
// validation, patchElement content/style merge + undo, export exclusion, fitGroup box)
// need no Python; the render tests drive render_group.py / PIL and skip cleanly when it
// is unavailable. addText/render read the bundled fonts.json from the repo root, so all
// tests use the REAL repo root as the ops root and only redirect the PROJECTS dir.
// Run: node --test ai_studio/assets/canvas/tests/text.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { main as runCanvasCli } from "../cli.mjs";
import {
  addImage,
  addText,
  assignToGroup,
  createGroup,
  createProject,
  exportElements,
  fitGroup,
  getProject,
  patchElement,
  renderGroup,
  undoOp,
} from "../ops.mjs";
import { decodePng, encodePng } from "./png_fixture.mjs";

const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));
const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-text-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

test("addText applies defaults + derives the layer name from the first content line", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Text" });
  const { element } = addText(REPO_ROOT, project.id, { x: 20, y: 30, content: "Heading\nsub" });

  assert.equal(element.type, "text");
  assert.equal(element.x, 20);
  assert.equal(element.y, 30);
  assert.equal(element.content, "Heading\nsub");
  assert.equal(element.name, "Heading"); // first non-empty line
  assert.ok(element.w > 0 && element.h > 0, "nominal box is positive");
  // Defaults from fonts.mjs DEFAULT_TEXT_STYLE.
  assert.equal(element.style.fontFamily, "Inter");
  assert.equal(element.style.fontWeight, 400);
  assert.equal(element.style.fontStyle, "normal");
  assert.equal(element.style.fontSize, 24);
  assert.equal(element.style.lineHeight, 1.2);
  assert.equal(element.style.align, "left");
  assert.equal(element.style.color, "#ffffff");
  assert.deepEqual(element.style.stroke, { width: 2, color: "#000000" });
  assert.equal(element.style.shadow, null);
  assert.equal(element.style.autoResize, "width");
});

test("addText validates style LOUDLY against fonts.json", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Text" });

  assert.throws(() => addText(REPO_ROOT, project.id, { style: { fontFamily: "Comic Sans" } }), /unknown font family/);
  // Inter has no 500 instance (400/600/700 only).
  assert.throws(() => addText(REPO_ROOT, project.id, { style: { fontFamily: "Inter", fontWeight: 500 } }), /no 500\/normal/);
  assert.throws(() => addText(REPO_ROOT, project.id, { style: { align: "middle" } }), /align must be/);
  assert.throws(() => addText(REPO_ROOT, project.id, { style: { color: "red" } }), /color must be #rrggbb/);
  assert.throws(() => addText(REPO_ROOT, project.id, { style: { fontSize: -5 } }), /fontSize must be/);
  assert.throws(() => addText(REPO_ROOT, project.id, { style: { stroke: { width: -1 } } }), /stroke\.width must be/);
  assert.throws(() => addText(REPO_ROOT, project.id, { style: "big" }), /text style must be an object/);
});

test("patchElement merges text content + style (nested stroke/shadow) and undo restores", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Text" });
  const { element } = addText(REPO_ROOT, project.id, { content: "hi" });

  const patched = patchElement(REPO_ROOT, project.id, element.id, {
    content: "HELLO",
    style: {
      fontFamily: "Rubik",
      fontWeight: 700,
      fontSize: 48,
      stroke: { width: 3, color: "#ff0000" },
      shadow: { dx: 4, dy: 4, color: "#000000" },
    },
  }).element;
  assert.equal(patched.content, "HELLO");
  assert.equal(patched.style.fontFamily, "Rubik");
  assert.equal(patched.style.fontWeight, 700);
  assert.equal(patched.style.fontSize, 48);
  assert.deepEqual(patched.style.stroke, { width: 3, color: "#ff0000" });
  assert.deepEqual(patched.style.shadow, { dx: 4, dy: 4, blur: 0, color: "#000000" }); // blur stored 0

  // A PARTIAL style patch shallow-merges over the current style (nested stroke keeps its
  // color when only the width changes).
  const partial = patchElement(REPO_ROOT, project.id, element.id, { style: { stroke: { width: 5 } } }).element;
  assert.deepEqual(partial.style.stroke, { width: 5, color: "#ff0000" });
  assert.equal(partial.style.fontFamily, "Rubik"); // untouched fields survive

  // Undo steps back the partial patch, then the full patch (two journal entries).
  undoOp(REPO_ROOT, { projectId: project.id });
  const afterUndo1 = getProject(REPO_ROOT, project.id).elements[0];
  assert.deepEqual(afterUndo1.style.stroke, { width: 3, color: "#ff0000" });
  undoOp(REPO_ROOT, { projectId: project.id });
  const afterUndo2 = getProject(REPO_ROOT, project.id).elements[0];
  assert.equal(afterUndo2.content, "hi");
  assert.equal(afterUndo2.style.fontFamily, "Inter");
});

test("content/style patches on a non-text element throw", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Mixed" });
  const image = addImage(REPO_ROOT, project.id, { name: "p.png", bytes: encodePng(4, 4, () => [10, 20, 30]) }).element;
  assert.throws(() => patchElement(REPO_ROOT, project.id, image.id, { content: "no" }), /not a text element/);
  assert.throws(() => patchElement(REPO_ROOT, project.id, image.id, { style: { fontSize: 10 } }), /not a text element/);
});

test("exportElements refuses a text element with a clear v1 message", async (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Text" });
  const { element } = addText(REPO_ROOT, project.id, { content: "no export" });
  await assert.rejects(
    () => exportElements(REPO_ROOT, { projectId: project.id, elementIds: [element.id] }),
    /text element/,
  );
});

test("fitGroup includes a text element's box", (t) => {
  tempProjects(t);
  const project = createProject(REPO_ROOT, { title: "Fit" });
  const image = addImage(REPO_ROOT, project.id, { name: "p.png", bytes: encodePng(10, 10, () => [10, 20, 30]) }).element;
  patchElement(REPO_ROOT, project.id, image.id, { x: 0, y: 0 });
  const { element: text } = addText(REPO_ROOT, project.id, { x: 300, y: 200, content: "far away" });
  const { group } = createGroup(REPO_ROOT, { projectId: project.id, name: "G", x: 0, y: 0, w: 20, h: 20 });
  assignToGroup(REPO_ROOT, { projectId: project.id, elementIds: [image.id, text.id], groupId: group.id });

  const fitted = fitGroup(REPO_ROOT, { projectId: project.id, groupId: group.id, padding: 8 }).group;
  const live = getProject(REPO_ROOT, project.id).elements.find((e) => e.id === text.id);
  // The fitted frame must cover the text element's far corner (+ padding).
  assert.ok(fitted.x <= image.x - 8 + 0.001, "left edge covers the image");
  assert.ok(fitted.y <= 0 - 8 + 0.001, "top edge covers the image");
  assert.ok(fitted.x + fitted.w >= live.x + live.w + 8 - 0.001, "right edge covers the text box");
  assert.ok(fitted.y + fitted.h >= live.y + live.h + 8 - 0.001, "bottom edge covers the text box");
});

// ---- render parity (PIL; skips cleanly without Python) -----------------------

function seedTextScreen(root, { content = "Aa", style } = {}) {
  const project = createProject(root, { title: "Screen" });
  const { element } = addText(root, project.id, { x: 20, y: 20, content, style });
  const { group } = createGroup(root, { projectId: project.id, name: "S", x: 0, y: 0, w: 200, h: 100 });
  assignToGroup(root, { projectId: project.id, elementIds: [element.id], groupId: group.id });
  return { projectId: project.id, groupId: group.id, element };
}

function countNonEmpty(png) {
  let n = 0;
  for (let y = 0; y < png.height; y += 1) for (let x = 0; x < png.width; x += 1) if (png.at(x, y)[3] > 20) n += 1;
  return n;
}

test("a text-bearing group renders a non-empty PNG with the expected dims and DIFFERENT pixels than an empty one", async (t) => {
  tempProjects(t);
  const { projectId, groupId, element } = seedTextScreen(REPO_ROOT, {
    content: "Заголовок",
    style: { fontFamily: "Rubik", fontWeight: 700, fontSize: 40, color: "#2244aa" },
  });

  let withText;
  try {
    withText = await renderGroup(REPO_ROOT, { projectId, groupId, scale: 2 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const pngWith = decodePng(readFileSync(withText.path));
  assert.equal(pngWith.width, 400, "200 * scale 2");
  assert.equal(pngWith.height, 200, "100 * scale 2");
  const withCount = countNonEmpty(pngWith);
  assert.ok(withCount > 0, "text rendered some pixels");

  // Hide the text -> the same transparent group renders (nearly) empty: DIFFERENT pixels.
  patchElement(REPO_ROOT, projectId, element.id, { visible: false });
  const without = await renderGroup(REPO_ROOT, { projectId, groupId, scale: 2 });
  const withoutCount = countNonEmpty(decodePng(readFileSync(without.path)));
  assert.ok(withCount > withoutCount + 50, `text adds pixels (${withCount} vs ${withoutCount})`);
});

test("stroke + hard offset shadow render distinct colored pixels (smoke)", async (t) => {
  tempProjects(t);
  // Blue fill, red shadow offset far down-right, white outline. Distinct colors let us
  // assert both the fill and the (offset) shadow actually painted.
  const { projectId, groupId } = seedTextScreen(REPO_ROOT, {
    content: "A",
    style: {
      fontFamily: "Rubik",
      fontWeight: 700,
      fontSize: 60,
      color: "#0000ff",
      stroke: { width: 3, color: "#ffffff" },
      shadow: { dx: 10, dy: 10, color: "#ff0000" },
    },
  });

  let result;
  try {
    result = await renderGroup(REPO_ROOT, { projectId, groupId, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const png = decodePng(readFileSync(result.path));
  let blue = 0;
  let red = 0;
  for (let y = 0; y < png.height; y += 1) {
    for (let x = 0; x < png.width; x += 1) {
      const [r, g, b, a] = png.at(x, y);
      if (a < 60) continue;
      if (b > 150 && r < 90 && g < 90) blue += 1; // fill glyph core
      if (r > 150 && g < 90 && b < 90) red += 1; // offset shadow
    }
  }
  assert.ok(blue > 0, "fill (blue) pixels present");
  assert.ok(red > 0, "shadow (red) pixels present at the offset");
});

// ---- CLI parity --------------------------------------------------------------

function runCliProcess(env, ...args) {
  const stdout = execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
  return JSON.parse(stdout.trim().split("\n").filter(Boolean).at(-1));
}

async function runCli(env, ...args) {
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = env.CANVAS_PROJECTS_ROOT;
  try {
    return await runCanvasCli(args, { repoRoot: REPO_ROOT, print: (value) => value });
  } finally {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
  }
}

test("cli add-text + element-set --content/--style-json parity", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-text-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const projectId = (await runCli(env, "create", "--title", "CLI Text")).project.id;
  const added = await runCli(env, "add-text", projectId, "--x", "12", "--y", "8", "--content", "Hi there");
  assert.equal(added.element.type, "text");
  assert.equal(added.element.x, 12);
  assert.equal(added.element.name, "Hi there");
  const elementId = added.element.id;

  // element-set --content updates the string.
  const recontent = await runCli(env, "element-set", projectId, "--element", elementId, "--content", "Changed");
  assert.equal(recontent.element.content, "Changed");

  // element-set --style-json shallow-merges + validates a partial style.
  const stylePath = join(dir, "style.json");
  writeFileSync(stylePath, JSON.stringify({ fontFamily: "JetBrains Mono", fontWeight: 700, fontSize: 30 }));
  const restyled = await runCli(env, "element-set", projectId, "--element", elementId, "--style-json", stylePath);
  assert.equal(restyled.element.style.fontFamily, "JetBrains Mono");
  assert.equal(restyled.element.style.fontWeight, 700);
  assert.equal(restyled.element.style.fontSize, 30);

  // A bad family from the CLI surfaces the loud op error (non-zero exit).
  const badPath = join(dir, "bad.json");
  writeFileSync(badPath, JSON.stringify({ fontFamily: "Nope" }));
  assert.throws(() => runCliProcess(env, "element-set", projectId, "--element", elementId, "--style-json", badPath));
});
