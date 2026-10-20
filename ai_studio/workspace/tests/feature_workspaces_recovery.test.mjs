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

test("creation defers the Studio Python environment until it is asked for", async (t) => {
  const fixture = pairFixture({ withPython: true });
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));

  const created = await createFeatureWorkspace({
    root: fixture.studioRoot,
    base: fixture.base,
    game: "fixture-game",
    task: "T0001",
    name: "python-deferred",
  });
  assert.equal(created.python.state, "deferred");
  assert.equal(existsSync(join(created.studioWorktree, ".venv")), false);
  const checked = await checkFeatureWorkspace({ base: fixture.base, name: "python-deferred" });
  assert.equal(checked.python.state, "deferred");
  assert.equal(checked.ok, true);
  assert.equal((await prepareFeatureWorkspacePython({ base: fixture.base, name: "python-deferred" })).python.state, "ready");
  assert.equal(parseCommandLine(["new", "--game", "g", "--task", "T0001", "--name", "n", "--python"]).options.python, true);
});
