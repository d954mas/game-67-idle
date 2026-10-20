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

test("removal refuses dirty work and preserves the game branch", async (t) => {
  const fixture = pairFixture({ withPython: true });
  t.after(() => rmSync(fixture.fixtureRoot, { recursive: true, force: true }));
  const created = await createFeatureWorkspace({ root: fixture.studioRoot, base: fixture.base, game: "fixture-game", task: "T0001", name: "remove-me", python: true });
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
