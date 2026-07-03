// T0254 Tier 1 #1 — per-project write lock tests. Covers both layers withProjectLock
// implements (store.mjs): the in-process promise-chain mutex (page+chat share one
// server process) and the cross-process advisory lockfile (the CLI is a separate
// process). Also covers the stale-before guard (ops.mjs commitMutation/
// refuseIfHeadMoved) that catches a race for the ops that deliberately do NOT hold
// the lock across their slow codex/agy call (generateFromRecipe and friends).
// Run: node --test ai_studio/assets/canvas/tests/lock.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { EventEmitter } from "node:events";
import { URL } from "node:url";
import { projectCachePaths, withProjectLock } from "../store.mjs";
import {
  createProject,
  createRecipeCard,
  expandRecipePrompt,
  getProject,
  patchProject,
  patchRecipe,
  readHistory,
} from "../ops.mjs";
import { createCanvasApi } from "../api.mjs";
import { solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-lock-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

// Minimal req/res doubles, matching api.test.mjs's own local harness (each test file
// keeps its own tiny copy rather than sharing one — the repo's own convention).
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

// ---- in-process: two concurrent API mutations on one project ------------------

test("withProjectLock: two concurrent API mutations on one project both land, sequential seqs, no lost update", async (t) => {
  tempProjects(t);
  const handler = createCanvasApi(ROOT);
  const created = await invokeApi(handler, "POST", "/api/canvas/projects", { title: "Race" });
  const projectId = created.json().project.id;

  // Fired together (Promise.all, not awaited one-by-one) — api.mjs's `locked` wrapper
  // (withProjectLock) must queue these so both survive; without it, a genuine race
  // would only matter for async ops, but this proves the plumbing end-to-end and
  // catches integration bugs a unit test on the lock alone would miss (it already
  // caught one: acquireFileLock used to mkdir a project folder for ANY id, including
  // ones that don't exist — fixed to check existsSync first).
  const png1 = solidPng(4, 4, [10, 0, 0]);
  const png2 = solidPng(4, 4, [0, 10, 0]);
  const [a, b] = await Promise.all([
    invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
      name: "a.png",
      bytes_base64: png1.toString("base64"),
    }),
    invokeApi(handler, "POST", `/api/canvas/projects/${projectId}/images`, {
      name: "b.png",
      bytes_base64: png2.toString("base64"),
    }),
  ]);
  assert.equal(a.status, 201);
  assert.equal(b.status, 201);

  const project = getProject(ROOT, projectId);
  assert.equal(project.elements.length, 2, "both concurrent adds landed — no lost update");
  assert.equal(project.history_seq, 2, "head advanced by exactly 2, once per mutation");

  const history = readHistory(ROOT, { projectId });
  const seqs = history.entries
    .filter((entry) => entry.op === "addImage")
    .map((entry) => entry.seq)
    .sort((x, y) => x - y);
  assert.deepEqual(seqs, [1, 2], "seqs are unique and sequential — no duplicate/collided seq");
});

// ---- stale-before race: the ops that don't hold the lock across their slow call ---

test("stale-before race: a mutation landing during a slow op's external call is a loud HEAD_CONFLICT refusal, journal stays clean", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Stale before" });
  const card = createRecipeCard(ROOT, { projectId: project.id, name: "Card" }).group;
  patchRecipe(ROOT, { projectId: project.id, groupId: card.id, patch: { prompt: "a red fox" } });
  const beforeHead = getProject(ROOT, project.id).history_seq;

  // A fake assistant that mutates the SAME project WHILE expandRecipePrompt's "slow
  // codex call" is in flight — exactly the race window generateFromRecipe/
  // expandRecipePrompt/extractFromElement/alphaDualPlateGenerate open by design (see
  // their withProjectLock comments in ops.mjs): they lock only their own final
  // commit, not the whole op, so a real external call never blocks other mutations.
  const expand = async () => {
    patchProject(ROOT, { projectId: project.id, title: "Renamed mid-flight" });
    return "expanded text";
  };

  await assert.rejects(
    () => expandRecipePrompt(ROOT, { projectId: project.id, groupId: card.id, assistant: { expand } }),
    (error) => {
      assert.equal(error.code, "HEAD_CONFLICT", "a stable marker, not prose-matched");
      assert.match(error.message, /changed underneath op "expandRecipePrompt"/);
      return true;
    },
  );

  // The concurrent rename landed and IS journaled; expandRecipePrompt's own refused
  // attempt left NOTHING — no partial/failed journal entry, and no unjournaled write
  // to project.json either (refuseIfHeadMoved runs BEFORE expandRecipePrompt's own
  // updateProject call, not just at the final commitMutation — see its comment).
  const after = getProject(ROOT, project.id);
  assert.equal(after.title, "Renamed mid-flight");
  assert.equal(after.history_seq, beforeHead + 1, "only the concurrent rename advanced the head");
  assert.equal(
    after.groups.find((g) => g.id === card.id).recipe.expanded,
    null,
    "the refused op wrote nothing at all, not even unjournaled — expanded stays at its unset default",
  );

  const history = readHistory(ROOT, { projectId: project.id });
  assert.equal(history.entries.filter((e) => e.op === "expandRecipePrompt").length, 0, "no entry for the refused op");
  assert.equal(history.entries.filter((e) => e.op === "patchProject").length, 1, "the concurrent op's own entry is untouched");
});

// ---- cross-process: the advisory lockfile ---------------------------------------

test("withProjectLock: a live cross-process lockfile blocks a second acquirer until it is released", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "File lock" });
  // T0259: the lock now lives in the LOCAL cache dir, not the synced project dir. createProject
  // writes no history, so the cache dir may not exist yet — make it before planting the lock.
  const lockPath = projectCachePaths(ROOT, project.id).lock;
  mkdirSync(projectCachePaths(ROOT, project.id).dir, { recursive: true });

  // Simulate ANOTHER process (e.g. the CLI) holding the lock — a live, fresh lock,
  // not a stale/abandoned one. Released 300ms later, well inside the ~2s retry budget.
  writeFileSync(lockPath, JSON.stringify({ pid: 999999, startedAt: Date.now() }));
  const releaseTimer = setTimeout(() => rmSync(lockPath, { force: true }), 300);
  t.after(() => clearTimeout(releaseTimer));

  const startedAt = Date.now();
  const result = await withProjectLock(ROOT, project.id, async () => "ours");
  const elapsed = Date.now() - startedAt;

  assert.equal(result, "ours");
  assert.ok(elapsed >= 250, `expected withProjectLock to wait for the external holder (waited ${elapsed}ms)`);
  assert.equal(existsSync(lockPath), false, "our own lock is released again after use");
});

test("withProjectLock: a stale (abandoned) lockfile is broken with a warning, not waited out", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Stale lock" });
  const lockPath = projectCachePaths(ROOT, project.id).lock; // lock lives in the cache dir (T0259)
  mkdirSync(projectCachePaths(ROOT, project.id).dir, { recursive: true });

  // Older than LOCK_STALE_MS (30s) simulates a crashed holder that never released it —
  // backdate startedAt instead of actually waiting 30 real seconds.
  writeFileSync(lockPath, JSON.stringify({ pid: 424242, startedAt: Date.now() - 45000 }));

  const warnings = [];
  const originalWarn = console.warn;
  console.warn = (...args) => warnings.push(args.join(" "));
  let elapsed;
  let result;
  try {
    const startedAt = Date.now();
    result = await withProjectLock(ROOT, project.id, async () => "ours");
    elapsed = Date.now() - startedAt;
  } finally {
    console.warn = originalWarn;
  }

  assert.equal(result, "ours");
  assert.ok(elapsed < 1000, `expected the stale lock to be broken immediately, not waited out (${elapsed}ms)`);
  assert.ok(
    warnings.some((line) => line.includes("stale project lock") && line.includes("424242")),
    "a loud warning names the stale holder pid",
  );
  assert.equal(existsSync(lockPath), false, "our own lock is released again after use");
});

// ---- project-less calls (list/create) never try to lock a folder that isn't there --

test("withProjectLock: a falsy projectId runs fn unlocked (list/create have no project folder yet)", async (t) => {
  tempProjects(t);
  const result = await withProjectLock(ROOT, undefined, async () => "unlocked");
  assert.equal(result, "unlocked");
});

test("withProjectLock: locking an id with no project folder never creates one (a refused-not-found call leaves zero trace)", async (t) => {
  const dir = tempProjects(t);
  await assert.rejects(() => withProjectLock(ROOT, "ghost-xyz", async () => getProject(ROOT, "ghost-xyz")));
  assert.equal(existsSync(join(dir, "ghost-xyz")), false, "no project folder was created just to acquire a lock on it");
  // T0259: the lock moved to the cache — locking a nonexistent project must ALSO create no
  // cache folder (acquireFileLock returns null before it mkdirs the cache dir).
  assert.equal(existsSync(projectCachePaths(ROOT, "ghost-xyz").dir), false, "no cache folder was created either");
});
