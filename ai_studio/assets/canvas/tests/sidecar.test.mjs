// Journal restructure tests: sidecar snapshots, thin lines, O(1) seq, transparent
// fat-journal migration (idempotent), history depth compaction with a clean horizon,
// undo/redo across sidecars, and the capped tool_runs spill. No Python needed. Run:
//   node --test ai_studio/assets/canvas/tests/sidecar.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import {
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import {
  addImage,
  createProject,
  exportElements,
  getProject,
  patchElement,
  readHistory,
  redoOp,
  undoOp,
} from "../ops.mjs";
import { projectCachePaths } from "../store.mjs"; // T0259: journal + snapshots now live in the local cache
import { solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";

function tempProjects(t, env = {}) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-sidecar-"));
  const saved = {
    CANVAS_PROJECTS_ROOT: process.env.CANVAS_PROJECTS_ROOT,
    CANVAS_HISTORY_DEPTH: process.env.CANVAS_HISTORY_DEPTH,
    CANVAS_TOOL_RUNS_CAP: process.env.CANVAS_TOOL_RUNS_CAP,
  };
  process.env.CANVAS_PROJECTS_ROOT = dir;
  for (const [key, value] of Object.entries(env)) process.env[key] = value;
  t.after(() => {
    for (const [key, value] of Object.entries(saved)) {
      if (value === undefined) delete process.env[key];
      else process.env[key] = value;
    }
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

function readJournalRaw(id) {
  return readFileSync(projectCachePaths(ROOT, id).journal, "utf8")
    .split("\n")
    .filter((line) => line.trim())
    .map((line) => JSON.parse(line));
}

test("mutation journal lines are thin metadata; snapshots live in sidecar files", (t) => {
  const dir = tempProjects(t);
  const project = createProject(ROOT, { title: "Sidecar" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  patchElement(ROOT, project.id, element.id, { x: 50 });

  const lines = readJournalRaw(project.id);
  assert.deepEqual(lines.map((l) => l.op), ["addImage", "patchElement"]);
  for (const line of lines) {
    // Thin: op metadata + duration_ms, and NO fat snapshot inline.
    assert.equal(line.has_snapshot, true);
    assert.equal(line.undo_patch, undefined, "no inline undo_patch");
    assert.equal(line.state, undefined, "no inline state");
    assert.equal(typeof line.duration_ms, "number");
    // The sidecar snapshot holds both before/after and fully restores the project.
    const snap = JSON.parse(readFileSync(join(projectCachePaths(ROOT, project.id).snapshots, `${line.seq}.json`), "utf8"));
    assert.ok(snap.undo_patch && snap.state, `snapshot ${line.seq}.json has undo_patch + state`);
  }

  // Roundtrip across the sidecar: undo restores x:0, redo restores x:50.
  assert.equal(undoOp(ROOT, { projectId: project.id }).project.elements[0].x, 0);
  assert.equal(redoOp(ROOT, { projectId: project.id }).project.elements[0].x, 50);
});

test("undo refuses a missing sidecar without changing project state or history", (t) => {
  const dir = tempProjects(t);
  const project = createProject(ROOT, { title: "Missing sidecar" });
  addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  const cache = projectCachePaths(ROOT, project.id);
  const projectPath = join(dir, project.id, "project.json");
  const projectBefore = readFileSync(projectPath, "utf8");
  const journalBefore = readFileSync(cache.journal, "utf8");

  rmSync(join(cache.snapshots, "1.json"));

  assert.throws(
    () => undoOp(ROOT, { projectId: project.id }),
    /Canvas history snapshot unavailable.*seq 1/,
  );
  assert.equal(readFileSync(projectPath, "utf8"), projectBefore);
  assert.equal(readFileSync(cache.journal, "utf8"), journalBefore);
});

test("undo refuses a corrupt sidecar without changing project state or history", (t) => {
  const dir = tempProjects(t);
  const project = createProject(ROOT, { title: "Corrupt sidecar" });
  addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  const cache = projectCachePaths(ROOT, project.id);
  const projectPath = join(dir, project.id, "project.json");
  const projectBefore = readFileSync(projectPath, "utf8");
  const journalBefore = readFileSync(cache.journal, "utf8");

  writeFileSync(join(cache.snapshots, "1.json"), "{ broken json", "utf8");

  assert.throws(
    () => undoOp(ROOT, { projectId: project.id }),
    /Canvas history snapshot unavailable.*seq 1/,
  );
  assert.equal(readFileSync(projectPath, "utf8"), projectBefore);
  assert.equal(readFileSync(cache.journal, "utf8"), journalBefore);
});

test("seq stays monotonic + unique across undo and a new branch (tail-read allocation)", (t) => {
  const dir = tempProjects(t);
  const project = createProject(ROOT, { title: "Seq" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  patchElement(ROOT, project.id, element.id, { x: 10 });
  undoOp(ROOT, { projectId: project.id });
  patchElement(ROOT, project.id, element.id, { x: 20 }); // new branch after undo

  const seqs = readJournalRaw(project.id).map((l) => Number(l.seq));
  assert.deepEqual([...seqs].sort((a, b) => a - b), seqs, "seqs are strictly increasing in append order");
  assert.equal(new Set(seqs).size, seqs.length, "seqs are unique");
});

test("legacy fat journal migrates to thin + sidecar on first mutating open, idempotently, keeps .bak", (t) => {
  const dir = tempProjects(t);
  const id = "fixture-legacy-abc123";
  const projectDir = join(dir, id);
  const now = "2026-07-02T00:00:00.000Z";
  const el = (x) => ({
    id: "el_1", type: "image", src: "files/x.png", x, y: 0, w: 4, h: 3,
    source_w: 4, source_h: 3, name: "a.png", meta: {},
  });
  const snap = (elements) => ({ title: "Fixture", elements, groups: [], tool_runs: [] });

  rmSync(projectDir, { recursive: true, force: true });
  mkdirSync(projectDir, { recursive: true });
  writeFileSync(join(projectDir, "project.json"), JSON.stringify({
    schema: "ai_studio.canvas.project.v1", id, title: "Fixture", created: now, updated: now,
    history_seq: 2, groups: [], elements: [el(50)], tool_runs: [],
  }, null, 2) + "\n");
  // Hand-crafted OLD fat journal: two mutation lines with inline undo_patch/state.
  const fatLines = [
    { seq: 1, at: now, op: "addImage", args_summary: { elementId: "el_1" }, undo_patch: snap([]), state: snap([el(0)]), parent: 0 },
    { seq: 2, at: now, op: "patchElement", args_summary: { elementId: "el_1" }, undo_patch: snap([el(0)]), state: snap([el(50)]), parent: 1 },
  ];
  const fatText = fatLines.map((l) => JSON.stringify(l)).join("\n") + "\n";
  writeFileSync(join(projectDir, "journal.jsonl"), fatText);
  assert.equal(existsSync(join(projectDir, "snapshots")), false, "no sidecar dir before migration");

  // First mutating open triggers a transparent migration into the LOCAL cache (T0259:
  // relocate legacy in-project history + fat->thin), then appends seq 3.
  patchElement(ROOT, id, "el_1", { x: 77 });
  const cache = projectCachePaths(ROOT, id);

  // Relocation removed the legacy journal from the (synced) project dir.
  assert.equal(existsSync(join(projectDir, "journal.jsonl")), false, "legacy journal moved out of the project dir");
  // .bak preserves the ORIGINAL fat journal byte-for-byte (non-destructive), now in the cache.
  assert.equal(readFileSync(cache.backup, "utf8"), fatText, "original fat journal kept as .bak");
  // journal.jsonl is now thin (no inline snapshots) with 3 lines.
  const thin = readJournalRaw(id);
  assert.deepEqual(thin.map((l) => l.seq), [1, 2, 3]);
  for (const line of thin) {
    assert.equal(line.undo_patch, undefined);
    assert.equal(line.state, undefined);
    assert.equal(line.has_snapshot, true);
  }
  // Sidecar snapshots exist for every migrated + new entry.
  for (const seq of [1, 2, 3]) {
    const s = JSON.parse(readFileSync(join(cache.snapshots, `${seq}.json`), "utf8"));
    assert.ok(s.undo_patch && s.state, `snapshots/${seq}.json migrated`);
  }

  // Undo chain works across the migrated sidecars: 77 -> 50 -> 0 -> empty -> stop.
  assert.equal(undoOp(ROOT, { projectId: id }).project.elements[0].x, 50);
  assert.equal(undoOp(ROOT, { projectId: id }).project.elements[0].x, 0);
  assert.equal(undoOp(ROOT, { projectId: id }).project.elements.length, 0);
  assert.throws(() => undoOp(ROOT, { projectId: id }), /nothing to undo/);

  // Idempotence: another mutating op does NOT re-migrate or clobber the backup.
  redoOp(ROOT, { projectId: id }); // back to a non-empty state
  redoOp(ROOT, { projectId: id });
  redoOp(ROOT, { projectId: id });
  patchElement(ROOT, id, "el_1", { x: 88 });
  assert.equal(readFileSync(cache.backup, "utf8"), fatText, ".bak unchanged after a second op");
  assert.equal(readdirSync(cache.dir).filter((n) => n === "journal.jsonl.bak").length, 1, "exactly one backup");
});

test("history depth cap compacts past the horizon and undo stops cleanly there", (t) => {
  const dir = tempProjects(t, { CANVAS_HISTORY_DEPTH: "3" });
  const project = createProject(ROOT, { title: "Compact" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() }); // seq1 (x:0)
  for (let x = 1; x <= 6; x += 1) patchElement(ROOT, project.id, element.id, { x }); // seq2..7 (x:1..6)

  const cache = projectCachePaths(ROOT, project.id); // journal subsystem lives in the cache (T0259)
  const kept = readdirSync(cache.snapshots).map((n) => Number(n.replace(".json", ""))).sort((a, b) => a - b);
  assert.deepEqual(kept, [5, 6, 7], "only the newest cap (3) snapshots survive");
  assert.equal(existsSync(cache.archive), true, "dropped lines archived");
  const archived = readFileSync(cache.archive, "utf8")
    .split("\n").filter(Boolean).map((l) => Number(JSON.parse(l).seq)).sort((a, b) => a - b);
  assert.deepEqual(archived, [1, 2, 3, 4], "archive holds exactly the dropped seqs");

  // Exactly cap (3) undos, then a clean "nothing to undo" at the horizon.
  assert.equal(getProject(ROOT, project.id).elements[0].x, 6);
  assert.equal(undoOp(ROOT, { projectId: project.id }).project.elements[0].x, 5);
  assert.equal(undoOp(ROOT, { projectId: project.id }).project.elements[0].x, 4);
  assert.equal(undoOp(ROOT, { projectId: project.id }).project.elements[0].x, 3);
  assert.equal(readHistory(ROOT, { projectId: project.id }).canUndo, false, "no undo past the horizon");
  assert.throws(() => undoOp(ROOT, { projectId: project.id }), /nothing to undo/);

  // Redo climbs back up the retained window from the horizon.
  assert.equal(redoOp(ROOT, { projectId: project.id }).project.elements[0].x, 4);
  assert.equal(redoOp(ROOT, { projectId: project.id }).project.elements[0].x, 5);
  assert.equal(redoOp(ROOT, { projectId: project.id }).project.elements[0].x, 6);
});

test("tool_runs cap keeps the last N in project.json and spills the rest to tool_runs.jsonl", (t) => {
  const dir = tempProjects(t, { CANVAS_TOOL_RUNS_CAP: "3" });
  const project = createProject(ROOT, { title: "ToolRuns" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  for (let i = 0; i < 5; i += 1) exportElements(ROOT, { projectId: project.id, elementIds: [element.id] });

  const stored = getProject(ROOT, project.id);
  assert.equal(stored.tool_runs.length, 3, "project.json holds only the last 3 tool_runs");
  const spillPath = join(dir, project.id, "tool_runs.jsonl");
  assert.equal(existsSync(spillPath), true, "overflow spilled to tool_runs.jsonl");
  const spilled = readFileSync(spillPath, "utf8").split("\n").filter(Boolean);
  assert.equal(spilled.length, 2, "the two oldest tool_runs spilled");
});
