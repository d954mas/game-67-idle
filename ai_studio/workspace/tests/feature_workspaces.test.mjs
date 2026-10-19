import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, realpathSync, rmSync, symlinkSync, utimesSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
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

function git(root, ...args) {
  return execFileSync("git", args, { cwd: root, encoding: "utf8" }).trim();
}

function write(root, relativePath, contents) {
  const file = join(root, relativePath);
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, contents, "utf8");
}

function taskDoc(id, status = "backlog", { project = "P001", epic = "E001" } = {}) {
  return `---\nid: ${id}\ntitle: Fixture task\nstatus: ${status}\nproject: ${project}\nepic: ${epic}\npriority: P2\ncreated: 2026-01-01\nupdated: 2026-01-01\n---\n\n## What\n\nFixture.\n\n## Done when\n\n- [ ] Fixture is complete.\n\n## Log\n\n- 2026-01-01: Fixture created.\n`;
}

function repoFixture() {
  const root = mkdtempSync(join(tmpdir(), "feature-workspace-contract-"));
  git(root, "init", "-b", "main");
  git(root, "config", "user.name", "Fixture");
  git(root, "config", "user.email", "fixture@example.invalid");
  write(root, ".ai_studio/taskboard/items/projects/P001.md", "---\nid: P001\ntitle: Fixture\nstatus: active\nkind: game\ntarget: games/private/fixture-game\n---\n");
  write(root, ".ai_studio/taskboard/items/epics/E001.md", "---\nid: E001\ntitle: Fixture epic\nstatus: active\nproject: P001\n---\n");
  write(root, ".ai_studio/taskboard/items/active/T0001.md", taskDoc("T0001"));
  write(root, ".ai_studio/taskboard/items/.counters.json", '{"P":1,"E":1,"T":1}\n');
  write(root, "tracked.txt", "committed\n");
  git(root, "add", ".");
  git(root, "commit", "-m", "fixture");
  return root;
}

function pairFixture({ withPython = false } = {}) {
  const fixtureRoot = mkdtempSync(join(tmpdir(), "feature-workspace-create-"));
  const engineRoot = join(fixtureRoot, "engine-source");
  mkdirSync(engineRoot);
  git(engineRoot, "init", "-b", "main");
  git(engineRoot, "config", "user.name", "Fixture");
  git(engineRoot, "config", "user.email", "fixture@example.invalid");
  write(engineRoot, "engine.txt", "engine\n");
  write(engineRoot, ".gitattributes", "missing.bin filter=lfs diff=lfs merge=lfs -text\n");
  write(engineRoot, "missing.bin", "version https://git-lfs.github.com/spec/v1\noid sha256:0000000000000000000000000000000000000000000000000000000000000000\nsize 1\n");
  git(engineRoot, "add", ".");
  git(engineRoot, "commit", "-m", "engine base");
  write(engineRoot, "engine-next.txt", "engine gitlink\n");
  git(engineRoot, "add", ".");
  git(engineRoot, "commit", "-m", "engine gitlink");
  const engineCommit = git(engineRoot, "rev-parse", "HEAD");

  const studioRoot = join(fixtureRoot, "studio");
  mkdirSync(studioRoot);
  git(studioRoot, "init", "-b", "main");
  git(studioRoot, "config", "user.name", "Fixture");
  git(studioRoot, "config", "user.email", "fixture@example.invalid");
  write(studioRoot, ".gitignore", "games/private/\n.vite/\n.venv/\n__pycache__/\nbuild/\n**/src/generated/\n**/release/artifacts/\n");
  write(studioRoot, ".gitmodules", '[submodule "external/neotolis-engine"]\n\tpath = external/neotolis-engine\n\turl = https://invalid.example/engine.git\n');
  write(studioRoot, "studio.txt", "studio\n");
  if (withPython) {
    const interpreterParts = process.platform === "win32" ? [".venv", "Scripts", "python.exe"] : [".venv", "bin", "python"];
    write(studioRoot, "ai_studio/dev_environment/python_setup.mjs", `import { mkdirSync, writeFileSync } from "node:fs";\nimport { dirname, join } from "node:path";\nconst root = process.cwd();\nconst python = join(root, ...${JSON.stringify(interpreterParts)});\nmkdirSync(dirname(python), { recursive: true });\nwriteFileSync(join(root, ".venv", "pyvenv.cfg"), "home = fixture\\n", "utf8");\nwriteFileSync(python, "fixture\\n", "utf8");\n`);
    write(studioRoot, "ai_studio/dev_environment/python_check.mjs", `import { existsSync } from "node:fs";\nimport { join } from "node:path";\nconst root = process.cwd();\nconst ready = existsSync(join(root, ".venv", "pyvenv.cfg")) && existsSync(join(root, ...${JSON.stringify(interpreterParts)}));\nconsole.log(JSON.stringify({ python: "3.12.4", prefix_ok: ready }));\nprocess.exitCode = ready ? 0 : 1;\n`);
  }
  git(studioRoot, "add", ".");
  git(studioRoot, "update-index", "--add", "--cacheinfo", `160000,${engineCommit},external/neotolis-engine`);
  git(studioRoot, "commit", "-m", "studio");
  mkdirSync(join(studioRoot, "external"), { recursive: true });
  execFileSync("git", ["clone", "--quiet", engineRoot, join(studioRoot, "external/neotolis-engine")], {
    env: { ...process.env, GIT_LFS_SKIP_SMUDGE: "1" },
  });
  if (withPython) {
    write(studioRoot, ".venv/pyvenv.cfg", `home = fixture\nexecutable = ${process.execPath}\n`);
  }

  const gameRoot = join(studioRoot, "games/private/fixture-game");
  mkdirSync(gameRoot, { recursive: true });
  git(gameRoot, "init", "-b", "main");
  git(gameRoot, "config", "user.name", "Fixture");
  git(gameRoot, "config", "user.email", "fixture@example.invalid");
  write(gameRoot, "game.json", '{"schema":"ai_studio.game.v1","id":"fixture-game","title":"Fixture Game","storageNamespace":"fixture-game"}\n');
  write(gameRoot, "dependencies.json", `${JSON.stringify({
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "external/neotolis-engine", version: "0.1.0", revision: engineCommit, compatibility: "fixture" },
    features: [],
    compatibility: "fixture",
  }, null, 2)}\n`);
  write(gameRoot, ".ai_studio/taskboard/items/projects/P001.md", "---\nid: P001\ntitle: Fixture\nstatus: active\nkind: game\ntarget: games/private/fixture-game\npriority: P2\ncreated: 2026-01-01\nupdated: 2026-01-01\n---\n\n## Goal\n\nFixture goal.\n\n## In scope\n\n- Fixture work.\n\n## Out of scope\n\n- Production work.\n\n## Log\n\n- 2026-01-01: Fixture created.\n");
  write(gameRoot, ".ai_studio/taskboard/items/epics/E001.md", "---\nid: E001\ntitle: Fixture epic\nstatus: active\nproject: P001\npriority: P2\ncreated: 2026-01-01\nupdated: 2026-01-01\n---\n\n## Goal\n\nFixture epic goal.\n\n## In scope\n\n- Fixture task.\n\n## Out of scope\n\n- Other tasks.\n\n## Log\n\n- 2026-01-01: Fixture created.\n");
  write(gameRoot, ".ai_studio/taskboard/items/active/T0001.md", taskDoc("T0001"));
  write(gameRoot, ".ai_studio/taskboard/items/active/T0002.md", taskDoc("T0002"));
  write(gameRoot, ".ai_studio/taskboard/items/.counters.json", '{"P":1,"E":1,"T":2}\n');
  write(gameRoot, ".gitignore", "build/\n__pycache__/\nsrc/generated/\nrelease/artifacts/\n.ai_studio/evidence/\nassets/restricted/\n");
  write(gameRoot, "tracked.txt", "committed\n");
  git(gameRoot, "add", ".");
  git(gameRoot, "commit", "-m", "game");
  return { fixtureRoot, studioRoot, gameRoot, engineRoot, engineCommit, base: join(fixtureRoot, "workspaces") };
}

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
  const root = join("C:\\projects", "game-67-idle");
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

test("only recovery reclaims a dead local registry lock", async (t) => {
  const staleFixture = pairFixture();
  t.after(() => rmSync(staleFixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: staleFixture.studioRoot, base: staleFixture.base, game: "fixture-game", task: "T0001", name: "stale-lock" });
  const staleLock = join(staleFixture.base, ".feature-workspaces/lock");
  mkdirSync(staleLock, { recursive: true });
  write(staleFixture.base, ".feature-workspaces/lock/owner.json", `${JSON.stringify({ pid: 999999, hostname: hostname() })}\n`);
  await assert.rejects(
    createFeatureWorkspace({ root: staleFixture.studioRoot, base: staleFixture.base, game: "fixture-game", task: "T0002", name: "blocked-by-stale" }),
    /stale lock/i,
  );
  const activePath = join(staleFixture.base, ".feature-workspaces/active/stale-lock.json");
  const record = JSON.parse(readFileSync(activePath, "utf8"));
  record.state = "recovery-required";
  record.transactionMode = "remove";
  writeFileSync(activePath, `${JSON.stringify(record, null, 2)}\n`, "utf8");
  const recovered = await recoverFeatureWorkspace({ base: staleFixture.base, name: "stale-lock" });
  assert.equal(recovered.state, "removed");
  assert.equal(git(staleFixture.gameRoot, "show-ref", "--verify", `refs/heads/${created.gameBranch}`).length > 0, true);

  const liveFixture = pairFixture();
  t.after(() => rmSync(liveFixture.fixtureRoot, { recursive: true, force: true }));
  const liveLock = join(liveFixture.base, ".feature-workspaces/lock");
  mkdirSync(liveLock, { recursive: true });
  write(liveFixture.base, ".feature-workspaces/lock/owner.json", `${JSON.stringify({ pid: process.pid, hostname: hostname() })}\n`);
  await assert.rejects(
    createFeatureWorkspace({ root: liveFixture.studioRoot, base: liveFixture.base, game: "fixture-game", task: "T0001", name: "live-lock", lockTimeoutMs: 50 }),
    /registry is locked/i,
  );
});

test("concurrent creation serializes registry allocation", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const [one, two] = await Promise.all([
    createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "parallel-one" }),
    createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0002", name: "parallel-two" }),
  ]);
  assert.equal(one.state, "ready");
  assert.equal(two.state, "ready");
  assert.equal(new Set([one.ports.devapi, one.ports.web, two.ports.devapi, two.ports.web]).size, 4);
});

test("every persisted creation boundary can be recovered", async (t) => {
  for (const point of [
    "after-active-record", "after-workspace-directory", "after-manifest", "after-studio-worktree",
    "after-engine", "after-game-worktree", "before-ready",
  ]) {
    await t.test(point, async (subtest) => {
      const fixture = pairFixture();
      subtest.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
      await assert.rejects(
        createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "crash-test", crashAt: point }),
        /injected crash/i,
      );
      const recovered = await recoverFeatureWorkspace({ base: fixture.base, name: "crash-test" });
      assert.equal(recovered.state, "rolled-back");
      assert.equal((await listFeatureWorkspaces({ base: fixture.base })).length, 0);
      const branches = execFileSync("git", ["-C", fixture.gameRoot, "branch", "--list", "agent/t0001-crash-test"], { encoding: "utf8" });
      assert.equal(branches.trim(), "", "a rolled-back creation leaves no game branch behind");
    });
  }
});

test("list and check report a ready workspace", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "inspect-me" });
  const listed = await listFeatureWorkspaces({ base: fixture.base });
  assert.equal(listed.length, 1);
  assert.equal(listed[0].name, "inspect-me");
  assert.equal(listed[0].state, "ready");
  assert.equal(listed[0].sourceCommits.studio, created.sourceStudioCommit);
  assert.deepEqual(listed[0].registrations, { studio: true, game: true });

  const checked = await checkFeatureWorkspace({ base: fixture.base, name: "inspect-me" });
  assert.equal(checked.ok, true);
  assert.equal(checked.studio.commit, created.sourceStudioCommit);
  assert.equal(checked.game.branch, created.gameBranch);
  assert.equal(checked.engine.commit, fixture.engineCommit);

  write(created.gameWorktree, ".ai_studio/taskboard/items/active/T0001.md", taskDoc("T0001", "review"));
  git(created.gameWorktree, "add", ".");
  git(created.gameWorktree, "commit", "-m", "finish feature");
  const afterWork = await checkFeatureWorkspace({ base: fixture.base, name: "inspect-me" });
  assert.equal(afterWork.ok, true);
  assert.equal(afterWork.game.aheadOfSource, 1);
});

test("ports can be reallocated while a workspace is stopped", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "new-ports" });
  const changed = await reallocateWorkspacePorts({ base: fixture.base, name: "new-ports" });
  assert.notEqual(changed.ports.devapi, created.ports.devapi);
  assert.notEqual(changed.ports.web, created.ports.web);
});

test("ports can be reallocated after an external bind race", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "bind-race" });
  const server = createServer();
  await new Promise((resolveListen, rejectListen) => {
    server.once("error", rejectListen);
    server.listen(created.ports.devapi, "127.0.0.1", resolveListen);
  });
  const changed = await reallocateWorkspacePorts({ base: fixture.base, name: "bind-race" });
  assert.notEqual(changed.ports.devapi, created.ports.devapi);
  await new Promise((resolveClose) => server.close(resolveClose));
});

test("an interrupted port update keeps the active lease recoverable", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "port-crash" });
  await assert.rejects(
    reallocateWorkspacePorts({ base: fixture.base, name: "port-crash", failAt: "after-active-port-lease" }),
    /injected failure/i,
  );
  const interrupted = await checkFeatureWorkspace({ base: fixture.base, name: "port-crash" });
  assert.equal(interrupted.ok, false);
  assert.match(interrupted.problems.join("\n"), /ports differ/i);
  await reallocateWorkspacePorts({ base: fixture.base, name: "port-crash" });
  assert.equal((await checkFeatureWorkspace({ base: fixture.base, name: "port-crash" })).ok, true);
});

test("creation rejects an overlapping explicit port pair", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  await assert.rejects(
    createFeatureWorkspace({
      root: fixture.studioRoot,
      base: fixture.base,
      game: "fixture-game",
      task: "T0001",
      name: "same-ports",
      devapiPort: 18042,
      webPort: 18042,
    }),
    /ports must be different/i,
  );
});

test("removal refuses dirty work and preserves the game branch", async (t) => {
  const fixture = pairFixture({ withPython: true });
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "remove-me" });
  const dirtyPath = join(created.gameWorktree, "dirty.txt");
  writeFileSync(dirtyPath, "keep me\n", "utf8");
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "remove-me" }), /dirty/i);
  assert.equal(git(created.gameWorktree, "rev-parse", "HEAD"), created.sourceGameCommit);

  rmSync(dirtyPath);
  write(created.gameWorktree, "build/ignored.txt", "reproducible\n");
  write(created.studioWorktree, ".vite/cache.bin", "reproducible\n");
  const linkedTarget = join(fixture.fixtureRoot, "venv-link-target");
  mkdirSync(linkedTarget);
  write(linkedTarget, "keep.txt", "keep\n");
  symlinkSync(linkedTarget, join(created.studioWorktree, ".venv", "linked-tool"), process.platform === "win32" ? "junction" : "dir");
  const removed = await removeFeatureWorkspace({ base: fixture.base, name: "remove-me" });
  assert.equal(removed.state, "removed");
  assert.equal(readFileSync(join(linkedTarget, "keep.txt"), "utf8"), "keep\n");
  assert.equal(git(fixture.gameRoot, "show-ref", "--verify", `refs/heads/${created.gameBranch}`).length > 0, true);
  const checked = await checkFeatureWorkspace({ base: fixture.base, name: "remove-me" });
  assert.equal(checked.removed, true);
});

test("removal recovers a crash immediately before tombstone rename", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "tombstone-crash" });
  await assert.rejects(
    removeFeatureWorkspace({ base: fixture.base, name: "tombstone-crash", failAt: "before-tombstone-rename" }),
    /injected failure/i,
  );
  const activePath = join(fixture.base, ".feature-workspaces/active/tombstone-crash.json");
  assert.equal(JSON.parse(readFileSync(activePath, "utf8")).schema, "ai_studio.feature_workspace_record.v1");
  const recovered = await recoverFeatureWorkspace({ base: fixture.base, name: "tombstone-crash" });
  assert.equal(recovered.state, "removed");
  assert.equal((await checkFeatureWorkspace({ base: fixture.base, name: "tombstone-crash" })).removed, true);
});

test("removal refuses unknown ignored data", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  writeFileSync(join(fixture.gameRoot, ".gitignore"), "build/\nprivate-cache/\n", "utf8");
  git(fixture.gameRoot, "add", ".gitignore");
  git(fixture.gameRoot, "commit", "-m", "ignore private cache");
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "ignored-data" });
  write(created.gameWorktree, "private-cache/keep.txt", "do not delete\n");
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "ignored-data" }), /unknown ignored data/i);
  assert.equal(readFileSync(join(created.gameWorktree, "private-cache/keep.txt"), "utf8"), "do not delete\n");
});

function markRemovalInterrupted(base, name) {
  const activePath = join(base, ".feature-workspaces/active", `${name}.json`);
  const record = JSON.parse(readFileSync(activePath, "utf8"));
  record.state = "recovery-required";
  record.transactionMode = "remove";
  record.removalStartedAt = new Date().toISOString();
  writeFileSync(activePath, `${JSON.stringify(record, null, 2)}\n`, "utf8");
}

function commitInEngine(engineRoot, message) {
  write(engineRoot, `${message}.txt`, `${message}\n`);
  git(engineRoot, "add", `${message}.txt`);
  git(engineRoot, "-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid", "commit", "-m", message);
  return git(engineRoot, "rev-parse", "HEAD");
}

test("removal refuses engine commits that only the workspace clone holds", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "engine-work" });
  const engineRoot = join(created.studioWorktree, "external/neotolis-engine");
  commitInEngine(engineRoot, "workspace-engine-change");
  git(created.studioWorktree, "checkout", "--quiet", "-b", "local-gitlink-bump");
  git(created.studioWorktree, "add", "external/neotolis-engine");
  git(created.studioWorktree, "commit", "-m", "bump engine gitlink");
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "engine-work" }), /engine HEAD is not in the source engine/);

  git(engineRoot, "branch", "keep-engine-change");
  git(engineRoot, "checkout", "--quiet", "--detach", fixture.engineCommit);
  git(created.studioWorktree, "checkout", "--quiet", "--detach", created.sourceStudioCommit);
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "engine-work" }), /engine refs\/heads\/keep-engine-change/);
  assert.equal(existsSync(join(engineRoot, "workspace-engine-change.txt")), false);
  assert.equal(git(engineRoot, "cat-file", "-t", "keep-engine-change"), "commit");
});

test("removal never deletes source evidence reached through a junction", { skip: process.platform !== "win32" }, async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "evidence-link" });
  write(fixture.gameRoot, ".ai_studio/evidence/t0001/shot.png", "source evidence\n");
  mkdirSync(join(created.gameWorktree, ".ai_studio"), { recursive: true });
  symlinkSync(join(fixture.gameRoot, ".ai_studio/evidence"), join(created.gameWorktree, ".ai_studio/evidence"), "junction");
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "evidence-link" }));
  assert.equal(readFileSync(join(fixture.gameRoot, ".ai_studio/evidence/t0001/shot.png"), "utf8"), "source evidence\n");
});

test("CLI parses the registry-wide cleanup commands", () => {
  assert.deepEqual(parseCommandLine(["remove-merged", "--apply", "--json"]), { command: "remove-merged", options: { apply: true, json: true } });
  assert.deepEqual(parseCommandLine(["forget-missing"]), { command: "forget-missing", options: {} });
  assert.throws(() => parseCommandLine(["forget-missing", "some-name"]), /unexpected argument/);
  assert.throws(() => parseCommandLine(["remove", "some-name", "--apply"]), /unknown option/);
});

test("removal accepts a Studio worktree advanced to a shared commit and refuses a workspace-only commit", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const advanced = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "studio-advanced" });
  const local = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0002", name: "studio-local" });
  write(fixture.studioRoot, "studio-next.txt", "next\n");
  git(fixture.studioRoot, "add", "studio-next.txt");
  git(fixture.studioRoot, "commit", "-m", "advance studio");
  git(advanced.studioWorktree, "checkout", "--quiet", "--detach", "main");
  assert.equal((await removeFeatureWorkspace({ base: fixture.base, name: "studio-advanced" })).state, "removed");

  write(local.studioWorktree, "workspace-only.txt", "unpushed\n");
  git(local.studioWorktree, "add", "workspace-only.txt");
  git(local.studioWorktree, "commit", "-m", "workspace-only studio work");
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "studio-local" }), /not on any source Studio branch/);
  assert.equal(existsSync(join(local.studioWorktree, "workspace-only.txt")), true);
});

test("removal deletes generated ignored data and source-mirrored evidence only", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  write(fixture.gameRoot, "release/artifacts/README.md", "tracked beside generated output\n");
  git(fixture.gameRoot, "add", "-f", "release/artifacts/README.md");
  git(fixture.gameRoot, "commit", "-m", "track an artifacts note");
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "generated" });
  write(created.gameWorktree, "devapi/__pycache__/smoke_bot.cpython-312.pyc", "pyc\n");
  write(created.gameWorktree, "src/generated/game.h", "generated\n");
  write(created.gameWorktree, "release/artifacts/game.zip", "zip\n");
  write(created.studioWorktree, "features/ui-kit/example/native/build/ui.o", "object\n");
  write(created.studioWorktree, "ai_studio/feature_testing/consumer/src/generated/state.h", "generated\n");
  write(created.studioWorktree, "templates/template/release/artifacts/template.zip", "zip\n");
  write(created.gameWorktree, ".ai_studio/evidence/t0001/shot.png", "evidence\n");
  write(created.gameWorktree, "assets/restricted/pack/hero.glb", "glb\n");

  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "generated" }), /unknown ignored data.*\.ai_studio\/evidence/);
  write(fixture.gameRoot, ".ai_studio/evidence/t0001/shot.png", "different\n");
  write(fixture.gameRoot, "assets/restricted/pack/hero.glb", "glb\n");
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "generated" }), /unknown ignored data.*shot\.png/);
  assert.equal(readFileSync(join(created.gameWorktree, ".ai_studio/evidence/t0001/shot.png"), "utf8"), "evidence\n");

  write(fixture.gameRoot, ".ai_studio/evidence/t0001/shot.png", "evidence\n");
  assert.equal((await removeFeatureWorkspace({ base: fixture.base, name: "generated" })).state, "removed");
  assert.equal(existsSync(join(fixture.base, "generated")), false);
  assert.equal(readFileSync(join(fixture.gameRoot, ".ai_studio/evidence/t0001/shot.png"), "utf8"), "evidence\n");
  assert.equal(readFileSync(join(fixture.gameRoot, "assets/restricted/pack/hero.glb"), "utf8"), "glb\n");
});

test("removal accepts a detached game worktree only at a commit kept by a source branch", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "detached-game" });
  git(created.gameWorktree, "checkout", "--quiet", "--detach");
  write(created.gameWorktree, "detached.txt", "unkept\n");
  git(created.gameWorktree, "add", "detached.txt");
  git(created.gameWorktree, "commit", "-m", "detached work");
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "detached-game" }), /not on any source game branch/);

  git(created.gameWorktree, "checkout", "--quiet", "--detach", "main");
  const removed = await removeFeatureWorkspace({ base: fixture.base, name: "detached-game" });
  assert.equal(removed.state, "removed");
  assert.equal(git(fixture.gameRoot, "show-ref", "--verify", `refs/heads/${created.gameBranch}`).length > 0, true);
});

test("recovery finishes a removal that Git left half-deleted", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const game = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "half-game" });
  rmSync(join(game.gameWorktree, ".git"));
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "half-game" }), /workspace game worktree is not a Git worktree root/);
  assert.equal(existsSync(join(game.gameWorktree, "tracked.txt")), true);
  markRemovalInterrupted(fixture.base, "half-game");
  const activePath = join(fixture.base, ".feature-workspaces/active/half-game.json");
  const record = JSON.parse(readFileSync(activePath, "utf8"));
  delete record.removalStartedAt;
  writeFileSync(activePath, `${JSON.stringify(record, null, 2)}\n`, "utf8");
  await assert.rejects(recoverFeatureWorkspace({ base: fixture.base, name: "half-game" }), /no start time/);
  assert.equal(existsSync(join(game.gameWorktree, "tracked.txt")), true);
  const later = new Date(Date.now() + 60_000);
  write(game.gameWorktree, "written-after-start.txt", "new work\n");
  utimesSync(join(game.gameWorktree, "written-after-start.txt"), later, later);
  await assert.rejects(recoverFeatureWorkspace({ base: fixture.base, name: "half-game" }), /changed after removal started/);
  assert.equal(existsSync(join(game.gameWorktree, "written-after-start.txt")), true);
  rmSync(join(game.gameWorktree, "written-after-start.txt"));
  assert.equal((await recoverFeatureWorkspace({ base: fixture.base, name: "half-game" })).state, "removed");
  assert.doesNotMatch(git(fixture.gameRoot, "worktree", "list"), /half-game/);

  const studio = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0002", name: "half-studio" });
  git(fixture.gameRoot, "worktree", "remove", studio.gameWorktree);
  rmSync(join(studio.studioWorktree, ".git"));
  markRemovalInterrupted(fixture.base, "half-studio");
  assert.equal((await recoverFeatureWorkspace({ base: fixture.base, name: "half-studio" })).state, "removed");
  assert.equal(existsSync(join(fixture.base, "half-studio")), false);
  assert.doesNotMatch(git(fixture.studioRoot, "worktree", "list"), /half-studio/);
});

test("creation and removal handle paths longer than the Windows limit", { skip: process.platform !== "win32" }, async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const deep = Array.from({ length: 10 }, () => "long-path-segment-xx").join("/");
  write(fixture.gameRoot, `${deep}/tracked.txt`, "deep\n");
  git(fixture.gameRoot, "-c", "core.longpaths=true", "add", ".");
  git(fixture.gameRoot, "-c", "core.longpaths=true", "commit", "-m", "deep path");
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "long-paths" });
  assert.ok(join(created.gameWorktree, deep, "tracked.txt").length > 260);
  write(created.gameWorktree, `${deep}/build/out.o`, "object\n");
  assert.equal((await removeFeatureWorkspace({ base: fixture.base, name: "long-paths" })).state, "removed");
  assert.equal(existsSync(join(fixture.base, "long-paths")), false);
});

test("forget-missing tombstones entries without a folder and prunes stale worktree records", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "kept" });
  await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0002", name: "gone" });
  const engineWork = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: null, name: "gone-engine" });
  commitInEngine(join(engineWork.studioWorktree, "external/neotolis-engine"), "unpublished-engine-change");
  const unrelated = join(fixture.fixtureRoot, "unrelated-worktree");
  git(fixture.studioRoot, "worktree", "add", "--detach", unrelated);
  for (const folder of [join(fixture.base, "gone"), join(fixture.base, "gone-engine"), unrelated]) {
    rmSync(folder, { recursive: true, force: true });
  }

  const dryRun = await forgetMissingFeatureWorkspaces({ base: fixture.base });
  const byName = Object.fromEntries(dryRun.workspaces.map((row) => [row.name, row]));
  assert.deepEqual(Object.keys(byName).sort(), ["gone", "gone-engine"]);
  assert.equal(byName.gone.worktreeRecords.length, 2, JSON.stringify(byName.gone));
  assert.match(byName["gone-engine"].skipped, /engine HEAD is not in the source engine/);
  assert.equal(existsSync(join(fixture.base, ".feature-workspaces/active/gone.json")), true);

  await forgetMissingFeatureWorkspaces({ base: fixture.base, apply: true });
  assert.deepEqual((await listFeatureWorkspaces({ base: fixture.base })).map((row) => row.name).sort(), ["gone-engine", "kept"]);
  assert.equal((await checkFeatureWorkspace({ base: fixture.base, name: "gone" })).removed, true);
  const worktrees = git(fixture.studioRoot, "worktree", "list");
  assert.doesNotMatch(worktrees, /[\\/]gone[\\/]/);
  assert.match(worktrees, /gone-engine/);
  assert.match(worktrees, /unrelated-worktree/);
  assert.match(worktrees, /[\\/]kept[\\/]/);
  assert.equal((await checkFeatureWorkspace({ base: fixture.base, name: "kept" })).ok, true);
});

test("remove-merged removes clean merged or patch-equivalent workspaces after a dry run", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const merged = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "merged" });
  const picked = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0002", name: "picked" });
  const open = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: null, name: "open" });
  for (const [workspace, file] of [[merged, "merged.txt"], [picked, "picked.txt"], [open, "open.txt"]]) {
    write(workspace.gameWorktree, file, `${file}\n`);
    git(workspace.gameWorktree, "add", file);
    git(workspace.gameWorktree, "commit", "-m", file);
  }
  git(fixture.gameRoot, "merge", "--quiet", "--ff-only", merged.gameBranch);
  git(fixture.gameRoot, "cherry-pick", picked.gameBranch);

  const dryRun = await removeMergedFeatureWorkspaces({ base: fixture.base });
  const byName = Object.fromEntries(dryRun.workspaces.map((row) => [row.name, row]));
  assert.equal(byName.merged.merged, "ancestor");
  assert.equal(byName.picked.merged, "patch-equivalent");
  assert.equal(byName.open.eligible, false);
  assert.match(byName.open.reason, /not merged/);
  assert.equal((await listFeatureWorkspaces({ base: fixture.base })).length, 3);

  const applied = await removeMergedFeatureWorkspaces({ base: fixture.base, apply: true });
  assert.deepEqual(applied.workspaces.filter((row) => row.removed).map((row) => row.name).sort(), ["merged", "picked"]);
  assert.deepEqual((await listFeatureWorkspaces({ base: fixture.base })).map((row) => row.name), ["open"]);
  assert.equal(git(fixture.gameRoot, "show-ref", "--verify", `refs/heads/${picked.gameBranch}`).length > 0, true);
});

test("recovery refuses a tampered source identity and new dirty work", async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "recover-safe" });
  const activePath = join(fixture.base, ".feature-workspaces/active/recover-safe.json");
  const record = JSON.parse(readFileSync(activePath, "utf8"));
  record.state = "recovery-required";
  record.transactionMode = "remove";
  record.sourceStudioRoot = fixture.gameRoot;
  writeFileSync(activePath, `${JSON.stringify(record, null, 2)}\n`, "utf8");
  await assert.rejects(
    import("../feature_workspaces.mjs").then(({ recoverFeatureWorkspace }) => recoverFeatureWorkspace({ base: fixture.base, name: "recover-safe" })),
    /common Git directory|source Studio/i,
  );
  assert.equal(git(created.gameWorktree, "rev-parse", "HEAD"), created.sourceGameCommit);

  record.sourceStudioRoot = fixture.studioRoot;
  writeFileSync(activePath, `${JSON.stringify(record, null, 2)}\n`, "utf8");
  writeFileSync(join(created.gameWorktree, "new-work.txt"), "keep\n", "utf8");
  await assert.rejects(
    import("../feature_workspaces.mjs").then(({ recoverFeatureWorkspace }) => recoverFeatureWorkspace({ base: fixture.base, name: "recover-safe" })),
    /dirty/i,
  );
});

test("workspace base cannot be a junction", { skip: process.platform !== "win32" }, async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const physical = join(fixture.fixtureRoot, "physical-base");
  const junction = join(fixture.fixtureRoot, "junction-base");
  mkdirSync(physical);
  symlinkSync(physical, junction, "junction");
  await assert.rejects(
    createFeatureWorkspace({ root: fixture.studioRoot, base: junction, game: "fixture-game", task: "T0001", name: "junction" }),
    /physical directory|reparse/i,
  );
});

test("all lifecycle commands reject a junction base", { skip: process.platform !== "win32" }, async (t) => {
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "junction-commands" });
  const junction = join(fixture.fixtureRoot, "workspace-junction");
  symlinkSync(fixture.base, junction, "junction");
  for (const action of [
    () => listFeatureWorkspaces({ base: junction }),
    () => checkFeatureWorkspace({ base: junction, name: "junction-commands" }),
    () => reallocateWorkspacePorts({ base: junction, name: "junction-commands" }),
    () => removeFeatureWorkspace({ base: junction, name: "junction-commands" }),
    () => import("../feature_workspaces.mjs").then(({ recoverFeatureWorkspace }) => recoverFeatureWorkspace({ base: junction, name: "junction-commands" })),
    () => listFeatureWorkspaces({ base: join(junction, "missing-child") }),
  ]) {
    await assert.rejects(action(), /link or reparse point/i);
  }
  assert.equal(git(created.gameWorktree, "rev-parse", "HEAD"), created.sourceGameCommit);
});
