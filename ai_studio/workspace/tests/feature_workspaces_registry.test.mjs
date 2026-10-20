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
