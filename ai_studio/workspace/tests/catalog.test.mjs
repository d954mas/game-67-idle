import assert from "node:assert/strict";
import test from "node:test";
import { execFileSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import { listWorkspaceMounts, writeGameDependencies } from "../catalog.mjs";

function writeJson(root, rel, value) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function identity(root, rel, kind, id, title = id, storageNamespace = id, aliases = []) {
  writeJson(root, `${rel}/${kind}.json`, {
    schema: `ai_studio.${kind}.v1`, id, title, storageNamespace, ...(aliases.length ? { aliases } : {}),
  });
  if (kind === "game") {
    writeJson(root, `${rel}/dependencies.json`, {
      schema: "ai_studio.game.dependencies.v3",
      engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" },
      features: [], compatibility: "test",
    });
  }
}

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "workspace-scan-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "games", "private"), { recursive: true });
  mkdirSync(join(root, "templates"), { recursive: true });
  return root;
}

function privateWorktree(root, id = "secret-game", alias = "secret-game-worktree") {
  const canonical = join(root, "games", "private", id);
  identity(root, `games/private/${id}`, "game", id, "Secret", `${id}-store`, ["Private Slot"]);
  execFileSync("git", ["init"], { cwd: canonical, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "tests@example.invalid"], { cwd: canonical });
  execFileSync("git", ["config", "user.name", "Tests"], { cwd: canonical });
  execFileSync("git", ["add", "."], { cwd: canonical });
  execFileSync("git", ["commit", "-m", "private game"], { cwd: canonical, stdio: "ignore" });
  const worktree = join(root, "games", "private", alias);
  execFileSync("git", ["worktree", "add", "--detach", worktree], { cwd: canonical, stdio: "ignore" });
  return { canonical, worktree };
}

test("scanner derives public templates and games from folders and store folders", (t) => {
  const root = fixture(t);
  identity(root, "templates/template", "template", "template", "Template", "base-template");
  mkdirSync(join(root, "templates/template/assets"), { recursive: true });
  identity(root, "games/public-game", "game", "public-game", "Public Game", "public-store");
  mkdirSync(join(root, "games/public-game/.ai_studio/taskboard/items"), { recursive: true });

  const mounts = listWorkspaceMounts(root);
  assert.deepEqual(mounts.map(({ kind, id, storeId, visibility, enabledStores }) => ({
    kind, id, storeId, visibility, enabledStores,
  })), [
    { kind: "game", id: "public-game", storeId: "game:public-store", visibility: "public", enabledStores: ["taskboard"] },
    { kind: "template", id: "template", storeId: "template:base-template", visibility: "public", enabledStores: ["assets"] },
  ]);
});

test("store discovery requires directories, not same-named files", (t) => {
  const root = fixture(t);
  identity(root, "games/public-game", "game", "public-game");
  writeFileSync(join(root, "games/public-game/assets"), "not a directory\n", "utf8");
  assert.deepEqual(listWorkspaceMounts(root)[0].enabledStores, []);
});

test("private games are discovered under games/private only when selected", (t) => {
  const root = fixture(t);
  identity(root, "games/private/secret-game", "game", "secret-game", "Secret", "secret-game", ["Private Slot"]);
  mkdirSync(join(root, "games/private/secret-game/.git"), { recursive: true });
  mkdirSync(join(root, "games/private/secret-game/.ai_studio/evidence"), { recursive: true });

  assert.deepEqual(listWorkspaceMounts(root), []);
  const [mount] = listWorkspaceMounts(root, { activeGameId: "secret-game" });
  assert.equal(mount.root, "games/private/secret-game");
  assert.equal(mount.visibility, "private");
  assert.equal(mount.commitPolicy, "nested-private");
  assert.equal(mount.publicAlias, "Private Slot");
  assert.deepEqual(mount.enabledStores, ["evidence"]);
});

test("registered private worktrees extend one canonical mount's audit roots", (t) => {
  const root = fixture(t);
  const { worktree } = privateWorktree(root);

  const mounts = listWorkspaceMounts(root, { includePrivate: true });
  assert.equal(mounts.length, 1);
  assert.equal(mounts[0].root, "games/private/secret-game");
  assert.deepEqual(mounts[0].auditRoots, [
    "games/private/secret-game",
    "games/private/secret-game-worktree",
  ]);
  assert.equal(mounts[0].auditRoots.includes(worktree), false);
});

test("private worktree aliases may carry a different release version and web budget", (t) => {
  const root = fixture(t);
  const { worktree } = privateWorktree(root);
  const manifest = join(worktree, "game.json");
  const aliasIdentity = JSON.parse(readFileSync(manifest, "utf8"));
  aliasIdentity.version = "1.1.0";
  aliasIdentity.webDownloadBudgetBytes = 12345678;
  writeFileSync(manifest, `${JSON.stringify(aliasIdentity, null, 2)}\n`, "utf8");

  const [mount] = listWorkspaceMounts(root, { includePrivate: true });
  assert.equal(mount.id, "secret-game");
  assert.deepEqual(mount.auditRoots, [
    "games/private/secret-game",
    "games/private/secret-game-worktree",
  ]);
});

test("an unregistered private identity alias fails closed", (t) => {
  const root = fixture(t);
  const { canonical } = privateWorktree(root);
  identity(root, "games/private/forged-worktree", "game", "secret-game", "Secret", "secret-game-store", ["Private Slot"]);
  writeFileSync(
    join(root, "games/private/forged-worktree/.git"),
    `gitdir: ${join(canonical, ".git")}\n`,
    "utf8",
  );

  assert.throws(
    () => listWorkspaceMounts(root, { includePrivate: true }),
    /registered Git worktree/i,
  );
});

test("a private identity alias cannot borrow another repository's Git common directory", (t) => {
  const root = fixture(t);
  privateWorktree(root);
  identity(root, "games/private/external-worktree", "game", "secret-game", "Secret", "secret-game-store", ["Private Slot"]);
  const external = join(root, "external-repository");
  mkdirSync(external, { recursive: true });
  execFileSync("git", ["init"], { cwd: external, stdio: "ignore" });
  writeFileSync(join(root, "games/private/external-worktree/.git"), `gitdir: ${join(external, ".git")}\n`, "utf8");

  assert.throws(
    () => listWorkspaceMounts(root, { includePrivate: true }),
    /common directory differs from canonical/i,
  );
});

test("a private worktree alias with a changed identity fails closed", (t) => {
  const root = fixture(t);
  const { worktree } = privateWorktree(root);
  identity(root, "games/private/secret-game-worktree", "game", "secret-game", "Other", "secret-game-store", ["Private Slot"]);

  assert.throws(
    () => listWorkspaceMounts(root, { includePrivate: true }),
    /identity differs from canonical/i,
  );
  assert.equal(worktree.endsWith("secret-game-worktree"), true);
});

test("private worktree aliases reject symlinks and canonical case collisions", (t) => {
  const root = fixture(t);
  const { canonical } = privateWorktree(root);
  symlinkSync(canonical, join(root, "games/private/secret-game-link"), process.platform === "win32" ? "junction" : "dir");
  assert.throws(() => listWorkspaceMounts(root, { includePrivate: true }), /private.*symlink|junction/i);

  rmSync(join(root, "games/private/secret-game-link"), { recursive: true, force: true });
  const caseRoot = fixture(t);
  identity(caseRoot, "games/private/Secret-Game", "game", "secret-game", "Secret", "secret-game-store");
  assert.throws(() => listWorkspaceMounts(caseRoot, { includePrivate: true }), /case-collides canonical/i);
});

test("folder without an identity manifest is skipped with a warning", (t) => {
  const root = fixture(t);
  mkdirSync(join(root, "games/half-created"), { recursive: true });
  const warnings = [];
  assert.deepEqual(listWorkspaceMounts(root, { warnings }), []);
  assert.match(warnings[0], /half-created.*missing game\.json/i);
});

test("public symlink mounts are rejected", (t) => {
  const root = fixture(t);
  identity(root, "outside", "game", "linked-game");
  symlinkSync(join(root, "outside"), join(root, "games", "linked-game"), process.platform === "win32" ? "junction" : "dir");
  assert.throws(() => listWorkspaceMounts(root), /public.*symlink|junction/i);
});

test("public symlink mounts are rejected even before they have a manifest", (t) => {
  const root = fixture(t);
  mkdirSync(join(root, "outside-empty"), { recursive: true });
  symlinkSync(join(root, "outside-empty"), join(root, "games", "linked-empty"), process.platform === "win32" ? "junction" : "dir");
  assert.throws(() => listWorkspaceMounts(root), /public.*symlink|junction/i);
});

test("duplicate namespaces across public and private games fail when private games are included", (t) => {
  const root = fixture(t);
  identity(root, "games/public-game", "game", "public-game", "Public", "shared");
  identity(root, "games/private/private-game", "game", "private-game", "Private", "shared");
  assert.throws(() => listWorkspaceMounts(root, { includePrivate: true }), /duplicate storage namespace/i);
});

test("dependency writer validates before replacing an existing record", (t) => {
  const root = fixture(t);
  const valid = {
    engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "tested" },
    features: [], compatibility: "tested",
  };
  writeGameDependencies(root, "stable-game", valid);
  const path = join(root, "games", "stable-game", "dependencies.json");
  const before = readFileSync(path, "utf8");
  assert.throws(() => writeGameDependencies(root, "stable-game", {
    ...valid, engine: { ...valid.engine, revision: "placeholder" },
  }), /exact Git revision/);
  assert.equal(readFileSync(path, "utf8"), before);
});

test("dependency records require exact engine and feature SemVer", (t) => {
  const root = fixture(t);
  const engine = { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "tested" };
  assert.throws(() => writeGameDependencies(root, "stable-game", {
    engine: { ...engine, version: "0.1" }, features: [], compatibility: "tested",
  }), /engine\.version.*exact SemVer/i);
  assert.throws(() => writeGameDependencies(root, "stable-game", {
    engine, compatibility: "tested", features: [{
      id: "game-state", source: "features/game-state", compatibility: "tested",
    }],
  }), /version.*exact SemVer/i);
});

test("a malformed folder is skipped with a warning instead of failing the scan", (t) => {
  const root = fixture(t);
  identity(root, "games/good-game", "game", "good-game", "Good", "good-game");
  identity(root, "games/mismatched", "game", "other-id", "Other", "other-id");
  const warnings = [];
  const mounts = listWorkspaceMounts(root, { warnings });
  assert.deepEqual(mounts.map((mount) => mount.id), ["good-game"]);
  assert.match(warnings[0], /mismatched.*basename must match/i);
});

test("a private identity alias without worktree registration cannot hide beside its canonical game", (t) => {
  const root = fixture(t);
  identity(root, "games/private/wanted-game", "game", "wanted-game", "Wanted", "wanted-game");
  identity(root, "games/private/wanted-game-perf", "game", "wanted-game", "Perf copy", "wanted-game-perf");
  assert.throws(
    () => listWorkspaceMounts(root, { activeGameId: "wanted-game", warnings: [] }),
    /identity differs from canonical|linked Git worktree metadata|registered Git worktree/i,
  );
});

test("asking for a broken folder by name still fails loudly", (t) => {
  const root = fixture(t);
  identity(root, "games/private/broken-game", "game", "different-id", "Broken", "different-id");
  assert.throws(
    () => listWorkspaceMounts(root, { activeGameId: "broken-game", warnings: [] }),
    /no canonical game/i,
  );
});

test("callers can read back exactly which folders were unusable", (t) => {
  const root = fixture(t);
  identity(root, "games/mismatched", "game", "other-id", "Other", "other-id");
  const unusable = [];
  listWorkspaceMounts(root, { warnings: [], unusable });
  assert.deepEqual(unusable.map((entry) => entry.root), ["games/mismatched"]);
});
