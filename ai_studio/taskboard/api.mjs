// Taskboard HTTP API adapter.
//
// Studio Shell mounts this handler, but Taskboard owns the JSON payloads and
// mutation routes.

import {
  createEpic,
  createTask,
  EPIC_STATUSES,
  listEpics,
  listTasks,
  PRIORITIES,
  TASK_STATUSES,
  updateDoc,
} from "./lib.mjs";
import { relative } from "node:path";

const ACTIVE_TASK_STATUSES = ["backlog", "todo", "doing", "review"];

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

function priorityRank(priority) {
  return { P0: 0, P1: 1, P2: 2, P3: 3 }[priority] ?? 9;
}

function idNumber(doc) {
  const match = String(doc.fields.id || "").match(/\d+/);
  return match ? Number(match[0]) : 0;
}

function taskRank(task) {
  const statusRank = { doing: 0, todo: 1, backlog: 2, review: 3, idea: 4, done: 5, dropped: 6 }[task.fields.status] ?? 9;
  return statusRank * 10 + priorityRank(task.fields.priority);
}

function countsByStatus(docs, statuses) {
  const counts = Object.fromEntries(statuses.map((status) => [status, 0]));
  for (const doc of docs) {
    const status = doc.fields.status || "unknown";
    counts[status] = (counts[status] || 0) + 1;
  }
  return counts;
}

export function agentTaskRow(root, doc, options = {}) {
  const row = {
    id: doc.fields.id,
    title: doc.fields.title,
    status: doc.fields.status,
    priority: doc.fields.priority || "",
    epic: doc.fields.epic || "",
    tags: doc.fields.tags || [],
    archived: doc.archived === true,
    file: relative(root, doc.file).replace(/\\/g, "/"),
  };
  if (options.includeBody) {
    row.body = doc.body;
  }
  return row;
}

export function agentEpicRow(root, doc) {
  return {
    id: doc.fields.id,
    title: doc.fields.title,
    status: doc.fields.status,
    priority: doc.fields.priority || "",
    tags: doc.fields.tags || [],
    file: relative(root, doc.file).replace(/\\/g, "/"),
  };
}

export function currentWorkRows(root, limit = 25) {
  return listTasks(root)
    .filter((task) => ACTIVE_TASK_STATUSES.includes(task.fields.status))
    .sort((a, b) => taskRank(a) - taskRank(b) || idNumber(b) - idNumber(a) || String(a.fields.id).localeCompare(String(b.fields.id)))
    .slice(0, limit)
    .map((task) => agentTaskRow(root, task));
}

export function agentContextPayload(root, options = {}) {
  const limit = Number.isFinite(Number(options.limit)) ? Number(options.limit) : 25;
  const tasks = listTasks(root);
  const epics = listEpics(root);
  const currentWork = currentWorkRows(root, limit);
  return {
    schema: "ai_studio.taskboard.agent_context.v1",
    root,
    counts: {
      tasks: countsByStatus(tasks, TASK_STATUSES),
      epics: countsByStatus(epics, EPIC_STATUSES),
      currentWork: tasks.filter((task) => ACTIVE_TASK_STATUSES.includes(task.fields.status)).length,
      review: tasks.filter((task) => task.fields.status === "review").length,
    },
    currentWork,
    agentNextStep: "Open only the task file(s) needed for the current decision; do not scan archives unless linked.",
  };
}

export function boardPayload(root) {
  return {
    root,
    taskStatuses: TASK_STATUSES,
    epicStatuses: EPIC_STATUSES,
    priorities: PRIORITIES,
    tasks: listTasks(root).map(publicDoc),
    epics: listEpics(root).map(publicDoc),
  };
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
  };
}
