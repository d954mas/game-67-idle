// Canvas store tests. Run:
//   node --test ai_studio/assets/canvas/tests/store.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import {
  addImage,
  createProject,
  deleteProject,
  getProject,
  imageSize,
  listProjects,
  patchElement,
  removeElement,
  resolveProjectFile,
  updateProject,
} from "../store.mjs";
import { solidPng } from "./png_fixture.mjs";

// The projects root is redirected to a throwaway temp dir via the env override so
// tests never touch the configured (YandexDisk) location. `root` is then unused
// by resolution but still passed as the store's first argument.
function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-store-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

const ROOT = "C:/unused-repo-root";

test("imageSize reads PNG header dimensions", () => {
  assert.deepEqual(imageSize(solidPng(7, 5)), { width: 7, height: 5 });
});

test("createProject writes a v1 project lazily and getProject reads it back", (t) => {
  const projectsRoot = tempProjects(t);
  const project = createProject(ROOT, { title: "My Canvas" });

  assert.equal(project.schema, "ai_studio.canvas.project.v1");
  assert.match(project.id, /^my-canvas-[0-9a-f]{6}$/);
  assert.deepEqual(project.elements, []);
  assert.deepEqual(project.tool_runs, []);
  assert.equal(existsSync(join(projectsRoot, project.id, "project.json")), true);
  assert.equal(existsSync(join(projectsRoot, project.id, "files")), true);
  assert.deepEqual(getProject(ROOT, project.id), project);
});

test("listProjects tolerates broken project folders", (t) => {
  const projectsRoot = tempProjects(t);
  const good = createProject(ROOT, { title: "Good" });
  mkdirSync(join(projectsRoot, "broken-dir"), { recursive: true });
  writeFileSync(join(projectsRoot, "broken-dir", "project.json"), "{ not json", "utf8");
  mkdirSync(join(projectsRoot, "no-json-dir"), { recursive: true });

  const ids = listProjects(ROOT).map((p) => p.id);
  assert.deepEqual(ids, [good.id]);
});

test("addImage stores an immutable content-addressed file with real dimensions", (t) => {
  const projectsRoot = tempProjects(t);
  const project = createProject(ROOT, { title: "Images" });
  const bytes = solidPng(12, 8, [200, 100, 50]);

  const { element } = addImage(ROOT, project.id, { name: "hero.png", bytes });
  assert.equal(element.type, "image");
  assert.match(element.src, /^files\/[0-9a-f]{64}\.png$/);
  assert.equal(element.w, 12);
  assert.equal(element.h, 8);
  assert.equal(element.name, "hero.png");

  const filePath = join(projectsRoot, project.id, element.src);
  assert.equal(existsSync(filePath), true);
  // Adding identical bytes reuses the same immutable file (no duplicate write).
  const again = addImage(ROOT, project.id, { name: "hero-copy.png", bytes });
  assert.equal(again.element.src, element.src);
  assert.equal(getProject(ROOT, project.id).elements.length, 2);
});

test("patchElement moves an element and updates the project", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Move" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });

  const { element: moved } = patchElement(ROOT, project.id, element.id, { x: 40, y: 25, name: "renamed" });
  assert.equal(moved.x, 40);
  assert.equal(moved.y, 25);
  assert.equal(moved.name, "renamed");
  assert.equal(getProject(ROOT, project.id).elements[0].x, 40);
});

test("removeElement drops the element but keeps the file on disk", (t) => {
  const projectsRoot = tempProjects(t);
  const project = createProject(ROOT, { title: "Remove" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  const filePath = join(projectsRoot, project.id, element.src);

  removeElement(ROOT, project.id, element.id);
  assert.equal(getProject(ROOT, project.id).elements.length, 0);
  assert.equal(existsSync(filePath), true, "backing image file must remain on disk");
});

test("deleteProject moves the folder to .trash and drops it from listProjects", (t) => {
  const projectsRoot = tempProjects(t);
  const keep = createProject(ROOT, { title: "Keep" });
  const doomed = createProject(ROOT, { title: "Doomed" });
  addImage(ROOT, doomed.id, { name: "a.png", bytes: solidPng() });

  const result = deleteProject(ROOT, doomed.id);
  assert.equal(result.id, doomed.id);
  // The live folder is gone; a recoverable copy lives under <root>/.trash/<id>-<stamp>/.
  assert.equal(existsSync(join(projectsRoot, doomed.id)), false, "live folder moved out");
  assert.equal(existsSync(result.trashed), true, "trash copy exists");
  assert.equal(existsSync(join(result.trashed, "project.json")), true, "trash keeps project.json");
  assert.ok(result.trashed.includes(join(projectsRoot, ".trash")), "trash lives under the projects root");

  // listProjects ignores the dot-prefixed .trash and no longer shows the project.
  const ids = listProjects(ROOT).map((p) => p.id);
  assert.deepEqual(ids, [keep.id]);
  assert.throws(() => getProject(ROOT, doomed.id), /not found/);

  // Deleting a missing project errors clearly; unsafe ids are still confined.
  assert.throws(() => deleteProject(ROOT, "does-not-exist"), /not found/);
  assert.throws(() => deleteProject(ROOT, "../evil"), /unsafe project id|escapes/);
});

test("updateProject preserves id/created/schema, bumps updated, writes atomically", async (t) => {
  const projectsRoot = tempProjects(t);
  const project = createProject(ROOT, { title: "Atomic" });
  const dir = join(projectsRoot, project.id);

  for (let i = 0; i < 8; i += 1) {
    updateProject(ROOT, project.id, { id: "HACK", created: "1999-01-01", schema: "x", title: `T${i}` });
    // project.json is always valid JSON and no temp files linger.
    const parsed = JSON.parse(readFileSync(join(dir, "project.json"), "utf8"));
    assert.equal(parsed.id, project.id);
    assert.equal(parsed.created, project.created);
    assert.equal(parsed.schema, "ai_studio.canvas.project.v1");
    assert.equal(readdirSync(dir).filter((n) => n.includes(".tmp-")).length, 0);
  }
  assert.equal(getProject(ROOT, project.id).title, "T7");
});

test("path confinement rejects unsafe ids and file names", (t) => {
  tempProjects(t);
  for (const badId of ["../evil", "a/b", "a\\b", "..", ".hidden"]) {
    assert.throws(() => getProject(ROOT, badId), /unsafe project id|escapes/);
  }
  const project = createProject(ROOT, { title: "Confine" });
  assert.throws(() => resolveProjectFile(ROOT, project.id, "../../secret.txt"), /unsafe file name|escapes/);
});
