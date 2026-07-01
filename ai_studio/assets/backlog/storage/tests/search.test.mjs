import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { searchAssets } from "../search.mjs";

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

test("searchAssets derives a local source id when sourcePath is provided without sourceId", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-search-local-id-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const assets = join(root, "template", "assets");
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
