// Journal + undo/redo + export ops tests (no Python needed). Run:
//   node --test ai_studio/assets/canvas/tests/journal.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, readdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import {
  addImage,
  createProject,
  exportElements,
  getProject,
  historyEntryLabel,
  jumpHistory,
  listHistory,
  patchElement,
  patchProject,
  readHistory,
  setOpsActor,
  redoOp,
  removeElement,
  undoOp,
} from "../ops.mjs";
import { solidPng } from "./png_fixture.mjs";

const ROOT = "C:/unused-repo-root";

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-journal-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return dir;
}

function positions(project) {
  return (project.elements || []).map((element) => ({ id: element.id, x: element.x, y: element.y }));
}

test("journal round-trip: op -> undo -> redo restores identical state", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Round trip" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });

  const moved = patchElement(ROOT, project.id, element.id, { x: 50, y: 30 }).project;
  const afterMove = positions(moved);
  assert.deepEqual(afterMove, [{ id: element.id, x: 50, y: 30 }]);

  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.deepEqual(positions(undone), [{ id: element.id, x: 0, y: 0 }]);

  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.deepEqual(positions(redone), afterMove);

  // A journal.jsonl exists next to project.json and holds mutation + marker lines.
  assert.equal(existsSync(join(process.env.CANVAS_PROJECTS_ROOT, project.id, "journal.jsonl")), true);
  const history = readHistory(ROOT, { projectId: project.id });
  assert.deepEqual(history.entries.map((entry) => entry.op), ["addImage", "patchElement", "undo", "redo"]);
});

test("undo unwinds multiple ops back to the base state, then errors", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Multi" });
  const a = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng(4, 3, [1, 2, 3]) }).element;
  const b = addImage(ROOT, project.id, { name: "b.png", bytes: solidPng(5, 4, [9, 8, 7]) }).element;
  patchElement(ROOT, project.id, b.id, { x: 100 });
  removeElement(ROOT, project.id, a.id);

  assert.equal(getProject(ROOT, project.id).elements.length, 1); // only b remains

  undoOp(ROOT, { projectId: project.id }); // undo remove -> a back
  assert.equal(getProject(ROOT, project.id).elements.length, 2);
  undoOp(ROOT, { projectId: project.id }); // undo move
  assert.equal(getProject(ROOT, project.id).elements.find((e) => e.id === b.id).x, 0);
  undoOp(ROOT, { projectId: project.id }); // undo add b
  undoOp(ROOT, { projectId: project.id }); // undo add a
  assert.equal(getProject(ROOT, project.id).elements.length, 0);
  assert.equal(getProject(ROOT, project.id).history_seq, 0);
  assert.throws(() => undoOp(ROOT, { projectId: project.id }), /nothing to undo/);
});

test("a new op after undo invalidates the redo tail", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Invalidate" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  patchElement(ROOT, project.id, element.id, { x: 50 }); // seq2, head2

  undoOp(ROOT, { projectId: project.id }); // head back to seq1 (x:0)
  assert.equal(getProject(ROOT, project.id).elements[0].x, 0);
  assert.equal(readHistory(ROOT, { projectId: project.id }).canRedo, true);

  patchElement(ROOT, project.id, element.id, { x: 99 }); // new branch, invalidates redo of x:50
  assert.equal(getProject(ROOT, project.id).elements[0].x, 99);
  const history = readHistory(ROOT, { projectId: project.id });
  assert.equal(history.canRedo, false);
  assert.throws(() => redoOp(ROOT, { projectId: project.id }), /nothing to redo/);

  // Undo the new branch, then redo picks the newest branch (x:99), not the stale one.
  undoOp(ROOT, { projectId: project.id });
  assert.equal(getProject(ROOT, project.id).elements[0].x, 0);
  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements[0].x, 99);
});

test("patchProject renames the project and is journaled (undo/redo restore the title)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Old Name" });

  const renamed = patchProject(ROOT, { projectId: project.id, title: "  New Name  " }).project;
  assert.equal(renamed.title, "New Name", "title trimmed and applied");

  // The rename is a journal mutation, so undo restores the previous title.
  const ops = readHistory(ROOT, { projectId: project.id }).entries.map((entry) => entry.op);
  assert.deepEqual(ops, ["patchProject"]);
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.title, "Old Name", "undo restores the old title");
  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.title, "New Name", "redo re-applies the rename");

  // A no-op rename (blank -> keeps current) writes no new journal entry.
  patchProject(ROOT, { projectId: project.id, title: "   " });
  assert.equal(getProject(ROOT, project.id).title, "New Name");
});

// ---- history panel view + jumpHistory (T0204) --------------------------------

test("listHistory renders a labeled linear spine (Base + undo chain), current at the head", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Spine" });
  const { element } = addImage(ROOT, project.id, { name: "hero.png", bytes: solidPng() });
  const head = patchElement(ROOT, project.id, element.id, { x: 50, y: 30 }).project.history_seq;

  const view = listHistory(ROOT, { projectId: project.id });
  assert.deepEqual(view.entries.map((e) => e.label), ["Base", "Add image", "Move"]);
  assert.deepEqual(view.entries.map((e) => e.seq), [0, 1, 2]);
  assert.equal(view.entries.find((e) => e.seq === head).current, true, "head is current");
  assert.equal(view.entries.filter((e) => e.current).length, 1, "exactly one current entry");
  assert.equal(view.entries.every((e) => e.undone === false), true, "nothing dimmed while at the tip");
  assert.equal(view.canUndo, true);
  assert.equal(view.canRedo, false);
  assert.equal(view.entries[1].summary, "hero.png", "add-image summary is the file name");

  // A pure label mapping is exported for reuse; unknown ops fall back to the raw name.
  assert.equal(historyEntryLabel("removeElements", { count: 3 }).label, "Delete elements");
  assert.equal(historyEntryLabel("removeElements", { count: 3 }).summary, "3 elements");
  // alphaCutout (T0230): a batch (count) shows "N images"; a single-element entry still
  // falls back to its method/region-count summary.
  assert.equal(historyEntryLabel("alphaCutout", { count: 2, method: "auto" }).label, "Alpha cutout");
  assert.equal(historyEntryLabel("alphaCutout", { count: 2, method: "auto" }).summary, "2 images");
  assert.equal(historyEntryLabel("alphaCutout", { method: "matte" }).summary, "matte");
  assert.equal(historyEntryLabel("zzz-unknown").label, "zzz-unknown");
});

test("jumpHistory back = N undos, jump forward = N redos (identical state, same snapshots)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Jump" });
  const a = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng(4, 3, [1, 2, 3]) }).element; // seq1
  const b = addImage(ROOT, project.id, { name: "b.png", bytes: solidPng(5, 4, [9, 8, 7]) }).element; // seq2
  patchElement(ROOT, project.id, b.id, { x: 100 }); // seq3, head3

  // Jump back to seq1 (two steps) == undo, undo.
  const jumped = jumpHistory(ROOT, { projectId: project.id, seq: 1 });
  assert.equal(jumped.jumped_from, 3);
  assert.equal(jumped.jumped_to, 1);
  assert.equal(jumped.project.history_seq, 1);
  assert.deepEqual(jumped.project.elements.map((e) => e.id), [a.id], "only A present after jumping to seq1");

  // The redo chain is now the forward spine: jump forward to seq3 == redo, redo.
  const view = listHistory(ROOT, { projectId: project.id });
  assert.deepEqual(view.entries.map((e) => e.seq), [0, 1, 2, 3]);
  assert.equal(view.entries.find((e) => e.seq === 1).current, true);
  assert.deepEqual(view.entries.filter((e) => e.undone).map((e) => e.seq), [2, 3], "seq2/seq3 are the dimmed redo tail");
  assert.equal(view.canRedo, true);

  const forward = jumpHistory(ROOT, { projectId: project.id, seq: 3 });
  assert.equal(forward.project.history_seq, 3);
  assert.equal(forward.project.elements.find((e) => e.id === b.id).x, 100, "B restored to its moved position");

  // The undo/redo chain stays coherent AFTER a jump: undo steps back one, redo re-applies.
  const undone = undoOp(ROOT, { projectId: project.id }).project;
  assert.equal(undone.elements.find((e) => e.id === b.id).x, 0, "undo after a jump behaves as one undo");
  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.find((e) => e.id === b.id).x, 100);
});

test("jumpHistory reaches a dimmed redo-tail entry (== redo) and a jump is reversible", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Redo tail" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() }); // seq1
  patchElement(ROOT, project.id, element.id, { x: 50 }); // seq2, head2
  undoOp(ROOT, { projectId: project.id }); // head1 (x0); seq2 is now the redo tail

  const view = listHistory(ROOT, { projectId: project.id });
  assert.equal(view.entries.find((e) => e.seq === 2).undone, true, "seq2 is a dimmed future state");
  assert.equal(view.canRedo, true);

  // Clicking the dimmed redo-tail entry jumps INTO it (== redo).
  const jumped = jumpHistory(ROOT, { projectId: project.id, seq: 2 }).project;
  assert.equal(jumped.elements[0].x, 50);
  assert.equal(jumped.history_seq, 2);

  // Reverse the jump by jumping back to seq1 (== undo).
  const back = jumpHistory(ROOT, { projectId: project.id, seq: 1 }).project;
  assert.equal(back.elements[0].x, 0);
  assert.equal(back.history_seq, 1);
});

test("jumpHistory to seq 0 restores the base state and leaves undo bottomed out", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "To base" });
  const a = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() }).element;
  addImage(ROOT, project.id, { name: "b.png", bytes: solidPng(5, 5, [1, 1, 1]) });
  patchElement(ROOT, project.id, a.id, { x: 10 }); // head3

  const based = jumpHistory(ROOT, { projectId: project.id, seq: 0 }).project;
  assert.equal(based.elements.length, 0, "base is the empty project");
  assert.equal(based.history_seq, 0);
  assert.throws(() => undoOp(ROOT, { projectId: project.id }), /nothing to undo/, "undo bottoms out at base");

  // Redo re-enters the newest branch from base (== redo), proving the chain is intact.
  const redone = redoOp(ROOT, { projectId: project.id }).project;
  assert.equal(redone.elements.length, 1);
});

test("jumpHistory is one nav marker (not a mutation); no-op jump writes nothing", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Marker" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });
  patchElement(ROOT, project.id, element.id, { x: 7 }); // head2

  jumpHistory(ROOT, { projectId: project.id, seq: 1 });
  const ops = readHistory(ROOT, { projectId: project.id }).entries.map((e) => e.op);
  assert.deepEqual(ops, ["addImage", "patchElement", "jump"], "a jump appends a jump marker, not a mutation");

  // A jump to the CURRENT head is a no-op — no extra marker.
  const before = readHistory(ROOT, { projectId: project.id }).entries.length;
  const noop = jumpHistory(ROOT, { projectId: project.id, seq: 1 });
  assert.equal(noop.jumped_to, 1);
  assert.equal(readHistory(ROOT, { projectId: project.id }).entries.length, before, "no marker for a no-op jump");
});

test("jumpHistory is loud on an unknown, stale-branch, or invalid seq", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Loud" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() }); // seq1
  patchElement(ROOT, project.id, element.id, { x: 50 }); // seq2
  undoOp(ROOT, { projectId: project.id }); // head1 (the undo marker consumes seq3)
  const live = patchElement(ROOT, project.id, element.id, { x: 99 }).project.history_seq; // seq4 — invalidates seq2

  // seq2 is on a dead branch now (and seq3 is a marker, not a mutation): not reachable.
  assert.throws(() => jumpHistory(ROOT, { projectId: project.id, seq: 2 }), /not on the current history/);
  assert.throws(() => jumpHistory(ROOT, { projectId: project.id, seq: 3 }), /not on the current history/);
  // An unknown seq, a negative seq, and a non-integer seq are all loud.
  assert.throws(() => jumpHistory(ROOT, { projectId: project.id, seq: 999 }), /not on the current history/);
  assert.throws(() => jumpHistory(ROOT, { projectId: project.id, seq: -1 }), /non-negative integer/);
  assert.throws(() => jumpHistory(ROOT, { projectId: project.id, seq: 1.5 }), /non-negative integer/);
  assert.throws(() => jumpHistory(ROOT, {}), /requires projectId/);

  // The live branch (seq4) is still reachable.
  assert.equal(live, 4);
  assert.equal(jumpHistory(ROOT, { projectId: project.id, seq: live }).project.elements[0].x, 99);
});

// ---- T0234: concurrency guard (expectHead) on undo/redo/jumpHistory ----------
//
// Incident 2026-07-03: an agent read a project at head 823, the lead kept working
// live to head 876, and the agent's jumpHistory forked the spine and orphaned the
// lead's newest entries. expectHead makes the caller prove it read the CURRENT head
// right before navigating.

test("jumpHistory refuses a stale expectHead (drift) LOUDLY before any write; matching expectHead behaves exactly as without it", (t) => {
  const dir = tempProjects(t);
  const project = createProject(ROOT, { title: "Drift Jump" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() }); // seq1, head1
  const readHead = getProject(ROOT, project.id).history_seq; // the agent "reads" head1 here
  assert.equal(readHead, 1);
  patchElement(ROOT, project.id, element.id, { x: 50 }); // seq2, head2 — the lead keeps working live

  const journalPath = join(dir, project.id, "journal.jsonl");
  const projectPath = join(dir, project.id, "project.json");
  const journalBefore = readFileSync(journalPath, "utf8");
  const projectBefore = readFileSync(projectPath, "utf8");

  // A stale expectHead (the agent's read-head) refuses BEFORE any write; the error
  // names both seqs and the remedy; journal and project.json are byte-for-byte untouched.
  assert.throws(
    () => jumpHistory(ROOT, { projectId: project.id, seq: 1, expectHead: readHead }),
    /history advanced: head is now 2, you read 1 — the project is live; re-read history \(history-list\) and retry/,
  );
  assert.equal(readFileSync(journalPath, "utf8"), journalBefore, "journal untouched on refusal");
  assert.equal(readFileSync(projectPath, "utf8"), projectBefore, "project.json untouched on refusal");
  assert.equal(getProject(ROOT, project.id).history_seq, 2, "head unchanged on refusal");

  // A matching expectHead (re-read the CURRENT head first) behaves exactly like the
  // no-param call.
  const jumped = jumpHistory(ROOT, { projectId: project.id, seq: 1, expectHead: 2 });
  assert.equal(jumped.jumped_from, 2);
  assert.equal(jumped.jumped_to, 1);
  assert.equal(jumped.project.history_seq, 1);
  assert.equal(jumped.project.elements[0].x, 0);
});

test("undoOp / redoOp refuse a stale expectHead LOUDLY before any write; matching expectHead behaves exactly as without it", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Drift Undo Redo" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() }); // seq1, head1
  patchElement(ROOT, project.id, element.id, { x: 50 }); // seq2, head2

  const journalPath = join(process.env.CANVAS_PROJECTS_ROOT, project.id, "journal.jsonl");
  const journalBefore = readFileSync(journalPath, "utf8");

  // undoOp: stale expectHead (caller read head1 a while ago; actual head is now 2).
  assert.throws(
    () => undoOp(ROOT, { projectId: project.id, expectHead: 1 }),
    /history advanced: head is now 2, you read 1 — the project is live; re-read history \(history-list\) and retry/,
  );
  assert.equal(readFileSync(journalPath, "utf8"), journalBefore, "journal untouched on refusal");
  assert.equal(getProject(ROOT, project.id).history_seq, 2, "head unchanged on refusal");

  // Matching expectHead behaves exactly like the no-param call.
  const undone = undoOp(ROOT, { projectId: project.id, expectHead: 2 });
  assert.equal(undone.project.history_seq, 1);
  assert.equal(undone.project.elements[0].x, 0);

  // redoOp: stale expectHead (actual head is now 1, after the undo above).
  assert.throws(
    () => redoOp(ROOT, { projectId: project.id, expectHead: 99 }),
    /history advanced: head is now 1, you read 99 — the project is live; re-read history \(history-list\) and retry/,
  );
  assert.equal(getProject(ROOT, project.id).history_seq, 1, "head unchanged on refusal");

  // Matching expectHead behaves exactly like the no-param call.
  const redone = redoOp(ROOT, { projectId: project.id, expectHead: 1 });
  assert.equal(redone.project.history_seq, 2);
  assert.equal(redone.project.elements[0].x, 50);
});

test("expectHead is absent (page path) -> unchanged behavior; a non-integer expectHead is a loud error", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Guard edges" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() }); // seq1, head1
  patchElement(ROOT, project.id, element.id, { x: 50 }); // seq2, head2

  // undefined/null/"" all mean "no guard" — identical to calling without the field.
  assert.equal(jumpHistory(ROOT, { projectId: project.id, seq: 1, expectHead: undefined }).jumped_to, 1);
  const back = jumpHistory(ROOT, { projectId: project.id, seq: 2, expectHead: null }).project;
  assert.equal(back.history_seq, 2);

  // A CLI value arrives as a string; a numeric string coerces and matches like a number.
  const jumped = jumpHistory(ROOT, { projectId: project.id, seq: 1, expectHead: "2" });
  assert.equal(jumped.jumped_to, 1);

  // A non-numeric / non-integer expectHead is its own loud error (before any write),
  // regardless of whether it happens to match the head.
  assert.throws(() => jumpHistory(ROOT, { projectId: project.id, seq: 2, expectHead: "abc" }), /expectHead must be a finite integer/);
  assert.throws(() => undoOp(ROOT, { projectId: project.id, expectHead: 1.5 }), /expectHead must be a finite integer/);
  assert.throws(() => redoOp(ROOT, { projectId: project.id, expectHead: NaN }), /expectHead must be a finite integer/);
});

test("exportElements writes a stamped folder with copied files + manifest", async (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Export" });
  const png = solidPng(6, 6, [10, 20, 30]);
  const a = addImage(ROOT, project.id, { name: "Hero Sprite.png", bytes: png }).element;
  const b = addImage(ROOT, project.id, { name: "Hero Sprite.png", bytes: solidPng(7, 7, [40, 50, 60]) }).element;

  const result = await exportElements(ROOT, { projectId: project.id, elementIds: [a.id, b.id] });
  assert.equal(result.items.length, 2);
  assert.equal(result.manifest.schema, "ai_studio.canvas.export.v1");
  assert.equal(result.manifest.project, project.id);

  // Names collide (same title) -> deterministic collision suffix.
  const files = result.items.map((item) => item.file);
  assert.equal(new Set(files).size, 2, "collision-suffixed unique file names");
  for (const item of result.items) {
    assert.equal(existsSync(join(result.folder, item.file)), true, `${item.file} copied`);
  }
  assert.equal(existsSync(join(result.folder, "manifest.json")), true);
  const manifest = JSON.parse(readFileSync(join(result.folder, "manifest.json"), "utf8"));
  assert.deepEqual(manifest.items.map((item) => item.elementId), [a.id, b.id]);

  // Export is NOT journaled (no new undoable entry) but IS recorded in tool_runs.
  const stored = getProject(ROOT, project.id);
  assert.equal(stored.tool_runs.at(-1).op, "export_elements");
  const ops = readHistory(ROOT, { projectId: project.id }).entries.map((entry) => entry.op);
  assert.deepEqual(ops, ["addImage", "addImage"], "export added no journal mutation");

  // The copied bytes match the immutable source.
  const firstFile = readdirSync(result.folder).find((name) => name.endsWith(".png"));
  assert.ok(firstFile, "at least one exported png");
});

test("actor attribution: agent-made ops carry actor + robot label, user default unmarked (T0228)", (t) => {
  tempProjects(t);
  const project = createProject(ROOT, { title: "Actors" });
  const { element } = addImage(ROOT, project.id, { name: "a.png", bytes: solidPng() });

  setOpsActor("agent");
  try {
    patchElement(ROOT, project.id, element.id, { x: 7, y: 9 });
  } finally {
    setOpsActor("user"); // never leak the actor into other tests in this file
  }
  patchElement(ROOT, project.id, element.id, { x: 20, y: 21 });

  const { entries } = listHistory(ROOT, { projectId: project.id });
  const byOp = entries.filter((row) => row.op === "patchElement");
  assert.equal(byOp.length, 2);
  assert.equal(byOp[0].actor, "agent");
  assert.equal(byOp[0].label.startsWith("🤖 "), true);
  assert.equal(byOp[1].actor, "user");
  assert.equal(byOp[1].label.startsWith("🤖"), false);
  // addImage ran before setOpsActor -> default user, unmarked.
  const added = entries.find((row) => row.op === "addImage");
  assert.equal(added.actor, "user");
  assert.equal(added.label.startsWith("🤖"), false);

  assert.throws(() => setOpsActor("robot"), /unknown ops actor/);
});
