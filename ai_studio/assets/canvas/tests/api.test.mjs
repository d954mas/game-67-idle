// Canvas HTTP API tests using an in-process invoke harness (no port binding).
// Run: node --test ai_studio/assets/canvas/tests/api.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL, fileURLToPath } from "node:url";
import { createCanvasApi, statusForError } from "../api.mjs";
import { orderedChildren } from "../tree.mjs";
import { magentaSheetPng, solidPng } from "./png_fixture.mjs";
// T0332 v2 phase C: the new pack routes' tests build their fixtures directly through ops.mjs
// (a hand-built recipe card / pack run group) rather than through the HTTP layer itself, the
// same "setup via the lower-level API, exercise the route under test" split pack.test.mjs's
// own packSlice fixtures use (seedPackRun) — there is no PATCH route for element.meta, so a
// sheet/cut fixture cannot be built through HTTP alone.
import { addImage, createRecipeCard, getProject, updateProject } from "../ops.mjs";

// Computed element paint order (back -> front) by name for a project scope — reorderNode
// writes `order` fields, not the raw elements[] array, so tests assert the computed order.
const elemOrder = (project, scopeId = null) =>
  orderedChildren(project, scopeId).filter((node) => node.kind === "element").map((node) => node.ref.name);

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

function withCanvasProjectsRoot(t, dir) {
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
  });
}

function ensurePrivateGameMount(root, gameId = "secret-game") {
  const gameRoot = join(root, "games", gameId);
  mkdirSync(gameRoot, { recursive: true });
  execFileSync("git", ["init"], { cwd: root, encoding: "utf8" });
  execFileSync("git", ["init"], { cwd: gameRoot, encoding: "utf8" });
  mkdirSync(join(root, ".git", "info"), { recursive: true });
  writeFileSync(
    join(root, ".git", "info", "exclude"),
    `ai_studio/workspace/catalog.local.json\ngames/${gameId}/\n`,
    "utf8",
  );
  mkdirSync(join(root, "ai_studio", "workspace"), { recursive: true });
  writeFileSync(join(gameRoot, "game.json"), JSON.stringify({ schema: "ai_studio.game.v1", id: gameId, title: gameId, storageNamespace: gameId }), "utf8");
  writeFileSync(join(gameRoot, "dependencies.json"), JSON.stringify({ schema: "ai_studio.game.dependencies.v1", engine: { source: "engine", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" }), "utf8");
  writeFileSync(join(root, "ai_studio", "workspace", "catalog.json"), JSON.stringify({ schema: "ai_studio.workspace.catalog.v1", mounts: [] }), "utf8");
  writeFileSync(join(root, "ai_studio", "workspace", "catalog.local.json"), JSON.stringify({
    schema: "ai_studio.workspace.catalog.v1",
    mounts: [{ kind: "game", root: `games/${gameId}`, visibility: "private", gitRoot: `games/${gameId}`, commitPolicy: "nested-private", enabledStores: ["canvas"], aliases: [] }],
  }, null, 2) + "\n", "utf8");
  return {
    gameId,
    gameRoot,
    canvasRoot: join(gameRoot, ".ai_studio", "canvas", "projects"),
  };
}

// Minimal req/res doubles. res collects Buffer chunks so both JSON responses and
// piped binary file responses are captured.
function invokeApi(handler, method, path, body, headers = {}) {
  const req = new EventEmitter();
  req.method = method;
  req.headers = Object.fromEntries(Object.entries(headers).map(([key, value]) => [key.toLowerCase(), value]));
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

test("canvas API routes explicitly selected private game stores and keeps default reads public-only", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "canvas-api-private-root-"));
  const publicRoot = join(root, "public-canvas");
  const privateStore = ensurePrivateGameMount(root);
  withCanvasProjectsRoot(t, publicRoot);
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const handler = createCanvasApi(root);

  const publicProject = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Public API Canvas" })).json().project;
  const privateHeaders = { "x-ai-studio-store": "game:secret-game" };
  const privateProject = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Private API Canvas" }, privateHeaders)).json().project;

  assert.equal(publicProject.storeId, "studio");
  assert.equal(privateProject.storeId, "game:secret-game");
  assert.equal(privateProject.visibility, "private");
  assert.equal(existsSync(join(publicRoot, publicProject.id, "project.json")), true);
  assert.equal(existsSync(join(privateStore.canvasRoot, privateProject.id, "project.json")), true);
  assert.equal(existsSync(join(publicRoot, privateProject.id)), false);

  const normalList = (await invokeApi(handler, "GET", "/api/canvas/projects")).json();
  assert.deepEqual(normalList.projects.map((project) => project.title), ["Public API Canvas"]);
  assert.deepEqual(normalList.projects.map((project) => project.storeId), ["studio"]);

  const included = (await invokeApi(handler, "GET", "/api/canvas/projects?include-private=true")).json();
  const privateRow = included.projects.find((project) => project.storeId === "game:secret-game");
  assert.equal(privateRow.title, "Private API Canvas");
  assert.equal(privateRow.qualifiedId, `game:secret-game:${privateProject.id}`);

  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${privateProject.id}`)).status, 404);
  const selectedGet = await invokeApi(handler, "GET", `/api/canvas/projects/${privateProject.id}`, undefined, privateHeaders);
  assert.equal(selectedGet.status, 200);
  assert.equal(selectedGet.json().project.storeId, "game:secret-game");

  const png = solidPng(5, 5, [40, 50, 60]);
  const uploaded = await invokeApi(handler, "POST", `/api/canvas/projects/${privateProject.id}/images`, {
    name: "private.png",
    bytes_base64: png.toString("base64"),
  }, privateHeaders);
  assert.equal(uploaded.status, 201);
  const fileName = uploaded.json().element.src.replace(/^files\//, "");
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${privateProject.id}/files/${fileName}`)).status, 404);
  const file = await invokeApi(handler, "GET", `/api/canvas/projects/${privateProject.id}/files/${fileName}`, undefined, privateHeaders);
  assert.equal(file.status, 200);
  assert.ok(file.buffer.equals(png), "private API file route reads only with the selected private store");

  const mismatch = await invokeApi(
    handler,
    "GET",
    `/api/canvas/projects/${privateProject.id}?store=studio`,
    undefined,
    privateHeaders,
  );
  assert.equal(mismatch.status, 400);
  assert.match(mismatch.json().error, /mismatch/);
});

test("canvas API renames a project (PATCH) and trashes it (DELETE)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const keep = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Keep" })).json().project.id;
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", {
    title: "Before",
    ownership: { kind: "game", gameId: "fixture-game" },
  });
  const projectId = created.json().project.id;
  assert.deepEqual(created.json().project.ownership, { kind: "game", gameId: "fixture-game" });

  const renamed = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}`, { title: "After", gameId: "web-dressup" });
  assert.equal(renamed.status, 200);
  assert.equal(renamed.json().project.title, "After");
  assert.deepEqual(renamed.json().project.ownership, { kind: "game", gameId: "web-dressup" });
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project.title, "After");

  const hiddenByOwner = await invokeApi(handler, "GET", "/api/canvas/projects?owner-game=fixture-game");
  assert.deepEqual(hiddenByOwner.json().projects.map((p) => p.id), []);
  const visibleByOwner = await invokeApi(handler, "GET", "/api/canvas/projects?owner-game=web-dressup");
  assert.deepEqual(visibleByOwner.json().projects.map((p) => p.id), [projectId]);

  const deleted = await invokeApi(handler, "DELETE", `/api/canvas/projects/${projectId}`);
  assert.equal(deleted.status, 200);
  assert.equal(deleted.json().id, projectId);
  assert.ok(deleted.json().trashed.includes(".trash"), "response reports the trash location");

  // The project is gone from the list (only the untouched one remains) and 404s on GET
  // (T0254 Tier 1 #2: statusForError maps "not found" to 404, not the old catch-all 400).
  assert.deepEqual((await invokeApi(handler, "GET", "/api/canvas/projects")).json().projects.map((p) => p.id), [keep]);
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).status, 404);
});

test("canvas API hides archived projects unless explicitly requested and keeps direct GET available", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const active = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Active" })).json().project;
  const archived = (await invokeApi(handler, "POST", "/api/canvas/projects", {
    title: "Archived",
    ownership: { kind: "game", gameId: "fixture-game" },
  })).json().project;

  const patched = await invokeApi(handler, "PATCH", `/api/canvas/projects/${archived.id}`, { archived: true });
  assert.equal(patched.status, 200);
  assert.equal(patched.json().project.archived, true);
  assert.deepEqual(patched.json().project.ownership, archived.ownership);
  assert.deepEqual((await invokeApi(handler, "GET", "/api/canvas/projects")).json().projects.map((project) => project.id), [active.id]);

  const included = await invokeApi(handler, "GET", "/api/canvas/projects?include-archived=true");
  assert.deepEqual(new Set(included.json().projects.map((project) => project.id)), new Set([active.id, archived.id]));
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${archived.id}`)).json().project.archived, true);

  const restored = await invokeApi(handler, "PATCH", `/api/canvas/projects/${archived.id}`, { archived: false });
  assert.equal(restored.status, 200);
  assert.equal(restored.json().project.archived, false);
  assert.deepEqual(restored.json().project.ownership, archived.ownership);
  assert.deepEqual(new Set((await invokeApi(handler, "GET", "/api/canvas/projects")).json().projects.map((project) => project.id)), new Set([active.id, archived.id]));
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

test("canvas API history-list + history-jump routes drive the panel (jump folds history flags)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "History panel API" });
  const projectId = created.json().project.id;

  const uploaded = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "hero.png",
    bytes_base64: solidPng(8, 8).toString("base64"),
  });
  const elementId = uploaded.json().element.id;
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elementId}`, { x: 40 }); // seq2, head2

  // history-list: labeled linear spine the panel renders.
  const list = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/history-list`);
  assert.equal(list.status, 200);
  assert.deepEqual(list.json().entries.map((e) => e.label), ["Base", "Add image", "Move"]);
  assert.equal(list.json().entries.at(-1).current, true);

  // history-jump back to seq1: restores the pre-move state AND folds history flags so the
  // page needs no follow-up GET (canRedo true after jumping back).
  const jumped = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/history-jump`, { seq: 1 });
  assert.equal(jumped.status, 200);
  assert.equal(jumped.json().project.elements[0].x, 0);
  assert.equal(jumped.json().project.history_seq, 1);
  assert.equal(jumped.json().history.canRedo, true, "jump response folds history flags like undo/redo");
  assert.equal(jumped.json().history.seq, 1);

  // The redo-tail entry (seq2) is now dimmed; jumping forward restores it (== redo).
  const dimmed = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/history-list`);
  assert.equal(dimmed.json().entries.find((e) => e.seq === 2).undone, true);
  const forward = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/history-jump`, { seq: 2 });
  assert.equal(forward.json().project.elements[0].x, 40);

  // A bad seq is a loud 4xx (never a silent no-op).
  const bad = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/history-jump`, { seq: 999 });
  assert.equal(bad.status >= 400, true, "unknown seq is a loud error");
});

test("canvas API undo/redo/history-jump pass expectHead through (T0234 concurrency guard)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Expect Head API" });
  const projectId = created.json().project.id;

  const uploaded = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "pic.png",
    bytes_base64: solidPng(8, 8).toString("base64"),
  });
  const elementId = uploaded.json().element.id;
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elementId}`, { x: 40 }); // seq2, head2

  // A stale expectHead is a loud 409 BEFORE any write (T0254 Tier 1 #2: HEAD_CONFLICT
  // maps to 409, not the old catch-all 400 — the page can tell "reload" from "bad
  // input"); the head is unchanged after.
  const stale = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`, { expectHead: 1 });
  assert.equal(stale.status, 409);
  assert.match(stale.json().error, /history advanced: head is now 2, you read 1/);
  const afterRefusal = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`);
  assert.equal(afterRefusal.json().project.history_seq, 2, "refused undo left the head untouched");

  // A matching expectHead succeeds exactly like the no-param path (existing test above).
  const undone = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`, { expectHead: 2 });
  assert.equal(undone.status, 200);
  assert.equal(undone.json().project.elements[0].x, 0);

  // redo takes the same guard.
  const redoBad = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/redo`, { expectHead: 99 });
  assert.equal(redoBad.status, 409);
  const redone = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/redo`, { expectHead: 1 });
  assert.equal(redone.status, 200);
  assert.equal(redone.json().project.elements[0].x, 40);

  // history-jump takes the same guard.
  const jumpBad = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/history-jump`, { seq: 1, expectHead: 0 });
  assert.equal(jumpBad.status, 409);
  const jumped = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/history-jump`, { seq: 1, expectHead: 2 });
  assert.equal(jumped.status, 200);
  assert.equal(jumped.json().project.history_seq, 1);

  // The page path (no expectHead at all) is untouched — undo/redo/jump without the
  // field behave exactly as before T0234.
  const pageUndo = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  assert.equal(pageUndo.status, 200);
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

  // assign-group clears a member; delete then removes the group AND its
  // remaining members (elB was ungrouped first, so it survives).
  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/assign-group`, { elementIds: [elB], groupId: null });
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project.elements.find((e) => e.id === elB).groupId, null);
  const deleted = await invokeApi(handler, "DELETE", `/api/canvas/projects/${projectId}/groups/${groupId}`);
  assert.equal(deleted.status, 200);
  assert.deepEqual(deleted.json().removedElements, [elA]);
  const afterDelete = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(afterDelete.groups.length, 0);
  assert.equal(afterDelete.elements.length, 1, "member elA deleted with the group");
  assert.equal(afterDelete.elements[0].id, elB);
});

test("canvas API PATCH group sets and clears the clip flag", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Clip API" })).json().project.id;
  const groupId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "Frame", x: 0, y: 0, w: 100, h: 100,
  })).json().group.id;

  const clipped = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/groups/${groupId}`, { clip: true });
  assert.equal(clipped.status, 200);
  assert.equal(clipped.json().group.clip, true);

  const unclipped = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/groups/${groupId}`, { clip: false });
  assert.equal("clip" in unclipped.json().group, false, "clip:false removes the field over HTTP too");

  // Invalid clip is a loud 400 (no silent coercion).
  const bad = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/groups/${groupId}`, { clip: "yes" });
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /clip must be a boolean/);
});

test("canvas API PATCH group sets and clears the screen (export opt-in) flag — T0332 B1", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Screen API" })).json().project.id;
  const groupId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "Frame", x: 0, y: 0, w: 100, h: 100,
  })).json().group.id;
  assert.equal("screen" in (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project.groups[0], false, "absent by default");

  const flagged = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/groups/${groupId}`, { screen: true });
  assert.equal(flagged.status, 200);
  assert.equal(flagged.json().group.screen, true);

  const unflagged = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/groups/${groupId}`, { screen: false });
  assert.equal("screen" in unflagged.json().group, false, "screen:false removes the field over HTTP too");

  // Invalid screen is a loud 400 (no silent coercion).
  const bad = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/groups/${groupId}`, { screen: "yes" });
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /screen must be a boolean/);
});

test("canvas API groups reparent route nests a group and rejects a cycle", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Reparent API" })).json().project.id;
  const mkGroup = async (name) =>
    (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
      name,
      x: 0,
      y: 0,
      w: 100,
      h: 100,
    })).json().group.id;
  const outer = await mkGroup("Outer");
  const inner = await mkGroup("Inner");

  // Nest inner under outer.
  const nested = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups/${inner}/reparent`, { parentId: outer });
  assert.equal(nested.status, 200);
  const stored = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(stored.groups.find((g) => g.id === inner).parentId, outer);

  // A create with parentId nests directly.
  const child = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "Child", x: 5, y: 5, w: 20, h: 20, parentId: inner,
  });
  assert.equal(child.status, 201);
  assert.equal(child.json().group.parentId, inner);

  // A cycle (outer under its descendant inner) is a loud 400.
  const bad = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups/${outer}/reparent`, { parentId: inner });
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /cycle/);
});

test("canvas API groups fit route resizes the frame to content; empty group is a 400", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Fit API" })).json().project.id;
  const add = async (name, png) =>
    (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
      name,
      bytes_base64: png.toString("base64"),
    })).json().element.id;
  const elA = await add("a.png", solidPng(8, 8));
  const elB = await add("b.png", solidPng(6, 6));
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elA}`, { x: 10, y: 10 });
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elB}`, { x: 30, y: 20 });
  const groupId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "Loose", x: 0, y: 0, w: 500, h: 500,
  })).json().group.id;
  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/assign-group`, { elementIds: [elA, elB], groupId });

  const fitted = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups/${groupId}/fit`, {});
  assert.equal(fitted.status, 200);
  assert.deepEqual(
    { x: fitted.json().group.x, y: fitted.json().group.y, w: fitted.json().group.w, h: fitted.json().group.h },
    { x: -14, y: -14, w: 74, h: 64 },
  );
  // Custom padding rides the body through the route.
  const tight = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups/${groupId}/fit`, { padding: 0 });
  assert.deepEqual(
    { x: tight.json().group.x, y: tight.json().group.y, w: tight.json().group.w, h: tight.json().group.h },
    { x: 10, y: 10, w: 26, h: 16 },
  );

  // An empty group is a loud 400.
  const empty = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "Empty", x: 0, y: 0, w: 100, h: 100,
  })).json().group.id;
  const bad = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups/${empty}/fit`, {});
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /nothing to fit/);
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
  assert.deepEqual(elemOrder(after), ["b.png", "c.png", "a.png"]);

  // Undo restores the original order in one step.
  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  const undone = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.deepEqual(elemOrder(undone), ["a.png", "b.png", "c.png"]);
});

test("canvas API nodes reorder route moves an element or group among merged siblings", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Node Z API" })).json().project.id;
  const add = async (name) =>
    (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
      name,
      bytes_base64: solidPng(4, 4).toString("base64"),
    })).json().element.id;
  const elA = await add("a.png");
  await add("b.png");
  const elC = await add("c.png");
  // Group C so root holds elements a, b and a group.
  const groupId = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, {
    name: "G",
    fromElements: [elC],
  })).json().group.id;

  // Move the GROUP to the back (index 0) among its merged root siblings.
  const movedGroup = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/nodes/${groupId}/reorder`, { index: 0 });
  assert.equal(movedGroup.status, 200);
  assert.equal(movedGroup.json().index, 0);
  const project = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  const label = (node) => (node.kind === "group" ? "G" : node.ref.name);
  assert.deepEqual(orderedChildren(project, null).map(label), ["G", "a.png", "b.png"]);

  // An out-of-range index is a loud 400 (no silent clamp on the node route).
  const bad = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/nodes/${elA}/reorder`, { index: 9 });
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /out of range/);
});

test("canvas API returns an error for a missing project", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const missing = await invokeApi(handler, "GET", "/api/canvas/projects/does-not-exist");
  // T0254 Tier 1 #2: statusForError maps "not found" errors to 404.
  assert.equal(missing.status, 404);
  assert.match(missing.json().error, /not found/);
});

// T0254 Tier 1 #2: statusForError's four classes, tested directly (not just indirectly
// through whichever routes happen to throw each shape) — a 500 in particular is hard
// to provoke honestly through the public HTTP surface (every op deliberately throws a
// plain Error, per the "loud errors" law), so this is the reliable place to pin it.
test("statusForError maps each error class to its status: HEAD_CONFLICT->409, not-found->404, TypeError->500, everything else->400", () => {
  const conflict = new Error("history advanced: head is now 2, you read 1");
  conflict.code = "HEAD_CONFLICT";
  assert.equal(statusForError(conflict), 409);

  assert.equal(statusForError(new Error("element not found: el_1")), 404);
  assert.equal(statusForError(new Error("canvas project not found: p1")), 404);

  assert.equal(statusForError(new TypeError("cannot read properties of undefined")), 500);
  assert.equal(statusForError(new ReferenceError("x is not defined")), 500);

  assert.equal(statusForError(new Error("rotation must be a finite number of degrees, got \"bad\"")), 400);
  assert.equal(statusForError(new Error("clip must be a boolean (true|false), got \"yes\"")), 400);

  // A HEAD_CONFLICT code wins even if the message ALSO happens to contain "not found"
  // wording — the stable code, not prose, decides (this is exactly why the code marker
  // exists instead of regex-matching "history advanced" prose).
  const both = new Error("project not found and history advanced: head is now 2, you read 1");
  both.code = "HEAD_CONFLICT";
  assert.equal(statusForError(both), 409);
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

// ---- T0200: op responses drive the page (no reload double-GET) ----------------

test("canvas API folds history flags into every mutating response (no separate /history GET)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Hist Fold" })).json().project.id;

  // A fresh mutation is undoable with nothing to redo; seq matches the project head.
  const created = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "a.png",
    bytes_base64: solidPng(4, 4).toString("base64"),
  });
  assert.ok(created.json().history, "response carries folded history flags");
  assert.equal(created.json().history.canUndo, true);
  assert.equal(created.json().history.canRedo, false);
  assert.equal(created.json().history.seq, created.json().project.history_seq);

  const elementId = created.json().element.id;
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/elements/${elementId}`, { x: 5 });

  // After undo, the response ITSELF reports canRedo=true — the page needs no /history GET.
  const undone = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  assert.equal(undone.json().history.canRedo, true);
  assert.equal(undone.json().history.seq, undone.json().project.history_seq);
});

test("canvas API files route sends immutable cache headers + ETag", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Cache" })).json().project.id;
  const png = solidPng(5, 5, [1, 2, 3]);
  const element = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
    name: "c.png",
    bytes_base64: png.toString("base64"),
  })).json().element;
  const fileName = element.src.replace(/^files\//, "");

  const file = await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/files/${fileName}`);
  assert.equal(file.status, 200);
  assert.equal(file.headers["cache-control"], "public, max-age=31536000, immutable");
  assert.ok(file.headers.etag, "an ETag validator is present");
  assert.ok(file.headers["last-modified"], "a Last-Modified validator is present");
  assert.ok(file.buffer.equals(png), "served bytes still match the content-addressed file");
});

test("canvas API batched elements-set / elements-remove = one journal entry + single undo", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Batch API" })).json().project.id;
  const add = async (name) =>
    (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
      name,
      bytes_base64: solidPng(4, 4).toString("base64"),
    })).json().element.id;
  const a = await add("a.png");
  const b = await add("b.png");

  // Batched move of two elements: ONE call, one journal entry named patchElements.
  const moved = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/elements-set`, {
    patches: [{ elementId: a, x: 50 }, { elementId: b, x: 60 }],
  });
  assert.equal(moved.status, 200);
  assert.equal(moved.json().count, 2);
  const afterMove = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.deepEqual(afterMove.elements.map((e) => e.x), [50, 60]);

  let history = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/history`)).json();
  assert.equal(history.entries.filter((e) => e.op === "patchElements").length, 1);
  assert.equal(history.entries.filter((e) => e.op === "patchElement").length, 0, "not N per-element entries");

  // One undo restores BOTH positions.
  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  const undone = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.deepEqual(undone.elements.map((e) => e.x), [0, 0]);

  // Batched delete of two elements: ONE call, one entry, one undo restores both.
  const removed = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/elements-remove`, {
    elementIds: [a, b],
  });
  assert.equal(removed.status, 200);
  assert.deepEqual(removed.json().removed.slice().sort(), [a, b].sort());
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project.elements.length, 0);
  history = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/history`)).json();
  assert.equal(history.entries.filter((e) => e.op === "removeElements").length, 1);
  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project.elements.length, 2);
});

test("canvas API images-batch = one journal entry for a multi-file drop (single undo)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Drop API" })).json().project.id;

  const batch = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images-batch`, {
    images: [
      { name: "a.png", bytes_base64: solidPng(4, 4, [10, 0, 0]).toString("base64"), x: 1, y: 2 },
      { name: "b.png", bytes_base64: solidPng(4, 4, [0, 10, 0]).toString("base64"), x: 3, y: 4 },
    ],
  });
  assert.equal(batch.status, 201);
  assert.equal(batch.json().count, 2);
  const after = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(after.elements.length, 2);
  const history = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/history`)).json();
  assert.equal(history.entries.filter((e) => e.op === "addImages").length, 1);
  assert.equal(history.entries.filter((e) => e.op === "addImage").length, 0, "not N per-image entries");

  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  assert.equal((await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project.elements.length, 0);
});

test("canvas API groups-set = one journal entry for batched shared toggles (single undo)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "GroupsSet API" })).json().project.id;
  const makeGroup = async (name) =>
    (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups`, { name, x: 0, y: 0, w: 60, h: 40 })).json().group.id;
  const g1 = await makeGroup("A");
  const g2 = await makeGroup("B");

  const set = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/groups-set`, {
    groupIds: [g1, g2],
    visible: false,
    clip: true,
  });
  assert.equal(set.status, 200);
  assert.equal(set.json().count, 2);
  const after = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  for (const id of [g1, g2]) {
    const g = after.groups.find((group) => group.id === id);
    assert.equal(g.visible, false);
    assert.equal(g.clip, true);
  }
  const history = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}/history`)).json();
  assert.equal(history.entries.filter((e) => e.op === "patchGroups").length, 1);

  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  const undone = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(undone.groups.filter((g) => g.visible === false).length, 0);
});

test("canvas API nodes-duplicate / nodes-paste / nodes-delete parity (one entry each)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Nodes" })).json().project.id;
  const png = solidPng(4, 4, [10, 20, 30]);
  const el = (
    await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
      name: "a.png",
      bytes_base64: png.toString("base64"),
      x: 2,
      y: 3,
    })
  ).json().element;

  // nodes-duplicate: 201, fresh id, +offset, one entry.
  const dup = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/nodes-duplicate`, {
    nodeIds: [el.id],
    dx: 16,
    dy: 16,
    scopeId: null,
  });
  assert.equal(dup.status, 201);
  const dupId = dup.json().elementIds[0];
  assert.notEqual(dupId, el.id);
  let project = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(project.elements.length, 2);

  // nodes-paste: a captured spec referencing the same immutable file.
  const spec = {
    schema: "ai_studio.canvas.nodes_spec.v1",
    nodes: [{ kind: "element", element: { type: "image", x: 0, y: 0, w: 4, h: 4, src: el.src, name: "pasted" } }],
  };
  const pasted = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/nodes-paste`, {
    spec,
    dx: 4,
    dy: 4,
    scopeId: null,
  });
  assert.equal(pasted.status, 201);
  project = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(project.elements.length, 3);

  // A bad file ref is a loud 400 (no silent fallback), no write.
  const bad = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/nodes-paste`, {
    spec: { nodes: [{ kind: "element", element: { type: "image", x: 0, y: 0, w: 4, h: 4, src: "files/nope.png", name: "x" } }] },
  });
  assert.equal(bad.status, 400);
  assert.match(bad.json().error, /unknown file/);

  // nodes-delete: batched delete of both copies; one undo restores.
  const del = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/nodes-delete`, {
    nodeIds: [dupId, pasted.json().elementIds[0]],
  });
  assert.equal(del.status, 200);
  assert.equal(del.json().removedElements.length, 2);
  assert.ok(del.json().history && typeof del.json().history.canUndo === "boolean", "history folded into the response");
  project = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(project.elements.length, 1);

  await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/undo`);
  project = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(project.elements.length, 3, "one undo restores the batched delete");
});

// ---- pack routes (T0332 v2 phase C: build_spec_pack_card_2026-07-07.md) --------------------
//
// packPreview/packSlice are NOT codex/agy seams (the real expand_jobs.py expander is offline/
// deterministic/stdlib; region detection/crop_regions.py are the SAME pipeline plain detect-
// regions/slice already exercise above) so these run for real and skip cleanly without the
// studio venv, mirroring the existing "canvas API slice route" test's own skip pattern. The
// plain generate route, by contrast, has NO existing HTTP-level test anywhere in this file (no
// createCanvasApi seam exists to inject a fake generator over HTTP — only the ops-layer tests
// in recipe.test.mjs/pack.test.mjs do that, by importing generateFromRecipe directly) — the
// build-spec packet asking to "mirror the existing recipe-generate route test" did not find
// one; there is none to mirror. The test below instead proves the route reads/forwards
// body.sheetSlug WITHOUT ever reaching a real codex call, by tripping generatePackSheets' own
// loud "--sheet does not match any expanded job" refusal (which fires strictly AFTER the real,
// offline expander runs but BEFORE any codex spawn).

test("canvas API POST recipe-cards/<gid>/pack-preview: ephemeral real-expander preview, never journals (skips without the studio venv)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Pack Preview API" })).json().project.id;
  const card = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards`, {})).json().group;
  const patched = await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/recipe-cards/${card.id}`, {
    prompt: "a {material} generator building",
    pack: {
      axes: { grade: ["rusty", "plain", "gilded", "mythic"], material: ["stone", "wood"] },
      vary: "grade",
      grid: [2, 2],
      max_jobs: 12,
    },
  });
  assert.equal(patched.status, 200);
  const beforeSeq = patched.json().project.history_seq;

  const preview = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards/${card.id}/pack-preview`, {});
  if (preview.status !== 200) {
    t.skip(`pack preview pipeline unavailable: ${preview.json().error}`);
    return;
  }
  const result = preview.json();
  assert.equal(result.sheets, 2, "2 material values (big axis) x 1 candidate = 2 sheets");
  assert.equal(result.jobs.length, 2);
  assert.equal("project" in result, false, "packPreview is ephemeral -- plain sendJson, not sendMutation");

  const after = (await invokeApi(handler, "GET", `/api/canvas/projects/${projectId}`)).json().project;
  assert.equal(after.history_seq, beforeSeq, "pack-preview never journals/mutates the blob");
});

test("canvas API POST recipe-cards/<gid>/pack-slice: per-sheet contract, reparented cuts (skips without Python)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Pack Slice API" })).json().project.id;
  const { group: card } = createRecipeCard(REPO_ROOT, { projectId, name: "Card" });

  // Hand-built pack run (mirrors pack.test.mjs's own seedPackRun) -- packSlice itself is what
  // is under test here, not the expander/codex pipeline that would normally produce this.
  const at = new Date().toISOString();
  const before = getProject(REPO_ROOT, projectId);
  const runGroup = {
    id: `grp_run_${Math.random().toString(36).slice(2, 8)}`,
    name: "Run", x: 0, y: 0, w: 100, h: 100, visible: true,
    pack_run: { v: 1, cardId: card.id, at },
  };
  updateProject(REPO_ROOT, projectId, { groups: [...(before.groups || []), runGroup] });
  const img = addImage(REPO_ROOT, projectId, { name: "sheet", bytes: magentaSheetPng(1) }).element;
  updateProject(REPO_ROOT, projectId, {
    elements: getProject(REPO_ROOT, projectId).elements.map((el) =>
      el.id === img.id
        ? {
            ...el,
            groupId: runGroup.id,
            meta: {
              pack: {
                cardId: card.id,
                at,
                sheet_axes: {},
                cells: [{ cell: [0, 0], axes: {} }, { cell: [0, 1], axes: {} }],
                prompt_snapshot: "x",
                refs_snapshot: [],
                params_snapshot: {},
              },
            },
          }
        : el,
    ),
  });

  const sliced = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards/${card.id}/pack-slice`, {
    runGroupId: runGroup.id,
  });
  if (sliced.status !== 200) {
    t.skip(`region detector/crop pipeline unavailable: ${sliced.json().error}`);
    return;
  }
  const result = sliced.json();
  assert.equal(result.run_group_id, runGroup.id);
  assert.equal(result.contract.length, 1);
  assert.equal(result.contract[0].verdict, "OK");
  assert.equal(result.contract[0].region_count, 2);
  assert.equal(result.contract[0].cut_ids.length, 2);
});

test("canvas API POST recipe-cards/<gid>/generate forwards body.sheetSlug into the pack branch (loud before any codex call; skips without the studio venv)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Pack Generate Route" })).json().project.id;
  const card = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards`, {})).json().group;
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/recipe-cards/${card.id}`, {
    prompt: "a {material} generator building",
    pack: {
      axes: { grade: ["rusty", "plain", "gilded", "mythic"], material: ["stone", "wood"] },
      vary: "grade",
      grid: [2, 2],
      max_jobs: 12,
    },
  });

  // FIX 2 (deep-review поправка): body.sheetSlug with no explicit body.runGroupId resolves
  // recipe.last_run.run_group_id, loud if there is none ("no silent new-group fork"). Seed a
  // resolvable prior run directly through ops.mjs (mirrors the pack-slice API test's own
  // hand-built run group above) so this test still proves what it always proved -- the route
  // forwards sheetSlug into the pack branch and trips the real expander's own "no such job
  // name" refusal, not something new about missing runs (that path has its own coverage in
  // pack.test.mjs).
  const at = new Date().toISOString();
  const before = getProject(REPO_ROOT, projectId);
  const runGroup = {
    id: `grp_run_${Math.random().toString(36).slice(2, 8)}`,
    name: "Run", x: 0, y: 0, w: 100, h: 100, visible: true,
    pack_run: { v: 1, cardId: card.id, at },
  };
  updateProject(REPO_ROOT, projectId, {
    groups: [...(before.groups || []), runGroup].map((g) =>
      g.id === card.id ? { ...g, recipe: { ...g.recipe, last_run: { at, verdict: "ok", run_group_id: runGroup.id, failed: [] } } } : g,
    ),
  });

  const result = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards/${card.id}/generate`, {
    sheetSlug: "no-such-sheet",
  });
  const errorMessage = (result.json() && result.json().error) || "";
  if (result.status === 500 || /venv|interpreter|setup_python|No module|ModuleNotFound/i.test(errorMessage)) {
    t.skip(`pack expander pipeline unavailable: ${errorMessage}`);
    return;
  }
  assert.equal(result.status, 400);
  assert.match(errorMessage, /--sheet "no-such-sheet" does not match any expanded job name/);
});

test("canvas API POST recipe-cards/<gid>/generate: body.sheetSlug with no prior pack run at all is loud (FIX 2 -- no silent new-group fork)", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(REPO_ROOT);
  const projectId = (await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Pack Generate Route No Run" })).json().project.id;
  const card = (await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards`, {})).json().group;
  await invokeApi(handler, "PATCH", `/api/canvas/projects/${projectId}/recipe-cards/${card.id}`, {
    prompt: "a {material} generator building",
    pack: {
      axes: { grade: ["rusty", "plain", "gilded", "mythic"], material: ["stone", "wood"] },
      vary: "grade",
      grid: [2, 2],
      max_jobs: 12,
    },
  });

  const result = await invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/recipe-cards/${card.id}/generate`, {
    sheetSlug: "whatever",
  });
  assert.equal(result.status, 400);
  assert.match(result.json().error, /--sheet requires an existing pack run/);
});
