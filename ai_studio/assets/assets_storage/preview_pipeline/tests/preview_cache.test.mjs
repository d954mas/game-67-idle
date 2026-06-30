import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { assetPreviewMetaPath, refreshAssetIndex } from "../../asset_index/asset_index.mjs";
import { refreshPreviewCache } from "../preview_cache.mjs";

function scanSource(path) {
  return {
    id: "template",
    type: "template",
    label: "Template",
    path,
    mode: "scan",
    available: true,
  };
}

test("refreshPreviewCache copies only missing or stale image previews", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "preview-cache-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets", "ui");
  mkdirSync(assets, { recursive: true });
  const image = join(assets, "button.png");
  writeFileSync(image, "png", "utf8");

  const source = scanSource(join(root, "template", "assets"));
  const assetId = "template__assets__ui__button.png";

  const first = await refreshPreviewCache(root, source);
  assert.equal(first.copiedImages, 1);
  assert.equal(first.cachedImages, 0);
  assert.equal(first.staleImages, 0);
  assert.equal(existsSync(assetPreviewMetaPath(root, source, assetId)), true);

  const second = await refreshPreviewCache(root, source);
  assert.equal(second.copiedImages, 0);
  assert.equal(second.cachedImages, 1);
  assert.equal(second.staleImages, 0);
  assert.equal(second.index.assetCount, 1);

  writeFileSync(image, "png changed", "utf8");
  await refreshAssetIndex(root, source);
  const third = await refreshPreviewCache(root, source);
  assert.equal(third.copiedImages, 1);
  assert.equal(third.staleImages, 1);

  const meta = JSON.parse(readFileSync(assetPreviewMetaPath(root, source, assetId), "utf8"));
  assert.equal(meta.sourceSize, "png changed".length);
});
