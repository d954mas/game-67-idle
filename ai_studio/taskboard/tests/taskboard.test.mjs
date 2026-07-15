// Taskboard core tests. Run: node --test ai_studio/taskboard/tests/taskboard.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readdirSync, rmSync, writeFileSync, readFileSync, mkdirSync, symlinkSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { tmpdir } from "node:os";
import { spawn, spawnSync } from "node:child_process";
import { EventEmitter } from "node:events";
import { URL } from "node:url";
import vm from "node:vm";
import {
  createTask, createEpic, createProject, listTasks,
  listEpics, listProjects, updateDoc, findDoc, sealTaskArchive, validateStore, validateStoreDetailed,
} from "../store.mjs";
import { createStoreZip, readStoreZip } from "../../core_harness/tool_lib/zip_store.mjs";
import { boardPayload, parseDoc, serializeDoc } from "../store.mjs";
import { createTaskboardApi } from "../api.mjs";
import {
  agentContextPayloadForStores,
  studioTaskboardStore,
  validateTaskboardStoresDetailed,
} from "../stores.mjs";
import { main as runTaskboardCli } from "../cli.mjs";

const taskboardDir = dirname(import.meta.dirname);
const cliPath = join(taskboardDir, "cli.mjs");
const activeProjectBody = "## Goal\n\nTrack scoped work.\n\n## In scope\n\n- Owned work\n\n## Out of scope\n\n- Unowned work\n\n## Log\n";

function runCliDirect(root, ...args) {
  let stdout = "";
  let stderr = "";
  const status = runTaskboardCli(args, {
    root,
    writeStdout: (text) => { stdout += text; },
    writeStderr: (text) => { stderr += text; },
  });
  return { status, stdout, stderr };
}

function tempRoot(t, options = {}) {
  const dir = mkdtempSync(join(tmpdir(), "taskboard-test-"));
  if (options.qualityCatalog) {
    for (const [group, id] of [["technical", "QTECH_001"], ["player_clarity", "QCLR_001"]]) {
      const checksRoot = join(dir, "ai_studio", "quality", "rules", group, "checks");
      mkdirSync(checksRoot, { recursive: true });
      writeFileSync(join(dirname(checksRoot), "README.md"), `# ${group}\n`, "utf8");
      writeFileSync(join(checksRoot, `${id}_test.md`), `---\nid: ${id}\ngroup: ${group}\n---\n# ${id} Test\n`, "utf8");
    }
  }
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

function captureChildJson(child, label) {
  const result = new Promise((resolve, reject) => {
    let stdout = "";
    let stderr = "";
    child.stdout.on("data", (chunk) => { stdout += chunk; });
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    child.on("error", reject);
    child.on("close", (code) => {
      if (code !== 0) return reject(new Error(`${label} exited ${code}: ${stderr}`));
      try {
        resolve(JSON.parse(stdout));
      } catch (error) {
        reject(new Error(`${label} returned invalid JSON: ${error.message}; stdout=${stdout}`));
      }
    });
  });
  result.catch(() => {});
  const closed = new Promise((resolve) => child.once("close", resolve));
  return { child, result, closed };
}

function spawnConcurrentCreator(root, workerId, kind) {
  const source = `
    import { existsSync, writeFileSync } from "node:fs";
    import { join } from "node:path";
    import { createTask, createEpic, createProject } from ${JSON.stringify(new URL("../store.mjs", import.meta.url).href)};
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
  return captureChildJson(child, `worker ${workerId}`);
}

function spawnConcurrentCloser(root, workerId) {
  const source = `
    import { existsSync, writeFileSync } from "node:fs";
    import { join } from "node:path";
    import { updateDoc } from ${JSON.stringify(new URL("../store.mjs", import.meta.url).href)};
    const [root, workerId] = process.argv.slice(1);
    writeFileSync(join(root, ".close-ready-" + workerId), "ready");
    while (!existsSync(join(root, ".close-go"))) Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 10);
    try {
      const doc = updateDoc(root, "T0001", {
        fields: { status: "done", quality: { notApplicable: { reason: "concurrent close fixture" } } },
      });
      process.stdout.write(JSON.stringify({ ok: true, file: doc.file }));
    } catch (error) {
      process.stdout.write(JSON.stringify({ ok: false, code: error.code || "", message: error.message }));
    }
  `;
  const child = spawn(process.execPath, ["--input-type=module", "--eval", source, root, String(workerId)], {
    cwd: root,
    stdio: ["ignore", "pipe", "pipe"],
  });
  return captureChildJson(child, `close worker ${workerId}`);
}

function spawnConcurrentArchivePublisher(root, workerId) {
  const source = `
    import { existsSync, mkdirSync, writeFileSync } from "node:fs";
    import { join } from "node:path";
    import { sealArchiveBatch } from ${JSON.stringify(new URL("../archive.mjs", import.meta.url).href)};
    const [root, workerId] = process.argv.slice(1);
    const archiveRoot = join(root, "ai_studio", "taskboard", "items", "archive");
    const sourceDir = join(archiveRoot, "pending", "E001");
    mkdirSync(sourceDir, { recursive: true });
    const body = Buffer.from("worker=" + workerId + "\\n" + "x".repeat(4 * 1024 * 1024));
    const sourceFile = join(sourceDir, "T000" + workerId + "-race.md");
    writeFileSync(sourceFile, body);
    writeFileSync(join(root, ".seal-ready-" + workerId), "ready");
    while (!existsSync(join(root, ".seal-go"))) Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 10);
    try {
      const result = sealArchiveBatch({
        archiveRoot,
        name: "concurrent-batch",
        entries: [
          { path: "E001/T0001-race.md", bytes: body },
          { path: "MANIFEST.md", bytes: Buffer.from("worker=" + workerId + "\\n") },
        ],
        sourceFiles: [sourceFile],
      });
      process.stdout.write(JSON.stringify({ ok: true, workerId, file: result.file, sourceFile }));
    } catch (error) {
      process.stdout.write(JSON.stringify({ ok: false, workerId, message: error.message, sourceFile }));
    }
  `;
  const child = spawn(process.execPath, ["--input-type=module", "--eval", source, root, String(workerId)], {
    cwd: root,
    stdio: ["ignore", "pipe", "pipe"],
  });
  return captureChildJson(child, `archive publisher ${workerId}`);
}

function ensurePrivateGameMount(root, gameId = "secret-game", storageNamespace = gameId) {
  const gameRoot = join(root, "games", "private", gameId);
  mkdirSync(gameRoot, { recursive: true });
  spawnSync("git", ["init"], { cwd: root, encoding: "utf8" });
  spawnSync("git", ["init"], { cwd: gameRoot, encoding: "utf8" });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(gameRoot, "game.json"), JSON.stringify({ schema: "ai_studio.game.v1", id: gameId, title: gameId, storageNamespace }), "utf8");
  writeFileSync(join(gameRoot, "dependencies.json"), JSON.stringify({ schema: "ai_studio.game.dependencies.v2", engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test" }), "utf8");
  mkdirSync(join(gameRoot, ".ai_studio", "taskboard", "items"), { recursive: true });
  return {
    gameId,
    gameRoot,
    itemsRoot: join(gameRoot, ".ai_studio", "taskboard", "items"),
  };
}

test("taskboard accepts a game id paired with its derived namespace store id", (t) => {
  const root = tempRoot(t);
  ensurePrivateGameMount(root, "secret-game", "secret-store");
  const result = runCliDirect(root, "context", "--game", "secret-game", "--store", "game:secret-store", "--json");
  assert.equal(result.status, 0, result.stderr);
  assert.deepEqual(JSON.parse(result.stdout).stores.map((store) => store.storeId), ["game:secret-store"]);
});

function explicitPrivateTaskboardStore(root, gameId = "secret-game") {
  return {
    storeId: `game:${gameId}`,
    visibility: "private",
    kind: "game",
    gameId,
    label: gameId,
    itemsRoot: join(root, "games", "private", gameId, ".ai_studio", "taskboard", "items"),
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
    quality: { checks: [{ id: "QTECH_001", outcome: "pass", evidence: "node --test" }] },
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

test("createTask allocates sequential ids and createEpic separate sequence", (t) => {
  const root = tempRoot(t);
  const t1 = createTask(root, { title: "First" });
  const t2 = createTask(root, { title: "Second" });
  const e1 = createEpic(root, { title: "Epic" });
  assert.equal(t1.fields.id, "T0001");
  assert.equal(t2.fields.id, "T0002");
  assert.equal(e1.fields.id, "E001");
  assert.equal(basename(t1.file), "T0001.md");
  assert.equal(listTasks(root).length, 2);
  assert.equal(listEpics(root).length, 1);
});

test("loader keeps legacy id-slug markdown names readable", (t) => {
  const root = tempRoot(t);
  const itemsRoot = join(root, "ai_studio", "taskboard", "items");
  writeTaskDoc(itemsRoot, "T0042", "Legacy slug", "backlog");
  assert.equal(listTasks(root)[0].fields.id, "T0042");
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

test("done tasks stay pending until an immutable ZIP batch is sealed", (t) => {
  const root = tempRoot(t);
  createEpic(root, { title: "Epic" });
  createTask(root, {
    title: "Archive me",
    epic: "E001",
    status: "todo",
    body: "## Done when\n\n- [x] archived\n\n## Log\n\n- 2026-07-11: Quality: not-applicable; reason: storage behavior fixture\n",
  });
  const updated = updateDoc(root, "T0001", { fields: { status: "done", quality: { notApplicable: { reason: "storage behavior fixture" } } } });
  assert.match(updated.file, /ai_studio[\\/]taskboard[\\/]items[\\/]archive[\\/]pending[\\/]E001[\\/]T0001\.md$/);
  assert.equal(existsSync(updated.file), true);
  assert.equal(listTasks(root).length, 0);
  assert.equal(listTasks(root, { includeArchive: true }).length, 1);
  assert.equal(findDoc(root, "T0001"), null, "normal reads exclude history");
  assert.equal(findDoc(root, "T0001", { includeArchive: true }).fields.status, "done");

  const seal = runCliDirect(root, "archive", "seal", "--name", "2026-07-14-test", "--json");
  assert.equal(seal.status, 0, seal.stderr);
  const sealed = JSON.parse(seal.stdout);
  const sealedFile = join(root, sealed.file);
  assert.match(sealedFile, /2026-07-14-test\.zip$/);
  assert.equal(existsSync(updated.file), false, "verified sources are removed after sealing");
  const entries = readStoreZip(sealedFile);
  assert.deepEqual([...entries.keys()], ["E001/T0001.md", "MANIFEST.md"]);
  assert.match(entries.get("E001/T0001.md").toString("utf8"), /status: done/);
  assert.match(entries.get("MANIFEST.md").toString("utf8"), /T0001.*Archive me/);
  assert.equal(findDoc(root, "T0001"), null);
  assert.equal(findDoc(root, "T0001", { includeArchive: true }).fields.status, "done");

  const normalShow = runCliDirect(root, "show", "T0001", "--json");
  assert.equal(normalShow.status, 1);
  assert.match(normalShow.stderr, /no doc with id T0001/);
  const archiveShow = runCliDirect(root, "show", "T0001", "--archive", "--json");
  assert.equal(archiveShow.status, 0, archiveShow.stderr);
  assert.equal(JSON.parse(archiveShow.stdout).doc.id, "T0001");
  const archiveList = runCliDirect(root, "list", "--archive", "--json");
  assert.equal(archiveList.status, 0, archiveList.stderr);
  const historicalRows = JSON.parse(archiveList.stdout).tasks;
  assert.deepEqual(historicalRows.map((row) => row.id), ["T0001"]);
  assert.equal(historicalRows[0].archived, true);
  assert.equal(historicalRows[0].body, undefined, "history list remains metadata-only");
});

test("archive sealing refuses overwrite before deleting pending sources", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Keep me safe",
    status: "todo",
    body: "## Done when\n\n- [x] safe\n\n## Log\n",
  });
  const updated = updateDoc(root, "T0001", { fields: { status: "done", quality: { notApplicable: { reason: "fixture" } } } });
  const archiveRoot = join(root, "ai_studio", "taskboard", "items", "archive");
  writeFileSync(join(archiveRoot, "occupied.zip"), createStoreZip([
    { path: "MANIFEST.md", bytes: Buffer.from("# Existing archive\n") },
  ]));

  assert.throws(() => sealTaskArchive(root, { name: "occupied" }), /already exists/);
  assert.equal(existsSync(updated.file), true, "refused sealing cannot lose pending history");
});

test("concurrent archive publication is atomic no-replace and preserves the loser source", async (t) => {
  const root = tempRoot(t);
  const workers = [spawnConcurrentArchivePublisher(root, 1), spawnConcurrentArchivePublisher(root, 2)];
  let results;
  try {
    await waitFor(() => workers.every((_, index) => existsSync(join(root, `.seal-ready-${index + 1}`))));
    writeFileSync(join(root, ".seal-go"), "go");
    results = await Promise.all(workers.map(({ result }) => result));
  } finally {
    for (const { child } of workers) if (child.exitCode === null) child.kill();
    await Promise.allSettled(workers.map(({ closed }) => closed));
    await Promise.allSettled(workers.map(({ result }) => result));
  }

  assert.equal(results.filter((result) => result.ok).length, 1);
  const loser = results.find((result) => !result.ok);
  assert.equal(existsSync(loser.sourceFile), true);
  const archive = join(root, "ai_studio", "taskboard", "items", "archive", "concurrent-batch.zip");
  assert.equal(readStoreZip(archive).size, 2);
});

test("archive sealing rejects a linked pending group without reading or deleting outside files", (t) => {
  const root = tempRoot(t);
  const outside = join(root, "outside-history");
  const outsideFile = join(outside, "T0999-outside.md");
  mkdirSync(outside, { recursive: true });
  writeFileSync(outsideFile, "---\nid: T0999\ntitle: Outside\nstatus: done\n---\n");
  const pending = join(root, "ai_studio", "taskboard", "items", "archive", "pending");
  mkdirSync(pending, { recursive: true });
  symlinkSync(outside, join(pending, "E001"), "junction");

  assert.throws(() => sealTaskArchive(root, { name: "linked-group" }), /link/i);
  assert.equal(existsSync(outsideFile), true);
});

test("archive ignores unsupported loose directories outside pending", (t) => {
  const root = tempRoot(t);
  const legacyDir = join(root, "ai_studio", "taskboard", "items", "archive", "E009");
  const legacyFile = join(legacyDir, "T0099-legacy-history.md");
  mkdirSync(legacyDir, { recursive: true });
  writeFileSync(legacyFile, "---\nid: T0099\ntitle: Legacy history\nstatus: done\nproject: P001\nepic: E009\npriority: P2\ntags: []\ncreated: 2026-07-01\nupdated: 2026-07-01\n---\n\n## Log\n");

  assert.equal(findDoc(root, "T0099", { includeArchive: true }), null);
  assert.equal(listTasks(root, { includeArchive: true }).some((doc) => doc.fields.id === "T0099"), false);
  assert.equal(existsSync(legacyFile), true, "unsupported loose history is never read or deleted");
});

test("cli list hides ideas by default and shows them explicitly", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Raw idea", status: "idea" });
  createTask(root, { title: "Actionable", status: "backlog" });
  const normal = runCliDirect(root, "list");
  assert.equal(normal.status, 0, normal.stderr);
  assert.match(normal.stdout, /Actionable/);
  assert.doesNotMatch(normal.stdout, /Raw idea/);
  const ideas = runCliDirect(root, "list", "--ideas");
  assert.equal(ideas.status, 0, ideas.stderr);
  assert.match(ideas.stdout, /Raw idea/);
});

test("cli list shows review by default and keeps ideas hidden", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Active work", status: "todo" });
  createTask(root, { title: "Needs review", status: "review" });
  createTask(root, { title: "Raw idea", status: "idea" });
  const normal = runCliDirect(root, "list");
  assert.equal(normal.status, 0, normal.stderr);
  assert.match(normal.stdout, /Active work/);
  assert.match(normal.stdout, /Needs review/);
  assert.doesNotMatch(normal.stdout, /Raw idea/);
  const ideas = runCliDirect(root, "list", "--ideas");
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
  const result = runCliDirect(root, "context");
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /# Current Context Digest/);
  assert.match(result.stdout, /active_task_counts: idea:1 backlog:0 todo:0 doing:1 review:1/);
  assert.match(result.stdout, /T0001 .* Large active task/);
  assert.match(result.stdout, /T0002 .* Review task/);
  assert.doesNotMatch(result.stdout, /T0003 .* Raw idea/);
  assert.doesNotMatch(result.stdout, /LARGE_TASK_BODY_SHOULD_NOT_APPEAR/);
  assert.match(result.stdout, /inspect only the linked task files/);
});

test("cli context defaults to five summaries and expands only through an explicit limit", (t) => {
  const root = tempRoot(t);
  for (let index = 1; index <= 8; index++) {
    createTask(root, {
      title: `Active task ${index}`,
      status: "doing",
      priority: "P1",
      body: `## What\n\nCONTEXT_BODY_${index}_MUST_STAY_SCOPED\n\n## Done when\n\n- [ ] done\n\n## Log\n`,
    });
  }
  createTask(root, { title: "Ready but outside row budget", status: "backlog", priority: "P0" });

  const defaultResult = runCliDirect(root, "context", "--json");
  assert.equal(defaultResult.status, 0, defaultResult.stderr);
  const defaultPayload = JSON.parse(defaultResult.stdout);
  assert.equal(defaultPayload.currentWork.length, 5);
  assert.equal(defaultPayload.counts.currentWork, 8);
  assert.doesNotMatch(defaultResult.stdout, /CONTEXT_BODY_/);

  const textResult = runCliDirect(root, "context");
  assert.equal(textResult.status, 0, textResult.stderr);
  assert.equal((textResult.stdout.match(/^- T\d{4}/gm) || []).length, 5);
  assert.match(textResult.stdout, /- omitted; context row limit reached/);

  const scopedResult = runCliDirect(root, "context", "--json", "--tasks-limit", "8");
  assert.equal(scopedResult.status, 0, scopedResult.stderr);
  assert.equal(JSON.parse(scopedResult.stdout).currentWork.length, 8);
  assert.doesNotMatch(scopedResult.stdout, /CONTEXT_BODY_/);
});

test("context JSON and text render the canonical task order identically", (t) => {
  const root = tempRoot(t);
  for (const [title, status, priority] of [
    ["Review", "review", "P0"], ["Doing", "doing", "P2"], ["Todo", "todo", "P1"],
    ["Backlog newer", "backlog", "P1"], ["Backlog older", "backlog", "P1"],
  ]) createTask(root, { title, status, priority });
  const json = JSON.parse(runCliDirect(root, "context", "--json").stdout);
  const textResult = runCliDirect(root, "context");
  const textIds = [...textResult.stdout.matchAll(/^- (T\d+)\s/gm)].map((match) => match[1]);
  assert.deepEqual(textIds, [...json.currentWork, ...json.readyQueue].map((row) => row.id));
});

test("agent context separates selected work from three ready backlog candidates", (t) => {
  const root = tempRoot(t);
  for (let index = 1; index <= 5; index++) {
    createTask(root, {
      title: `Ready backlog ${index}`,
      status: "backlog",
      priority: index === 5 ? "P0" : "P1",
      body: `## What\n\nBACKLOG_BODY_${index}_MUST_STAY_SCOPED\n\n## Done when\n\n- [ ] done\n\n## Log\n`,
    });
  }
  createTask(root, { title: "Selected todo", status: "todo", priority: "P1" });
  createTask(root, { title: "Active implementation", status: "doing", priority: "P2" });
  createTask(root, { title: "Awaiting review", status: "review", priority: "P0" });

  const result = runCliDirect(root, "context", "--json");
  assert.equal(result.status, 0, result.stderr);
  const payload = JSON.parse(result.stdout);
  assert.equal(payload.schema, "ai_studio.taskboard.agent_context.v2");
  assert.deepEqual(payload.currentWork.map((task) => task.status), ["doing", "todo", "review"]);
  assert.equal(payload.counts.currentWork, 3);
  assert.equal(payload.counts.readyQueue, 5);
  assert.deepEqual(payload.readyQueue.map((task) => task.id), ["T0005", "T0004"]);
  assert.equal(payload.currentWork.length + payload.readyQueue.length <= 5, true);
  assert.equal(payload.readyQueue.every((task) => task.status === "backlog"), true);
  assert.doesNotMatch(result.stdout, /BACKLOG_BODY_/);

  const textResult = runCliDirect(root, "context");
  assert.equal(textResult.status, 0, textResult.stderr);
  assert.equal((textResult.stdout.match(/^- T\d{4}/gm) || []).length, 5);
  assert.match(textResult.stdout, /T0005 .* Ready backlog 5/);
  assert.match(textResult.stdout, /T0004 .* Ready backlog 4/);
  assert.doesNotMatch(textResult.stdout, /T0003 .* Ready backlog 3/);
});

test("Taskboard agent API context defaults to five body-free summaries", async (t) => {
  const root = tempRoot(t);
  for (let index = 1; index <= 7; index++) {
    createTask(root, {
      title: `API active ${index}`,
      status: "doing",
      body: `## What\n\nAPI_CONTEXT_BODY_${index}_MUST_STAY_SCOPED\n\n## Done when\n\n- [ ] done\n\n## Log\n`,
    });
  }
  const response = await invokeApi(createTaskboardApi(root), "GET", "/api/agent/context");
  assert.equal(response.status, 200);
  assert.equal(response.data.currentWork.length, 5);
  assert.equal(response.data.counts.currentWork, 7);
  assert.deepEqual(response.data.readyQueue, []);
  assert.equal(response.data.counts.readyQueue, 0);
  assert.doesNotMatch(JSON.stringify(response.data), /API_CONTEXT_BODY_/);
});

test("aggregate context reports unsliced totals and globally ranks work across stores", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Studio backlog", status: "backlog", priority: "P2" });
  const privateStore = explicitPrivateTaskboardStore(root);
  writeTaskDoc(privateStore.itemsRoot, "T0001", "Private doing", "doing", { priority: "P0" });

  const payload = agentContextPayloadForStores(root, [studioTaskboardStore(root), privateStore], { limit: 2 });
  assert.equal(payload.currentWork.length, 1);
  assert.equal(payload.counts.currentWork, 1);
  assert.equal(payload.counts.readyQueue, 1);
  assert.equal(payload.currentWork[0].storeId, "game:secret-game");
  assert.equal(payload.currentWork[0].priority, "P0");
  assert.equal(payload.readyQueue[0].storeId, "studio");
  assert.equal(payload.readyQueue[0].status, "backlog");
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

  const list = runCliDirect(root, "list", "--json");
  assert.equal(list.status, 0, list.stderr);
  const listPayload = JSON.parse(list.stdout);
  assert.equal(listPayload.schema, "ai_studio.taskboard.list.v1");
  assert.deepEqual(listPayload.tasks.map((task) => task.id), ["T0001"]);
  assert.equal("body" in listPayload.tasks[0], false);

  const show = runCliDirect(root, "show", "T0001", "--json");
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

  const normal = runCliDirect(root, "list", "--json");
  assert.equal(normal.status, 0, normal.stderr);
  const normalPayload = JSON.parse(normal.stdout);
  assert.deepEqual(normalPayload.tasks.map((task) => task.title), ["Public task"]);
  assert.deepEqual(normalPayload.tasks.map((task) => task.storeId), ["studio"]);

  const included = runCliDirect(root, "list", "--json", "--include-private");
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

  const ambiguous = runCliDirect(root, "show", "T0001", "--json", "--include-private");
  assert.notEqual(ambiguous.status, 0);
  assert.match(ambiguous.stderr || ambiguous.stdout, /ambiguous/i);

  const qualified = runCliDirect(root, "show", "game:secret-game:T0001", "--json", "--include-private");
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

  const taskResult = runCliDirect(root,
    "new", "task",
    "--game", privateStore.gameId,
    "--title", "Private created",
    "--status", "backlog",
    "--json",
  );

  assert.equal(taskResult.status, 0, taskResult.stderr);
  const taskPayload = JSON.parse(taskResult.stdout);
  assert.equal(taskPayload.doc.storeId, "game:secret-game");
  assert.equal(taskPayload.doc.id, "T0001");
  assert.match(taskPayload.doc.file, /games\/private\/secret-game\/\.ai_studio\/taskboard\/items\/active\/T0001\.md$/);

  const projectResult = runCliDirect(root,
    "new", "project",
    "--game", privateStore.gameId,
    "--title", "Private project",
    "--status", "idea",
    "--json",
  );
  assert.equal(projectResult.status, 0, projectResult.stderr);
  const projectPayload = JSON.parse(projectResult.stdout);
  assert.equal(projectPayload.doc.storeId, "game:secret-game");
  assert.equal(projectPayload.doc.id, "P001");
  assert.match(projectPayload.doc.file, /games\/private\/secret-game\/\.ai_studio\/taskboard\/items\/projects\/P001\.md$/);
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
  assert.deepEqual(Object.keys(normal.data.stores[0]).sort(), ["gameId", "kind", "label", "storeId", "visibility"].sort());
  assert.equal(normal.data.root, undefined);

  const included = await invokeApi(handler, "GET", "/api/board?includePrivate=1");
  const privateTask = included.data.tasks.find((task) => task.storeId === "game:secret-game");
  assert.equal(privateTask.fields.title, "Private task");
  assert.equal(privateTask.visibility, "private");
  assert.equal(privateTask.qualifiedId, "game:secret-game:T0001");
  assert.equal(included.data.root, undefined);
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
  assert.deepEqual(Object.keys(scoped.data.stores[0]).sort(), ["gameId", "kind", "label", "storeId", "visibility"].sort());
  assert.equal(scoped.data.root, undefined);

  const created = await invokeApi(handler, "POST", "/api/tasks", {
    title: "Header created",
    status: "backlog",
  }, { "x-ai-studio-store": "game:secret-game" });
  assert.equal(created.status, 201);
  assert.equal(created.data.storeId, "game:secret-game");
  assert.equal(existsSync(join(privateStore.itemsRoot, "active", "T0002.md")), true);

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
  const privateStore = explicitPrivateTaskboardStore(root);
  const stores = [studioTaskboardStore(root), privateStore];
  writeTaskDoc(privateStore.itemsRoot, "T0001", "Ambiguous private task", "backlog", { project: "P001" });

  const ambiguous = validateTaskboardStoresDetailed(root, stores);
  assert.equal(ambiguous.some((problem) => /bare cross-store reference/.test(problem.message)), true);

  rmSync(join(privateStore.itemsRoot, "active"), { recursive: true, force: true });
  writeTaskDoc(privateStore.itemsRoot, "T0001", "Qualified private task", "backlog", { project: "studio:P001" });
  assert.deepEqual(validateTaskboardStoresDetailed(root, stores), []);
});

test("taskboard cli help exits successfully and documents commands", () => {
  const standalone = spawnSync(process.execPath, [cliPath], { encoding: "utf8" });
  for (const result of [
    standalone,
    runCliDirect(process.cwd(), "help"),
    runCliDirect(process.cwd(), "--help"),
    runCliDirect(process.cwd(), "-h"),
  ]) {

    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /usage: cli\.mjs <list\|context\|show\|archive\|new\|set\|validate\|help>/);
    assert.match(result.stdout, /new project --title/);
    assert.match(result.stdout, /context \[--json\]/);
    assert.doesNotMatch(result.stdout, /summary/);
    assert.match(result.stdout, /validate \[--json\]/);
  }
});

test("taskboard cli rejects removed and unrelated commands", () => {
  for (const command of ["profile", "summary", "workflow-run"]) {
    const result = runCliDirect(process.cwd(), command);
    assert.notEqual(result.status, 0);
    assert.match(result.stdout, /usage: cli\.mjs <list\|context\|show\|archive\|new\|set\|validate\|help>/);
    assert.doesNotMatch(result.stdout, /profile|summary|workflow-run/);
  }
});

test("taskboard direct CLI reports a missing set target exactly once", (t) => {
  const root = tempRoot(t);
  const result = runCliDirect(root, "set", "T9999", "--status", "doing", "--json");
  assert.equal(result.status, 1);
  assert.equal(result.stderr, "error: no doc with id T9999\n");
  assert.equal(result.stdout, "");
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

  assert.throws(
    () => updateDoc(root, "T0001", {
      fields: { status: "done" },
      body: task.body.replace("- [ ] behavior is proven", "- [x] behavior is proven"),
    }),
    (error) => error.problem.code === "task_quality_decision_required",
  );
  assert.equal(readFileSync(task.file, "utf8"), original, "missing Quality must not write or archive");
});

test("updateDoc accepts checked criteria plus passing canonical quality evidence", (t) => {
  const root = tempRoot(t, { qualityCatalog: true });
  createTask(root, {
    title: "Checked close",
    status: "review",
    body: "## Done when\n\n- [x] implementation\n- [x] tests\n\n## Log\n\n- 2026-07-11: Quality: QCLR_001=pass; QTECH_001=pass; evidence: focused tests and review\n",
  });

  const updated = updateDoc(root, "T0001", { fields: { status: "done", quality: { checks: [
    { id: "QCLR_001", outcome: "pass", evidence: "player clarity review" },
    { id: "QTECH_001", outcome: "pass", evidence: "focused tests" },
  ] } } });
  assert.equal(updated.fields.status, "done");
  assert.match(updated.file, /archive/);
});

test("updateDoc blocks non-pass Quality outcomes and rejects removed skip", (t) => {
  for (const outcome of ["block", "review", "unverified"]) {
    const root = tempRoot(t, { qualityCatalog: true });
    createTask(root, {
      title: `Blocked ${outcome}`,
      status: "review",
      body: `## Done when\n\n- [x] implementation\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=${outcome}; evidence: explicit ${outcome} decision\n`,
    });
    assert.throws(
      () => updateDoc(root, "T0001", { fields: { status: "done", quality: { checks: [{ id: "QTECH_001", outcome, evidence: `explicit ${outcome} decision` }] } } }),
      (error) => error.problem.code === "task_quality_blocked" && error.problem.details.outcomes.QTECH_001 === outcome,
    );
  }
  const root = tempRoot(t, { qualityCatalog: true });
  createTask(root, { title: "Removed skip", status: "review", body: "## Done when\n\n- [x] implementation\n\n## Log\n" });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done", quality: { checks: [{ id: "QTECH_001", outcome: "skip", evidence: "not run" }] } } }),
    (error) => error.problem.code === "task_quality_invalid",
  );
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

  const root = tempRoot(t, { qualityCatalog: true });
  createTask(root, {
    title: "Criterion continuation",
    status: "review",
    body: "## Done when\n\n- [x] primary criterion\n      with legitimate continuation text\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: continuation test\n",
  });
  assert.equal(updateDoc(root, "T0001", { fields: { status: "done", quality: { checks: [{ id: "QTECH_001", outcome: "pass", evidence: "continuation test" }] } } }).fields.status, "done");
});

test("historical Quality logs never satisfy current state and closure examples stay inert", (t) => {
  const historyRoot = tempRoot(t);
  createTask(historyRoot, { title: "Historical quality", status: "review", body: "## Done when\n\n- [x] done\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: old cycle\n" });
  assert.throws(
    () => updateDoc(historyRoot, "T0001", { fields: { status: "done" } }),
    (error) => error.problem.code === "task_quality_decision_required",
  );

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
    title: "Commented closure record",
    status: "review",
    body: "## Done when\n\n- [ ] open\n\n## Log\n\n<!-- - 2026-07-11: Closure: waived; reason: fake; evidence: comment -->\n",
  });
  assert.throws(
    () => updateDoc(sameLineRoot, "T0001", { fields: { status: "done", quality: { notApplicable: { reason: "scanner fixture" } } } }),
    (error) => error.problem.code === "task_closure_required",
  );
});

test("closeout H2 sections end at the next H1 or H2 outside comments and fences", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Appendix example",
    status: "review",
    body: "## Done when\n\n- [ ] open\n\n## Log\n\n### Notes\n\nNo decision yet.\n\n# Appendix\n\n- 2026-07-11: Closure: waived; reason: appendix; evidence: example only\n",
  });
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done", quality: { notApplicable: { reason: "scanner fixture" } } } }),
    (error) => error.problem.code === "task_closure_required",
  );

  const hashesRoot = tempRoot(t, { qualityCatalog: true });
  createTask(hashesRoot, {
    title: "ATX closing hashes",
    status: "review",
    body: "## Done when ##\n\n- [x] real criterion\n\n## Log ###\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: closing hash regression\n",
  });
  assert.equal(updateDoc(hashesRoot, "T0001", { fields: { status: "done", quality: { checks: [{ id: "QTECH_001", outcome: "pass", evidence: "heading regression" }] } } }).fields.status, "done");
});

test("closeout fences require the same marker and sufficient opening length with no suffix", (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Long fence",
    status: "review",
    body: "## Done when\n\n- [ ] open\n\n## Log\n\n````markdown\n- 2026-07-11: Closure: waived; reason: fenced; evidence: before short pseudo-close\n```\n- 2026-07-11: Closure: waived; reason: fenced; evidence: after short pseudo-close\n```` suffix\n- 2026-07-11: Closure: waived; reason: fenced; evidence: after suffixed pseudo-close\n````\n",
  });

  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done", quality: { notApplicable: { reason: "scanner fixture" } } } }),
    (error) => error.problem.code === "task_closure_required",
  );
});

test("Done when ignores fenced and indented examples but rejects ordered checkbox forms", (t) => {
  const ignoredRoot = tempRoot(t, { qualityCatalog: true });
  createTask(ignoredRoot, {
    title: "Ignored examples",
    status: "review",
    body: "## Done when\n\n```markdown\n- [ ] fenced example\n```\n    - [ ] indented code example\n- [x] real criterion\n\n## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: scanner regression\n",
  });
  assert.equal(updateDoc(ignoredRoot, "T0001", { fields: { status: "done", quality: { checks: [{ id: "QTECH_001", outcome: "pass", evidence: "scanner regression" }] } } }).fields.status, "done");

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
  const root = tempRoot(t, { qualityCatalog: true });
  createTask(root, {
    title: "Indented headings",
    status: "review",
    body: "   ## Done when\n\n   - [x] real criterion\n\n   ## Log\n\n- 2026-07-11: Quality: QTECH_001=pass; evidence: heading indentation regression\n",
  });

  assert.equal(updateDoc(root, "T0001", { fields: { status: "done", quality: { checks: [{ id: "QTECH_001", outcome: "pass", evidence: "heading indentation regression" }] } } }).fields.status, "done");
});

test("quality CLI rejects duplicate rule ids", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Duplicate quality", status: "review" });

  const cli = runCliDirect(root, "set", "T0001", "--quality", "QTECH_001=pass; QTECH_001=pass", "--quality-evidence", "duplicate", "--json");
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

  assert.equal(updateDoc(root, "T0001", { fields: { status: "done", quality: { notApplicable: { reason: "planning-only replacement" } } } }).fields.status, "done");
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

test("structured quality rejects empty, duplicate, and evidence-free check state", (t) => {
  const invalid = [
    { checks: [] },
    { checks: [{ id: "QTECH_001", outcome: "pass", evidence: "" }] },
    { checks: [
      { id: "QTECH_001", outcome: "pass", evidence: "one" },
      { id: "QTECH_001", outcome: "pass", evidence: "two" },
    ] },
    { checks: [{ id: "QTECH_001", outcome: "pass", evidence: "proof" }], notApplicable: { reason: "conflict" } },
    { notApplicable: { reason: "" } },
  ];
  for (const [index, quality] of invalid.entries()) {
    const root = tempRoot(t);
    createTask(root, { title: `Invalid structured quality ${index}`, status: "review" });
    assert.throws(
      () => updateDoc(root, "T0001", { fields: { quality } }),
      (error) => error.problem.code === "task_quality_invalid",
    );
  }

  const catalogRoot = tempRoot(t, { qualityCatalog: true });
  createTask(catalogRoot, { title: "Unknown catalog id", status: "review" });
  assert.throws(
    () => updateDoc(catalogRoot, "T0001", { fields: { quality: { checks: [{ id: "QFAKE_999", outcome: "pass", evidence: "fake" }] } } }),
    (error) => error.problem.code === "task_quality_invalid" && error.problem.details.unknownIds[0] === "QFAKE_999",
  );
});

test("updateDoc grandfathers existing done tasks and leaves other document transitions unaffected", (t) => {
  const root = tempRoot(t);
  writeTaskDoc(join(root, "ai_studio", "taskboard", "items"), "T0001", "Legacy done", "done");
  createEpic(root, { title: "Epic" });

  assert.equal(updateDoc(root, "T0001", { fields: { priority: "P0" } }).fields.priority, "P0");
  assert.equal(updateDoc(root, "E001", { fields: { status: "done" } }).fields.status, "done");
});

test("reopening a task clears its structured quality and requires a new decision", (t) => {
  const root = tempRoot(t, { qualityCatalog: true });
  createTask(root, { title: "Reopen", status: "review", body: "## Done when\n\n- [x] done\n\n## Log\n" });
  updateDoc(root, "T0001", { fields: { status: "done", quality: { checks: [{ id: "QTECH_001", outcome: "pass", evidence: "first cycle" }] } } });
  const reopened = updateDoc(root, "T0001", { fields: { status: "review" } }, { includeArchive: true });
  assert.equal(Object.hasOwn(reopened.fields, "quality"), false);
  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done" } }),
    (error) => error.problem.code === "task_quality_decision_required",
  );
});

test("taskboard cli appends passing structured closeout log lines before updateDoc", (t) => {
  const root = tempRoot(t, { qualityCatalog: true });
  createTask(root, {
    title: "CLI close",
    status: "review",
    body: "## Done when\n\n- [ ] waived\n\n## Log\n",
  });

  const result = runCliDirect(root, "set", "T0001", "--status", "done",
    "--waiver-reason", "criterion superseded", "--closure-evidence", "E001 decision",
    "--quality", "QCLR_001=pass; QTECH_001=pass", "--quality-evidence", "QCLR_001=scope review; QTECH_001=tests", "--json");

  assert.equal(result.status, 0, result.stderr || result.stdout);
  const closed = findDoc(root, "T0001", { includeArchive: true });
  assert.match(closed.body, /Closure: waived; reason: criterion superseded; evidence: E001 decision/);
  assert.deepEqual(closed.fields.quality, { checks: [
    { id: "QCLR_001", outcome: "pass", evidence: "scope review" },
    { id: "QTECH_001", outcome: "pass", evidence: "tests" },
  ] });
  assert.match(closed.body, /Quality: QCLR_001=pass; QTECH_001=pass; evidence: QCLR_001=scope review; QTECH_001=tests/);
});

test("taskboard cli rejects skip and unions coarse suggestions for multi-domain work", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "Player HUD polish", status: "review", tags: ["ui", "art"], body: "## Done when\n\n- [x] HUD reviewed\n\n## Log\n" });
  const result = runCliDirect(root, "set", "T0001", "--quality", "QTECH_001=skip", "--quality-evidence", "not run", "--json");
  assert.notEqual(result.status, 0);
  const problem = JSON.parse(result.stdout).problem;
  assert.equal(problem.code, "quality_input_invalid");
  assert.deepEqual(problem.details.suggestedGroups, ["QART", "QASSET", "QTECH", "QCLR"]);

  const close = runCliDirect(root, "set", "T0001", "--status", "done", "--json");
  assert.notEqual(close.status, 0);
  assert.deepEqual(JSON.parse(close.stdout).problem.details.suggestedGroups, ["QART", "QASSET", "QTECH", "QCLR"]);
});

test("taskboard cli rejects incomplete or conflicting structured closeout inputs early", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "CLI invalid", status: "review" });
  const incomplete = runCliDirect(root, "set", "T0001", "--waiver-reason", "because", "--json");
  assert.notEqual(incomplete.status, 0);
  assert.equal(JSON.parse(incomplete.stdout).problem.code, "closure_input_invalid");

  const conflicting = runCliDirect(root, "set", "T0001", "--quality", "QCLR_001=pass", "--quality-evidence", "proof", "--quality-not-applicable", "docs only", "--json");
  assert.notEqual(conflicting.status, 0);
  assert.equal(JSON.parse(conflicting.stdout).problem.code, "quality_input_conflict");

  const duplicateEvidence = runCliDirect(root, "set", "T0001", "--quality", "QCLR_001=pass; QTECH_001=pass", "--quality-evidence", "QCLR_001=one; QCLR_001=two; QTECH_001=tests", "--json");
  assert.notEqual(duplicateEvidence.status, 0);
  assert.equal(JSON.parse(duplicateEvidence.stdout).problem.code, "quality_input_invalid");
});

test("taskboard cli appends a structured quality not-applicable decision", (t) => {
  const root = tempRoot(t);
  createTask(root, { title: "CLI not applicable", status: "review", body: "## Done when\n\n- [x] docs updated\n\n## Log\n" });

  const result = runCliDirect(root, "set", "T0001", "--status", "done", "--quality-not-applicable", "documentation-only", "--json");

  assert.equal(result.status, 0, result.stderr || result.stdout);
  assert.deepEqual(findDoc(root, "T0001", { includeArchive: true }).fields.quality, { notApplicable: { reason: "documentation-only" } });
  assert.match(findDoc(root, "T0001", { includeArchive: true }).body, /Quality: not-applicable; reason: documentation-only/);
});

test("Taskboard API guards raw final task body and returns the machine problem", async (t) => {
  const root = tempRoot(t, { qualityCatalog: true });
  createTask(root, { title: "API close", status: "review", body: "## Done when\n\n- [x] done\n\n## Log\n" });
  const handler = createTaskboardApi(root);

  const response = await invokeApi(handler, "PATCH", "/api/tasks/T0001", {
    fields: { status: "done" },
  });

  assert.equal(response.status, 400);
  assert.equal(response.data.problem.code, "task_quality_decision_required");
  assert.equal(findDoc(root, "T0001").fields.status, "review");

  const accepted = await invokeApi(handler, "PATCH", "/api/tasks/T0001", {
    fields: { status: "done", quality: { checks: [{ id: "QTECH_001", outcome: "pass", evidence: "API contract test" }] } },
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
  const archiveDir = join(root, "ai_studio", "taskboard", "items", "archive", "pending", "E001");
  mkdirSync(archiveDir, { recursive: true });
  writeFileSync(join(archiveDir, "T0001.md"), "existing archive task\n");

  assert.throws(
    () => updateDoc(root, "T0001", { fields: { status: "done", quality: { notApplicable: { reason: "storage conflict fixture" } } } }),
    /target already exists/,
  );
  assert.equal(readFileSync(task.file, "utf8"), original);
});

test("updateDoc archives an unsafe epic reference under unassigned", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Unsafe archive group",
    status: "doing",
    epic: "../../escaped",
    body: "## Done when\n\n- [x] archived safely\n\n## Log\n",
  });

  const updated = updateDoc(root, task.fields.id, {
    fields: { status: "done", quality: { notApplicable: { reason: "path safety fixture" } } },
  });

  const expected = join(root, "ai_studio", "taskboard", "items", "archive", "pending", "unassigned", basename(task.file));
  assert.equal(updated.file, expected);
  assert.equal(existsSync(expected), true);
});

test("updateDoc maps a qualified epic reference to its canonical archive group", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Qualified archive group",
    status: "doing",
    epic: "game:secret-game:E007",
    body: "## Done when\n\n- [x] archived safely\n\n## Log\n",
  });

  const updated = updateDoc(root, task.fields.id, {
    fields: { status: "done", quality: { notApplicable: { reason: "qualified path fixture" } } },
  });

  const expected = join(root, "ai_studio", "taskboard", "items", "archive", "pending", "E007", basename(task.file));
  assert.equal(updated.file, expected);
  assert.equal(existsSync(expected), true);
});

test("updateDoc prepares the archive destination before rewriting source", (t) => {
  const root = tempRoot(t);
  const task = createTask(root, {
    title: "Blocked archive directory",
    status: "doing",
    epic: "E001",
    body: "## Done when\n\n- [x] source remains unchanged\n\n## Log\n",
  });
  const original = readFileSync(task.file, "utf8");
  const archiveRoot = join(root, "ai_studio", "taskboard", "items", "archive");
  mkdirSync(archiveRoot, { recursive: true });
  mkdirSync(join(archiveRoot, "pending"), { recursive: true });
  writeFileSync(join(archiveRoot, "pending", "E001"), "not a directory\n");

  assert.throws(
    () => updateDoc(root, task.fields.id, {
      fields: { status: "done", quality: { notApplicable: { reason: "destination failure fixture" } } },
    }),
  );
  assert.equal(readFileSync(task.file, "utf8"), original);
});

test("concurrent close never resurrects an active task beside its archive", async (t) => {
  const root = tempRoot(t);
  createTask(root, {
    title: "Concurrent close",
    status: "doing",
    epic: "E001",
    body: `## What\n\n${"x".repeat(512 * 1024)}\n\n## Done when\n\n- [x] closed once\n\n## Log\n`,
  });
  const workers = [spawnConcurrentCloser(root, 0), spawnConcurrentCloser(root, 1)];
  let results;
  try {
    await waitFor(() => workers.every((_, index) => existsSync(join(root, `.close-ready-${index}`))));
    writeFileSync(join(root, ".close-go"), "go");
    results = await Promise.all(workers.map(({ result }) => result));
  } finally {
    for (const { child } of workers) {
      if (child.exitCode === null) child.kill();
    }
    await Promise.allSettled(workers.map(({ closed }) => closed));
    await Promise.allSettled(workers.map(({ result }) => result));
  }

  assert.equal(results.some(({ ok }) => ok), true);
  const active = join(root, "ai_studio", "taskboard", "items", "active");
  const archived = join(root, "ai_studio", "taskboard", "items", "archive", "pending", "E001");
  assert.equal(readdirSync(active).filter((name) => name.endsWith(".md")).length, 0);
  assert.equal(readdirSync(archived).filter((name) => name.endsWith(".md")).length, 1);
  assert.equal(listTasks(root, { includeArchive: true }).filter((task) => task.fields.id === "T0001").length, 1);
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

test("counter reconciliation rescans after a stale writer overwrites a newer high-water mark", (t) => {
  const root = tempRoot(t);
  const itemsDir = join(root, "ai_studio", "taskboard", "items");
  const activeDir = join(itemsDir, "active");
  let injected = false;

  const task = createTask(root, { title: "First task" }, {
    _testBeforeCounterWrite() {
      if (injected) return;
      injected = true;
      writeFileSync(join(activeDir, "T0002.md"), serializeDoc({
        id: "T0002",
        title: "Concurrent higher task",
        status: "backlog",
        priority: "P2",
        tags: [],
        created: "2026-07-15",
        updated: "2026-07-15",
      }, "## What\n\nConcurrent fixture.\n\n## Done when\n\n- [ ] preserved\n\n## Open questions\n\n## Log\n"));
      writeFileSync(join(itemsDir, ".counters.json"), JSON.stringify({ T: 2 }));
    },
  });

  assert.equal(task.fields.id, "T0001");
  assert.deepEqual(listTasks(root).map((doc) => doc.fields.id).sort(), ["T0001", "T0002"]);
  assert.deepEqual(JSON.parse(readFileSync(join(itemsDir, ".counters.json"), "utf8")), { T: 2 });
});

test("failed counter persistence removes the exact task file it reserved", (t) => {
  const root = tempRoot(t);
  const itemsDir = join(root, "ai_studio", "taskboard", "items");
  const activeDir = join(itemsDir, "active");
  mkdirSync(join(itemsDir, ".counters.json"), { recursive: true });

  assert.throws(() => createTask(root, { title: "Must roll back" }, { counterWriteMaxAttempts: 1 }));
  assert.equal(existsSync(join(activeDir, "T0001.md")), false);
  assert.deepEqual(listTasks(root), []);
});

test("ten concurrent task creates preserve unique ids and the counter high-water mark", async (t) => {
  const root = tempRoot(t);
  const workerCount = 10;
  const workers = Array.from({ length: workerCount }, (_, index) => spawnConcurrentCreator(root, index, "task"));
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

  const ids = created.map(({ id }) => id);
  assert.equal(new Set(ids).size, workerCount);
  assert.equal(listTasks(root).length, workerCount);
  const counters = JSON.parse(readFileSync(join(root, "ai_studio", "taskboard", "items", ".counters.json"), "utf8"));
  assert.deepEqual(counters, { T: 10 });
  assert.deepEqual(
    readdirSync(join(root, "ai_studio", "taskboard", "items", "active")).sort(),
    ids.sort().map((id) => `${id}.md`),
  );
});
