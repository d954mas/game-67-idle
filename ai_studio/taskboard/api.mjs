// Taskboard HTTP API adapter.
//
// Studio Shell mounts this handler, but Taskboard owns the JSON payloads and
// mutation routes.

import {
  createEpic,
  createProject,
  createTask,
  listEpics,
  listProjects,
  listTasks,
  publicDoc,
  updateDoc,
} from "./lib.mjs";
import {
  agentContextPayloadForStores,
  boardPayloadForStores,
  findTaskboardDoc,
  mutationStore,
  storeOptions,
  taskboardStoresForQuery,
} from "./stores.mjs";

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

function queryOptions(url) {
  return {
    store: url.searchParams.get("store") || "",
    game: url.searchParams.get("game") || "",
    includePrivate: ["1", "true", "yes"].includes(String(url.searchParams.get("includePrivate") || "").toLowerCase()),
  };
}

function storesFromQuery(root, url) {
  return taskboardStoresForQuery(root, queryOptions(url));
}

function collectionPayload(root, stores, kind) {
  const key = `${kind}s`;
  const list = kind === "project" ? listProjects : (kind === "epic" ? listEpics : listTasks);
  return {
    [key]: stores.flatMap((store) =>
      list(root, storeOptions(store)).map((doc) => publicDoc(doc, { store }))
    ),
  };
}

function mutationOptionsFromInput(url, input) {
  return {
    ...queryOptions(url),
    store: input.store || input.storeId || url.searchParams.get("store") || "",
    game: input.game || input.gameId || url.searchParams.get("game") || "",
  };
}

export function createTaskboardApi(root) {
  return async function handleTaskboardApi(req, res, url) {
    const parts = url.pathname.split("/").filter(Boolean);
    try {
      if (req.method === "GET" && url.pathname === "/api/board") {
        return sendJson(res, 200, boardPayloadForStores(root, storesFromQuery(root, url)));
      }
      if (req.method === "GET" && url.pathname === "/api/agent/context") {
        return sendJson(res, 200, agentContextPayloadForStores(root, storesFromQuery(root, url)));
      }
      if (req.method === "GET" && url.pathname === "/api/projects") {
        return sendJson(res, 200, collectionPayload(root, storesFromQuery(root, url), "project"));
      }
      if (req.method === "GET" && url.pathname === "/api/epics") {
        return sendJson(res, 200, collectionPayload(root, storesFromQuery(root, url), "epic"));
      }
      if (req.method === "GET" && url.pathname === "/api/tasks") {
        return sendJson(res, 200, collectionPayload(root, storesFromQuery(root, url), "task"));
      }
      if (req.method === "GET" && parts.length === 3 && ["tasks", "epics", "projects"].includes(parts[1])) {
        const expectedKind = { tasks: "task", epics: "epic", projects: "project" }[parts[1]];
        const resolved = findTaskboardDoc(root, parts[2], queryOptions(url));
        const doc = resolved ? resolved.doc : null;
        if (!doc || doc.kind !== expectedKind) {
          return sendJson(res, 404, { error: `${expectedKind} not found: ${parts[2]}` });
        }
        return sendJson(res, 200, publicDoc(doc, { store: resolved.store, includeBody: true }));
      }
      if (req.method === "POST" && url.pathname === "/api/tasks") {
        const input = await readBody(req);
        const store = mutationStore(root, mutationOptionsFromInput(url, input));
        return sendJson(res, 201, publicDoc(createTask(root, input, storeOptions(store)), { store, includeBody: true }));
      }
      if (req.method === "POST" && url.pathname === "/api/epics") {
        const input = await readBody(req);
        const store = mutationStore(root, mutationOptionsFromInput(url, input));
        return sendJson(res, 201, publicDoc(createEpic(root, input, storeOptions(store)), { store, includeBody: true }));
      }
      if (req.method === "POST" && url.pathname === "/api/projects") {
        const input = await readBody(req);
        const store = mutationStore(root, mutationOptionsFromInput(url, input));
        return sendJson(res, 201, publicDoc(createProject(root, input, storeOptions(store)), { store, includeBody: true }));
      }
      if (req.method === "PATCH" && parts.length === 3 && ["tasks", "epics", "projects"].includes(parts[1])) {
        const expectedKind = { tasks: "task", epics: "epic", projects: "project" }[parts[1]];
        const resolved = findTaskboardDoc(root, parts[2], queryOptions(url));
        const doc = resolved ? resolved.doc : null;
        if (!doc || doc.kind !== expectedKind) {
          return sendJson(res, 404, { error: `${expectedKind} not found: ${parts[2]}` });
        }
        const patch = await readBody(req);
        return sendJson(res, 200, publicDoc(updateDoc(root, resolved.id, patch, storeOptions(resolved.store)), { store: resolved.store, includeBody: true }));
      }
      return sendJson(res, 404, { error: "not found" });
    } catch (err) {
      return sendJson(res, err.conflict ? 409 : 400, { error: err.message });
    }
  };
}
