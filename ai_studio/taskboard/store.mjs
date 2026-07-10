// Taskboard markdown store.
//
// One project/epic/task is one markdown file under ai_studio/taskboard/items/.
// This file intentionally owns the small private details too: statuses,
// frontmatter parsing, paths, templates, mutation, and validation. Keep the
// public facade in lib.mjs small; do not split this again without a real need.

import { existsSync, mkdirSync, readdirSync, readFileSync, renameSync, rmSync, statSync, writeFileSync } from "node:fs";
import { randomUUID } from "node:crypto";
import { basename, dirname, join, relative, resolve } from "node:path";

export const TASK_STATUSES = ["idea", "backlog", "todo", "doing", "review", "done"];
export const EPIC_STATUSES = ["idea", "active", "done"];
export const PROJECT_STATUSES = ["idea", "active", "done"];
export const PROJECT_KINDS = ["ai-studio", "game", "template", "tooling", "research", "other"];
export const PRIORITIES = ["P0", "P1", "P2", "P3"];
export const ACTIVE_TASK_STATUSES = ["backlog", "todo", "doing", "review"];
export const DEFAULT_TASKBOARD_STORE = {
  storeId: "studio",
  visibility: "public",
};

const TASK_BODY_TEMPLATE = `## What

## Done when

- [ ]

## Open questions

## Log
`;

const EPIC_BODY_TEMPLATE = `## Goal

## In scope

## Out of scope

## Log
`;

const PROJECT_BODY_TEMPLATE = `## Goal

## In scope

## Out of scope

## Log
`;

export function findRoot(start = process.cwd()) {
  if (process.env.TASKBOARD_ROOT) {
    return resolve(process.env.TASKBOARD_ROOT);
  }
  let dir = resolve(start);
  for (;;) {
    if (existsSync(join(dir, ".git")) || existsSync(join(dir, "ai_studio"))) {
      return dir;
    }
    const parent = dirname(dir);
    if (parent === dir) {
      return resolve(start);
    }
    dir = parent;
  }
}

function itemDir(root, options = {}) {
  const itemsRoot = options.itemsRoot || (options.store && options.store.itemsRoot);
  return itemsRoot ? resolve(root, itemsRoot) : join(root, "ai_studio", "taskboard", "items");
}

function projectDir(root, options = {}) {
  return join(itemDir(root, options), "projects");
}

function activeTaskDir(root, options = {}) {
  return join(itemDir(root, options), "active");
}

function archiveTaskDir(root, options = {}) {
  return join(itemDir(root, options), "archive");
}

function epicDir(root, options = {}) {
  return join(itemDir(root, options), "epics");
}

function todayStamp() {
  return new Date().toISOString().slice(0, 10);
}

export function priorityRank(priority) {
  return { P0: 0, P1: 1, P2: 2, P3: 3 }[priority] ?? 9;
}

export function idNumber(doc) {
  const match = String(doc.fields.id || "").match(/\d+/);
  return match ? Number(match[0]) : 0;
}

export function taskRank(task) {
  const statusRank = { doing: 0, todo: 1, backlog: 2, review: 3, idea: 4, done: 5 }[task.fields.status] ?? 9;
  return statusRank * 10 + priorityRank(task.fields.priority);
}

export function countsByStatus(docs, statuses) {
  const counts = Object.fromEntries(statuses.map((status) => [status, 0]));
  for (const doc of docs) {
    const status = doc.fields.status || "unknown";
    counts[status] = (counts[status] || 0) + 1;
  }
  return counts;
}

function statusesForKind(kind) {
  if (kind === "task") return TASK_STATUSES;
  if (kind === "epic") return EPIC_STATUSES;
  if (kind === "project") return PROJECT_STATUSES;
  return [];
}

export function parseDoc(text) {
  const lines = text.split(/\r?\n/);
  if (lines[0] !== "---") {
    return { fields: {}, body: text };
  }
  const end = lines.indexOf("---", 1);
  if (end === -1) {
    return { fields: {}, body: text };
  }
  const fields = {};
  for (const line of lines.slice(1, end)) {
    const match = line.match(/^([A-Za-z_][\w-]*):\s*(.*)$/);
    if (match) {
      fields[match[1]] = parseValue(match[2]);
    }
  }
  return { fields, body: lines.slice(end + 1).join("\n").replace(/^\n/, "") };
}

function parseValue(raw) {
  const value = raw.trim();
  if (value.startsWith("[") && value.endsWith("]")) {
    const inner = value.slice(1, -1).trim();
    return inner ? inner.split(",").map((item) => unquote(item.trim())) : [];
  }
  return unquote(value);
}

function unquote(value) {
  if (value.startsWith('"') && value.endsWith('"')) {
    try {
      return JSON.parse(value);
    } catch {
      return value.slice(1, -1);
    }
  }
  if (value.startsWith("'") && value.endsWith("'")) {
    return value.slice(1, -1);
  }
  return value;
}

function quoteIfNeeded(value) {
  const text = String(value);
  if (text === "" || /[:#\[\]{}"']|^\s|\s$/.test(text)) {
    return JSON.stringify(text);
  }
  return text;
}

export function serializeDoc(fields, body) {
  const out = ["---"];
  for (const [key, value] of Object.entries(fields)) {
    if (value === undefined || value === null) {
      continue;
    }
    if (Array.isArray(value)) {
      out.push(`${key}: [${value.map(quoteIfNeeded).join(", ")}]`);
    } else {
      out.push(`${key}: ${quoteIfNeeded(value)}`);
    }
  }
  out.push("---", "");
  out.push(body.replace(/\s+$/, ""), "");
  return out.join("\n");
}

export function slugify(title) {
  const slug = String(title)
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 48);
  return slug || "item";
}

function listDocs(dir, kind, options = {}) {
  if (!existsSync(dir)) {
    return [];
  }
  const ignoredDocs = new Set(["readme.md", "status.md"]);
  const docs = [];
  for (const name of readdirSync(dir)) {
    if (!name.endsWith(".md") || ignoredDocs.has(name.toLowerCase())) {
      continue;
    }
    const file = join(dir, name);
    if (!statSync(file).isFile()) {
      continue;
    }
    const { fields, body } = parseDoc(readFileSync(file, "utf8"));
    docs.push({
      kind,
      file,
      name,
      fields,
      body,
      archived: options.archived === true,
      rev: String(statSync(file).mtimeMs),
    });
  }
  docs.sort((a, b) => String(a.fields.id || a.name).localeCompare(String(b.fields.id || b.name)));
  return docs;
}

function docFromFile(file, kind, options = {}) {
  const { fields, body } = parseDoc(readFileSync(file, "utf8"));
  return {
    kind,
    file,
    name: basename(file),
    fields,
    body,
    archived: options.archived === true,
    rev: String(statSync(file).mtimeMs),
  };
}

function findDocInDir(dir, kind, id, options = {}) {
  if (!existsSync(dir)) {
    return null;
  }
  const prefix = `${id}-`;
  for (const name of readdirSync(dir)) {
    if (!name.endsWith(".md") || name.toLowerCase() === "readme.md" || name.toLowerCase() === "status.md") {
      continue;
    }
    if (name !== `${id}.md` && !name.startsWith(prefix)) {
      continue;
    }
    const doc = docFromFile(join(dir, name), kind, options);
    if (doc.fields.id === id) {
      return doc;
    }
  }
  return null;
}

function findArchivedTask(root, id, options = {}) {
  const archive = archiveTaskDir(root, options);
  if (!existsSync(archive)) {
    return null;
  }
  for (const group of readdirSync(archive)) {
    const dir = join(archive, group);
    if (!statSync(dir).isDirectory()) {
      continue;
    }
    const doc = findDocInDir(dir, "task", id, { archived: true });
    if (doc) {
      return doc;
    }
  }
  return null;
}

function listArchiveTasks(root, options = {}) {
  const archive = archiveTaskDir(root, options);
  if (!existsSync(archive)) {
    return [];
  }
  const docs = [];
  for (const group of readdirSync(archive)) {
    const dir = join(archive, group);
    if (statSync(dir).isDirectory()) {
      docs.push(...listDocs(dir, "task", { archived: true }));
    }
  }
  docs.sort((a, b) => String(a.fields.id || a.name).localeCompare(String(b.fields.id || b.name)));
  return docs;
}

export function listTasks(root, options = {}) {
  const active = listDocs(activeTaskDir(root, options), "task");
  return options.includeArchive ? [...active, ...listArchiveTasks(root, options)] : active;
}

export function listProjects(root, options = {}) {
  return listDocs(projectDir(root, options), "project");
}

export function listEpics(root, options = {}) {
  return listDocs(epicDir(root, options), "epic");
}

export function findDoc(root, id, options = {}) {
  const docId = String(id || "");
  if (docId.startsWith("P")) {
    return findDocInDir(projectDir(root, options), "project", docId);
  }
  if (docId.startsWith("E")) {
    return findDocInDir(epicDir(root, options), "epic", docId);
  }
  if (docId.startsWith("T")) {
    return findDocInDir(activeTaskDir(root, options), "task", docId) || findArchivedTask(root, docId, options);
  }
  const all = [...listProjects(root, options), ...listEpics(root, options), ...listTasks(root, { ...options, includeArchive: true })];
  return all.find((doc) => doc.fields.id === docId) || null;
}

function countersPath(root, options = {}) {
  return join(itemDir(root, options), ".counters.json");
}

function allocationLockPath(root, options = {}) {
  return join(itemDir(root, options), ".allocation.lock");
}

function readCounters(root, options = {}) {
  const path = countersPath(root, options);
  if (!existsSync(path)) return {};
  try {
    const parsed = JSON.parse(readFileSync(path, "utf8"));
    return parsed && typeof parsed === "object" && !Array.isArray(parsed) ? parsed : {};
  } catch {
    return {};
  }
}

function sleepSync(milliseconds) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, milliseconds);
}

function reclaimStaleAllocationLock(lockPath, staleMs, attempt) {
  let ageMs;
  try {
    ageMs = Date.now() - statSync(lockPath).mtimeMs;
  } catch (error) {
    if (error.code === "ENOENT") return true;
    throw error;
  }
  if (ageMs <= staleMs) return false;
  try {
    const owner = JSON.parse(readFileSync(join(lockPath, "owner.json"), "utf8"));
    if (Number.isInteger(owner.pid) && owner.pid > 0) {
      try {
        process.kill(owner.pid, 0);
        return false;
      } catch (error) {
        if (error.code === "EPERM") return false;
      }
    }
  } catch {
    // Missing or malformed metadata cannot make an expired lock permanent.
  }
  const stalePath = `${lockPath}.stale-${process.pid}-${attempt}-${randomUUID()}`;
  try {
    renameSync(lockPath, stalePath);
  } catch (error) {
    if (error.code === "ENOENT") return true;
    if (error.code === "EEXIST" || error.code === "EPERM" || error.code === "ENOTEMPTY") return false;
    throw error;
  }
  rmSync(stalePath, { recursive: true, force: true });
  return true;
}

function readAllocationOwner(lockPath) {
  try {
    return JSON.parse(readFileSync(join(lockPath, "owner.json"), "utf8"));
  } catch {
    return null;
  }
}

function releaseAllocationLock(lockPath, token) {
  const owner = readAllocationOwner(lockPath);
  if (!owner || owner.token !== token) return;
  rmSync(lockPath, { recursive: true, force: true });
}

function withAllocationLock(root, options, allocate) {
  const items = itemDir(root, options);
  const lockPath = allocationLockPath(root, options);
  const retryMs = Math.max(1, Number(options.allocationLockRetryMs) || 10);
  const timeoutMs = Math.max(retryMs, Number(options.allocationLockTimeoutMs) || 5000);
  const staleMs = Math.max(retryMs, Number(options.allocationLockStaleMs) || 30000);
  const deadline = Date.now() + timeoutMs;
  mkdirSync(items, { recursive: true });

  for (let attempt = 0; attempt === 0 || Date.now() < deadline; attempt += 1) {
    const token = randomUUID();
    const candidatePath = `${lockPath}.candidate-${process.pid}-${token}`;
    let acquired = false;
    try {
      mkdirSync(candidatePath);
      writeFileSync(join(candidatePath, "owner.json"), JSON.stringify({
        pid: process.pid,
        token,
        acquiredAt: new Date().toISOString(),
      }) + "\n", { flag: "wx" });
      renameSync(candidatePath, lockPath);
      acquired = true;
    } catch (error) {
      if (error.code !== "EEXIST" && error.code !== "EPERM" && error.code !== "ENOTEMPTY") throw error;
    } finally {
      if (!acquired) rmSync(candidatePath, { recursive: true, force: true });
    }
    if (acquired) {
      try {
        return allocate();
      } finally {
        releaseAllocationLock(lockPath, token);
      }
    }
    if (reclaimStaleAllocationLock(lockPath, staleMs, attempt)) continue;
    const remainingMs = deadline - Date.now();
    if (remainingMs > 0) sleepSync(Math.min(retryMs, remainingMs));
  }
  throw new Error(`Timed out waiting for Taskboard allocation lock ${lockPath}; recover it only if older than ${staleMs}ms`);
}

function writeCountersAtomic(root, counters, options = {}) {
  const target = countersPath(root, options);
  const temp = `${target}.tmp-${process.pid}-${Date.now()}`;
  try {
    writeFileSync(temp, JSON.stringify(counters, null, 2) + "\n", { flag: "wx" });
    renameSync(temp, target);
  } finally {
    rmSync(temp, { force: true });
  }
}

// Ids must stay monotonic even after archive pruning: scanning files alone
// rewinds the sequence when history is deleted, so the high-water mark persists
// in items/.counters.json and the scan only ever raises it.
function commitNextId(root, docs, prefix, pad, options = {}) {
  const counters = readCounters(root, options);
  let max = Number(counters[prefix]) || 0;
  for (const doc of docs) {
    const match = String(doc.fields.id || doc.name).match(new RegExp(`^${prefix}(\\d+)`));
    if (match) {
      max = Math.max(max, Number(match[1]));
    }
  }
  const next = max + 1;
  counters[prefix] = next;
  writeCountersAtomic(root, counters, options);
  return prefix + String(next).padStart(pad, "0");
}

function createAllocatedDoc(root, options, config) {
  mkdirSync(config.dir, { recursive: true });
  return withAllocationLock(root, options, () => {
    const maxAttempts = Math.max(1, Number(options.allocationMaxAttempts) || 100);
    for (let attempt = 0; attempt < maxAttempts; attempt += 1) {
      const id = commitNextId(root, config.listDocs(), config.prefix, config.pad, options);
      const fields = config.makeFields(id);
      const file = join(config.dir, `${id}-${slugify(fields.title)}.md`);
      try {
        writeFileSync(file, serializeDoc(fields, config.body), { flag: "wx" });
        return { kind: config.kind, file, fields, body: config.body };
      } catch (error) {
        if (error.code !== "EEXIST") throw error;
      }
    }
    throw new Error(`Could not allocate a unique ${config.kind} id after ${maxAttempts} attempts`);
  });
}

export function createProject(root, input = {}, options = {}) {
  const dir = projectDir(root, options);
  const body = input.body || PROJECT_BODY_TEMPLATE;
  return createAllocatedDoc(root, options, {
    dir, body, kind: "project", prefix: "P", pad: 3,
    listDocs: () => listProjects(root, options),
    makeFields: (id) => ({
      id,
      title: input.title || "Untitled project",
      status: PROJECT_STATUSES.includes(input.status) ? input.status : "idea",
      kind: PROJECT_KINDS.includes(input.kind) ? input.kind : "other",
      target: input.target || "",
      priority: PRIORITIES.includes(input.priority) ? input.priority : "P2",
      tags: Array.isArray(input.tags) ? input.tags : [],
      created: todayStamp(),
      updated: todayStamp(),
    }),
  });
}

export function ensureProject(root, input = {}, options = {}) {
  const target = String(input.target || "");
  const existing = target ? listProjects(root, options).find((project) => project.fields.target === target) : null;
  if (existing) {
    return { project: existing, created: false };
  }
  return {
    project: createProject(root, {
      ...input,
      status: input.status || "active",
      body: input.body || defaultEnsureProjectBody(input),
    }, options),
    created: true,
  };
}

function defaultEnsureProjectBody(input) {
  return `## Goal

Track work for \`${input.target || input.title || "this project"}\`.

## In scope

- Setup, validation, and follow-up tasks owned by this project.

## Out of scope

- Unrelated AI Studio or game work.

## Log
`;
}

export function createEpic(root, input = {}, options = {}) {
  const dir = epicDir(root, options);
  const body = input.body || EPIC_BODY_TEMPLATE;
  return createAllocatedDoc(root, options, {
    dir, body, kind: "epic", prefix: "E", pad: 3,
    listDocs: () => listEpics(root, options),
    makeFields: (id) => ({
      id,
      title: input.title || "Untitled epic",
      status: EPIC_STATUSES.includes(input.status) ? input.status : "idea",
      project: input.project || "",
      priority: PRIORITIES.includes(input.priority) ? input.priority : "P2",
      tags: Array.isArray(input.tags) ? input.tags : [],
      created: todayStamp(),
      updated: todayStamp(),
    }),
  });
}

export function createTask(root, input = {}, options = {}) {
  const dir = activeTaskDir(root, options);
  const epic = input.epic ? findDoc(root, input.epic, options) : null;
  const body = input.body || TASK_BODY_TEMPLATE;
  return createAllocatedDoc(root, options, {
    dir, body, kind: "task", prefix: "T", pad: 4,
    listDocs: () => listTasks(root, { ...options, includeArchive: true }),
    makeFields: (id) => ({
      id,
      title: input.title || "Untitled task",
      status: TASK_STATUSES.includes(input.status) ? input.status : "idea",
      project: input.project || (epic && epic.kind === "epic" ? (epic.fields.project || "") : ""),
      epic: input.epic || "",
      priority: PRIORITIES.includes(input.priority) ? input.priority : "P2",
      tags: Array.isArray(input.tags) ? input.tags : [],
      created: todayStamp(),
      updated: todayStamp(),
    }),
  });
}

export function updateDoc(root, id, patch = {}, options = {}) {
  const doc = findDoc(root, id, options);
  if (!doc) {
    throw new Error(`No task, epic, or project with id ${id}`);
  }
  if (patch.rev !== undefined && patch.rev !== doc.rev) {
    const err = new Error(`${id} changed on disk since it was loaded; reload and retry`);
    err.conflict = true;
    throw err;
  }
  const statuses = statusesForKind(doc.kind);
  const fields = { ...doc.fields };
  for (const [key, value] of Object.entries(patch.fields || {})) {
    if (key === "id" || key === "created") {
      continue;
    }
    if (key === "status" && !statuses.includes(value)) {
      throw new Error(`Invalid status "${value}" for ${doc.kind} (allowed: ${statuses.join(", ")})`);
    }
    if (key === "priority" && value && !PRIORITIES.includes(value)) {
      throw new Error(`Invalid priority "${value}" (allowed: ${PRIORITIES.join(", ")})`);
    }
    if (key === "kind") {
      if (doc.kind !== "project") {
        throw new Error("Only project docs can change `kind`");
      }
      if (value && !PROJECT_KINDS.includes(value)) {
        throw new Error(`Invalid project kind "${value}" (allowed: ${PROJECT_KINDS.join(", ")})`);
      }
    }
    fields[key] = value;
  }
  if (doc.kind === "task" && patch.fields && Object.hasOwn(patch.fields, "epic") && !Object.hasOwn(patch.fields, "project")) {
    const epic = fields.epic ? findDoc(root, fields.epic, options) : null;
    fields.project = epic && epic.kind === "epic" ? (epic.fields.project || "") : "";
  }
  fields.updated = todayStamp();
  const body = patch.body !== undefined ? patch.body : doc.body;
  let file = doc.file;
  let targetFile = doc.file;
  if (doc.kind === "task") {
    targetFile = join(taskStorageDir(root, fields, options), basename(doc.file));
    if (resolve(targetFile) !== resolve(doc.file) && existsSync(targetFile)) {
      throw new Error(`Cannot move ${id}; target already exists: ${targetFile}`);
    }
  }
  writeFileSync(doc.file, serializeDoc(fields, body));
  if (doc.kind === "task" && resolve(targetFile) !== resolve(doc.file)) {
    mkdirSync(dirname(targetFile), { recursive: true });
    renameSync(doc.file, targetFile);
    file = targetFile;
  }
  return { ...doc, file, fields, body };
}

function taskStorageDir(root, fields, options = {}) {
  if (fields.status === "done") {
    return join(archiveTaskDir(root, options), fields.epic || "unassigned");
  }
  return activeTaskDir(root, options);
}

export function validateStore(root, options = {}) {
  return validateStoreDetailed(root, options).map((problem) => typeof problem === "string" ? problem : problem.message);
}

export function validateStoreDetailed(root, options = {}) {
  const problems = [];
  const projects = listProjects(root, options);
  const epics = listEpics(root, options);
  const tasks = listTasks(root, { ...options, includeArchive: true });
  const seen = new Map();
  for (const doc of [...projects, ...epics, ...tasks]) {
    const id = doc.fields.id;
    if (!id) {
      problems.push(problem(`${doc.name}: missing id`));
      continue;
    }
    if (seen.has(id)) {
      problems.push(problem(`${doc.name}: duplicate id ${id} (also in ${seen.get(id)})`));
    }
    seen.set(id, doc.name);
    if (!doc.fields.title) {
      problems.push(problem(`${id}: missing title`, { taskId: id }));
    }
    const statuses = statusesForKind(doc.kind);
    if (!statuses.includes(doc.fields.status)) {
      problems.push(problem(`${id}: invalid status "${doc.fields.status}"`, { taskId: id }));
    }
    if (doc.kind === "project" && doc.fields.status === "active" && !hasFilledSections(doc.body)) {
      problems.push(problem(`${id}: active project needs non-empty Goal, In scope, and Out of scope sections`, { taskId: id }));
    }
    if (doc.kind === "epic" && doc.fields.status === "active" && !hasFilledSections(doc.body)) {
      problems.push(problem(`${id}: active epic needs non-empty Goal, In scope, and Out of scope sections`, { taskId: id }));
    }
  }
  const projectIds = new Set(projects.map((project) => project.fields.id));
  const epicIds = new Set(epics.map((epic) => epic.fields.id));
  const epicsById = new Map(epics.map((epic) => [epic.fields.id, epic]));
  for (const epic of epics) {
    if (epic.fields.project && !isQualifiedRef(epic.fields.project) && !projectIds.has(epic.fields.project)) {
      problems.push(problem(`${epic.fields.id}: references missing project "${epic.fields.project}"`, { taskId: epic.fields.id }));
    }
  }
  for (const task of tasks) {
    if (task.fields.epic && !isQualifiedRef(task.fields.epic) && !epicIds.has(task.fields.epic)) {
      problems.push(problem(`${task.fields.id}: references missing epic "${task.fields.epic}"`, { taskId: task.fields.id }));
    }
    if (task.fields.project && !isQualifiedRef(task.fields.project) && !projectIds.has(task.fields.project)) {
      problems.push(problem(`${task.fields.id}: references missing project "${task.fields.project}"`, { taskId: task.fields.id }));
    }
    const epic = task.fields.epic ? epicsById.get(task.fields.epic) : null;
    if (
      epic &&
      task.fields.project &&
      epic.fields.project &&
      !isQualifiedRef(task.fields.project) &&
      !isQualifiedRef(epic.fields.project) &&
      task.fields.project !== epic.fields.project
    ) {
      problems.push(problem(`${task.fields.id}: project ${task.fields.project} does not match epic ${epic.fields.id} project ${epic.fields.project}`, { taskId: task.fields.id }));
    }
    if (ACTIVE_TASK_STATUSES.includes(task.fields.status) && !hasActionableTaskBody(task.body)) {
      problems.push(problem(`${task.fields.id}: actionable task needs non-empty What and Done when sections`, { taskId: task.fields.id }));
    }
  }
  return problems;
}

function storeMeta(options = {}) {
  return options.store || DEFAULT_TASKBOARD_STORE;
}

function addStoreFields(out, id, options = {}) {
  const store = storeMeta(options);
  out.storeId = store.storeId;
  out.visibility = store.visibility;
  out.qualifiedId = `${store.storeId}:${id || ""}`;
  return out;
}

export function publicDoc(doc, options = {}) {
  const out = { kind: doc.kind, fields: doc.fields, rev: doc.rev };
  if (options.includeBody) {
    out.body = doc.body;
  }
  return addStoreFields(out, doc.fields.id, options);
}

function relativeFile(root, doc) {
  return relative(root, doc.file).replace(/\\/g, "/");
}

function epicsById(root, options = {}) {
  if (options.epicsById) {
    return options.epicsById;
  }
  const epics = options.epics || listEpics(root, options);
  return new Map(epics.map((epic) => [epic.fields.id, epic]));
}

function projectForTask(root, doc, options = {}) {
  if (doc.fields.project) {
    return doc.fields.project;
  }
  const epic = epicsById(root, options).get(doc.fields.epic);
  return epic ? (epic.fields.project || "") : "";
}

export function agentProjectRow(root, doc, options = {}) {
  const row = {
    id: doc.fields.id,
    title: doc.fields.title,
    status: doc.fields.status,
    kind: doc.fields.kind || "",
    target: doc.fields.target || "",
    priority: doc.fields.priority || "",
    tags: doc.fields.tags || [],
    file: relativeFile(root, doc),
  };
  if (options.includeBody) {
    row.body = doc.body;
  }
  return addStoreFields(row, doc.fields.id, options);
}

export function agentEpicRow(root, doc, options = {}) {
  const row = {
    id: doc.fields.id,
    title: doc.fields.title,
    status: doc.fields.status,
    project: doc.fields.project || "",
    priority: doc.fields.priority || "",
    tags: doc.fields.tags || [],
    file: relativeFile(root, doc),
  };
  if (options.includeBody) {
    row.body = doc.body;
  }
  return addStoreFields(row, doc.fields.id, options);
}

export function agentTaskRow(root, doc, options = {}) {
  const row = {
    id: doc.fields.id,
    title: doc.fields.title,
    status: doc.fields.status,
    project: projectForTask(root, doc, options),
    priority: doc.fields.priority || "",
    epic: doc.fields.epic || "",
    tags: doc.fields.tags || [],
    archived: doc.archived === true,
    file: relativeFile(root, doc),
  };
  if (options.includeBody) {
    row.body = doc.body;
  }
  return addStoreFields(row, doc.fields.id, options);
}

export function currentWorkRows(root, limit = 25, options = {}) {
  const epics = epicsById(root, options);
  const tasks = options.tasks || listTasks(root, options);
  return tasks
    .filter((task) => ACTIVE_TASK_STATUSES.includes(task.fields.status))
    .sort((a, b) => taskRank(a) - taskRank(b) || idNumber(b) - idNumber(a) || String(a.fields.id).localeCompare(String(b.fields.id)))
    .slice(0, limit)
    .map((task) => agentTaskRow(root, task, { ...options, epicsById: epics }));
}

export function agentContextPayload(root, options = {}) {
  const limit = Number.isFinite(Number(options.limit)) ? Number(options.limit) : 25;
  const tasks = listTasks(root, options);
  const epics = listEpics(root, options);
  const projects = listProjects(root, options);
  const epicsByIdMap = new Map(epics.map((epic) => [epic.fields.id, epic]));
  const currentWork = currentWorkRows(root, limit, { ...options, tasks, epicsById: epicsByIdMap });
  return {
    schema: "ai_studio.taskboard.agent_context.v1",
    root,
    counts: {
      projects: countsByStatus(projects, PROJECT_STATUSES),
      epics: countsByStatus(epics, EPIC_STATUSES),
      tasks: countsByStatus(tasks, TASK_STATUSES),
      currentWork: tasks.filter((task) => ACTIVE_TASK_STATUSES.includes(task.fields.status)).length,
      review: tasks.filter((task) => task.fields.status === "review").length,
    },
    currentWork,
    agentNextStep: "Open only the task file(s) needed for the current decision; do not scan archives unless linked.",
  };
}

export function boardPayload(root) {
  const store = DEFAULT_TASKBOARD_STORE;
  return {
    root,
    projectStatuses: PROJECT_STATUSES,
    projectKinds: PROJECT_KINDS,
    epicStatuses: EPIC_STATUSES,
    taskStatuses: TASK_STATUSES,
    priorities: PRIORITIES,
    stores: [store],
    projects: listProjects(root).map((doc) => publicDoc(doc, { store })),
    epics: listEpics(root).map((doc) => publicDoc(doc, { store })),
    tasks: listTasks(root).map((doc) => publicDoc(doc, { store })),
  };
}

function problem(message, extras = {}) {
  return { code: "taskboard_problem", message, ...extras };
}

function isQualifiedRef(value) {
  const text = String(value || "");
  const colon = text.lastIndexOf(":");
  return colon > 0 && /^[PET]\d+$/i.test(text.slice(colon + 1));
}

function hasActionableTaskBody(body) {
  const what = sectionText(body, "What");
  const doneWhen = sectionText(body, "Done when");
  return what.length > 0 && /- \[[ xX]\]\s+\S/.test(doneWhen);
}

function hasFilledSections(body) {
  return ["Goal", "In scope", "Out of scope"].every((section) => sectionText(body, section).length > 0);
}

function sectionText(body, title) {
  const pattern = new RegExp(`(?:^|\\n)## ${escapeRegExp(title)}[ \\t]*\\n([\\s\\S]*?)(?=\\n##\\s+|$)`, "i");
  const match = body.match(pattern);
  return match ? match[1].replace(/- \[ \]\s*$/, "").trim() : "";
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}
