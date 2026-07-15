import assert from "node:assert/strict";
import test from "node:test";
import { execFileSync, spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import { auditPrivateGamePreflight, listGameMounts, runPrivateGamePreflight } from "../games.mjs";

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
    schema: "ai_studio.game.dependencies.v2",
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
