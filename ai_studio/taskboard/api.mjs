// Taskboard HTTP API adapter.
//
// Studio Shell mounts this handler, but Taskboard owns the JSON payloads and
// mutation routes.

import {
  createEpic,
  createProject,
  createTask,
  findDoc,
  agentContextPayload,
  boardPayload,
  listEpics,
  listProjects,
  listTasks,
  publicDoc,
  updateDoc,
} from "./lib.mjs";

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

export function createTaskboardApi(root) {
  return async function handleTaskboardApi(req, res, url) {
    const parts = url.pathname.split("/").filter(Boolean);
    try {
      if (req.method === "GET" && url.pathname === "/api/board") {
        return sendJson(res, 200, boardPayload(root));
      }
      if (req.method === "GET" && url.pathname === "/api/agent/context") {
        return sendJson(res, 200, agentContextPayload(root));
      }
      if (req.method === "GET" && url.pathname === "/api/projects") {
        return sendJson(res, 200, { projects: listProjects(root).map((doc) => publicDoc(doc)) });
      }
      if (req.method === "GET" && url.pathname === "/api/epics") {
        return sendJson(res, 200, { epics: listEpics(root).map((doc) => publicDoc(doc)) });
      }
      if (req.method === "GET" && url.pathname === "/api/tasks") {
        return sendJson(res, 200, { tasks: listTasks(root).map((doc) => publicDoc(doc)) });
      }
      if (req.method === "GET" && parts.length === 3 && ["tasks", "epics", "projects"].includes(parts[1])) {
        const expectedKind = { tasks: "task", epics: "epic", projects: "project" }[parts[1]];
        const doc = findDoc(root, parts[2]);
        if (!doc || doc.kind !== expectedKind) {
          return sendJson(res, 404, { error: `${expectedKind} not found: ${parts[2]}` });
        }
        return sendJson(res, 200, publicDoc(doc, { includeBody: true }));
      }
      if (req.method === "POST" && url.pathname === "/api/tasks") {
        const input = await readBody(req);
        return sendJson(res, 201, publicDoc(createTask(root, input), { includeBody: true }));
      }
      if (req.method === "POST" && url.pathname === "/api/epics") {
        const input = await readBody(req);
        return sendJson(res, 201, publicDoc(createEpic(root, input), { includeBody: true }));
      }
      if (req.method === "POST" && url.pathname === "/api/projects") {
        const input = await readBody(req);
        return sendJson(res, 201, publicDoc(createProject(root, input), { includeBody: true }));
      }
      if (req.method === "PATCH" && parts.length === 3 && ["tasks", "epics", "projects"].includes(parts[1])) {
        const expectedKind = { tasks: "task", epics: "epic", projects: "project" }[parts[1]];
        const doc = findDoc(root, parts[2]);
        if (!doc || doc.kind !== expectedKind) {
          return sendJson(res, 404, { error: `${expectedKind} not found: ${parts[2]}` });
        }
        const patch = await readBody(req);
        return sendJson(res, 200, publicDoc(updateDoc(root, parts[2], patch), { includeBody: true }));
      }
      return sendJson(res, 404, { error: "not found" });
    } catch (err) {
      return sendJson(res, err.conflict ? 409 : 400, { error: err.message });
    }
  };
}
