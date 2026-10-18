import assert from "node:assert/strict";
import test from "node:test";
import { execFileSync, spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";

import {
  SHARED_BINARIES_MANIFEST,
  auditPrivateGamePreflight,
  listGameMounts,
  readSharedBinaryWaivers,
  runPrivateGamePreflight,
} from "../games.mjs";

function writeJson(root, rel, value) {
  const path = join(root, rel);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function game(root, rel, id, title = id, nestedGit = false) {
  writeJson(root, `${rel}/game.json`, {
    schema: "ai_studio.game.v1", id, title, storageNamespace: id,
  });
  writeJson(root, `${rel}/dependencies.json`, {
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" },
    features: [], compatibility: "test",
  });
  if (nestedGit) execFileSync("git", ["init"], { cwd: join(root, rel), stdio: "ignore" });
}

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "private-games-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "games", "private"), { recursive: true });
  mkdirSync(join(root, "templates"), { recursive: true });
  return root;
}

function registeredPrivateWorktree(root, id = "secret-game", alias = "secret-game-worktree") {
  const canonical = `games/private/${id}`;
  game(root, canonical, id, "Secret Title", true);
  const canonicalRoot = join(root, canonical);
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: canonicalRoot });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: canonicalRoot });
  execFileSync("git", ["add", "."], { cwd: canonicalRoot });
  execFileSync("git", ["commit", "-m", "private fixture"], { cwd: canonicalRoot, stdio: "ignore" });
  const worktree = join(root, "games", "private", alias);
  execFileSync("git", ["worktree", "add", "--detach", worktree], { cwd: canonicalRoot, stdio: "ignore" });
  return { canonical: canonicalRoot, worktree, alias };
}

test("listGameMounts keeps private games hidden unless explicitly selected", (t) => {
  const root = fixture(t);
  game(root, "games/public-game", "public-game", "Public");
  game(root, "games/private/secret-game", "secret-game", "Secret", true);
  assert.deepEqual(listGameMounts(root).map((mount) => mount.id), ["public-game"]);
  assert.deepEqual(
    listGameMounts(root, { includePrivate: true, skipPreflight: true }).map((mount) => `${mount.visibility}:${mount.id}`),
    ["public:public-game", "private:secret-game"],
  );
});

test("privacy preflight requires nested git metadata and blocks tracked private tokens", (t) => {
  const root = fixture(t);
  game(root, "games/private/secret-game", "secret-game", "Secret Title");
  const [mount] = listGameMounts(root, { includePrivate: true, skipPreflight: true });
  const result = auditPrivateGamePreflight([mount], {
    nestedGitRoots: [],
    trackedTextFiles: [{ path: "README.md", text: "Secret Title" }],
  });
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /nested git metadata/);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /private token/);
});

test("privacy preflight preserves nested Git setup errors instead of reporting missing metadata", (t) => {
  const root = fixture(t);
  game(root, "games/private/secret-game", "secret-game");
  const [mount] = listGameMounts(root, { includePrivate: true, skipPreflight: true });
  const result = auditPrivateGamePreflight([mount], {
    nestedGitRoots: [],
    nestedGitErrors: [{
      gitRoot: mount.gitRoot,
      reason: "fatal: detected dubious ownership",
    }],
  });
  assert.equal(result.ok, false);
  assert.match(result.violations[0].reason, /nested git validation failed.*dubious ownership/i);
  assert.doesNotMatch(result.violations.map((item) => item.reason).join("\n"), /missing nested git metadata/i);
});

test("privacy preflight rejects fake nested git metadata", (t) => {
  const root = fixture(t);
  game(root, "games/private/secret-game", "secret-game");
  mkdirSync(join(root, "games/private/secret-game/.git"), { recursive: true });
  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /nested git metadata/);
});

test("runPrivateGamePreflight scans tracked text and otherwise accepts an ignored nested repo", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });

  assert.equal(runPrivateGamePreflight(root).ok, true);
  writeFileSync(join(root, "README.md"), "Secret Title\n", "utf8");
  const leaked = runPrivateGamePreflight(root);
  assert.equal(leaked.ok, false);
  assert.match(leaked.violations[0].reason, /private token/);
});

test("privacy preflight audits registered private worktree paths and untracked binary contents", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  const { worktree, alias } = registeredPrivateWorktree(root);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });

  const secret = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x42]);
  mkdirSync(join(worktree, "assets"), { recursive: true });
  writeFileSync(join(worktree, "assets", "alias-secret.png"), secret);
  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "leaked.png"), secret);
  writeFileSync(join(root, "README.md"), `games/private/${alias}\n`, "utf8");
  execFileSync("git", ["add", "README.md", "assets/leaked.png"], { cwd: root });

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /private token.*secret-game/i);
  assert.equal(result.violations.some((item) => item.path === "assets/leaked.png"), true);
});

test("runPrivateGamePreflight confines every Git call with the exact repository safe.directory", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });

  const calls = [];
  const inheritedReplacement = process.env.GIT_NO_REPLACE_OBJECTS;
  process.env.GIT_NO_REPLACE_OBJECTS = "0";
  let result;
  try {
    result = runPrivateGamePreflight(root, {
      spawnGit(command, args, options) {
        calls.push({ command, args, cwd: options.cwd, env: options.env });
        return spawnSync(command, args, options);
      },
    });
  } finally {
    if (inheritedReplacement === undefined) delete process.env.GIT_NO_REPLACE_OBJECTS;
    else process.env.GIT_NO_REPLACE_OBJECTS = inheritedReplacement;
  }
  assert.equal(result.ok, true);
  assert.ok(calls.length > 0);
  for (const call of calls) {
    assert.equal(call.command, "git");
    assert.deepEqual(call.args.slice(0, 3), [
      "--no-replace-objects",
      "-c",
      `safe.directory=${resolve(call.cwd).replaceAll("\\", "/")}`,
    ]);
    assert.equal(call.env.GIT_NO_REPLACE_OBJECTS, undefined);
  }
});

test("privacy preflight batches private index blobs without skipping hidden binary text files", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });
  const privateRoot = join(root, "games/private/secret-game");
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: privateRoot });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: privateRoot });
  mkdirSync(join(privateRoot, "notes"), { recursive: true });
  for (let index = 0; index < 24; index += 1) {
    writeFileSync(join(privateRoot, "notes", `plain ${index}.txt`), `note ${index}\n`, "utf8");
  }
  writeFileSync(join(privateRoot, "notes", "space name-кириллица.txt"), "plain text\n", "utf8");
  const secret = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x42]);
  const privatePath = join(privateRoot, "assets", "секрет name.txt");
  mkdirSync(dirname(privatePath), { recursive: true });
  writeFileSync(privatePath, secret);
  execFileSync("git", ["add", "."], { cwd: privateRoot });
  execFileSync("git", ["commit", "-m", "private sources"], { cwd: privateRoot, stdio: "ignore" });

  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "leaked.txt"), secret);
  execFileSync("git", ["add", "assets/leaked.txt"], { cwd: root });

  const calls = [];
  const result = runPrivateGamePreflight(root, {
    spawnGit(command, args, options) {
      calls.push({ args, cwd: resolve(options.cwd) });
      return spawnSync(command, args, options);
    },
  });
  assert.equal(result.ok, false);
  assert.equal(result.violations.some((item) => item.path === "assets/leaked.txt"), true);
  const privateCalls = calls.filter((call) => call.cwd === resolve(privateRoot));
  assert.equal(privateCalls.filter((call) => call.args.includes("show")).length, 0);
  assert.equal(privateCalls.filter((call) => call.args.includes("cat-file")).length, 2);
  assert.equal(privateCalls.some((call) => call.args.includes("--batch-check")), true);
  assert.equal(privateCalls.some((call) => call.args.includes("--batch")), true);
});

test("privacy preflight blocks a staged parent binary copied byte-identically from a private game", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });

  const privateRoot = join(root, "games/private/secret-game");
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: privateRoot });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: privateRoot });
  const secretBytes = Buffer.from([0x01, 0x02, 0x03, 0x04]);
  const privateAsset = join(privateRoot, "assets", "secret.dat");
  mkdirSync(dirname(privateAsset), { recursive: true });
  writeFileSync(privateAsset, secretBytes);
  execFileSync("git", ["add", "assets/secret.dat"], { cwd: privateRoot });
  execFileSync("git", ["commit", "-m", "private asset"], { cwd: privateRoot, stdio: "ignore" });
  assert.equal(runPrivateGamePreflight(root).ok, true);

  const leakedAsset = join(root, "assets", "leaked.dat");
  mkdirSync(dirname(leakedAsset), { recursive: true });
  writeFileSync(leakedAsset, secretBytes);
  execFileSync("git", ["add", "assets/leaked.dat"], { cwd: root });
  const leaked = runPrivateGamePreflight(root);
  assert.equal(leaked.ok, false);
  assert.match(leaked.violations.map((item) => item.reason).join("\n"), /byte-identical.*private binary/i);
  assert.equal(leaked.violations.some((item) => item.path === "assets/leaked.dat"), true);
});

test("privacy preflight ignores Git replacement refs when reading private staged blobs", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });

  const privateRoot = join(root, "games/private/secret-game");
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: privateRoot });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: privateRoot });
  const secret = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x42]);
  const privateAsset = join(privateRoot, "assets", "secret.dat");
  mkdirSync(dirname(privateAsset), { recursive: true });
  writeFileSync(privateAsset, secret);
  execFileSync("git", ["add", "assets/secret.dat"], { cwd: privateRoot });
  execFileSync("git", ["commit", "-m", "private binary"], { cwd: privateRoot, stdio: "ignore" });
  const original = execFileSync("git", ["rev-parse", ":assets/secret.dat"], { cwd: privateRoot, encoding: "utf8" }).trim();
  const replacement = execFileSync("git", ["hash-object", "-w", "--stdin"], {
    cwd: privateRoot,
    encoding: "utf8",
    input: "benign text\n",
  }).trim();
  execFileSync("git", ["replace", original, replacement], { cwd: privateRoot, stdio: "ignore" });
  rmSync(privateAsset);

  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "leaked.dat"), secret);
  execFileSync("git", ["add", "assets/leaked.dat"], { cwd: root });

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.equal(result.violations.some((item) => item.path === "assets/leaked.dat"), true);
});

test("privacy preflight rejects a private junction before reading outside the game root", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });

  const secret = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x42]);
  const privateRoot = join(root, "games/private/secret-game");
  const nested = join(privateRoot, "nested");
  const replacedPath = join(nested, "original.txt");
  mkdirSync(nested, { recursive: true });
  writeFileSync(replacedPath, "benign text\n", "utf8");
  execFileSync("git", ["add", "nested/original.txt"], { cwd: privateRoot });
  rmSync(replacedPath, { force: true });
  const outside = join(root, "outside-content");
  mkdirSync(outside, { recursive: true });
  try {
    symlinkSync(outside, replacedPath, process.platform === "win32" ? "junction" : "dir");
  } catch (error) {
    t.skip(`directory symlink unavailable: ${error.code || error.message}`);
    return;
  }
  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "leaked.dat"), secret);
  execFileSync("git", ["add", "assets/leaked.dat"], { cwd: root });

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /not a directory|resolves outside|symlink|reparse/i);
  assert.equal(result.violations.some((item) => item.path === "assets/leaked.dat"), false);
});

test("privacy preflight accepts one trailing slash for an ignored nested Git directory", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });
  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "trigger.dat"), Buffer.from([0x00, 0x42]));
  execFileSync("git", ["add", "assets/trigger.dat"], { cwd: root });

  const privateRoot = join(root, "games/private/secret-game");
  writeFileSync(join(privateRoot, ".gitignore"), "tmp/nested/\n", "utf8");
  const nested = join(privateRoot, "tmp", "nested");
  mkdirSync(nested, { recursive: true });
  execFileSync("git", ["init"], { cwd: nested, stdio: "ignore" });
  const listed = execFileSync("git", ["ls-files", "-z", "--others", "--ignored", "--exclude-standard", "--", "."], {
    cwd: privateRoot,
  });
  assert.equal(listed.includes(Buffer.from("tmp/nested/\0", "utf8")), true);

  assert.equal(runPrivateGamePreflight(root).ok, true);
});

test("privacy preflight also compares untracked and ignored private binaries", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });

  const privateRoot = join(root, "games/private/secret-game");
  writeFileSync(join(privateRoot, ".gitignore"), "assets/ignored/\n", "utf8");
  const untrackedBytes = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x11]);
  const ignoredBytes = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x22]);
  const evidenceBytes = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x33]);
  writeFileSync(join(privateRoot, "untracked.png"), untrackedBytes);
  mkdirSync(join(privateRoot, "assets", "ignored"), { recursive: true });
  writeFileSync(join(privateRoot, "assets", "ignored", "secret.png"), ignoredBytes);
  mkdirSync(join(privateRoot, ".ai_studio", "evidence"), { recursive: true });
  writeFileSync(join(privateRoot, ".ai_studio", "evidence", "private-frame.png"), evidenceBytes);

  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "leaked-untracked.png"), untrackedBytes);
  writeFileSync(join(root, "assets", "leaked-ignored.png"), ignoredBytes);
  writeFileSync(join(root, "assets", "leaked-evidence.png"), evidenceBytes);
  execFileSync("git", [
    "add",
    "assets/leaked-untracked.png",
    "assets/leaked-ignored.png",
    "assets/leaked-evidence.png",
  ], { cwd: root });

  const leaked = runPrivateGamePreflight(root);
  assert.equal(leaked.ok, false);
  const leakedPaths = new Set(leaked.violations.map((item) => item.path));
  assert.equal(leakedPaths.has("assets/leaked-untracked.png"), true);
  assert.equal(leakedPaths.has("assets/leaked-ignored.png"), true);
  assert.equal(leakedPaths.has("assets/leaked-evidence.png"), true);
});

test("privacy preflight compares registered external private Canvas bytes without a mounted junction", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });

  const secret = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x44]);
  const projectsRoot = join(root, "external-private-canvas");
  mkdirSync(join(projectsRoot, "project-a", "files"), { recursive: true });
  writeFileSync(join(projectsRoot, "project-a", "files", "secret.png"), secret, { encoding: "binary", flag: "w" });
  writeJson(root, "ai_studio/studio.config.local.json", {
    canvasPrivateGameProjectsRoots: { "secret-game": projectsRoot },
  });
  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "leaked.png"), secret);
  execFileSync("git", ["add", "assets/leaked.png"], { cwd: root });

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.equal(result.violations.some((item) => item.path === "assets/leaked.png"), true);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /canvas\/projects\/project-a\/files\/secret\.png/i);
});

test("privacy preflight accepts a registered private Canvas junction", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });

  const secret = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x55]);
  const projectsRoot = join(root, "registered-private-canvas");
  mkdirSync(join(projectsRoot, "project-a", "files"), { recursive: true });
  writeFileSync(join(projectsRoot, "project-a", "files", "secret.png"), secret);
  const privateRoot = join(root, "games", "private", "secret-game");
  mkdirSync(join(privateRoot, ".ai_studio", "canvas"), { recursive: true });
  try {
    symlinkSync(projectsRoot, join(privateRoot, ".ai_studio", "canvas", "projects"), process.platform === "win32" ? "junction" : "dir");
  } catch (error) {
    t.skip(`directory symlink unavailable: ${error.code || error.message}`);
    return;
  }
  writeJson(root, "ai_studio/studio.config.local.json", {
    canvasPrivateGameProjectsRoots: { "secret-game": projectsRoot },
  });
  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "leaked.png"), secret);
  execFileSync("git", ["add", "assets/leaked.png"], { cwd: root });

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.equal(result.violations.some((item) => item.path === "assets/leaked.png"), true);
  assert.equal(result.violations.some((item) => /Canvas projects mount/.test(item.reason)), false);
});

test("scoped privacy preflight accepts registrations owned by another private game", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/game-a", "game-a", "Game A", true);
  game(root, "games/private/game-b", "game-b", "Game B", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });

  const projectsA = join(root, "external-private-canvas-a");
  const projectsB = join(root, "external-private-canvas-b");
  mkdirSync(projectsA, { recursive: true });
  mkdirSync(projectsB, { recursive: true });
  writeJson(root, "ai_studio/studio.config.local.json", {
    canvasPrivateGameProjectsRoots: { "game-a": projectsA, "game-b": projectsB },
  });

  const result = runPrivateGamePreflight(root, { activeGameId: "game-a" });
  assert.equal(result.ok, true);
  assert.equal(result.violations.some((item) => item.path === "<canvas-config>"), false);
});

test("privacy preflight rejects an unregistered private Canvas junction", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });

  const privateRoot = join(root, "games", "private", "secret-game");
  const outside = join(root, "unregistered-private-canvas");
  mkdirSync(outside, { recursive: true });
  mkdirSync(join(privateRoot, ".ai_studio", "canvas"), { recursive: true });
  try {
    symlinkSync(outside, join(privateRoot, ".ai_studio", "canvas", "projects"), process.platform === "win32" ? "junction" : "dir");
  } catch (error) {
    t.skip(`directory symlink unavailable: ${error.code || error.message}`);
    return;
  }
  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "trigger.png"), Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00]));
  execFileSync("git", ["add", "assets/trigger.png"], { cwd: root });

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /not a directory|resolves outside|symlink|reparse/i);
});

test("privacy preflight rejects a registered Canvas root with a nested reparse escape", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });

  const projectsRoot = join(root, "registered-private-canvas");
  const outside = join(root, "outside-content");
  mkdirSync(projectsRoot, { recursive: true });
  mkdirSync(outside, { recursive: true });
  try {
    symlinkSync(outside, join(projectsRoot, "escape"), process.platform === "win32" ? "junction" : "dir");
  } catch (error) {
    t.skip(`directory symlink unavailable: ${error.code || error.message}`);
    return;
  }
  writeJson(root, "ai_studio/studio.config.local.json", {
    canvasPrivateGameProjectsRoots: { "secret-game": projectsRoot },
  });
  mkdirSync(join(root, "assets"), { recursive: true });
  writeFileSync(join(root, "assets", "trigger.png"), Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00]));
  execFileSync("git", ["add", "assets/trigger.png"], { cwd: root });

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /Canvas projects root contains a symlink or reparse point/i);
});

test("privacy preflight compares only the changed Git view of a parent binary", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  const sharedBytes = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x73, 0x68, 0x61, 0x72, 0x65, 0x64]);
  const publicAsset = join(root, "assets", "shared.png");
  mkdirSync(dirname(publicAsset), { recursive: true });
  writeFileSync(publicAsset, sharedBytes);
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md", "assets/shared.png"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });

  const privateRoot = join(root, "games/private/secret-game");
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: privateRoot });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: privateRoot });
  const privateAsset = join(privateRoot, "assets", "shared.png");
  mkdirSync(dirname(privateAsset), { recursive: true });
  writeFileSync(privateAsset, sharedBytes);
  execFileSync("git", ["add", "assets/shared.png"], { cwd: privateRoot });
  execFileSync("git", ["commit", "-m", "shared private asset"], { cwd: privateRoot, stdio: "ignore" });

  writeFileSync(publicAsset, Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x00, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x63]));
  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, true);
});

test("privacy preflight ignores a moved submodule pointer", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);

  // A gitlink is not a file: neither the worktree nor the index holds bytes to
  // read at that path, so a scanner that treats it as one can never let a
  // dependency pin move.
  const dependency = join(root, "external", "dep");
  mkdirSync(dependency, { recursive: true });
  execFileSync("git", ["init"], { cwd: dependency, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: dependency });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: dependency });
  writeFileSync(join(dependency, "engine.txt"), "first\n", "utf8");
  execFileSync("git", ["add", "engine.txt"], { cwd: dependency });
  execFileSync("git", ["commit", "-m", "pinned"], { cwd: dependency, stdio: "ignore" });

  execFileSync("git", ["add", ".gitignore", "README.md", "external/dep"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });

  writeFileSync(join(dependency, "engine.txt"), "second\n", "utf8");
  execFileSync("git", ["commit", "-am", "moved"], { cwd: dependency, stdio: "ignore" });
  assert.equal(runPrivateGamePreflight(root).ok, true, "unstaged pointer move stays committable");

  execFileSync("git", ["add", "external/dep"], { cwd: root });
  assert.equal(runPrivateGamePreflight(root).ok, true, "staged pointer move stays committable");
});

test("preflight CLI reports a tracked token leak", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "secret-game\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  const script = join(import.meta.dirname, "..", "games.mjs");
  const result = spawnSync(process.execPath, [script, "preflight", "--root", root, "--json"], { encoding: "utf8" });
  assert.equal(result.status, 1);
  assert.equal(JSON.parse(result.stdout).ok, false);
});

// A shared-binary waiver is the ONE way a byte-identical pair is allowed
// through, so the tests below are the ones that matter most: an exception list
// nobody can grow silently is safe, and one that can be is worse than no gate.

function waiverDoc(entries) {
  return { schema: "ai_studio.workspace.shared_private_binaries.v1", entries };
}

function sharedFixture(t) {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "parent fixture"], { cwd: root, stdio: "ignore" });

  const privateRoot = join(root, "games/private/secret-game");
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: privateRoot });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: privateRoot });
  const bytes = Buffer.from([0x09, 0x08, 0x07, 0x06]);
  const digest = createHash("sha256").update(bytes).digest("hex");
  const privateAsset = join(privateRoot, "assets", "kit.dat");
  mkdirSync(dirname(privateAsset), { recursive: true });
  writeFileSync(privateAsset, bytes);
  execFileSync("git", ["add", "assets/kit.dat"], { cwd: privateRoot });
  execFileSync("git", ["commit", "-m", "private asset"], { cwd: privateRoot, stdio: "ignore" });

  const shared = join(root, "assets", "kit.dat");
  mkdirSync(dirname(shared), { recursive: true });
  writeFileSync(shared, bytes);
  execFileSync("git", ["add", "assets/kit.dat"], { cwd: root });

  const entry = {
    path: "assets/kit.dat",
    game_id: "secret-game",
    private_path: "assets/kit.dat",
    sha256: digest,
    reason: "shared default theme drawn from one token sheet",
    approved_by: "lead",
    approved_on: "2026-09-04",
  };
  return { root, entry, bytes, digest };
}

test("a shared-binary waiver clears exactly the pair it names", (t) => {
  const { root, entry } = sharedFixture(t);
  assert.equal(runPrivateGamePreflight(root).ok, false);
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([entry]));
  execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });
  assert.equal(runPrivateGamePreflight(root).ok, true);
});

test("a committed shared-binary waiver remains valid in a clean parent repo", (t) => {
  const { root, entry } = sharedFixture(t);
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([entry]));
  execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });
  execFileSync("git", ["commit", "-m", "shared asset"], { cwd: root, stdio: "ignore" });

  assert.equal(runPrivateGamePreflight(root).ok, true);
});

test("a committed waiver validates its ignored private worktree file on a text-only change", (t) => {
  const { root, entry, bytes } = sharedFixture(t);
  const privateRoot = join(root, "games/private/secret-game");
  const privatePath = ".ai_studio/evidence/audio-selection/ui-1.wav";
  const evidence = join(privateRoot, ...privatePath.split("/"));
  writeFileSync(join(privateRoot, ".gitignore"), ".ai_studio/evidence/\n", "utf8");
  execFileSync("git", ["add", ".gitignore"], { cwd: privateRoot });
  execFileSync("git", ["commit", "-m", "ignore evidence"], { cwd: privateRoot, stdio: "ignore" });
  mkdirSync(dirname(evidence), { recursive: true });
  writeFileSync(evidence, bytes);
  const ignored = { ...entry, private_path: privatePath };
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([ignored]));
  execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });
  execFileSync("git", ["commit", "-m", "approve shared asset"], { cwd: root, stdio: "ignore" });

  writeFileSync(join(root, "README.md"), "public studio update\n", "utf8");
  execFileSync("git", ["add", "README.md"], { cwd: root });
  assert.equal(runPrivateGamePreflight(root).ok, true);

  writeFileSync(evidence, Buffer.from([0x01, 0x02, 0x03, 0x04]));
  let result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /waiver matches no tracked binary/);

  rmSync(evidence);
  result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /waiver matches no tracked binary/);
});

test("a committed waiver becomes stale when its tracked public bytes change", (t) => {
  const { root, entry } = sharedFixture(t);
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([entry]));
  execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });
  execFileSync("git", ["commit", "-m", "shared asset"], { cwd: root, stdio: "ignore" });

  writeFileSync(join(root, "assets", "kit.dat"), Buffer.from([0x01, 0x02, 0x03, 0x04]));
  execFileSync("git", ["add", "assets/kit.dat"], { cwd: root });
  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /waiver matches no tracked binary/);
});

test("a scoped preflight ignores waivers for private games outside its workspace", (t) => {
  const { root, entry } = sharedFixture(t);
  game(root, "games/private/other-game", "other-game", "Other Title", true);
  const other = {
    ...entry,
    game_id: "other-game",
    private_path: "assets/missing.dat",
  };
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([entry, other]));
  execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });

  const scoped = runPrivateGamePreflight(root, { activeGameId: "secret-game" });
  assert.equal(scoped.ok, true);

  const full = runPrivateGamePreflight(root);
  assert.equal(full.ok, false);
  assert.match(full.violations.map((item) => item.reason).join("\n"), /waiver matches no tracked binary/);
});

test("a scoped preflight rejects a missing active private game", (t) => {
  const { root } = sharedFixture(t);
  const result = runPrivateGamePreflight(root, { activeGameId: "missing-game" });

  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /active private game.*not found/i);
});

test("an untracked approval manifest cannot waive a staged shared binary", (t) => {
  const { root, entry } = sharedFixture(t);
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([entry]));

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /approval manifest must be tracked/i);
});

test("an untracked manifest cannot preserve a waiver staged for deletion", (t) => {
  const { root, entry } = sharedFixture(t);
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([entry]));
  execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });
  execFileSync("git", ["commit", "-m", "approve shared asset", "--", "assets/kit.dat", SHARED_BINARIES_MANIFEST], {
    cwd: root,
    stdio: "ignore",
  });
  execFileSync("git", ["rm", "--cached", SHARED_BINARIES_MANIFEST], { cwd: root, stdio: "ignore" });

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /approval manifest.*staged for deletion/i);
});

test("a deleted approval manifest cannot hide a clean committed shared pair", (t) => {
  const { root, entry } = sharedFixture(t);
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([entry]));
  execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });
  execFileSync("git", ["commit", "-m", "approve shared asset", "--", "assets/kit.dat", SHARED_BINARIES_MANIFEST], {
    cwd: root,
    stdio: "ignore",
  });
  execFileSync("git", ["rm", SHARED_BINARIES_MANIFEST], { cwd: root, stdio: "ignore" });

  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /approval manifest.*staged for deletion/i);
});

test("a waiver stops applying when the bytes it names change", (t) => {
  const { root, entry } = sharedFixture(t);
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([entry]));
  execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });
  assert.equal(runPrivateGamePreflight(root).ok, true);

  // Different content behind the same two paths must NOT inherit the approval.
  const swapped = Buffer.from([0x11, 0x22, 0x33, 0x44]);
  writeFileSync(join(root, "assets", "kit.dat"), swapped);
  writeFileSync(join(root, "games/private/secret-game/assets/kit.dat"), swapped);
  execFileSync("git", ["add", "assets/kit.dat"], { cwd: root });
  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /byte-identical.*private binary/i);
});

test("a waiver that matches nothing is reported as stale", (t) => {
  const { root, entry } = sharedFixture(t);
  const stale = { ...entry, path: "assets/gone.dat", private_path: "assets/gone.dat" };
  writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([entry, stale]));
  execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });
  const result = runPrivateGamePreflight(root);
  assert.equal(result.ok, false);
  assert.match(result.violations.map((item) => item.reason).join("\n"), /waiver matches no tracked binary/);
});

test("an incomplete waiver is rejected instead of silently widening the gate", (t) => {
  const { root, entry } = sharedFixture(t);
  for (const field of ["reason", "approved_by", "approved_on"]) {
    const partial = { ...entry };
    delete partial[field];
    writeJson(root, SHARED_BINARIES_MANIFEST, waiverDoc([partial]));
    execFileSync("git", ["add", SHARED_BINARIES_MANIFEST], { cwd: root });
    const result = runPrivateGamePreflight(root);
    assert.equal(result.ok, false, `missing ${field} was accepted`);
    assert.match(result.violations.map((item) => item.reason).join("\n"), new RegExp(`missing ${field}`));
  }
});

test("readSharedBinaryWaivers rejects a malformed manifest rather than ignoring it", (t) => {
  const root = fixture(t);
  mkdirSync(dirname(join(root, SHARED_BINARIES_MANIFEST)), { recursive: true });
  writeFileSync(join(root, SHARED_BINARIES_MANIFEST), "{", "utf8");
  const broken = readSharedBinaryWaivers(root);
  assert.equal(broken.waivers.length, 0);
  assert.match(broken.errors.map((item) => item.reason).join("\n"), /invalid JSON/);

  writeJson(root, SHARED_BINARIES_MANIFEST, { schema: "something.else", entries: [] });
  const wrongSchema = readSharedBinaryWaivers(root);
  assert.match(wrongSchema.errors.map((item) => item.reason).join("\n"), /expected schema/);
});

// A pre-commit hook exports GIT_DIR and GIT_INDEX_FILE for the repository being
// committed. Inheriting them makes every query below read THAT repository's
// index whatever cwd says, which reported every shared-binary waiver as stale
// and blocked studio commits from a feature workspace.
test("preflight Git calls do not inherit the committing repository's Git context", (t) => {
  const root = fixture(t);
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  execFileSync("git", ["config", "user.email", "test@example.com"], { cwd: root });
  execFileSync("git", ["config", "user.name", "Test"], { cwd: root });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  writeFileSync(join(root, "README.md"), "public studio\n", "utf8");
  game(root, "games/private/secret-game", "secret-game", "Secret Title", true);
  execFileSync("git", ["add", ".gitignore", "README.md"], { cwd: root });
  execFileSync("git", ["commit", "-m", "fixture"], { cwd: root, stdio: "ignore" });

  const previous = { GIT_DIR: process.env.GIT_DIR, GIT_INDEX_FILE: process.env.GIT_INDEX_FILE };
  process.env.GIT_DIR = join(root, ".git");
  process.env.GIT_INDEX_FILE = join(root, ".git", "index");
  t.after(() => {
    for (const [name, value] of Object.entries(previous)) {
      if (value === undefined) delete process.env[name];
      else process.env[name] = value;
    }
  });

  const calls = [];
  const result = runPrivateGamePreflight(root, {
    spawnGit(command, args, options) {
      calls.push(options.env);
      return spawnSync(command, args, options);
    },
  });
  assert.equal(result.ok, true);
  assert.ok(calls.length > 0);
  for (const env of calls) {
    assert.equal(env.GIT_DIR, undefined);
    assert.equal(env.GIT_INDEX_FILE, undefined);
  }
});
