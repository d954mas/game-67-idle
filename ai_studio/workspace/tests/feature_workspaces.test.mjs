import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { hostname, tmpdir } from "node:os";
import { createServer } from "node:net";

import {
  checkFeatureWorkspace,
  createFeatureWorkspace,
  defaultWorkspaceBase,
  inspectCommittedTask,
  inspectRepository,
  normalizeWorkspaceName,
  parseCommandLine,
  listFeatureWorkspaces,
  reallocateWorkspacePorts,
  recoverFeatureWorkspace,
  removeFeatureWorkspace,
} from "../feature_workspaces.mjs";

function git(root, ...args) {
  return execFileSync("git", args, { cwd: root, encoding: "utf8" }).trim();
}

function write(root, relativePath, contents) {
  const file = join(root, relativePath);
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, contents, "utf8");
}

function taskDoc(id, status = "backlog") {
  return `---\nid: ${id}\ntitle: Fixture task\nstatus: ${status}\nproject: P001\nepic: E001\npriority: P2\ncreated: 2026-01-01\nupdated: 2026-01-01\n---\n\n## What\n\nFixture.\n\n## Done when\n\n- [ ] Fixture is complete.\n\n## Log\n\n- 2026-01-01: Fixture created.\n`;
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

function pairFixture() {
  const fixtureRoot = mkdtempSync(join(tmpdir(), "feature-workspace-create-"));
  const engineRoot = join(fixtureRoot, "engine-source");
  mkdirSync(engineRoot);
  git(engineRoot, "init", "-b", "main");
  git(engineRoot, "config", "user.name", "Fixture");
  git(engineRoot, "config", "user.email", "fixture@example.invalid");
  write(engineRoot, "engine.txt", "engine\n");
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
  write(studioRoot, ".gitignore", "games/private/\n.vite/\n");
  write(studioRoot, ".gitmodules", '[submodule "external/neotolis-engine"]\n\tpath = external/neotolis-engine\n\turl = https://invalid.example/engine.git\n');
  write(studioRoot, "studio.txt", "studio\n");
  git(studioRoot, "add", ".");
  git(studioRoot, "update-index", "--add", "--cacheinfo", `160000,${engineCommit},external/neotolis-engine`);
  git(studioRoot, "commit", "-m", "studio");
  mkdirSync(join(studioRoot, "external"), { recursive: true });
  execFileSync("git", ["clone", "--quiet", engineRoot, join(studioRoot, "external/neotolis-engine")]);

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
  write(gameRoot, ".gitignore", "build/\n");
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
  const fixture = pairFixture();
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "remove-me" });
  const dirtyPath = join(created.gameWorktree, "dirty.txt");
  writeFileSync(dirtyPath, "keep me\n", "utf8");
  await assert.rejects(removeFeatureWorkspace({ base: fixture.base, name: "remove-me" }), /dirty/i);
  assert.equal(git(created.gameWorktree, "rev-parse", "HEAD"), created.sourceGameCommit);

  rmSync(dirtyPath);
  write(created.gameWorktree, "build/ignored.txt", "reproducible\n");
  write(created.studioWorktree, ".vite/cache.bin", "reproducible\n");
  const removed = await removeFeatureWorkspace({ base: fixture.base, name: "remove-me" });
  assert.equal(removed.state, "removed");
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
