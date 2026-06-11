// Taskboard web UI server. No dependencies.
//
//   node tools/taskboard/server.mjs            -> http://127.0.0.1:8070/
//   PORT=9000 node tools/taskboard/server.mjs
//
// Serves the kanban UI from public/ and a small JSON API over the markdown
// task store (tasks/ at the repo root).

import { createReadStream, existsSync } from "node:fs";
import { createServer } from "node:http";
import { extname, join, normalize, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import {
  findRoot, listTasks, listEpics, createTask, createEpic, updateDoc,
  TASK_STATUSES, EPIC_STATUSES, PRIORITIES,
} from "./lib.mjs";

const publicDir = resolve(fileURLToPath(new URL("./public", import.meta.url)));
const root = findRoot();
const port = Number(process.env.PORT || 8070);

const mime = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".svg": "image/svg+xml",
};

function sendJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(data));
}

function readBody(req) {
  return new Promise((resolveBody, reject) => {
    let data = "";
    req.on("data", (chunk) => {
      data += chunk;
      if (data.length > 1_000_000) {
        reject(new Error("body too large"));
        req.destroy();
      }
    });
    req.on("end", () => {
      try {
        resolveBody(data ? JSON.parse(data) : {});
      } catch {
        reject(new Error("invalid JSON body"));
      }
    });
    req.on("error", reject);
  });
}

function publicDoc(doc) {
  return { kind: doc.kind, fields: doc.fields, body: doc.body, rev: doc.rev };
}

function boardPayload() {
  return {
    root,
    taskStatuses: TASK_STATUSES,
    epicStatuses: EPIC_STATUSES,
    priorities: PRIORITIES,
    tasks: listTasks(root).map(publicDoc),
    epics: listEpics(root).map(publicDoc),
  };
}

async function handleApi(req, res, url) {
  const parts = url.pathname.split("/").filter(Boolean); // ["api", ...]
  try {
    if (req.method === "GET" && url.pathname === "/api/board") {
      return sendJson(res, 200, boardPayload());
    }
    if (req.method === "POST" && url.pathname === "/api/tasks") {
      const input = await readBody(req);
      return sendJson(res, 201, publicDoc(createTask(root, input)));
    }
    if (req.method === "POST" && url.pathname === "/api/epics") {
      const input = await readBody(req);
      return sendJson(res, 201, publicDoc(createEpic(root, input)));
    }
    if (req.method === "PATCH" && parts.length === 3 && (parts[1] === "tasks" || parts[1] === "epics")) {
      const patch = await readBody(req);
      return sendJson(res, 200, publicDoc(updateDoc(root, parts[2], patch)));
    }
    return sendJson(res, 404, { error: "not found" });
  } catch (err) {
    return sendJson(res, err.conflict ? 409 : 400, { error: err.message });
  }
}

function serveStatic(res, urlPath) {
  const relativePath = urlPath === "/" ? "index.html" : urlPath.replace(/^\/+/, "");
  const full = resolve(join(publicDir, normalize(relativePath)));
  if (!full.startsWith(publicDir) || !existsSync(full)) {
    res.writeHead(404, { "content-type": "text/plain" });
    res.end("not found");
    return;
  }
  res.writeHead(200, { "content-type": mime[extname(full)] || "application/octet-stream" });
  createReadStream(full).pipe(res);
}

const server = createServer((req, res) => {
  const url = new URL(req.url, `http://${req.headers.host || "localhost"}`);
  if (url.pathname.startsWith("/api/")) {
    handleApi(req, res, url);
  } else {
    serveStatic(res, url.pathname);
  }
});

server.listen(port, "127.0.0.1", () => {
  console.log(`taskboard: http://127.0.0.1:${port}/  (store: ${join(root, "tasks")})`);
});
