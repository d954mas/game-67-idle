// Read-only Items Viewer contract tests over the single-source Lua route.
import test from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import {
  getCatalogView,
  listCatalogs,
  loadCatalogView,
  parseJsonOrThrow,
  routeIssues,
  runItemsCliRaw,
} from "../ops.mjs";

const REPO_ROOT = fileURLToPath(new URL("../../../..", import.meta.url)).replace(/[\\/]$/, "");
const STATE_SCHEMA = readFileSync(join(REPO_ROOT, "templates", "template", "state", "items.schema.json"), "utf8");

function tempDir(t, prefix = "items-viewer-") {
  const dir = mkdtempSync(join(tmpdir(), prefix));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function writeJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify(value, null, 2), "utf8");
}

function writeGameIdentity(root, gameId, privateGame = false) {
  const rel = privateGame ? join("games", "private", gameId) : join("games", gameId);
  writeJson(join(root, rel, "game.json"), {
    schema: "ai_studio.game.v1", id: gameId, title: gameId, storageNamespace: gameId,
  });
  writeJson(join(root, rel, "dependencies.json"), {
    schema: "ai_studio.game.dependencies.v2",
    engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" },
    features: [],
    compatibility: "test",
  });
}

function writeNeutralWorkspace(root) {
  writeGameIdentity(root, "fixture-game");
  writeJson(join(root, "templates", "fixture-template", "template.json"), {
    schema: "ai_studio.template.v1",
    id: "fixture-template",
    title: "Fixture Template",
    storageNamespace: "fixture-template",
  });
}

function writeLuaFixture(dir, ids = ["fx.a", "fx.b"]) {
  writeJson(join(dir, "items.lua.json"), {
    schema: "items.lua.sandbox.v1",
    modules: [{ name: "fixture.items", file: "design/items/catalog.lua" }],
    entries: ["fixture.items"],
  });
  const definitions = ids.map((id) => `items.define({
  id = ${JSON.stringify(id)},
  created = "2026-07-08",
  name = ${JSON.stringify(id)},
  icon = ${JSON.stringify(`icons/${id}`)},
  kind = "material",
  base_value = 1,
  stack = 10,
})`).join("\n\n");
  const luaPath = join(dir, "design", "items", "catalog.lua");
  mkdirSync(dirname(luaPath), { recursive: true });
  writeFileSync(luaPath, `local items = require("studio.items")\n\n${definitions}\n`, "utf8");
  const statePath = join(dir, "state", "items.schema.json");
  mkdirSync(dirname(statePath), { recursive: true });
  writeFileSync(statePath, STATE_SCHEMA, "utf8");
  writeJson(join(dir, "content", "items.lock.json"), {
    schema: "game_seed.items_lock",
    schema_version: 4,
    receipt: {
      schema: "items.release_receipt.v2",
      items_core_version: "1.14.0",
      lua_evaluation_schema: "items.lua.evaluation.v1",
      snapshot_schema: "items.snapshot.v1",
      state_schema: { schema: "game_seed.items", schema_version: 2, version: 1 },
      field_ids: { active: [], reserved: [] },
    },
    def_ids: {},
    removed: {},
  });
}

function privateGameFixture(root) {
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), "games/private/\n", "utf8");
  const game = join(root, "games", "private", "secret-game");
  mkdirSync(game, { recursive: true });
  execFileSync("git", ["init"], { cwd: game, stdio: "ignore" });
  writeGameIdentity(root, "secret-game", true);
  writeJson(join(game, "items.lua.json"), {});
}

test("listCatalogs uses Items manifests and hides private games by default", (t) => {
  const root = tempDir(t);
  writeNeutralWorkspace(root);
  writeJson(join(root, "templates", "fixture-template", "items.lua.json"), {});
  assert.deepEqual(listCatalogs(root).catalogs.map((entry) => [entry.id, entry.hasItems]), [
    ["template:fixture-template", true],
    ["game:fixture-game", false],
  ]);

  const privateRoot = tempDir(t, "items-viewer-private-");
  privateGameFixture(privateRoot);
  assert.deepEqual(listCatalogs(privateRoot).catalogs, []);
  assert.deepEqual(listCatalogs(privateRoot, { includePrivate: true }).catalogs.map((entry) => entry.id), ["game:secret-game"]);
});

test("live template view exposes Snapshot fields, receipt status, and no catalog containers", async () => {
  const view = await getCatalogView(REPO_ROOT, "template:template");
  assert.ok(view);
  assert.equal(view.meta.hasItems, true);
  assert.equal(view.namespace, "tmpl");
  assert.equal(view.items.length, 6);
  assert.equal(Object.hasOwn(view, "containers"), false);
  assert.equal(Object.hasOwn(view, "schema"), false);
  assert.deepEqual(view.item_kinds.map((entry) => entry.id), ["consumable", "currency", "material", "weapon"]);
  assert.equal(view.validate.available, true);
  assert.equal(view.validate.ok, true);
  assert.deepEqual(view.validate.errors, []);

  const energy = view.items.find((item) => item.id === "tmpl.energy");
  assert.equal(energy.name, "Energy");
  assert.equal(energy.icon, "icons/energy");
  assert.deepEqual(energy.currency, { cap: 100, hud: "counter" });
  assert.deepEqual(energy.tags, []);
  for (const item of view.items) assert.equal(view.lock.status_by_id[item.id], "shipped");
});

test("unknown catalog id returns null", async () => {
  assert.equal(await getCatalogView(REPO_ROOT, "template:does-not-exist"), null);
  assert.equal(await getCatalogView(REPO_ROOT, "bogus:template"), null);
});

test("missing Items manifest is an honest empty state", async (t) => {
  const dir = tempDir(t);
  const view = await loadCatalogView(REPO_ROOT, dir, { id: "x:y", kind: "game", title: "Y", folder: "x/y" });
  assert.equal(view.meta.hasItems, false);
  assert.deepEqual(view.items, []);
  assert.equal(Object.hasOwn(view, "schema"), false);
  assert.match(view.validate.reason, /items\.lua\.json/);
});

test("lock status remains a release-receipt projection, not catalog data", async (t) => {
  const dir = tempDir(t);
  writeLuaFixture(dir, ["fx.a", "fx.b", "fx.c"]);
  const lockPath = join(dir, "content", "items.lock.json");
  const lock = JSON.parse(readFileSync(lockPath, "utf8"));
  lock.def_ids = { "fx.a": { storage: "stack", level_count: 0 } };
  lock.removed = {
    "fx.c": { storage: "stack", level_count: 0, fragment_version: 2 },
    "fx.zzz": { storage: "stack", level_count: 0, fragment_version: 2 },
  };
  writeJson(lockPath, lock);
  const view = await loadCatalogView(REPO_ROOT, dir, { id: "t:fx", kind: "template", title: "Fx", folder: "fx" });
  assert.deepEqual(view.lock.status_by_id, { "fx.a": "shipped", "fx.b": "draft", "fx.c": "removed" });
  assert.deepEqual(Object.keys(view.lock.removed).sort(), ["fx.c", "fx.zzz"]);
});

test("invalid Lua is a catalog content error with no legacy fallback", async (t) => {
  const dir = tempDir(t);
  writeLuaFixture(dir);
  writeFileSync(join(dir, "design", "items", "catalog.lua"), "this is not valid lua", "utf8");
  const view = await loadCatalogView(REPO_ROOT, dir, { id: "t:fx", kind: "template", title: "Fx", folder: "fx" });
  assert.equal(view.content_error.source, "catalog");
  assert.deepEqual(view.items, []);
  assert.equal(view.validate.available, false);
});

test("routeIssues attaches only issues whose item still exists", () => {
  const { byItem, unrouted } = routeIssues(["fx.a"], [
    { rule: "a", id: "fx.a" },
    { rule: "removed", id: "fx.deleted" },
    { rule: "global", id: null },
  ]);
  assert.equal(byItem.get("fx.a").length, 1);
  assert.deepEqual(unrouted.map((entry) => entry.rule), ["removed", "global"]);
});

test("runItemsCliRaw rejects when the Studio Python cannot be spawned", async () => {
  await assert.rejects(
    () => runItemsCliRaw(REPO_ROOT, join(REPO_ROOT, "templates", "template"), ["list"], { bin: "python-does-not-exist-items-viewer" }),
    /could not be spawned/,
  );
});

test("parseJsonOrThrow rejects invalid semantic CLI output", () => {
  assert.throws(() => parseJsonOrThrow("not json", "list"), /unparseable JSON/);
  assert.deepEqual(parseJsonOrThrow('{"ok":true}', "list"), { ok: true });
});
