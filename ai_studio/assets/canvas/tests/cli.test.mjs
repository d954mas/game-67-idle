// Canvas CLI smoke test for the increment-2 commands. Drives the real cli.mjs as
// a child process with the projects root redirected to a temp dir (no Python).
// Run: node --test ai_studio/assets/canvas/tests/cli.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { solidPng } from "./png_fixture.mjs";

const CLI = fileURLToPath(new URL("../cli.mjs", import.meta.url));

function run(env, ...args) {
  const stdout = execFileSync(process.execPath, [CLI, ...args], {
    env: { ...process.env, ...env },
    encoding: "utf8",
  });
  const line = stdout.trim().split("\n").filter(Boolean).at(-1);
  return JSON.parse(line);
}

// Spawn the CLI expecting a non-zero exit (fail()'s contract: "error: <message>" to
// stderr, exit 1) — used to assert the T0234 --expect-head guard's loud-failure path.
function runFail(env, ...args) {
  try {
    execFileSync(process.execPath, [CLI, ...args], { env: { ...process.env, ...env }, encoding: "utf8" });
    assert.fail(`expected "${args.join(" ")}" to fail`);
  } catch (error) {
    return error;
  }
}

test("cli create/add-image/undo/redo/history/export smoke", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(9, 6, [12, 34, 56]));

  const projectId = run(env, "create", "--title", "CLI Canvas").project.id;
  const added = run(env, "add-image", projectId, "--file", pngPath);
  const elementId = added.element.id;
  assert.equal(added.element.w, 9);

  const moved = run(env, "move", projectId, "--element", elementId, "--x", "25", "--y", "10");
  const undone = run(env, "undo", projectId, "--expect-head", String(moved.project.history_seq));
  assert.equal(undone.project.elements[0].x, 0);
  const redone = run(env, "redo", projectId, "--expect-head", String(undone.project.history_seq));
  assert.equal(redone.project.elements[0].x, 25);

  const history = run(env, "history", projectId);
  assert.deepEqual(history.entries.map((entry) => entry.op), ["addImage", "patchElement", "undo", "redo"]);

  const exported = run(env, "export", projectId, "--all");
  assert.equal(exported.items.length, 1);
  assert.equal(exported.manifest.schema, "ai_studio.canvas.export.v1");

  // ops-stats parity: the CLI reports the per-op timing rollup from the journal.
  const stats = run(env, "ops-stats", projectId);
  assert.equal(stats.projectId, projectId);
  const ops = Object.fromEntries(stats.ops.map((o) => [o.op, o]));
  assert.equal(ops.addImage.count, 1);
  assert.equal(ops.patchElement.count, 1);
  assert.equal(stats.errors.count, 0);
});

test("cli history-list + history-jump parity (Base spine + jump reaches panel states)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-history-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(9, 6, [12, 34, 56]));
  const projectId = run(env, "create", "--title", "CLI History").project.id;
  const elementId = run(env, "add-image", projectId, "--file", pngPath).element.id; // seq1
  run(env, "move", projectId, "--element", elementId, "--x", "25", "--y", "10"); // seq2, head2

  // history-list: the same labeled linear spine the page panel renders. CLI-made
  // entries are agent-attributed (T0228), so their labels carry the robot marker.
  const list = run(env, "history-list", projectId);
  assert.deepEqual(list.entries.map((e) => e.label), ["Base", "🤖 Add image", "🤖 Move"]);
  assert.deepEqual(list.entries.map((e) => e.actor), ["user", "agent", "agent"]);
  assert.equal(list.entries.at(-1).current, true);
  // T0234: head is prominent in the JSON too (additive; history_seq is unchanged).
  assert.equal(list.head, 2);
  assert.equal(list.history_seq, 2);

  // history-jump back to seq1 (== undo): the CLI reaches the same state as the panel.
  // --expect-head proves the caller read the CURRENT head (T0234).
  const back = run(env, "history-jump", projectId, "--seq", "1", "--expect-head", String(list.head));
  assert.equal(back.project.elements[0].x, 0);
  assert.equal(back.jumped_to, 1);

  // history-jump forward into the dimmed redo tail (seq2, == redo).
  const forward = run(env, "history-jump", projectId, "--seq", "2", "--expect-head", String(back.project.history_seq));
  assert.equal(forward.project.elements[0].x, 25);

  // history-jump to base (0): empty project.
  const base = run(env, "history-jump", projectId, "--seq", "0", "--expect-head", String(forward.project.history_seq));
  assert.equal(base.project.elements.length, 0);
  assert.equal(base.project.history_seq, 0);
});

test("cli history-list prints the head prominently (\"head: N\" line before the JSON)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-head-line-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(4, 4, [1, 2, 3]));
  const projectId = run(env, "create", "--title", "CLI Head Line").project.id;
  run(env, "add-image", projectId, "--file", pngPath); // seq1, head1

  const stdout = execFileSync(process.execPath, [CLI, "history-list", projectId], {
    env: { ...process.env, ...env },
    encoding: "utf8",
  });
  const lines = stdout.trim().split("\n").filter(Boolean);
  assert.equal(lines[0], "head: 1", "the head line comes before the JSON row");
  const parsed = JSON.parse(lines.at(-1));
  assert.equal(parsed.head, 1);
});

test("cli undo/redo/history-jump REQUIRE --expect-head (T0234); missing flag fails loudly and writes nothing", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-expecthead-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(4, 4, [1, 2, 3]));
  const projectId = run(env, "create", "--title", "CLI Expect Head").project.id;
  run(env, "add-image", projectId, "--file", pngPath); // seq1, head1

  const journalPath = join(dir, projectId, "journal.jsonl");
  const journalBefore = readFileSync(journalPath, "utf8");

  const undoFail = runFail(env, "undo", projectId);
  assert.equal(undoFail.status, 1);
  assert.match(undoFail.stderr, /undo requires --expect-head/);

  const redoFail = runFail(env, "redo", projectId);
  assert.equal(redoFail.status, 1);
  assert.match(redoFail.stderr, /redo requires --expect-head/);

  const jumpFail = runFail(env, "history-jump", projectId, "--seq", "0");
  assert.equal(jumpFail.status, 1);
  assert.match(jumpFail.stderr, /history-jump requires --expect-head/);

  // history-jump still requires --seq first (unchanged existing check).
  const seqFail = runFail(env, "history-jump", projectId);
  assert.match(seqFail.stderr, /history-jump requires --seq/);

  // None of the above wrote anything.
  assert.equal(readFileSync(journalPath, "utf8"), journalBefore);
});

test("cli batched elements-set / elements-remove parity (one undo each)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-batch-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const a = join(dir, "a.png");
  const b = join(dir, "b.png");
  writeFileSync(a, solidPng(4, 4, [10, 0, 0]));
  writeFileSync(b, solidPng(4, 4, [0, 10, 0]));

  const projectId = run(env, "create", "--title", "CLI Batch").project.id;
  const elA = run(env, "add-image", projectId, "--file", a).element.id;
  const elB = run(env, "add-image", projectId, "--file", b).element.id;

  // elements-set: batched move via a JSON patches file -> one journal entry.
  const patchesPath = join(dir, "patches.json");
  writeFileSync(patchesPath, JSON.stringify([{ elementId: elA, x: 40 }, { elementId: elB, x: 60 }]));
  const set = run(env, "elements-set", projectId, "--json", patchesPath);
  assert.equal(set.count, 2);
  assert.deepEqual(run(env, "show", projectId).project.elements.map((e) => e.x), [40, 60]);
  const h1 = run(env, "history", projectId);
  assert.equal(h1.entries.filter((e) => e.op === "patchElements").length, 1);
  run(env, "undo", projectId, "--expect-head", String(set.project.history_seq));
  assert.deepEqual(run(env, "show", projectId).project.elements.map((e) => e.x), [0, 0]);

  // elements-remove: batched delete -> one journal entry, one undo restores both.
  const removed = run(env, "elements-remove", projectId, "--elements", `${elA},${elB}`);
  assert.deepEqual(removed.removed.slice().sort(), [elA, elB].sort());
  assert.equal(run(env, "show", projectId).project.elements.length, 0);
  const h2 = run(env, "history", projectId);
  assert.equal(h2.entries.filter((e) => e.op === "removeElements").length, 1);
  run(env, "undo", projectId, "--expect-head", String(removed.project.history_seq));
  assert.equal(run(env, "show", projectId).project.elements.length, 2);
});

test("cli group-reparent / group-create --parent nesting smoke (no python)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-nest-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const projectId = run(env, "create", "--title", "CLI Nest").project.id;
  const outer = run(env, "group-create", projectId, "--name", "Outer", "--x", "0", "--y", "0", "--w", "100", "--h", "100").group.id;
  // group-create --parent nests directly.
  const child = run(env, "group-create", projectId, "--name", "Child", "--x", "5", "--y", "5", "--w", "20", "--h", "20", "--parent", outer);
  assert.equal(child.group.parentId, outer);

  // A separate top-level group, then group-reparent it under outer, then back to root.
  const widget = run(env, "group-create", projectId, "--name", "Widget", "--x", "40", "--y", "0", "--w", "30", "--h", "30").group.id;
  run(env, "group-reparent", projectId, "--group", widget, "--parent", outer);
  let shown = run(env, "show", projectId).project;
  assert.equal(shown.groups.find((g) => g.id === widget).parentId, outer);

  run(env, "group-reparent", projectId, "--group", widget, "--parent", "none");
  shown = run(env, "show", projectId).project;
  assert.equal(shown.groups.find((g) => g.id === widget).parentId, undefined, "reparent to none = top level");
});

test("cli group-set --clip toggles the frame clip flag (no python)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-clip-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const projectId = run(env, "create", "--title", "CLI Clip").project.id;
  const groupId = run(env, "group-create", projectId, "--name", "Frame", "--x", "0", "--y", "0", "--w", "80", "--h", "60").group.id;

  run(env, "group-set", projectId, "--group", groupId, "--clip", "true");
  assert.equal(run(env, "show", projectId).project.groups[0].clip, true, "clip true set");

  run(env, "group-set", projectId, "--group", groupId, "--clip", "false");
  assert.equal("clip" in run(env, "show", projectId).project.groups[0], false, "clip false removes the field");
});

test("cli nodes-duplicate / nodes-delete / nodes-paste parity (one undo each)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-nodes-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const a = join(dir, "a.png");
  writeFileSync(a, solidPng(4, 4, [10, 20, 30]));
  const projectId = run(env, "create", "--title", "CLI Nodes").project.id;
  const elA = run(env, "add-image", projectId, "--file", a).element.id;

  // nodes-duplicate: one new element in place (+16); one undo restores.
  const dup = run(env, "nodes-duplicate", projectId, "--nodes", elA, "--dx", "16", "--dy", "16");
  assert.equal(dup.count, 1);
  const dupId = dup.elementIds[0];
  assert.notEqual(dupId, elA);
  assert.equal(run(env, "show", projectId).project.elements.length, 2);
  const h1 = run(env, "history", projectId);
  assert.equal(h1.entries.filter((e) => e.op === "pasteNodes").length, 1, "duplicate journals one pasteNodes entry");

  // nodes-delete: batched delete; one undo restores.
  const del = run(env, "nodes-delete", projectId, "--nodes", dupId);
  assert.deepEqual(del.removedElements, [dupId]);
  assert.equal(run(env, "show", projectId).project.elements.length, 1);
  run(env, "undo", projectId, "--expect-head", String(del.project.history_seq));
  assert.equal(run(env, "show", projectId).project.elements.length, 2);

  // nodes-paste via a hand-authored spec referencing the immutable file.
  const src = run(env, "show", projectId).project.elements[0].src;
  const specPath = join(dir, "spec.json");
  writeFileSync(
    specPath,
    JSON.stringify({
      schema: "ai_studio.canvas.nodes_spec.v1",
      nodes: [{ kind: "element", element: { type: "image", x: 0, y: 0, w: 4, h: 4, src, name: "pasted" } }],
    }),
  );
  const pasted = run(env, "nodes-paste", projectId, "--spec", specPath, "--dx", "5", "--dy", "5");
  assert.equal(pasted.count, 1);
  const proj = run(env, "show", projectId).project;
  assert.equal(proj.elements.length, 3);
  assert.ok(proj.elements.some((e) => e.name === "pasted"), "hand-authored spec pasted");
});

test("cli add-images batched multi-image add parity (one undo restores all)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-addimgs-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const a = join(dir, "a.png");
  const b = join(dir, "b.png");
  const c = join(dir, "c.png");
  writeFileSync(a, solidPng(4, 4, [10, 0, 0]));
  writeFileSync(b, solidPng(4, 4, [0, 10, 0]));
  writeFileSync(c, solidPng(4, 4, [0, 0, 10]));

  const projectId = run(env, "create", "--title", "CLI AddImages").project.id;
  const added = run(env, "add-images", projectId, "--files", `${a},${b},${c}`);
  assert.equal(added.count, 3);
  assert.equal(run(env, "show", projectId).project.elements.length, 3);
  // One journal entry for the whole batch; one undo removes all three.
  const h = run(env, "history", projectId);
  assert.equal(h.entries.filter((e) => e.op === "addImages").length, 1);
  run(env, "undo", projectId, "--expect-head", String(added.project.history_seq));
  assert.equal(run(env, "show", projectId).project.elements.length, 0);
});

test("cli groups-set batched shared toggles parity (one undo restores all)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-groupsset-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const projectId = run(env, "create", "--title", "CLI GroupsSet").project.id;
  const g1 = run(env, "group-create", projectId, "--name", "A", "--x", "0", "--y", "0", "--w", "50", "--h", "50").group.id;
  const g2 = run(env, "group-create", projectId, "--name", "B", "--x", "0", "--y", "0", "--w", "50", "--h", "50").group.id;

  const set = run(env, "groups-set", projectId, "--groups", `${g1},${g2}`, "--visible", "false", "--clip", "true");
  assert.equal(set.count, 2);
  const shown = run(env, "show", projectId).project;
  for (const id of [g1, g2]) {
    const g = shown.groups.find((group) => group.id === id);
    assert.equal(g.visible, false);
    assert.equal(g.clip, true);
  }
  const h = run(env, "history", projectId);
  assert.equal(h.entries.filter((e) => e.op === "patchGroups").length, 1);
  run(env, "undo", projectId, "--expect-head", String(set.project.history_seq));
  const undone = run(env, "show", projectId).project;
  assert.equal(undone.groups.filter((g) => g.visible === false).length, 0, "one undo restores all");
});

test("cli group-fit resizes the frame to content (no python)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-fit-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const a = join(dir, "a.png");
  const b = join(dir, "b.png");
  writeFileSync(a, solidPng(8, 8, [10, 20, 30]));
  writeFileSync(b, solidPng(6, 6, [30, 40, 50]));

  const projectId = run(env, "create", "--title", "CLI Fit").project.id;
  const elA = run(env, "add-image", projectId, "--file", a).element.id;
  const elB = run(env, "add-image", projectId, "--file", b).element.id;
  run(env, "move", projectId, "--element", elA, "--x", "10", "--y", "10");
  run(env, "move", projectId, "--element", elB, "--x", "30", "--y", "20");

  // An oversized explicit group, assign both, then fit it down to content + 24px pad.
  const groupId = run(env, "group-create", projectId, "--name", "Loose", "--x", "0", "--y", "0", "--w", "500", "--h", "500").group.id;
  run(env, "group-assign", projectId, "--elements", `${elA},${elB}`, "--group", groupId);

  const fitted = run(env, "group-fit", projectId, "--group", groupId).group;
  assert.deepEqual({ x: fitted.x, y: fitted.y, w: fitted.w, h: fitted.h }, { x: -14, y: -14, w: 74, h: 64 });

  const tight = run(env, "group-fit", projectId, "--group", groupId, "--padding", "0").group;
  assert.deepEqual({ x: tight.x, y: tight.y, w: tight.w, h: tight.h }, { x: 10, y: 10, w: 26, h: 16 });
});

test("cli group-create/move/set/assign/delete smoke (no python)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-groups-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const a = join(dir, "a.png");
  const b = join(dir, "b.png");
  writeFileSync(a, solidPng(8, 8, [10, 20, 30]));
  writeFileSync(b, solidPng(6, 6, [30, 40, 50]));

  const projectId = run(env, "create", "--title", "CLI Groups").project.id;
  const elA = run(env, "add-image", projectId, "--file", a).element.id;
  const elB = run(env, "add-image", projectId, "--file", b).element.id;
  run(env, "move", projectId, "--element", elA, "--x", "10", "--y", "10");
  run(env, "move", projectId, "--element", elB, "--x", "30", "--y", "20");

  // group-create from two elements -> a bbox-padded group owning both.
  const created = run(env, "group-create", projectId, "--name", "Main Menu", "--elements", `${elA},${elB}`);
  const groupId = created.group.id;
  assert.equal(created.group.visible, true);
  assert.equal(created.project.elements.every((e) => e.groupId === groupId), true);

  // group-move translates members (verified via show).
  const g0 = created.group;
  run(env, "group-move", projectId, "--group", groupId, "--x", String(g0.x + 40), "--y", String(g0.y + 40));
  let shown = run(env, "show", projectId).project;
  assert.equal(shown.elements.find((e) => e.id === elA).x, 50); // 10 + 40

  // group-set renames + hides.
  run(env, "group-set", projectId, "--group", groupId, "--name", "Hidden", "--visible", "false");
  shown = run(env, "show", projectId).project;
  assert.equal(shown.groups[0].name, "Hidden");
  assert.equal(shown.groups[0].visible, false);

  // group-assign none clears a member's group; group-delete then removes the
  // group AND its remaining members (elB was ungrouped first, so it survives).
  run(env, "group-assign", projectId, "--elements", elB, "--group", "none");
  assert.equal(run(env, "show", projectId).project.elements.find((e) => e.id === elB).groupId, null);
  run(env, "group-delete", projectId, "--group", groupId);
  shown = run(env, "show", projectId).project;
  assert.equal(shown.groups.length, 0);
  assert.equal(shown.elements.length, 1, "member elA deleted with the group");
  assert.equal(shown.elements[0].id, elB);
});

// ---- T0254 Tier 1 #3: `list` summary by default, `--full` restores the full dump --

test("cli list: summary by default (id/title/created/updated/counts/head); --full restores the full project dump", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-list-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(4, 4, [1, 2, 3]));
  const projectId = run(env, "create", "--title", "List Summary").project.id;
  run(env, "add-image", projectId, "--file", pngPath);
  run(env, "group-create", projectId, "--name", "Frame", "--x", "0", "--y", "0", "--w", "10", "--h", "10");

  const summary = run(env, "list").projects;
  assert.equal(summary.length, 1);
  const row = summary[0];
  assert.deepEqual(Object.keys(row).sort(), ["created", "elements", "groups", "head", "id", "title", "updated"].sort());
  assert.equal(row.id, projectId);
  assert.equal(row.title, "List Summary");
  assert.equal(row.elements, 1);
  assert.equal(row.groups, 1);
  assert.equal(row.head, 2, "head matches the project's actual history_seq");
  // Additive contract: the summary carries NO element/group bodies at all.
  assert.equal(row.project, undefined);

  const full = run(env, "list", "--full").projects;
  assert.equal(full.length, 1);
  assert.equal(full[0].id, projectId);
  assert.equal(full[0].elements.length, 1, "--full restores today's exact full-project dump");
  assert.equal(full[0].elements[0].w, 4);
  assert.equal(full[0].groups.length, 1);
});

// ---- T0254 Tier 1 #4: strict boolean parser on every boolean flag site ------------

test("cli parseBool: junk boolean values are a loud, flag-naming error on every boolean flag site (no silent coercion either direction)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-bool-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(4, 4, [1, 2, 3]));
  const projectId = run(env, "create", "--title", "Bool Parity").project.id;
  const elementId = run(env, "add-image", projectId, "--file", pngPath).element.id;
  const groupId = run(env, "group-create", projectId, "--name", "G", "--x", "0", "--y", "0", "--w", "10", "--h", "10").group.id;

  // Junk on EVERY boolean flag site is a loud, flag-naming refusal — not a silent
  // guess (the bug this replaces: junk resolved to *false* in element-set but *true*
  // in group-set for the exact same --visible flag).
  const elementSetFail = runFail(env, "element-set", projectId, "--element", elementId, "--visible", "maybe");
  assert.match(String(elementSetFail.stderr || elementSetFail.message), /--visible must be true, false, 1, or 0/);

  const flipFail = runFail(env, "element-set", projectId, "--element", elementId, "--flip-h", "maybe");
  assert.match(String(flipFail.stderr || flipFail.message), /--flip-h must be true, false, 1, or 0/);

  const groupSetFail = runFail(env, "group-set", projectId, "--group", groupId, "--visible", "maybe");
  assert.match(String(groupSetFail.stderr || groupSetFail.message), /--visible must be true, false, 1, or 0/);

  const groupClipFail = runFail(env, "group-set", projectId, "--group", groupId, "--clip", "maybe");
  assert.match(String(groupClipFail.stderr || groupClipFail.message), /--clip must be true, false, 1, or 0/);

  const groupsSetFail = runFail(env, "groups-set", projectId, "--groups", groupId, "--visible", "maybe");
  assert.match(String(groupsSetFail.stderr || groupsSetFail.message), /--visible must be true, false, 1, or 0/);

  // Both directions resolve CONSISTENTLY once the value is valid: element-set and
  // group-set --visible agree with each other, and 1/0 behave like true/false.
  run(env, "element-set", projectId, "--element", elementId, "--visible", "false");
  assert.equal(run(env, "show", projectId).project.elements[0].visible, false);
  run(env, "element-set", projectId, "--element", elementId, "--visible", "1");
  assert.equal(run(env, "show", projectId).project.elements[0].visible, true, "1 behaves like true");

  run(env, "group-set", projectId, "--group", groupId, "--visible", "false");
  assert.equal(run(env, "show", projectId).project.groups[0].visible, false);
  run(env, "group-set", projectId, "--group", groupId, "--visible", "0");
  assert.equal(run(env, "show", projectId).project.groups[0].visible, false, "0 behaves like false");
});

// ---- T0254 Tier 1 #5: element-set --w/--h (+ --x/--y) single-element resize -------

test("cli element-set --w/--h/--x/--y: single-element resize/reposition lands and journals (parity with the API PATCH route)", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "canvas-cli-resize-"));
  const env = { CANVAS_PROJECTS_ROOT: dir };
  t.after(() => rmSync(dir, { recursive: true, force: true }));

  const pngPath = join(dir, "pic.png");
  writeFileSync(pngPath, solidPng(8, 6, [4, 5, 6]));
  const projectId = run(env, "create", "--title", "Resize CLI").project.id;
  const elementId = run(env, "add-image", projectId, "--file", pngPath).element.id;

  const resized = run(env, "element-set", projectId, "--element", elementId, "--w", "40", "--h", "30", "--x", "5", "--y", "7");
  assert.equal(resized.element.w, 40);
  assert.equal(resized.element.h, 30);
  assert.equal(resized.element.x, 5);
  assert.equal(resized.element.y, 7);

  const shown = run(env, "show", projectId).project.elements[0];
  assert.deepEqual({ w: shown.w, h: shown.h, x: shown.x, y: shown.y }, { w: 40, h: 30, x: 5, y: 7 });

  // One journal entry (patchElement, same op move/the API PATCH route use); undo restores.
  const history = run(env, "history", projectId);
  assert.equal(history.entries.filter((e) => e.op === "patchElement").length, 1);
  run(env, "undo", projectId, "--expect-head", String(resized.project.history_seq));
  const undone = run(env, "show", projectId).project.elements[0];
  assert.deepEqual({ w: undone.w, h: undone.h, x: undone.x, y: undone.y }, { w: 8, h: 6, x: 0, y: 0 });
});
