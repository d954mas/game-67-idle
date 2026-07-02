// Canvas HTTP API tests using an in-process invoke harness (no port binding).
// Run: node --test ai_studio/assets/canvas/tests/api.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import { createCanvasApi } from "../api.mjs";
import { magentaSheetPng, solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";
// The bridged slice/detect routes run raster2d Python with cwd = repo root, so
// that one test drives a handler bound to the real repo root (store paths stay
// redirected by CANVAS_PROJECTS_ROOT).
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-api-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// Minimal req/res doubles. res collects Buffer chunks so both JSON responses and
// piped binary file responses are captured.
function invokeApi(handler, method, path, body) {
  const req = new EventEmitter();
  req.method = method;
  req.setEncoding = () => {};
  req.destroy = () => {};
  const chunks = [];
  const res = {
    statusCode: 0,
    headers: {},
    writeHead(status, headers) {
      this.statusCode = status;
      this.headers = headers || {};
    },
    write(chunk) {
      chunks.push(Buffer.from(chunk));
      return true;
    },
    end(chunk) {
      if (chunk !== undefined && chunk !== null && chunk !== "") chunks.push(Buffer.from(chunk));
      const buffer = Buffer.concat(chunks);
      this._resolve({
        status: this.statusCode,
        headers: this.headers,
        buffer,
        json() {
          return JSON.parse(buffer.toString("utf8"));
        },
      });
    },
  };
  const done = new Promise((r) => {
    res._resolve = r;
  });
  handler(req, res, new URL(path, "http://canvas.local"));
  queueMicrotask(() => {
    if (body !== undefined) req.emit("data", Buffer.from(typeof body === "string" ? body : JSON.stringify(body)));
    req.emit("end");
  });
  return done;
}

test("canvas API supports the full project + element lifecycle", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);

  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "API Canvas" });
  assert.equal(created.status, 201);
  const projectId = created.json().project.id;
  assert.match(projectId, /^api-canvas-/);

  const list = await invokeApi(handler, "GET", "/api/canvas/projects");
  assert.equal(list.status, 200);
  assert.deepEqual(list.json().projects.map((p) => p.id), [projectId]);

  const png = solidPng(9, 6, [30, 60, 90]);
  const uploaded = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "pic.png",
    bytes_base64: png.toString("base64"),
  });
  assert.equal(uploaded.status, 201);
  const element = uploaded.json().element;
  assert.equal(element.w, 9);
  assert.equal(element.h, 6);

  const got = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`);
  assert.equal(got.status, 200);
  assert.equal(got.json().project.elements.length, 1);

  const fileName = element.src.replace(/^files\//, "");
  const file = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/files/${fileName}`);
  assert.equal(file.status, 200);
  assert.equal(file.headers["content-type"], "image/png");
  assert.ok(file.buffer.equals(png), "served file bytes match the uploaded PNG");

  const patched = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${element.id}`, {
    x: 15,
    y: 22,
  });
  assert.equal(patched.status, 200);
  assert.equal(patched.json().element.x, 15);
  assert.equal(patched.json().element.y, 22);

  const removed = await invokeApi(handler, "DELETE", `/api/canvas/projects/${projectId}/elements/${element.id}`);
  assert.equal(removed.status, 200);
  assert.equal(removed.json().removed, element.id);

  const afterRemove = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`);
  assert.equal(afterRemove.json().project.elements.length, 0);
});

test("canvas API renames a project (PATCH) and trashes it (DELETE)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const keep = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Keep" })).json().project.id;
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Before" });
  const projectId = created.json().project.id;

  const renamed = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}`, { title: "After" });
  assert.equal(renamed.status, 200);
  assert.equal(renamed.json().project.title, "After");
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project.title, "After");

  const deleted = await invokeApi(handler, "DELETE", `/api/canvas/projects/${projectId}`);
  assert.equal(deleted.status, 200);
  assert.equal(deleted.json().id, projectId);
  assert.ok(deleted.json().trashed.includes(".trash"), "response reports the trash location");

  // The project is gone from the list (only the untouched one remains) and 400s on GET.
  assert.deepEqual((await invokeApi(handler, "GET", "/api/canvas/projects")).json().projects.map((p) => p.id), [keep]);
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).status, 400);
});

test("canvas API export download route serves a confined file and rejects traversal", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Export DL" });
  const projectId = created.json().project.id;
  const png = solidPng(6, 6, [11, 22, 33]);
  const elementId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "hero.png",
    bytes_base64: png.toString("base64"),
  })).json().element.id;

  const exported = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/export`, { elementIds: [elementId] });
  assert.equal(exported.status, 200);
  const stamp = exported.json().folder.replace(/[\\/]+$/, "").split(/[\\/]/).at(-1);
  const file = exported.json().items[0].file;

  // The GET download route serves the exported bytes back, path-confined per segment.
  const download = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/export/${stamp}/${file}`);
  assert.equal(download.status, 200);
  assert.equal(download.headers["content-type"], "image/png");
  assert.ok(download.buffer.equals(png), "downloaded bytes match the exported image");

  // manifest.json is downloadable too.
  const manifest = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/export/${stamp}/manifest.json`);
  assert.equal(manifest.status, 200);

  // A traversal segment is rejected before any file is read.
  const bad = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/export/${encodeURIComponent("../../secret")}`);
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /unsafe path segment|escapes/);
});

test("canvas API undo/redo/history routes round-trip an element move", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "History API" });
  const projectId = created.json().project.id;

  const uploaded = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "pic.png",
    bytes_base64: solidPng(8, 8).toString("base64"),
  });
  const elementId = uploaded.json().element.id;

  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elementId}`, { x: 40, y: 12 });

  const undone = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  assert.equal(undone.status, 200);
  assert.equal(undone.json().project.elements[0].x, 0);

  const redone = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/redo`);
  assert.equal(redone.status, 200);
  assert.equal(redone.json().project.elements[0].x, 40);

  const history = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/history`);
  assert.equal(history.status, 200);
  assert.deepEqual(history.json().entries.map((entry) => entry.op), ["addImage", "patchElement", "undo", "redo"]);
});

test("canvas API export route writes a folder + manifest", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Export API" });
  const projectId = created.json().project.id;
  const uploaded = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "pic.png",
    bytes_base64: solidPng(5, 5).toString("base64"),
  });
  const elementId = uploaded.json().element.id;

  const exported = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/export`, {
    elementIds: [elementId],
  });
  assert.equal(exported.status, 200);
  const payload = exported.json();
  assert.equal(payload.items.length, 1);
  assert.equal(payload.manifest.schema, "ai_studio.canvas.export.v1");
});

test("canvas API slice route creates crop elements (skips without Python)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Slice API" });
  const projectId = created.json().project.id;
  const uploaded = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "sheet.png",
    bytes_base64: magentaSheetPng().toString("base64"),
  });
  const elementId = uploaded.json().element.id;

  const detected = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/detect-regions`, { elementId });
  if (detected.status !== 200) {
    t.skip(`raster2d/python pipeline unavailable: ${detected.json().error}`);
    return;
  }
  const sliced = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/slice`, { elementId });
  if (sliced.status !== 200) {
    t.skip(`raster2d/python slicing unavailable: ${sliced.json().error}`);
    return;
  }
  t.after(() => {
    const sessionId = sliced.json().run.result_summary.session_id;
    const detectSession = detected.json().run.result_summary.session_id;
    for (const id of [sessionId, detectSession]) {
      if (id) rmSync(join(REPO_ROOT, "tmp", "ai_studio", "assets", "raster2d", id), { recursive: true, force: true });
    }
  });
  assert.ok(sliced.json().created.length >= 1, "slice created crop elements");
});

test("canvas API group routes: create/patch/assign/delete round-trip", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Group API" })).json().project.id;
  const elA = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "a.png",
    bytes_base64: solidPng(8, 8).toString("base64"),
  })).json().element.id;
  const elB = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "b.png",
    bytes_base64: solidPng(6, 6).toString("base64"),
  })).json().element.id;
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elA}`, { x: 10, y: 10 });
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elB}`, { x: 30, y: 20 });

  const created = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "Main Menu",
    fromElements: [elA, elB],
  });
  assert.equal(created.status, 201);
  const groupId = created.json().group.id;
  assert.deepEqual(
    { x: created.json().group.x, y: created.json().group.y, w: created.json().group.w, h: created.json().group.h },
    { x: -14, y: -14, w: 74, h: 64 },
  );

  // patchGroup move translates members.
  const moved = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/groups/${groupId}`, { x: 86, y: 36 });
  assert.equal(moved.status, 200);
  const afterMove = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(afterMove.elements.find((e) => e.id === elA).x, 110);

  // assign-group clears a member, delete keeps elements.
  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/assign-group`, { elementIds: [elB], groupId: null });
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project.elements.find((e) => e.id === elB).groupId, null);
  const deleted = await invokeApi(handler, "DELETE", `/api/canvas/projects/${projectId}/groups/${groupId}`);
  assert.equal(deleted.status, 200);
  const afterDelete = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(afterDelete.groups.length, 0);
  assert.equal(afterDelete.elements.length, 2);
});

test("canvas API render-screen route composites a group PNG (skips without Python)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Render API" })).json().project.id;
  const elA = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "red.png",
    bytes_base64: solidPng(8, 8, [220, 40, 40]).toString("base64"),
  })).json().element.id;
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elA}`, { x: 10, y: 10 });
  const groupId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "Screen",
    fromElements: [elA],
  })).json().group.id;

  const rendered = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups/${groupId}/render`, { scale: 1 });
  if (rendered.status !== 200) {
    t.skip(`render_group.py / PIL unavailable: ${rendered.json().error}`);
    return;
  }
  assert.equal(rendered.json().manifest.kind, "screen");
  assert.equal(rendered.json().members, 1);
});

test("canvas API reorder route moves an element among its siblings (z-order)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Z API" })).json().project.id;
  const add = async (name) =>
    (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
      name,
      bytes_base64: solidPng(4, 4).toString("base64"),
    })).json().element.id;
  const elA = await add("a.png");
  await add("b.png");
  await add("c.png");

  // Send A (index 0) to the front (index 2).
  const moved = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/elements/${elA}/reorder`, { index: 2 });
  assert.equal(moved.status, 200);
  assert.equal(moved.json().index, 2);
  const after = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.deepEqual(after.elements.map((e) => e.name), ["b.png", "c.png", "a.png"]);

  // Undo restores the original order in one step.
  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  const undone = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.deepEqual(undone.elements.map((e) => e.name), ["a.png", "b.png", "c.png"]);
});

test("canvas API returns an error for a missing project", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const missing = await invokeApi(handler, "GET", "/api/canvas/projects/does-not-exist");
  assert.equal(missing.status, 400);
  assert.match(missing.json().error, /not found/);
});

test("canvas API rejects a path-traversal file request", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Confine" });
  const projectId = created.json().project.id;
  const bad = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/files/${encodeURIComponent("../../secret")}`);
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /unsafe file name|escapes/);
});
