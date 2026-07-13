import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";
import { localWorkspaceCatalogRelPath } from "../../../workspace/games.mjs";
import { searchAssets } from "../ops.mjs";

function writeJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function writeGameCatalog(root, id, local = false) {
  writeJson(join(root, "games", id, "game.json"), { schema: "ai_studio.game.v1", id, title: id, storageNamespace: id });
  writeJson(join(root, "games", id, "dependencies.json"), {
    schema: "ai_studio.game.dependencies.v2", engine: { source: "engine", version: "0.1.0", revision: "0000000000000000000000000000000000000000", compatibility: "test" }, features: [], compatibility: "test",
  });
  writeJson(join(root, "ai_studio", "workspace", "catalog.json"), { schema: "ai_studio.workspace.catalog.v1", mounts: local ? [] : [{ kind: "game", root: `games/${id}`, visibility: "public", gitRoot: "", commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [] }] });
  if (local) writeJson(join(root, "ai_studio", "workspace", "catalog.local.json"), { schema: "ai_studio.workspace.catalog.v1", mounts: [{ kind: "game", root: `games/${id}`, visibility: "private", gitRoot: `games/${id}`, commitPolicy: "nested-private", enabledStores: ["assets"], aliases: [] }] });
}

test("searchAssets queries generated index for a pack manifest source", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-search-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const assets = join(root, "library");
  const packDir = join(assets, "packs", "starter-props");
  mkdirSync(join(packDir, "files"), { recursive: true });
  mkdirSync(join(packDir, "previews"), { recursive: true });
  writeFileSync(join(packDir, "files", "crate.glb"), "glb", "utf8");
  writeFileSync(join(packDir, "previews", "crate.webp"), "webp", "utf8");
  writeFileSync(join(packDir, "pack.json"), JSON.stringify({
    pack: "starter-props",
    title: "Starter Props",
    source: "local",
    kind: "model",
    origin: "mine",
    license: "CC0",
    tags: ["starter"],
  }, null, 2), "utf8");
  writeFileSync(join(packDir, "assets.jsonl"), JSON.stringify({
    asset_id: "starter__crate__cc0",
    title: "Crate",
    description: "Reusable wooden crate.",
    kind: "model",
    resource: "files/crate.glb",
    preview: "previews/crate.webp",
    tags: ["crate", "wood"],
  }) + "\n", "utf8");

  const result = await searchAssets(root, {
    sourceId: "test-library",
    sourcePath: assets,
    type: "library",
    query: "wooden",
    filters: { kind: ["model"] },
    limit: 24,
  });

  assert.equal(result.total, 1);
  assert.equal(result.assets[0].id, "starter__crate__cc0");
  assert.equal(result.assets[0].thumb, "lib/packs/starter-props/previews/crate.webp");
});

test("searchAssets derives a local source id when an ancestor is named tmp", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-search-local-id-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const assets = join(root, "tmp", "template", "assets");
  mkdirSync(join(assets, "ui"), { recursive: true });
  writeFileSync(join(assets, "ui", "button.png"), "png", "utf8");

  const result = await searchAssets(root, {
    sourcePath: assets,
    type: "local",
    query: "button",
    limit: 24,
  });

  assert.equal(result.sourceId, "local-assets");
  assert.equal(result.total, 1);
  assert.equal(result.assets[0].origin, "unregistered");
});

test("searchAssets uses the existing index until refresh is requested", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-search-refresh-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const assets = join(root, "template", "assets");
  mkdirSync(join(assets, "ui"), { recursive: true });
  writeFileSync(join(assets, "ui", "button.png"), "png", "utf8");

  const baseOptions = {
    sourceId: "template",
    sourcePath: assets,
    type: "local",
    query: "button",
    limit: 24,
  };

  const first = await searchAssets(root, baseOptions);
  assert.equal(first.total, 1);

  writeFileSync(join(assets, "ui", "button-secondary.png"), "png", "utf8");

  const withoutRefresh = await searchAssets(root, baseOptions);
  assert.equal(withoutRefresh.total, 1);

  const withRefresh = await searchAssets(root, { ...baseOptions, refresh: true });
  assert.equal(withRefresh.total, 2);
});

test("searchAssets resolves a public game source by game id", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-search-public-game-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const assets = join(root, "games", "public-game", "assets");
  mkdirSync(join(assets, "ui"), { recursive: true });
  writeFileSync(join(assets, "ui", "button.png"), "png", "utf8");
  writeGameCatalog(root, "public-game");

  const result = await searchAssets(root, {
    game: "public-game",
    query: "button",
    refresh: true,
    limit: 24,
  });

  assert.equal(result.sourceId, "game:public-game");
  assert.equal(result.total, 1);
});

test("searchAssets resolves a private game source through preflighted mounts", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-search-private-game-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  execFileSync("git", ["init"], { cwd: root, stdio: "ignore" });
  writeFileSync(join(root, ".gitignore"), `${localWorkspaceCatalogRelPath()}\n`, "utf8");
  writeFileSync(join(root, ".git", "info", "exclude"), "games/secret-game/\n", "utf8");

  const gameRoot = join(root, "games", "secret-game");
  const assets = join(gameRoot, "assets");
  mkdirSync(join(assets, "ui"), { recursive: true });
  writeFileSync(join(assets, "ui", "private-button.png"), "png", "utf8");
  execFileSync("git", ["init"], { cwd: gameRoot, stdio: "ignore" });
  writeGameCatalog(root, "secret-game", true);

  const result = await searchAssets(root, {
    game: "secret-game",
    query: "private-button",
    refresh: true,
    limit: 24,
  });

  assert.equal(result.sourceId, "game:secret-game");
  assert.equal(result.total, 1);
});

test("searchAssets rejects ambiguous explicit game and source path options", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-search-game-source-path-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  await assert.rejects(
    searchAssets(root, { game: "secret-game", sourcePath: join(root, "games", "secret-game", "assets") }),
    /--game cannot be combined with --source-path/,
  );
});
