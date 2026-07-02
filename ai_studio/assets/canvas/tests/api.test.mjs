// Canvas HTTP API tests using an in-process invoke harness (no port binding).
// Run: node --test ai_studio/assets/canvas/tests/api.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL } from "node:url";
import { createCanvasApi } from "../api.mjs";
import { solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";

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
