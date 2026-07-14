// T0259 — the per-gesture history subsystem (journal.jsonl, sidecar snapshots/, the
// compaction archive and the cross-process .lock) lives in a LOCAL,
// per-machine cache OFF the cloud-synced projects folder. project.json + files/ STAY synced
// (current state + assets travel); undo history deliberately does not. These tests set BOTH
// CANVAS_PROJECTS_ROOT and an explicit CANVAS_LOCAL_CACHE_ROOT to temp dirs so the relocated
// layout is asserted at a known location and nothing leaks onto the real filesystem. Run:
//   node --test ai_studio/assets/canvas/tests/local_cache.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import {
  addImage,
  createProject,
  deleteProject,
  getProject,
  jumpHistory,
  readHistory,
  undoOp,
} from "../ops.mjs";
import { projectCachePaths, withProjectLock } from "../store.mjs";
import { solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";

function restoreEnv(saved) {
  for (const [key, value] of Object.entries(saved)) {
    if (value === undefined) delete process.env[key];
    else process.env[key] = value;
  }
}

// Redirect BOTH the projects root and the local cache root to fresh temp dirs.
function tempEnv(t) {
  const projectsDir = mkdtempSync(join(tmpdir(), "canvas-lc-proj-"));
  const cacheDir = mkdtempSync(join(tmpdir(), "canvas-lc-cache-"));
  const saved = {
    CANVAS_PROJECTS_ROOT: process.env.CANVAS_PROJECTS_ROOT,
    CANVAS_LOCAL_CACHE_ROOT: process.env.CANVAS_LOCAL_CACHE_ROOT,
  };
  process.env.CANVAS_PROJECTS_ROOT = projectsDir;
  process.env.CANVAS_LOCAL_CACHE_ROOT = cacheDir;
  t.after(() => {
    restoreEnv(saved);
    rmSync(projectsDir, { recursive: true, force: true });
    rmSync(cacheDir, { recursive: true, force: true });
  });
  return { projectsDir, cacheDir };
}

// Fabricate a project.json directly (bypassing createProject's random id) so a test can
// control the id and the synced head — the two-machine scenarios can't come from createProject.
function writeProjectJson(projectsDir, id, { history_seq = 0, elements = [] } = {}) {
  const now = "2026-07-04T00:00:00.000Z";
  const dir = join(projectsDir, id);
  mkdirSync(join(dir, "files"), { recursive: true });
  writeFileSync(
    join(dir, "project.json"),
    `${JSON.stringify(
      { schema: "ai_studio.canvas.project.v1", id, title: id, created: now, updated: now, history_seq, groups: [], elements, tool_runs: [] },
      null,
      2,
    )}\n`,
  );
  return id;
}

// A 1-image-element snapshot pair helper for fabricating legacy sidecar snapshots.
const legacyEl = (x) => ({ id: "el_1", type: "image", src: "files/x.png", x, y: 0, w: 4, h: 3, source_w: 4, source_h: 3, name: "a.png", meta: {} });
const legacySnap = (elements) => ({ title: "legacy", elements, groups: [], tool_runs: [] });

test("fresh project: journal/snapshots/.lock live ONLY in the cache; the project dir holds only project.json + files/", async (t) => {
  const { projectsDir, cacheDir } = tempEnv(t);
  const project = createProject(ROOT, { title: "Fresh" });
  const projectDir = join(projectsDir, project.id);
  const cache = projectCachePaths(ROOT, project.id);

  // Drive a real gesture THROUGH the per-project lock (the page/CLI path) so .lock is exercised.
  let lockInCache = false;
  let lockInProjectDir = false;
  await withProjectLock(ROOT, project.id, async () => {
    lockInCache = existsSync(cache.lock);
    lockInProjectDir = existsSync(join(projectDir, ".lock"));
    return addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  });
  assert.ok(lockInCache, ".lock is held in the cache dir during the gesture");
  assert.equal(lockInProjectDir, false, ".lock is never created in the synced project dir");

  // The cache holds the journal + the seq-1 sidecar snapshot, under the configured cache root.
  assert.equal(existsSync(cache.journal), true, "journal is in the cache");
  assert.equal(existsSync(join(cache.snapshots, "1.json")), true, "sidecar snapshot is in the cache");
  assert.ok(cache.dir.startsWith(resolve(cacheDir)), "cache entry lives under the configured cache root");
  assert.ok(!cache.dir.startsWith(resolve(projectsDir)), "cache entry is NOT under the synced projects root");

  // The synced project dir carries ONLY project.json + files/ — no journal, snapshots, or lock.
  assert.deepEqual(readdirSync(projectDir).sort(), ["files", "project.json"]);
});

test("cross-machine: a synced project.json with no local journal mutates from its head; undo/jump into the un-synced history refuse LOUDLY without corruption", (t) => {
  const { projectsDir } = tempEnv(t);
  const id = "synced-proj";
  // project.json arrived via sync at head 3, but this machine's cache has NO journal/snapshots.
  writeProjectJson(projectsDir, id, { history_seq: 3, elements: [legacyEl(0)] });
  const cache = projectCachePaths(ROOT, id);
  assert.equal(existsSync(cache.journal), false, "no local journal for a freshly-synced project");

  // (4c) expect-head checks work off project.json's head: a stale expectHead refuses loudly.
  assert.throws(() => undoOp(ROOT, { projectId: id, expectHead: 2 }), /history advanced: head is now 3/);

  // (4b) undo/jump into the history that only exists on the OTHER machine refuses loudly —
  // never crashes, never wipes state.
  assert.throws(() => undoOp(ROOT, { projectId: id, expectHead: 3 }), /nothing to undo/);
  assert.throws(() => jumpHistory(ROOT, { projectId: id, seq: 1, expectHead: 3 }), /not on the current history/);
  assert.equal(getProject(ROOT, id).elements.length, 1, "refused undo/jump left the project untouched");
  assert.equal(getProject(ROOT, id).history_seq, 3, "head unchanged after the refusals");

  // (4a) a NEW local mutation proceeds and continues the seq from the synced head (4, not 1) —
  // the fresh local journal legitimately begins mid-history.
  addImage(ROOT, id, { name: "b.png", bytes: solidPng() });
  assert.equal(getProject(ROOT, id).history_seq, 4, "seq continues from the synced head 3 -> 4");
  const mut = readHistory(ROOT, { projectId: id }).entries.find((e) => e.op === "addImage");
  assert.equal(mut.seq, 4, "the fresh local journal begins at seq 4, not 1");

  // The local mutation is itself undoable (back to head 3); a further undo again refuses loudly.
  assert.equal(undoOp(ROOT, { projectId: id, expectHead: 4 }).project.history_seq, 3);
  assert.throws(() => undoOp(ROOT, { projectId: id, expectHead: 3 }), /nothing to undo/);
});

test("collision-free: two different projects roots holding the SAME project id keep separate cache entries (keyed by resolved-root hash)", (t) => {
  const cacheDir = mkdtempSync(join(tmpdir(), "canvas-lc-shared-cache-"));
  const rootA = mkdtempSync(join(tmpdir(), "canvas-lc-rootA-"));
  const rootB = mkdtempSync(join(tmpdir(), "canvas-lc-rootB-"));
  const saved = {
    CANVAS_PROJECTS_ROOT: process.env.CANVAS_PROJECTS_ROOT,
    CANVAS_LOCAL_CACHE_ROOT: process.env.CANVAS_LOCAL_CACHE_ROOT,
  };
  process.env.CANVAS_LOCAL_CACHE_ROOT = cacheDir; // ONE shared cache root for both stores
  t.after(() => {
    restoreEnv(saved);
    for (const d of [cacheDir, rootA, rootB]) rmSync(d, { recursive: true, force: true });
  });
  const id = "dup-id";

  process.env.CANVAS_PROJECTS_ROOT = rootA;
  writeProjectJson(rootA, id);
  addImage(ROOT, id, { name: "a.png", bytes: solidPng() });
  const cacheA = projectCachePaths(ROOT, id).dir;

  process.env.CANVAS_PROJECTS_ROOT = rootB;
  writeProjectJson(rootB, id);
  addImage(ROOT, id, { name: "b.png", bytes: solidPng() });
  addImage(ROOT, id, { name: "c.png", bytes: solidPng() });
  const cacheB = projectCachePaths(ROOT, id).dir;

  // Same id, different resolved roots → DIFFERENT cache dirs (hash of the resolved root).
  assert.notEqual(cacheA, cacheB, "the same id under two roots maps to two distinct cache dirs");
  assert.equal(readdirSync(cacheDir).length, 2, "exactly two rootHash subtrees, no shared entry");

  // Neither project's history bled into the other.
  process.env.CANVAS_PROJECTS_ROOT = rootA;
  assert.equal(getProject(ROOT, id).history_seq, 1, "rootA project has exactly its own 1 mutation");
  process.env.CANVAS_PROJECTS_ROOT = rootB;
  assert.equal(getProject(ROOT, id).history_seq, 2, "rootB project has exactly its own 2 mutations");
});

test("deleteProject removes the project's cache entry (undo history is local + not trashed)", (t) => {
  tempEnv(t);
  const project = createProject(ROOT, { title: "Doomed" });
  addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  const cache = projectCachePaths(ROOT, project.id);
  assert.equal(existsSync(cache.journal), true, "the gesture wrote a cache journal");
  assert.equal(existsSync(cache.dir), true);

  const result = deleteProject(ROOT, { projectId: project.id });
  assert.ok(result.trashed.includes(".trash"), "the project folder is trashed (recoverable)");
  assert.equal(existsSync(cache.dir), false, "the cache entry is removed outright");
});
