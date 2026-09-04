import assert from "node:assert/strict";
import test from "node:test";
import { execFileSync, spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
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
  const result = runPrivateGamePreflight(root, {
    spawnGit(command, args, options) {
      calls.push({ command, args, cwd: options.cwd });
      return spawnSync(command, args, options);
    },
  });
  assert.equal(result.ok, true);
  assert.ok(calls.length > 0);
  for (const call of calls) {
    assert.equal(call.command, "git");
    assert.deepEqual(call.args.slice(0, 2), [
      "-c",
      `safe.directory=${resolve(call.cwd).replaceAll("\\", "/")}`,
    ]);
  }
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
