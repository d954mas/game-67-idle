import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { resolveAssetViewerGalleryPath, safeResolve, selectSource } from "../api.mjs";

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
  assert.equal(safeResolve(root, "game/../template/assets"), resolve(root, "template/assets"));
  assert.equal(safeResolve(root, "../outside"), null);
  assert.equal(safeResolve(root, "C:/outside/assets"), null);
});

test("selectSource returns registered source or confined custom game source", () => {
  const root = resolve("C:/repo");
  const sources = [{
    id: "template",
    type: "template",
    label: "Template",
    path: resolve(root, "template/assets"),
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
      resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/repo/template/assets/cube.glb"),
      join(root, "template", "assets", "cube.glb"),
    );

    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/../secret.txt"), null);
    assert.equal(resolveAssetViewerGalleryPath(root, "/asset_viewer/gallery/global-library/lib/../../secret.txt"), null);
    assert.equal(resolveAssetViewerGalleryPath(root, "/not-gallery/global-library/viewer.js"), null);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});
