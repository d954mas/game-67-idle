import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { assetPreviewCachePath, refreshAssetIndex } from "../../index/index.mjs";
import { refreshPreviewCache } from "../cache.mjs";

function scanSource(path) {
  return {
    id: "template",
    type: "template",
    label: "Template",
    path,
    available: true,
  };
}

test("refreshPreviewCache treats raw image files as ready previews", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "preview-cache-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets", "ui");
  mkdirSync(assets, { recursive: true });
  const image = join(assets, "button.png");
  writeFileSync(image, "png", "utf8");

  const source = scanSource(join(root, "template", "assets"));
  const first = await refreshPreviewCache(root, source);
  assert.equal(first.copiedImages, 0);
  assert.equal(first.cachedImages, 1);
  assert.equal(first.staleImages, 0);

  const second = await refreshPreviewCache(root, source);
  assert.equal(second.copiedImages, 0);
  assert.equal(second.cachedImages, 1);
  assert.equal(second.staleImages, 0);
  assert.equal(second.index.assetCount, 1);

  writeFileSync(image, "png changed", "utf8");
  await refreshAssetIndex(root, source);
  const third = await refreshPreviewCache(root, source);
  assert.equal(third.copiedImages, 0);
  assert.equal(third.cachedImages, 1);
});

// Regression: the preview cache for a folder-scanned asset must be keyed by the
// SAME id the index derives (path relative to the source root, separators -> __),
// or the index never finds the rendered/copied preview and silently falls back
// to the raw source. A repo-relative derivation used to leak the whole absolute
// path into the id on Windows, orphaning every folder-source model preview.
test("refreshPreviewCache keys folder previews by the index asset id", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "preview-cache-id-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets", "ui");
  mkdirSync(assets, { recursive: true });
  writeFileSync(join(assets, "button.png"), "png", "utf8");

  const source = scanSource(join(root, "template", "assets"));
  await refreshPreviewCache(root, source, { force: true });

  // scanFolderRecords ids `ui/button.png` (relative to the source root) as
  // `ui__button.png`; the cache must be discoverable under that exact id.
  assert.ok(
    assetPreviewCachePath(root, source, "ui__button.png"),
    "folder preview cache must be found under the index asset id",
  );
});
