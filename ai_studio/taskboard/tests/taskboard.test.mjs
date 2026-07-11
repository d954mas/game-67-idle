// Taskboard core tests. Run: node --test ai_studio/taskboard/tests/taskboard.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, rmSync, writeFileSync, readFileSync, mkdirSync, renameSync, utimesSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";
import { spawn, spawnSync } from "node:child_process";
import { EventEmitter } from "node:events";
import { URL } from "node:url";
import vm from "node:vm";
import {
  createTask, createEpic, createProject, listTasks,
  listEpics, listProjects, updateDoc, findDoc, validateStore, validateStoreDetailed,
} from "../lib.mjs";
import { boardPayload, parseDoc, serializeDoc, slugify } from "../store.mjs";
import { createTaskboardApi } from "../api.mjs";

const taskboardDir = dirname(import.meta.dirname);
const cliPath = join(taskboardDir, "cli.mjs");
const activeProjectBody = "## Goal\n\nTrack scoped work.\n\n## In scope\n\n- Owned work\n\n## Out of scope\n\n- Unowned work\n\n## Log\n";

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "taskboard-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function waitFor(predicate, timeoutMs = 10000) {
  const deadline = Date.now() + timeoutMs;
  return new Promise((resolve, reject) => {
    const poll = () => {
      if (predicate()) return resolve();
      if (Date.now() >= deadline) return reject(new Error("Timed out waiting for child processes"));
      setTimeout(poll, 10);
    };
    poll();
  });
}

function spawnConcurrentCreator(root, workerId, kind) {
  const source = `
    import { existsSync, writeFileSync } from "node:fs";
    import { join } from "node:path";
    import { createTask, createEpic, createProject } from ${JSON.stringify(new URL("../lib.mjs", import.meta.url).href)};
    const [root, workerId, kind] = process.argv.slice(1);
    writeFileSync(join(root, \`.ready-\${workerId}\`), "ready");
    while (!existsSync(join(root, ".go"))) Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 10);
    const create = { task: createTask, epic: createEpic, project: createProject }[kind];
    const doc = create(root, { title: \`Concurrent \${kind} \${workerId}\` });
    process.stdout.write(JSON.stringify({ id: doc.fields.id, file: doc.file, kind }));
  `;
  const child = spawn(process.execPath, ["--input-type=module", "--eval", source, root, String(workerId), kind], {
    cwd: root,
    stdio: ["ignore", "pipe", "pipe"],
  });
  const result = new Promise((resolve, reject) => {
    let stdout = "";
    let stderr = "";
    child.stdout.on("data", (chunk) => { stdout += chunk; });
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    child.on("error", reject);
    child.on("close", (code) => {
      if (code !== 0) return reject(new Error(`worker ${workerId} exited ${code}: ${stderr}`));
      try {
        resolve(JSON.parse(stdout));
      } catch (error) {
        reject(new Error(`worker ${workerId} returned invalid JSON: ${error.message}; stdout=${stdout}`));
      }
    });
  });
  result.catch(() => {});
  const closed = new Promise((resolve) => child.once("close", resolve));
  return { child, result, closed };
}

function ensurePrivateGameMount(root, gameId = "secret-game") {
  const gameRoot = join(root, "games", gameId);
  mkdirSync(gameRoot, { recursive: true });
  spawnSync("git", ["init"], { cwd: root, encoding: "utf8" });
  spawnSync("git", ["init"], { cwd: gameRoot, encoding: "utf8" });
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
    mounts: [{ kind: "game", root: `games/${gameId}`, visibility: "private", gitRoot: `games/${gameId}`, commitPolicy: "nested-private", enabledStores: ["taskboard"], aliases: [] }],
  }, null, 2) + "\n", "utf8");
  return {
    gameId,
    gameRoot,
    itemsRoot: join(gameRoot, ".ai_studio", "taskboard", "items"),
  };
}

function writeTaskDoc(itemsRoot, id, title, status = "backlog", fields = {}) {
  const dir = join(itemsRoot, "active");
  mkdirSync(dir, { recursive: true });
  const extraFields = Object.entries(fields).map(([key, value]) => `${key}: ${value}`).join("\n");
  writeFileSync(join(dir, `${id}-${title.toLowerCase().replace(/[^a-z0-9]+/g, "-")}.md`), `---
id: ${id}
title: ${title}
status: ${status}
priority: P1
${extraFields ? `${extraFields}\n` : ""}created: 2026-07-09
updated: 2026-07-09
---

## What

${title}

## Done when

- [ ] done

## Log
`, "utf8");
}

function invokeApi(handler, method, path, body = {}, headers = {}) {
  const req = new EventEmitter();
  req.method = method;
  req.headers = Object.fromEntries(Object.entries(headers).map(([key, value]) => [key.toLowerCase(), value]));
  req.destroy = () => {};
  const res = {
    status: 0,
    headers: {},
    body: "",
    writeHead(status, headers) {
      this.status = status;
      this.headers = headers;
    },
    end(chunk = "") {
      this.body = String(chunk);
      this.resolve({
        status: this.status,
        headers: this.headers,
        data: this.body ? JSON.parse(this.body) : null,
      });
    },
  };
  const done = new Promise((resolve) => { res.resolve = resolve; });
  handler(req, res, new URL(path, "http://taskboard.local"));
  queueMicrotask(() => {
    req.emit("data", Buffer.from(JSON.stringify(body)));
    req.emit("end");
  });
  return done;
}

test("frontmatter roundtrip preserves fields and body", () => {
  const fields = {
    id: "T0001",
    title: 'Tricky: title with #hash and "quotes"',
    status: "backlog",
    tags: ["a-b", "c d"],
    created: "2026-06-11",
  };
  const body = "## What\n\nLine one.\n\n- [ ] box\n";
  const parsed = parseDoc(serializeDoc(fields, body));
  assert.deepEqual(parsed.fields, fields);
  assert.equal(parsed.body.trim(), body.trim());
});

test("parseDoc tolerates files without frontmatter", () => {
  const parsed = parseDoc("just text\n");
  assert.deepEqual(parsed.fields, {});
  assert.equal(parsed.body, "just text\n");
});

test("slugify handles non-ascii and empty titles", () => {
  assert.equal(slugify("Camp Rest Action!"), "camp-rest-action");
  assert.equal(slugify("\u0418\u0434\u0435\u044f \u0431\u0435\u0437 \u043b\u0430\u0442\u0438\u043d\u0438\u0446\u044b"), "item");
});

test("createTask allocates sequential ids and createEpic separate sequence", (t) => {
  const root = tempRoot(t);
  const t1 = createTask(root, { title: "First" });
  const t2 = createTask(root, { title: "Second" });
  const e1 = createEpic(root, { title: "Epic" });
  assert.equal(t1.fields.id, "T0001");
  assert.equal(t2.fields.id, "T0002");
  assert.equal(e1.fields.id, "E001");
  assert.equal(listTasks(root).length, 2);
  assert.equal(listEpics(root).length, 1);
});

test("createTask rejects status done before allocating or writing", (t) => {
  const root = tempRoot(t);

  assert.throws(
    () => createTask(root, { title: "Invalid closed creation", status: "done" }),
    (error) => {
      assert.equal(error.problem.code, "task_create_done_forbidden");
      return true;
    },
  );
  assert.equal(createTask(root, { title: "First valid task" }).fields.id, "T0001");
  assert.equal(listTasks(root, { includeArchive: true }).length, 1);
});

test("CLI and API reject creating a done task with a machine problem", async (t) => {
  const root = tempRoot(t);
  const cli = spawnSync(process.execPath, [cliPath, "new", "task", "--title", "CLI done", "--status", "done", "--json"], { cwd: root, encoding: "utf8" });
  assert.notEqual(cli.status, 0);
  assert.equal(JSON.parse(cli.stdout).problem.code, "task_create_done_forbidden");

  const handler = createTaskboardApi(root);
  const api = await invokeApi(handler, "POST", "/api/tasks", { title: "API done", status: "done" });
  assert.equal(api.status, 400);
  assert.equal(api.data.problem.code, "task_create_done_forbidden");
  assert.equal(createTask(root, { title: "Counter remains clean" }).fields.id, "T0001");
});

test("projects are first-class parents for epics and tasks", (t) => {
  const root = tempRoot(t);
  const project = createProject(root, {
    title: "AI Studio",
    status: "active",
    kind: "ai-studio",
    target: "ai_studio",
    body: "## Goal\n\nImprove the reusable pipeline.\n\n## In scope\n\n- Taskboard\n\n## Out of scope\n\n- Game lore\n\n## Log\n",
  });
  const epic = createEpic(root, {
    title: "Taskboard decomposition",
    status: "active",
    project: project.fields.id,
    body: "## Goal\n\nMake it reusable.\n\n## In scope\n\n- Projects\n\n## Out of scope\n\n- Heavy PM suite\n\n## Log\n",
  });
  const task = createTask(root, {
    title: "Implement project API",
    status: "backlog",
    epic: epic.fields.id,
    body: "## What\n\nAdd project APIs.\n\n## Done when\n\n- [ ] APIs are tested\n\n## Open questions\n\n## Log\n",
  });

  assert.equal(project.fields.id, "P001");
  assert.equal(epic.fields.project, "P001");
  assert.equal(task.fields.project, "P001");
  assert.deepEqual(listProjects(root).map((doc) => doc.fields.id), ["P001"]);
  assert.deepEqual(validateStore(root), []);
});

test("validation enforces project references and task epic/project consistency", (t) => {
  const root = tempRoot(t);
  const projectA = createProject(root, { title: "AI Studio", status: "active", kind: "ai-studio", body: activeProjectBody });
  const projectB = createProject(root, { title: "Game", status: "active", kind: "game", body: activeProjectBody });
  createEpic(root, { title: "Missing project", status: "idea", project: "P999" });
  createEpic(root, { title: "Real epic", status: "idea", project: projectA.fields.id });
  createTask(root, { title: "Mismatch", status: "idea", epic: "E002", project: projectB.fields.id });
  createTask(root, { title: "Missing task project", status: "idea", project: "P999" });

  assert.deepEqual(
    validateStoreDetailed(root).map((problem) => problem.message),
    [
      'E001: references missing project "P999"',
      "T0001: project P002 does not match epic E002 project P001",
      'T0002: references missing project "P999"',
    ],
  );
});

test("task store ignores operational docs", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "First" });
  writeFileSync(join(root, "ai_studio", "taskboard", "items", "active", "README.md"), "# Task Store\n");
  assert.equal(listTasks(root).length, 1);
  assert.deepEqual(validateStore(root), []);
});

test("Taskboard API board payload exposes public project, epic, and task state", (t) => {
  const root = tempRoot(t);
  createProject(root, { title: "Visible project", status: "active", kind: "ai-studio" });
  createTask(root, { title: "Visible task", status: "todo", project: "P001" });
  createEpic(root, { title: "Visible epic", status: "active", project: "P001" });

  const payload = boardPayload(root);
  assert.equal(payload.root, root);
  assert.deepEqual(payload.tasks.map((doc) => doc.fields.id), ["T0001"]);
  assert.deepEqual(payload.epics.map((doc) => doc.fields.id), ["E001"]);
  assert.deepEqual(payload.projects.map((doc) => doc.fields.id), ["P001"]);
  assert.ok(payload.taskStatuses.includes("doing"));
  assert.ok(payload.epicStatuses.includes("active"));
  assert.ok(payload.projectStatuses.includes("active"));
  assert.equal(payload.taskStatuses.includes("dropped"), false);
  assert.equal(payload.epicStatuses.includes("dropped"), false);
  assert.equal(payload.projectStatuses.includes("dropped"), false);
  assert.ok(payload.projectKinds.includes("ai-studio"));
  assert.ok(payload.priorities.includes("P1"));
  assert.equal(payload.tasks[0].body, undefined);
  assert.equal(payload.tasks[0].file, undefined);
});

test("Taskboard API refuses kind-mismatched patch routes", async (t) => {
  const root = tempRoot(t);
  createProject(root, { title: "Project" });
  createEpic(root, { title: "Epic", status: "idea", project: "P001" });
  const handler = createTaskboardApi(root);

  const response = await invokeApi(handler, "PATCH", "/api/tasks/E001", {
    fields: { status: "done" },
  });

  assert.equal(response.status, 404);
  assert.match(response.data.error, /task not found/);
  assert.equal(findDoc(root, "E001").fields.status, "idea");
});

test("Taskboard API exposes explicit project, epic, and task collections", async (t) => {
  const root = tempRoot(t);
  createProject(root, { title: "Project" });
  createEpic(root, { title: "Epic", project: "P001" });
  createTask(root, { title: "Task", project: "P001", epic: "E001" });
  const handler = createTaskboardApi(root);

  const projects = await invokeApi(handler, "GET", "/api/projects");
  const epics = await invokeApi(handler, "GET", "/api/epics");
  const tasks = await invokeApi(handler, "GET", "/api/tasks");
  const task = await invokeApi(handler, "GET", "/api/tasks/T0001");

  assert.deepEqual(projects.data.projects.map((doc) => doc.fields.id), ["P001"]);
  assert.deepEqual(epics.data.epics.map((doc) => doc.fields.id), ["E001"]);
  assert.deepEqual(tasks.data.tasks.map((doc) => doc.fields.id), ["T0001"]);
  assert.equal(tasks.data.tasks[0].body, undefined);
  assert.match(task.data.body, /## What/);
});

test("done tasks move to archive and stay addressable by id", (t) => {
  const root = tempRoot(t);
  createEpic(root, { title: "Epic" });
  createTask(root, {
    title: "Archive me",
    epic: "E001",
    status: "todo",
    body: "## Done when\n\n- [x] archived\n\n## Log\n\n- 2026-07-11: Quality: not-applicable; reason: storage behavior fixture\n",
  });
  const updated = updateDoc(root, "T0001", { fields: { status: "done" } });
  assert.match(updated.file, /ai_studio[\\/]taskboard[\\/]items[\\/]archive[\\/]E001[\\/]T0001-/);
  assert.equal(existsSync(updated.file), true);
  assert.equal(listTasks(root).length, 0);
  assert.equal(listTasks(root, { includeArchive: true }).length, 1);
  assert.equal(findDoc(root, "T0001").fields.status, "done");
});

test("cli list hides ideas by default and shows them explicitly", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Raw idea", status: "idea" });
  createTask(root, { title: "Actionable", status: "backlog" });
  const cli = cliPath;
  const base = { cwd: root, encoding: "utf8" };
  const normal = spawnSync(process.execPath, [cli, "list"], base);
  assert.equal(normal.status, 0, normal.stderr);
  assert.match(normal.stdout, /Actionable/);
  assert.doesNotMatch(normal.stdout, /Raw idea/);
  const ideas = spawnSync(process.execPath, [cli, "list", "--ideas"], base);
  assert.equal(ideas.status, 0, ideas.stderr);
  assert.match(ideas.stdout, /Raw idea/);
});

test("cli list shows review by default and keeps ideas hidden", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Active work", status: "todo" });
  createTask(root, { title: "Needs review", status: "review" });
  createTask(root, { title: "Raw idea", status: "idea" });
  const cli = cliPath;
  const base = { cwd: root, encoding: "utf8" };
  const normal = spawnSync(process.execPath, [cli, "list"], base);
  assert.equal(normal.status, 0, normal.stderr);
  assert.match(normal.stdout, /Active work/);
  assert.match(normal.stdout, /Needs review/);
  assert.doesNotMatch(normal.stdout, /Raw idea/);
  const ideas = spawnSync(process.execPath, [cli, "list", "--ideas"], base);
  assert.equal(ideas.status, 0, ideas.stderr);
  assert.match(ideas.stdout, /Raw idea/);
});

test("cli context is task-derived and includes review without task bodies", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Large active task",
    status: "doing",
    priority: "P0",
    body: `## What

${"LARGE_TASK_BODY_SHOULD_NOT_APPEAR\n".repeat(300)}

## Done when

- [ ] compact context stays row-only

## Open questions

## Log
`,
  });
  createTask(root, { title: "Review task", status: "review", priority: "P1" });
  createTask(root, { title: "Raw idea", status: "idea", priority: "P1" });
  const cli = cliPath;
  const result = spawnSync(process.execPath, [cli, "context"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /# Current Context Digest/);
  assert.match(result.stdout, /active_task_counts: idea:1 backlog:0 todo:0 doing:1 review:1/);
  assert.match(result.stdout, /T0001 .* Large active task/);
  assert.match(result.stdout, /T0002 .* Review task/);
  assert.doesNotMatch(result.stdout, /T0003 .* Raw idea/);
  assert.doesNotMatch(result.stdout, /LARGE_TASK_BODY_SHOULD_NOT_APPEAR/);
  assert.match(result.stdout, /inspect only the linked task files/);
});

test("cli summary is task-derived and shows review as current work", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Doing task", status: "doing", priority: "P1" });
  createTask(root, { title: "Review task", status: "review", priority: "P1" });
  createTask(root, { title: "Idea task", status: "idea", priority: "P1" });
  const cli = cliPath;
  const result = spawnSync(process.execPath, [cli, "summary", "--tasks-limit", "5"], { cwd: root, encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /# Taskboard Summary/);
  assert.match(result.stdout, /active_task_counts: idea:1 backlog:0 todo:0 doing:1 review:1/);
  assert.match(result.stdout, /open_work_items: 2/);
  assert.match(result.stdout, /review_tasks: 1/);
  assert.match(result.stdout, /T0001 .* Doing task/);
  assert.match(result.stdout, /T0002 .* Review task/);
  assert.doesNotMatch(result.stdout, /T0003 .* Idea task/);
  assert.doesNotMatch(result.stdout, /## Current Goal/);
});

test("cli summary json is a compact agent API payload", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Implement thing",
    status: "doing",
    priority: "P0",
    body: `## What

BODY_SHOULD_NOT_APPEAR

## Done when

- [ ] done

## Log
`,
  });
  createTask(root, { title: "Needs review", status: "review", priority: "P1" });
  createTask(root, { title: "Raw idea", status: "idea", priority: "P2" });

  const result = spawnSync(process.execPath, [cliPath, "summary", "--json"], { cwd: root, encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr);
  const payload = JSON.parse(result.stdout);
  assert.equal(payload.schema, "ai_studio.taskboard.agent_context.v1");
  assert.equal(payload.counts.tasks.doing, 1);
  assert.equal(payload.counts.tasks.review, 1);
  assert.equal(payload.counts.tasks.idea, 1);
  assert.deepEqual(payload.currentWork.map((task) => task.id), ["T0001", "T0002"]);
  assert.equal(payload.currentWork[0].file.includes("ai_studio/taskboard/items/active/T0001-"), true);
  assert.equal("body" in payload.currentWork[0], false);
  assert.doesNotMatch(result.stdout, /BODY_SHOULD_NOT_APPEAR/);
});

test("cli list and show json expose stable agent rows", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Visible task",
    status: "todo",
    priority: "P1",
    tags: ["agent-api"],
    body: `## What

Detailed body.

## Done when

- [ ] done

## Log
`,
  });
  createTask(root, { title: "Hidden idea", status: "idea" });

  const list = spawnSync(process.execPath, [cliPath, "list", "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(list.status, 0, list.stderr);
  const listPayload = JSON.parse(list.stdout);
  assert.equal(listPayload.schema, "ai_studio.taskboard.list.v1");
  assert.deepEqual(listPayload.tasks.map((task) => task.id), ["T0001"]);
  assert.equal("body" in listPayload.tasks[0], false);

  const show = spawnSync(process.execPath, [cliPath, "show", "T0001", "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(show.status, 0, show.stderr);
  const showPayload = JSON.parse(show.stdout);
  assert.equal(showPayload.schema, "ai_studio.taskboard.doc.v1");
  assert.equal(showPayload.doc.id, "T0001");
  assert.match(showPayload.doc.body, /Detailed body/);
});

test("cli list is public-only by default and can explicitly include private game stores", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Public task", status: "backlog" });
  const privateStore = ensurePrivateGameMount(root);
  writeTaskDoc(privateStore.itemsRoot, "T0001", "Private task");

  const normal = spawnSync(process.execPath, [cliPath, "list", "--json"], { cwd: root, encoding: "utf8" });
  assert.equal(normal.status, 0, normal.stderr);
  const normalPayload = JSON.parse(normal.stdout);
  assert.deepEqual(normalPayload.tasks.map((task) => task.title), ["Public task"]);
  assert.deepEqual(normalPayload.tasks.map((task) => task.storeId), ["studio"]);

  const included = spawnSync(process.execPath, [cliPath, "list", "--json", "--include-private"], { cwd: root, encoding: "utf8" });
  assert.equal(included.status, 0, included.stderr);
  const includedPayload = JSON.parse(included.stdout);
  const privateTask = includedPayload.tasks.find((task) => task.storeId === "game:secret-game");
  assert.equal(privateTask.title, "Private task");
  assert.equal(privateTask.visibility, "private");
  assert.equal(privateTask.qualifiedId, "game:secret-game:T0001");
});

test("cli rejects ambiguous bare ids in aggregate context and accepts qualified ids", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Public task", status: "backlog" });
  const privateStore = ensurePrivateGameMount(root);
  writeTaskDoc(privateStore.itemsRoot, "T0001", "Private task");

  const ambiguous = spawnSync(process.execPath, [cliPath, "show", "T0001", "--json", "--include-private"], { cwd: root, encoding: "utf8" });
  assert.notEqual(ambiguous.status, 0);
  assert.match(ambiguous.stderr || ambiguous.stdout, /ambiguous/i);

  const qualified = spawnSync(process.execPath, [cliPath, "show", "game:secret-game:T0001", "--json", "--include-private"], { cwd: root, encoding: "utf8" });
  assert.equal(qualified.status, 0, qualified.stderr);
  const payload = JSON.parse(qualified.stdout);
  assert.equal(payload.doc.title, "Private task");
  assert.equal(payload.doc.storeId, "game:secret-game");
  assert.equal(payload.doc.qualifiedId, "game:secret-game:T0001");
});

test("cli new task and project write to explicit private game store without public fallback", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Public task", status: "backlog" });
  const privateStore = ensurePrivateGameMount(root);

  const taskResult = spawnSync(process.execPath, [
    cliPath, "new", "task",
    "--game", privateStore.gameId,
    "--title", "Private created",
    "--status", "backlog",
    "--json",
  ], { cwd: root, encoding: "utf8" });

  assert.equal(taskResult.status, 0, taskResult.stderr);
  const taskPayload = JSON.parse(taskResult.stdout);
  assert.equal(taskPayload.doc.storeId, "game:secret-game");
  assert.equal(taskPayload.doc.id, "T0001");
  assert.match(taskPayload.doc.file, /games\/secret-game\/\.ai_studio\/taskboard\/items\/active\/T0001-/);

  const projectResult = spawnSync(process.execPath, [
    cliPath, "new", "project",
    "--game", privateStore.gameId,
    "--title", "Private project",
    "--status", "idea",
    "--json",
  ], { cwd: root, encoding: "utf8" });
  assert.equal(projectResult.status, 0, projectResult.stderr);
  const projectPayload = JSON.parse(projectResult.stdout);
  assert.equal(projectPayload.doc.storeId, "game:secret-game");
  assert.equal(projectPayload.doc.id, "P001");
  assert.match(projectPayload.doc.file, /games\/secret-game\/\.ai_studio\/taskboard\/items\/projects\/P001-/);
  assert.deepEqual(listTasks(root).map((task) => task.fields.title), ["Public task"]);
  assert.deepEqual(listProjects(root).map((project) => project.fields.title), []);
});

test("Taskboard API payloads are public-only by default and store-qualified with explicit private include", async (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Public task", status: "backlog" });
  const privateStore = ensurePrivateGameMount(root);
  writeTaskDoc(privateStore.itemsRoot, "T0001", "Private task");
  const handler = createTaskboardApi(root);

  const normal = await invokeApi(handler, "GET", "/api/board");
  assert.deepEqual(normal.data.tasks.map((task) => task.fields.title), ["Public task"]);
  assert.deepEqual(normal.data.tasks.map((task) => task.storeId), ["studio"]);

  const included = await invokeApi(handler, "GET", "/api/board?includePrivate=1");
  const privateTask = included.data.tasks.find((task) => task.storeId === "game:secret-game");
  assert.equal(privateTask.fields.title, "Private task");
  assert.equal(privateTask.visibility, "private");
  assert.equal(privateTask.qualifiedId, "game:secret-game:T0001");
});

test("Taskboard API accepts private store scope through headers without query paths", async (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Public task", status: "backlog" });
  const privateStore = ensurePrivateGameMount(root);
  writeTaskDoc(privateStore.itemsRoot, "T0001", "Private task");
  const handler = createTaskboardApi(root);

  const scoped = await invokeApi(handler, "GET", "/api/board", {}, { "x-ai-studio-store": "game:secret-game" });
  assert.deepEqual(scoped.data.tasks.map((task) => task.fields.title), ["Private task"]);
  assert.equal(scoped.data.tasks[0].qualifiedId, "game:secret-game:T0001");

  const created = await invokeApi(handler, "POST", "/api/tasks", {
    title: "Header created",
    status: "backlog",
  }, { "x-ai-studio-store": "game:secret-game" });
  assert.equal(created.status, 201);
  assert.equal(created.data.storeId, "game:secret-game");
  assert.equal(existsSync(join(privateStore.itemsRoot, "active", "T0002-header-created.md")), true);

  const mismatch = await invokeApi(handler, "POST", "/api/tasks", {
    title: "Wrong store",
    status: "backlog",
    storeId: "studio",
  }, { "x-ai-studio-store": "game:secret-game" });
  assert.equal(mismatch.status, 400);
  assert.match(mismatch.data.error, /mismatch/);
});

test("aggregate validation rejects ambiguous bare cross-store links and accepts qualified links", (t) => {
  const root = tempRoot(t);
  createProject(root, { title: "Public project", status: "idea" });
  const privateStore = ensurePrivateGameMount(root);
  writeTaskDoc(privateStore.itemsRoot, "T0001", "Ambiguous private task", "backlog", { project: "P001" });

  const ambiguous = spawnSync(process.execPath, [cliPath, "validate", "--json", "--include-private"], { cwd: root, encoding: "utf8" });
  assert.notEqual(ambiguous.status, 0);
  assert.match(ambiguous.stdout, /bare cross-store reference/);

  rmSync(join(privateStore.itemsRoot, "active"), { recursive: true, force: true });
  writeTaskDoc(privateStore.itemsRoot, "T0001", "Qualified private task", "backlog", { project: "studio:P001" });
  const qualified = spawnSync(process.execPath, [cliPath, "validate", "--json", "--include-private"], { cwd: root, encoding: "utf8" });
  assert.equal(qualified.status, 0, qualified.stderr || qualified.stdout);
});

test("taskboard cli help exits successfully and documents commands", () => {
  for (const args of [[], ["help"], ["--help"], ["-h"]]) {
    const result = spawnSync(process.execPath, [cliPath, ...args], { encoding: "utf8" });

    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /usage: cli\.mjs <list\|summary\|context\|show\|new\|set\|validate\|help>/);
    assert.match(result.stdout, /new project --title/);
    assert.match(result.stdout, /summary \[--json\]/);
    assert.match(result.stdout, /validate \[--json\]/);
  }
});

test("taskboard cli rejects unrelated core commands", () => {
  const result = spawnSync(process.execPath, [cliPath, "workflow-run"], { encoding: "utf8" });

  assert.notEqual(result.status, 0);
  assert.match(result.stdout, /usage: cli\.mjs <list\|summary\|context\|show\|new\|set\|validate\|help>/);
  assert.doesNotMatch(result.stdout, /workflow-run/);
});

test("updateDoc patches fields, keeps id/created, bumps updated", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Patch me", status: "idea" });
  const doc = updateDoc(root, "T0001", { fields: { status: "doing", id: "HACK", created: "1999-01-01" } });
  assert.equal(doc.fields.status, "doing");
  assert.equal(doc.fields.id, "T0001");
  assert.notEqual(doc.fields.created, "1999-01-01");
});

test("updateDoc rejects invalid status", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Bad status" });
  assert.throws(() => updateDoc(root, "T0001", { fields: { status: "nonsense" } }), /Invalid status/);
  assert.throws(() => updateDoc(root, "T0001", { fields: { status: "dropped" } }), /Invalid status/);
});

test("updateDoc requires truthful closure and an explicit quality decision for a new done task", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Close truthfully",
    status: "review",
    body: "## What\n\nClose it.\n\n## Done when\n\n- [ ] behavior is proven\n\n## Log\n",
  });
  const original = readFileSync(task.file, "utf8");

  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    (error) => {
      assert.equal(error.problem.code, "task_closure_required");
      assert.deepEqual(error.problem.details.unchecked, ["behavior is proven"]);
      return true;
    },
  );
  assert.equal(readFileSync(task.file, "utf8"), original, "rejected close must not write or archive");
});

test("updateDoc accepts checked criteria plus canonical quality evidence", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Checked close",
    status: "review",
    body: "## Done when\n\n- [x] implementation\n- [x] tests\n\n## Log\n\n- 2026-07-11: Quality: QCLR_001=pass; QTECH_002=review; evidence: focused tests and review\n",
  });

  const updated = updateDoc(root, "T0001", { fields: { status: "done" } });
  assert.equal(updated.fields.status, "done");
  assert.match(updated.file, /archive/);
});

test("updateDoc requires a non-empty canonical checked Done when list unless waived", (t) => {
  const invalidSections = [
    "",
    "## Done when\n",
    "## Done when\n\n- [ ]\n",
    "## Done when\n\n- [x]\n",
    "## Done when\n\n* [x] alternate bullet\n",
    "## Done when\n\n- [X] alternate marker\n",
    "## Done when\n\n- [-] bad marker\n",
  ];
  for (const [index, section] of invalidSections.entries()) {
    const root = tempRoot(t);
    createTask(root, {
      title: `Invalid criteria ${index}`,
      status: "review",
      body: `${section}\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: isolated closure test\n`,
    });
    assert.throws(
      () => updateDoc(root, "T0001", { fields: { status: "done" } }),
      (error) => error.problem.code === "task_closure_required",
      `invalid Done when variant ${index} must require a waiver`,
    );
  }

  const root = tempRoot(t);
  createTask(root, {
    title: "Criterion continuation",
    status: "review",
    body: "## Done when\n\n- [x] primary criterion\n      with legitimate continuation text\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: continuation test\n",
  });
  assert.equal(updateDoc(root, "T0001", { fields: { status: "done" } }).fields.status, "done");
});

test("updateDoc ignores bare and fenced closeout examples in Log", (t) => {
  const qualityVariants = [
    "Quality: QTECH_001=pass; evidence: bare line",
    "```text\n- 2026-07-11: Quality: QTECH_001=pass; evidence: fenced example\n```",
  ];
  for (const [index, qualityLog] of qualityVariants.entries()) {
    const root = tempRoot(t);
    createTask(root, { title: `Invalid quality log ${index}`, status: "review", body: `## Done when\n\n- [x] done\n\n## Log\n\n${qualityLog}\n` });
    assert.throws(
      () => updateDoc(root, "T0001", { fields: { status: "done" } }),
      (error) => error.problem.code === "task_quality_decision_required",
    );
  }

  const closureVariants = [
    "Closure: waived; reason: bare; evidence: example",
    "```text\n- 2026-07-11: Closure: waived; reason: fenced; evidence: example\n```",
  ];
  for (const [index, closureLog] of closureVariants.entries()) {
    const root = tempRoot(t);
    createTask(root, { title: `Invalid closure log ${index}`, status: "review", body: `## Done when\n\n- [ ] open\n\n## Log\n\n${closureLog}\n- 2026-07-11: Quality: QTECH_001=pass; evidence: isolated closure log test\n` });
    assert.throws(
      () => updateDoc(root, "T0001", { fields: { status: "done" } }),
      (error) => error.problem.code === "task_closure_required",
    );
  }
});

test("closeout discovery ignores an entire fenced fake task document", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Fenced fake document",
    status: "review",
    body: "```markdown\n## Done when\n\n- [x] fake criterion\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: fake fenced evidence\n```\n",
  });

  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    (error) => error.problem.code === "task_closure_required",
  );
});

test("closeout discovery ignores fake sections and records inside HTML comments", (t) => {
  const multilineRoot = tempRoot(t);
  createTask(multilineRoot, {
    title: "Commented fake document",
    status: "review",
    body: "<!--\n## Done when\n\n- [x] fake criterion\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: fake comment evidence\n-->\n",
  });
  assert.throws(
    () => updateDoc(multilineRoot, "T0001", { fields: { status: "done" } }),
    (error) => error.problem.code === "task_closure_required",
  );

  const sameLineRoot = tempRoot(t);
  createTask(sameLineRoot, {
    title: "Commented quality record",
    status: "review",
    body: "## Done when\n\n- [x] real criterion\n\n## Log\n\n<!-- - 2026-07-11: Quality: QTECH_001=pass; evidence: same-line comment -->\n",
  });
  assert.throws(
    () => updateDoc(sameLineRoot, "T0001", { fields: { status: "done" } }),
    (error) => error.problem.code === "task_quality_decision_required",
  );
});

test("closeout H2 sections end at the next H1 or H2 outside comments and fences", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Appendix example",
    status: "review",
    body: "## Done when\n\n- [x] real criterion\n\n## Log\n\n### Notes\n\nNo decision yet.\n\n# Appendix\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: appendix example only\n",
  });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    (error) => error.problem.code === "task_quality_decision_required",
  );

  const hashesRoot = tempRoot(t);
  createTask(hashesRoot, {
    title: "ATX closing hashes",
    status: "review",
    body: "## Done when ##\n\n- [x] real criterion\n\n## Log ###\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: closing hash regression\n",
  });
  assert.equal(updateDoc(hashesRoot, "T0001", { fields: { status: "done" } }).fields.status, "done");
});

test("closeout fences require the same marker and sufficient opening length with no suffix", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Long fence",
    status: "review",
    body: "## Done when\n\n- [x] real criterion\n\n## Log\n\n````markdown\n- 2026-07-11: Quality: QTECH_001=pass; evidence: before short pseudo-close\n```\n- 2026-07-11: Quality: QTECH_001=pass; evidence: after short pseudo-close\n```` suffix\n- 2026-07-11: Quality: QTECH_001=pass; evidence: after suffixed pseudo-close\n````\n",
  });

  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    (error) => error.problem.code === "task_quality_decision_required",
  );
});

test("Done when ignores fenced and indented examples but rejects ordered checkbox forms", (t) => {
  const ignoredRoot = tempRoot(t);
  createTask(ignoredRoot, {
    title: "Ignored examples",
    status: "review",
    body: "## Done when\n\n```markdown\n- [ ] fenced example\n```\n    - [ ] indented code example\n- [x] real criterion\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: scanner regression\n",
  });
  assert.equal(updateDoc(ignoredRoot, "T0001", { fields: { status: "done" } }).fields.status, "done");

  const orderedRoot = tempRoot(t);
  createTask(orderedRoot, {
    title: "Ordered checkbox",
    status: "review",
    body: "## Done when\n\n- [x] real criterion\n1. [ ] ordered open criterion\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: ordered regression\n",
  });
  assert.throws(
    () => updateDoc(orderedRoot, "T0001", { fields: { status: "done" } }),
    (error) => error.problem.code === "task_closure_required" && error.problem.details.malformed.includes("1. [ ] ordered open criterion"),
  );
});

test("closeout headings consistently allow up to three leading spaces", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Indented headings",
    status: "review",
    body: "   ## Done when\n\n   - [x] real criterion\n\n   ## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: heading indentation regression\n",
  });

  assert.equal(updateDoc(root, "T0001", { fields: { status: "done" } }).fields.status, "done");
});

test("quality decisions reject duplicate rule ids", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Duplicate quality",
    status: "review",
    body: "## Done when\n\n- [x] done\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; QTECH_001=block; evidence: conflicting duplicates\n",
  });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    (error) => error.problem.code === "task_quality_decision_required",
  );

  const cli = spawnSync(process.execPath, [cliPath, "set", "T0001", "--quality", "QTECH_001=pass; QTECH_001=pass", "--quality-evidence", "duplicate", "--json"], { cwd: root, encoding: "utf8" });
  assert.notEqual(cli.status, 0);
  assert.equal(JSON.parse(cli.stdout).problem.code, "quality_input_invalid");
});

test("updateDoc accepts canonical closure waiver and quality not-applicable decision", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Waived close",
    status: "review",
    body: "## Done when\n\n- [ ] obsolete criterion\n\n## Log\n\n- 2026-07-11: Closure: waived; reason: superseded by T0002; evidence: decision recorded in E001\n- 2026-07-11: Quality: not-applicable; reason: planning-only replacement\n",
  });

  assert.equal(updateDoc(root, "T0001", { fields: { status: "done" } }).fields.status, "done");
});

test("updateDoc rejects malformed or missing quality decisions with a machine problem", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Bad quality close",
    status: "review",
    body: "## Done when\n\n- [x] done\n\n## Log\n\n- 2026-07-11: Quality: QCLR_001=maybe; evidence: unsupported outcome\n",
  });

  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    (error) => {
      assert.equal(error.problem.code, "task_quality_decision_required");
      assert.equal(error.problem.details.allowedOutcomes.includes("pass"), true);
      return true;
    },
  );
});

test("updateDoc grandfathers existing done tasks and leaves other document transitions unaffected", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(join(root, "ai_studio", "taskboard", "items"), "T0001", "Legacy done", "done");
  createEpic(root, { title: "Epic" });

  assert.equal(updateDoc(root, "T0001", { fields: { priority: "P0" } }).fields.priority, "P0");
  assert.equal(updateDoc(root, "E001", { fields: { status: "done" } }).fields.status, "done");
});

test("taskboard cli appends structured closeout log lines before updateDoc", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "CLI close",
    status: "review",
    body: "## Done when\n\n- [ ] waived\n\n## Log\n",
  });

  const result = spawnSync(process.execPath, [cliPath, "set", "T0001", "--status", "done",
    "--waiver-reason", "criterion superseded", "--closure-evidence", "E001 decision",
    "--quality", "QCLR_001=pass; QTECH_002=skip", "--quality-evidence", "tests and scope review", "--json"],
  { cwd: root, encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr || result.stdout);
  const closed = findDoc(root, "T0001");
  assert.match(closed.body, /Closure: waived; reason: criterion superseded; evidence: E001 decision/);
  assert.match(closed.body, /Quality: QCLR_001=pass; QTECH_002=skip; evidence: tests and scope review/);
});

test("taskboard cli rejects incomplete or conflicting structured closeout inputs early", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "CLI invalid", status: "review" });
  const incomplete = spawnSync(process.execPath, [cliPath, "set", "T0001", "--waiver-reason", "because", "--json"], { cwd: root, encoding: "utf8" });
  assert.notEqual(incomplete.status, 0);
  assert.equal(JSON.parse(incomplete.stdout).problem.code, "closure_input_invalid");

  const conflicting = spawnSync(process.execPath, [cliPath, "set", "T0001", "--quality", "QCLR_001=pass", "--quality-evidence", "proof", "--quality-not-applicable", "docs only", "--json"], { cwd: root, encoding: "utf8" });
  assert.notEqual(conflicting.status, 0);
  assert.equal(JSON.parse(conflicting.stdout).problem.code, "quality_input_conflict");
});

test("taskboard cli appends a structured quality not-applicable decision", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "CLI not applicable", status: "review", body: "## Done when\n\n- [x] docs updated\n\n## Log\n" });

  const result = spawnSync(process.execPath, [cliPath, "set", "T0001", "--status", "done", "--quality-not-applicable", "documentation-only", "--json"], { cwd: root, encoding: "utf8" });

  assert.equal(result.status, 0, result.stderr || result.stdout);
  assert.match(findDoc(root, "T0001").body, /Quality: not-applicable; reason: documentation-only/);
});

test("Taskboard API guards raw final task body and returns the machine problem", async (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "API close", status: "review", body: "## Done when\n\n- [x] done\n\n## Log\n" });
  const handler = createTaskboardApi(root);

  const response = await invokeApi(handler, "PATCH", "/api/tasks/T0001", {
    fields: { status: "done" },
  });

  assert.equal(response.status, 400);
  assert.equal(response.data.problem.code, "task_quality_decision_required");
  assert.equal(findDoc(root, "T0001").fields.status, "review");

  const accepted = await invokeApi(handler, "PATCH", "/api/tasks/T0001", {
    fields: { status: "done" },
    body: "## Done when\n\n- [x] done\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: API contract test\n",
  });
  assert.equal(accepted.status, 200);
  assert.equal(accepted.data.fields.status, "done");
});

test("updateDoc checks archive move conflicts before rewriting source", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Conflict move",
    status: "doing",
    epic: "E001",
    body: "## What\n\nKeep source stable.\n\n## Done when\n\n- [x] source unchanged on move conflict\n\n## Log\n\n- 2026-07-11: Quality: not-applicable; reason: storage conflict fixture\n",
  });
  const original = readFileSync(task.file, "utf8");
  const archiveDir = join(root, "ai_studio", "taskboard", "items", "archive", "E001");
  mkdirSync(archiveDir, { recursive: true });
  writeFileSync(join(archiveDir, "T0001-conflict-move.md"), "existing archive task\n");

  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    /target already exists/,
  );
  assert.equal(readFileSync(task.file, "utf8"), original);
});

test("markdown preview renders task syntax and escapes html", () => {
  const sandbox = {};
  sandbox.globalThis = sandbox;
  const source = readFileSync(join(taskboardDir, "public", "markdown_preview.js"), "utf8");
  vm.runInNewContext(source, sandbox);
  const html = sandbox.TaskboardMarkdown.renderMarkdown(`# Title

- [x] **done** \`code\`
- item

\`\`\`
<tag>
\`\`\`

<script>alert(1)</script>`);
  assert.match(html, /<h1>Title<\/h1>/);
  assert.match(html, /<input type="checkbox" disabled checked>/);
  assert.match(html, /<strong>done<\/strong>/);
  assert.match(html, /<code>code<\/code>/);
  assert.match(html, /&lt;tag&gt;/);
  assert.match(html, /&lt;script&gt;alert\(1\)&lt;\/script&gt;/);
});

test("nextId stays monotonic across archive pruning via items/.counters.json", (t) => {
  const root = tempRoot(t);
  const itemsDir = join(root, "ai_studio", "taskboard", "items");
  mkdirSync(itemsDir, { recursive: true });
  writeFileSync(join(itemsDir, ".counters.json"), JSON.stringify({ T: 199 }));

  const task = createTask(root, { title: "Fresh task" });
  assert.equal(task.fields.id, "T0200", "counter must beat the empty file scan");

  rmSync(task.file);
  const second = createTask(root, { title: "Second task" });
  assert.equal(second.fields.id, "T0201", "deleting task files must not rewind ids");

  const counters = JSON.parse(readFileSync(join(itemsDir, ".counters.json"), "utf8"));
  assert.equal(counters.T, 201);
});

test("concurrent processes preserve unique ids and all shared counter keys", async (t) => {
  const root = tempRoot(t);
  const workerCount = 24;
  const kinds = ["task", "epic", "project"];
  const workers = Array.from({ length: workerCount }, (_, index) => spawnConcurrentCreator(root, index, kinds[index % kinds.length]));
  let created;
  try {
    await waitFor(() => Array.from({ length: workerCount }, (_, index) => existsSync(join(root, `.ready-${index}`))).every(Boolean));
    writeFileSync(join(root, ".go"), "go");
    created = await Promise.all(workers.map(({ result }) => result));
  } finally {
    for (const { child } of workers) {
      if (child.exitCode === null) child.kill();
    }
    await Promise.allSettled(workers.map(({ closed }) => closed));
    await Promise.allSettled(workers.map(({ result }) => result));
  }

  for (const kind of kinds) {
    const ids = created.filter((doc) => doc.kind === kind).map(({ id }) => id);
    assert.equal(new Set(ids).size, workerCount / kinds.length);
  }
  assert.equal(listTasks(root).length, workerCount / kinds.length);
  assert.equal(listEpics(root).length, workerCount / kinds.length);
  assert.equal(listProjects(root).length, workerCount / kinds.length);
  const counters = JSON.parse(readFileSync(join(root, "ai_studio", "taskboard", "items", ".counters.json"), "utf8"));
  assert.deepEqual(counters, { T: 8, E: 8, P: 8 });
});

test("allocation failure after counter commit preserves valid monotonic state", (t) => {
  const root = tempRoot(t);
  const activeDir = join(root, "ai_studio", "taskboard", "items", "active");
  const itemsDir = dirname(activeDir);
  mkdirSync(itemsDir, { recursive: true });
  writeFileSync(join(itemsDir, ".counters.json"), JSON.stringify({ E: 9, P: 4 }));
  mkdirSync(join(activeDir, "T0001-injected-failure.md"), { recursive: true });

  assert.throws(
    () => createTask(root, { title: "Injected failure" }, { allocationMaxAttempts: 1 }),
    /Could not allocate a unique task id after 1 attempts/,
  );

  const countersAfterFailure = JSON.parse(readFileSync(join(root, "ai_studio", "taskboard", "items", ".counters.json"), "utf8"));
  assert.deepEqual(countersAfterFailure, { E: 9, P: 4, T: 1 });
  assert.equal(listTasks(root).length, 0);
  rmSync(join(activeDir, "T0001-injected-failure.md"), { recursive: true });
  const recovered = createTask(root, { title: "Recovered" });
  assert.equal(recovered.fields.id, "T0002");
});

test("allocation reclaims a stale lock without blocking reads", (t) => {
  const root = tempRoot(t);
  const itemsDir = join(root, "ai_studio", "taskboard", "items");
  const lockDir = join(itemsDir, ".allocation.lock");
  mkdirSync(lockDir, { recursive: true });
  writeFileSync(join(lockDir, "owner.json"), JSON.stringify({ pid: 999999, acquiredAt: "2000-01-01T00:00:00.000Z" }));
  const old = new Date(Date.now() - 60_000);
  utimesSync(lockDir, old, old);

  assert.deepEqual(listTasks(root), []);
  const task = createTask(root, { title: "After stale lock" }, { allocationLockStaleMs: 100 });

  assert.equal(task.fields.id, "T0001");
  assert.equal(existsSync(lockDir), false);
});

test("allocation release does not delete a successor lock with a different token", (t) => {
  const root = tempRoot(t);
  const itemsDir = join(root, "ai_studio", "taskboard", "items");
  const lockDir = join(itemsDir, ".allocation.lock");
  const displacedLock = join(itemsDir, ".allocation.lock.displaced");
  let replaced = false;
  const title = {
    toString() {
      if (!replaced) {
        replaced = true;
        renameSync(lockDir, displacedLock);
        mkdirSync(lockDir);
        writeFileSync(join(lockDir, "owner.json"), JSON.stringify({
          pid: process.pid,
          token: "successor-token",
          acquiredAt: new Date().toISOString(),
        }));
      }
      return "ABA replacement";
    },
  };

  const task = createTask(root, { title });

  assert.equal(task.fields.id, "T0001");
  assert.equal(JSON.parse(readFileSync(join(lockDir, "owner.json"), "utf8")).token, "successor-token");
  assert.equal(existsSync(displacedLock), true);
});

test("fresh live lock times out without blocking reads or edits", (t) => {
  const root = tempRoot(t);
  const existing = createTask(root, { title: "Existing", status: "todo" });
  const lockDir = join(root, "ai_studio", "taskboard", "items", ".allocation.lock");
  mkdirSync(lockDir);
  writeFileSync(join(lockDir, "owner.json"), JSON.stringify({
    pid: process.pid,
    token: "live-token",
    acquiredAt: new Date().toISOString(),
  }));
  const old = new Date(Date.now() - 60_000);
  utimesSync(lockDir, old, old);

  assert.equal(listTasks(root).length, 1);
  assert.equal(updateDoc(root, existing.fields.id, { fields: { priority: "P0" } }).fields.priority, "P0");
  const startedAt = Date.now();
  assert.throws(
    () => createTask(root, { title: "Must time out" }, {
      allocationLockRetryMs: 5,
      allocationLockTimeoutMs: 30,
      allocationLockStaleMs: 10,
    }),
    /Timed out waiting for Taskboard allocation lock/,
  );
  const elapsedMs = Date.now() - startedAt;
  assert.ok(elapsedMs >= 20, `timeout returned too early after ${elapsedMs}ms`);
  assert.ok(elapsedMs < 250, `timeout exceeded its bounded margin at ${elapsedMs}ms`);
  assert.equal(JSON.parse(readFileSync(join(lockDir, "owner.json"), "utf8")).token, "live-token");
});
