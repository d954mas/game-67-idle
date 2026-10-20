import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, symlinkSync, utimesSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { hostname, tmpdir } from "node:os";
import { createServer } from "node:net";

import {
  checkFeatureWorkspace,
  createFeatureWorkspace,
  defaultWorkspaceBase,
  forgetMissingFeatureWorkspaces,
  inspectCommittedTask,
  inspectRepository,
  normalizeWorkspaceName,
  parseCommandLine,
  listFeatureWorkspaces,
  prepareFeatureWorkspacePython,
  reallocateWorkspacePorts,
  recoverFeatureWorkspace,
  removeFeatureWorkspace,
  removeMergedFeatureWorkspaces,
} from "../feature_workspaces.mjs";


import { commitInEngine, git, markRemovalInterrupted, pairFixture, repoFixture, taskDoc, write } from "./feature_workspaces_fixture.mjs";

test("CLI contract normalizes creation arguments", () => {
  const parsed = parseCommandLine([
    "new", "--game", "fixture-game", "--task", "t0001", "--name", "Death Reward",
    "--devapi-port", "17942", "--web-port", "5242", "--json",
  ]);
  assert.equal(parsed.command, "new");
  assert.equal(parsed.options.task, "T0001");
  assert.equal(parsed.options.name, "death-reward");
  assert.equal(parsed.options.devapiPort, 17942);
  assert.equal(parsed.options.webPort, 5242);
  assert.equal(parsed.options.json, true);
});

test("workspace creation accepts taskboard IDs after allocator width rollover", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  write(fixture.gameRoot, ".ai_studio/taskboard/items/projects/P1000.md", "---\nid: P1000\ntitle: Rollover project\nstatus: active\nkind: game\ntarget: games/private/fixture-game\npriority: P2\ncreated: 2026-01-01\nupdated: 2026-01-01\n---\n\n## Goal\n\nFixture goal.\n\n## In scope\n\n- Fixture work.\n\n## Out of scope\n\n- Production work.\n");
  write(fixture.gameRoot, ".ai_studio/taskboard/items/epics/E1000.md", "---\nid: E1000\ntitle: Rollover epic\nstatus: active\nproject: P1000\npriority: P2\ncreated: 2026-01-01\nupdated: 2026-01-01\n---\n\n## Goal\n\nFixture epic goal.\n\n## In scope\n\n- Fixture task.\n\n## Out of scope\n\n- Other tasks.\n");
  write(fixture.gameRoot, ".ai_studio/taskboard/items/active/T10000.md", taskDoc("T10000", "backlog", { project: "P1000", epic: "E1000" }));
  write(fixture.gameRoot, ".ai_studio/taskboard/items/.counters.json", '{"P":1000,"E":1000,"T":10000}\n');
  git(fixture.gameRoot, "add", ".");
  git(fixture.gameRoot, "commit", "-m", "roll over taskboard ids");

  const parsed = parseCommandLine(["new", "--game", "fixture-game", "--task", "t10000", "--name", "rollover"]);
  assert.equal(parsed.options.task, "T10000");
  const created = await createFeatureWorkspace({
    root: fixture.studioRoot,
    base: fixture.base,
    game: "fixture-game",
    task: parsed.options.task,
    name: "rollover",
  });
  assert.equal(created.taskId, "T10000");
  assert.equal(created.gameBranch, "agent/t10000-rollover");
});

test("CLI contract rejects unsafe names and unknown flags", () => {
  assert.throws(() => normalizeWorkspaceName("../escape"), /workspace name/i);
  assert.throws(
    () => parseCommandLine(["new", "--game", "fixture-game", "--task", "T0001", "--name", "safe", "--ref", "HEAD~1"]),
    /unknown option/i,
  );
});

test("CLI accepts explicit replacement ports", () => {
  const parsed = parseCommandLine(["reallocate-ports", "feature", "--devapi-port", "18001", "--web-port", "5301"]);
  assert.equal(parsed.options.devapiPort, 18001);
  assert.equal(parsed.options.webPort, 5301);
});

test("CLI accepts Python preparation for an existing workspace", () => {
  const parsed = parseCommandLine(["prepare-python", "feature", "--json"]);
  assert.equal(parsed.command, "prepare-python");
  assert.equal(parsed.options.name, "feature");
  assert.equal(parsed.options.json, true);
});

test("default workspace base is a sibling of the Studio checkout", () => {
  const root = resolve("/projects/game-67-idle");
  assert.equal(defaultWorkspaceBase(root), join(dirname(root), `${basename(root)}-workspaces`));
});

test("repository inspection reports dirty state without changing the selected commit", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const head = git(root, "rev-parse", "HEAD");
  write(root, "tracked.txt", "modified\n");
  write(root, "untracked.txt", "untracked\n");

  const inspection = inspectRepository(root, { requireAttached: true });
  assert.equal(inspection.commit, head);
  assert.equal(inspection.ref, "refs/heads/main");
  assert.deepEqual(inspection.dirty, { staged: 0, unstaged: 1, untracked: 1, ignored: 0 });
});

test("an exploration workspace needs no task and gets a branch without a task id", async (t) => {
  const parsed = parseCommandLine(["new", "--game", "fixture-game", "--no-task", "--name", "Idea Probe"]);
  assert.equal(parsed.options.task, null);
  assert.equal(parsed.options.name, "idea-probe");
  assert.throws(
    () => parseCommandLine(["new", "--game", "fixture-game", "--no-task", "--task", "T0001", "--name", "both"]),
    /mutually exclusive/,
  );
  assert.throws(() => parseCommandLine(["new", "--game", "fixture-game", "--name", "neither"]), /--no-task/);

  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({
    root: fixture.studioRoot,
    base: fixture.base,
    game: "fixture-game",
    task: null,
    name: "idea-probe",
  });
  assert.equal(created.state, "ready");
  assert.equal(created.taskId, null);
  assert.equal(git(created.gameWorktree, "symbolic-ref", "HEAD"), "refs/heads/agent/idea-probe");
  const listed = await listFeatureWorkspaces({ base: fixture.base });
  assert.equal(listed.find((row) => row.name === "idea-probe").taskId, null);
  const checked = await checkFeatureWorkspace({ base: fixture.base, name: "idea-probe" });
  assert.equal(checked.ok, true, JSON.stringify(checked.problems));
  await assert.rejects(
    createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: null, name: "bad", branch: "agent/t0001-bad" }),
    /without a task id/,
  );
});

test("committed task validation ignores working-tree edits", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  write(root, ".ai_studio/taskboard/items/active/T0001.md", taskDoc("T0001", "done"));

  const task = inspectCommittedTask(root, "HEAD", "T0001", "fixture-game");
  assert.equal(task.id, "T0001");
  assert.equal(task.status, "backlog");
  assert.equal(task.project, "P001");
  assert.equal(task.epic, "E001");
});

test("committed task validation rejects ineligible and uncommitted tasks", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  write(root, ".ai_studio/taskboard/items/active/T0002.md", taskDoc("T0002"));
  assert.throws(() => inspectCommittedTask(root, "HEAD", "T0002", "fixture-game"), /committed active task/i);

  write(root, ".ai_studio/taskboard/items/active/T0001.md", taskDoc("T0001", "done"));
  git(root, "add", ".");
  git(root, "commit", "-m", "finish task");
  assert.throws(() => inspectCommittedTask(root, "HEAD", "T0001", "fixture-game"), /eligible/i);
});

test("committed task lookup ignores non-Markdown and nested lookalikes", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  write(root, ".ai_studio/taskboard/items/active/T0002-backup.txt", taskDoc("T0002"));
  write(root, ".ai_studio/taskboard/items/active/nested/T0002.md", taskDoc("T0002"));
  write(root, ".ai_studio/taskboard/items/.counters.json", '{"P":1,"E":1,"T":2}\n');
  git(root, "add", ".");
  git(root, "commit", "-m", "lookalikes");
  assert.throws(() => inspectCommittedTask(root, "HEAD", "T0002", "fixture-game"), /found 0/i);
});

test("source game must be on an attached integration branch", (t) => {
  const root = repoFixture();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  git(root, "checkout", "--detach");
  assert.throws(() => inspectRepository(root, { requireAttached: true }), /attached branch/i);
});

test("creation makes detached Studio and branched game worktrees at committed heads", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const studioCommit = git(fixture.studioRoot, "rev-parse", "HEAD");
  const gameCommit = git(fixture.gameRoot, "rev-parse", "HEAD");
  const studioConfig = git(fixture.studioRoot, "config", "--local", "--list");
  const gameConfig = git(fixture.gameRoot, "config", "--local", "--list");
  write(fixture.studioRoot, "dirty-studio.txt", "not copied\n");
  write(fixture.gameRoot, "dirty-game.txt", "not copied\n");
  write(fixture.studioRoot, ".vite/source-cache.bin", "not copied\n");
  write(fixture.gameRoot, "build/source-cache.bin", "not copied\n");

  const result = await createFeatureWorkspace({
    root: fixture.studioRoot,
    base: fixture.base,
    game: "fixture-game",
    task: "T0001",
    name: "death-reward",
  });

  assert.equal(result.state, "ready");
  assert.equal(git(result.studioWorktree, "rev-parse", "HEAD"), studioCommit);
  assert.equal(git(result.gameWorktree, "rev-parse", "HEAD"), gameCommit);
  assert.equal(git(result.studioWorktree, "rev-parse", "--abbrev-ref", "HEAD"), "HEAD");
  assert.equal(git(result.gameWorktree, "symbolic-ref", "HEAD"), "refs/heads/agent/t0001-death-reward");
  assert.equal(git(join(result.studioWorktree, "external/neotolis-engine"), "rev-parse", "HEAD"), fixture.engineCommit);
  assert.equal(result.sourceDirty.studio.untracked, 1);
  assert.equal(result.sourceDirty.game.untracked, 1);
  assert.equal(result.sourceDirty.studio.ignored > 0, true);
  assert.equal(result.sourceDirty.game.ignored, 1);
  assert.equal(git(fixture.studioRoot, "config", "--local", "--list"), studioConfig);
  assert.equal(git(fixture.gameRoot, "config", "--local", "--list"), gameConfig);
  assert.throws(() => git(result.studioWorktree, "show", "HEAD:dirty-studio.txt"));
  assert.throws(() => git(result.gameWorktree, "show", "HEAD:dirty-game.txt"));
});

test("creation prepares and check validates an isolated Studio Python environment", async (t) => {
  const fixture = pairFixture({ withPython: true });
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));

  const created = await createFeatureWorkspace({
    root: fixture.studioRoot,
    base: fixture.base,
    game: "fixture-game",
    task: "T0001",
    name: "python-ready",
    python: true,
  });

  const workspaceVenv = join(created.studioWorktree, ".venv");
  const workspacePython = join(workspaceVenv, process.platform === "win32" ? "Scripts/python.exe" : "bin/python");
  assert.equal(created.python.state, "ready");
  assert.equal(existsSync(workspacePython), true);
  assert.notEqual(realpathSync(workspaceVenv), realpathSync(join(fixture.studioRoot, ".venv")));
  assert.equal((await checkFeatureWorkspace({ base: fixture.base, name: "python-ready" })).python.state, "ready");

  const activePath = join(fixture.base, ".feature-workspaces/active/python-ready.json");
  const manifestPath = join(fixture.base, "python-ready/workspace.json");
  for (const path of [activePath, manifestPath]) {
    const value = JSON.parse(readFileSync(path, "utf8"));
    delete value.python;
    writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
  }
  assert.equal((await prepareFeatureWorkspacePython({ base: fixture.base, name: "python-ready" })).python.state, "ready");

  rmSync(join(fixture.studioRoot, ".venv"), { recursive: true, force: true });
  rmSync(join(workspaceVenv, "pyvenv.cfg"));
  const broken = await checkFeatureWorkspace({ base: fixture.base, name: "python-ready" });
  assert.equal(broken.ok, false);
  assert.equal(broken.python.state, "missing");
  assert.match(broken.problems.join("\n"), /Python environment/i);
});

test("creation checks out a locally available engine object unreachable from its branch", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  git(fixture.engineRoot, "reset", "--hard", "HEAD^");
  assert.equal(git(fixture.engineRoot, "cat-file", "-t", fixture.engineCommit), "commit");
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "unreachable-engine" });
  assert.equal(git(join(created.studioWorktree, "external/neotolis-engine"), "rev-parse", "HEAD"), fixture.engineCommit);
});

test("creation refuses a second live assignment for the same game task", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "one" });
  await assert.rejects(
    createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "two" }),
    /already assigned/i,
  );
});
