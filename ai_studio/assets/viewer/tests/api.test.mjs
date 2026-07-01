import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { listAssetViewerSources, resolveAssetViewerGalleryPath, safeResolve, selectSource } from "../api.mjs";

function tempRoot() {
  return mkTemp("asset-viewer-api-");
}

function mkTemp(prefix) {
  const root = join(tmpdir(), `${prefix}${process.pid}-${Date.now()}-${Math.random().toString(16).slice(2)}`);
  mkdirSync(root, { recursive: true });
  return root;
}

test("safeResolve keeps paths inside the selected base", () => {
  const root = resolve("C:/repo");
  assert.equal(safeResolve(root, "game/assets"), resolve(root, "game/assets"));
  assert.equal(safeResolve(root, "games/demo/../demo/assets"), resolve(root, "games/demo/assets"));
  assert.equal(safeResolve(root, "../outside"), null);
  assert.equal(safeResolve(root, "C:/outside/assets"), null);
});

test("selectSource returns registered source or confined custom game source", () => {
  const root = resolve("C:/repo");
  const sources = [{
    id: "template",
    type: "template",
    label: "Template",
    path: resolve(root, "templates/template/assets"),
    available: true,
  }];

  assert.equal(selectSource(sources, { sourceId: "template" }, root).id, "template");

  const custom = selectSource(sources, { type: "game", path: "games/demo/assets" }, root);
  assert.equal(custom.id, "game:games_demo_assets");
  assert.equal(custom.type, "game");
  assert.equal(custom.path, resolve(root, "games/demo/assets"));

  assert.throws(() => selectSource(sources, { type: "game", path: "../outside/assets" }, root), /inside the repository/);
  assert.throws(() => selectSource(sources, { sourceId: "missing" }, root), /unknown asset source/);
});

test("listAssetViewerSources keeps global library with workspace template and game registries", async () => {
  const root = tempRoot();
  try {
    mkdirSync(join(root, "shared-library", "packs"), { recursive: true });
    mkdirSync(join(root, "templates", "template", "assets"), { recursive: true });
    mkdirSync(join(root, "games", "test-game", "assets"), { recursive: true });
    mkdirSync(join(root, "ai_studio", "assets", "storage", "sources"), { recursive: true });
    writeFileSync(join(root, "ai_studio", "assets", "storage", "sources", "libraries.json"), JSON.stringify({
      schema: "ai_studio.assets.libraries.v1",
      libraries: [{
        id: "global-library",
        title: "All Assets",
        assets: "shared-library",
        status: "active",
      }],
    }), "utf8");
    writeFileSync(join(root, "templates", "templates.json"), JSON.stringify({
      schema: "ai_studio.assets.templates.v1",
      templates: [{
        id: "template",
        title: "Template",
        folder: "templates/template",
        assets: "templates/template/assets",
        status: "active",
      }],
    }), "utf8");
    writeFileSync(join(root, "games", "games.json"), JSON.stringify({
      schema: "ai_studio.assets.games.v1",
      games: [{
        id: "test-game",
        title: "Test Game",
        folder: "games/test-game",
        assets: "games/test-game/assets",
        status: "active",
      }],
    }), "utf8");

    const { sources } = await listAssetViewerSources(root);

    assert.deepEqual(
      sources.map((source) => ({ id: source.id, type: source.type, available: source.available })),
      [
        { id: "global-library", type: "library", available: true },
        { id: "template", type: "template", available: true },
        { id: "game:test-game", type: "game", available: true },
      ],
    );
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("resolveAssetViewerGalleryPath confines gallery, library, and repo routes", () => {
  const root = tempRoot();
  try {
    const galleryDir = join(root, "tmp", "ai-studio-asset-viewer", "global-library");
    const libraryRoot = join(root, "library");
    mkdirSync(galleryDir, { recursive: true });
    mkdirSync(join(libraryRoot, "packs", "kit"), { recursive: true });
    writeFileSync(join(galleryDir, ".asset-viewer-source.json"), JSON.stringify({ libraryRoot }), "utf8");

    assert.equal(
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/viewer.js"),
      join(galleryDir, "viewer.js"),
    );
    assert.equal(
      resolveAssetViewerGalleryPath(root, "/viewer/gallery/global-library/lib/packs/kit/model.glb"),
      join(libraryRoot, "packs", "kit", "model.glb"),
    );
    assert.equal(
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/repo/templates/template/assets/cube.glb"),
      join(root, "templates", "template", "assets", "cube.glb"),
    );

    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/../secret.txt"), null);
    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/../../secret.txt"), null);
    assert.equal(resolveAssetViewerGalleryPath(root, "/not-gallery/global-library/viewer.js"), null);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("resolveAssetViewerGalleryPath ignores unusable gallery meta", () => {
  const root = tempRoot();
  try {
    const galleryDir = join(root, "tmp", "ai-studio-asset-viewer", "global-library");
    mkdirSync(galleryDir, { recursive: true });
    const metaPath = join(galleryDir, ".asset-viewer-source.json");

    writeFileSync(metaPath, "{not json", "utf8");
    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/model.glb"), null);

    writeFileSync(metaPath, JSON.stringify({ sourceId: "global-library" }), "utf8");
    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/model.glb"), null);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("asset viewer shell does not require external scripts", () => {
  const html = readFileSync(new URL("../index.html", import.meta.url), "utf8");
  assert.doesNotMatch(html, /<script[^>]+src=["']https?:\/\//i);
});
