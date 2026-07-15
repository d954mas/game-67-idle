import assert from "node:assert/strict";
import test from "node:test";
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
      schema: "ai_studio.game.dependencies.v2",
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
      id: "game-state", source: "features/game-state", revision: engine.revision, compatibility: "tested",
    }],
  }), /version.*exact SemVer/i);
});
