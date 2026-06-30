import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import {
  assetIndexPath,
  listIndexedPacks,
  queryIndexedAssets,
  rebuildAssetIndex,
  resolveIndexedModel,
} from "../asset_index.mjs";

function writeRecord(path, frontmatter, body = "") {
  writeFileSync(path, `---\n${frontmatter.trim()}\n---\n${body}`, "utf8");
}

function tempLibrary() {
  const root = mkdtempSync(join(tmpdir(), "asset-index-"));
  const library = join(root, "library");
  mkdirSync(join(library, "catalog", "models", "furniture"), { recursive: true });
  mkdirSync(join(library, "files", "models", "furniture", "kenney__red-sofa__cc0"), { recursive: true });
  mkdirSync(join(library, "files", "models", "furniture", "kenney__blue-chair__cc0"), { recursive: true });
  mkdirSync(join(library, "previews", "kenney__red-sofa__cc0"), { recursive: true });
  mkdirSync(join(library, "previews", "kenney__blue-chair__cc0"), { recursive: true });
  writeFileSync(join(library, "files", "models", "furniture", "kenney__red-sofa__cc0", "sofa.glb"), "glb", "utf8");
  writeFileSync(join(library, "files", "models", "furniture", "kenney__blue-chair__cc0", "chair.glb"), "glb", "utf8");
  writeFileSync(join(library, "previews", "kenney__red-sofa__cc0", "preview.webp"), "webp", "utf8");
  writeFileSync(join(library, "previews", "kenney__blue-chair__cc0", "preview.webp"), "webp", "utf8");
  writeRecord(join(library, "catalog", "models", "furniture", "_pack.md"), `
type: Asset Pack
pack: furniture
title: Furniture Pack
source: Kenney
kind: model
license: CC0
origin: sourced
genre: [cozy]
style: [low-poly]
tags: [furniture, interior]
description: Reusable furniture pack.
`);
  writeRecord(join(library, "catalog", "models", "furniture", "red-sofa.md"), `
type: Game Asset
asset_id: kenney__red-sofa__cc0
title: Red Sofa
description: Cozy red sofa for room scenes.
kind: model
origin: sourced
license: CC0
pack: furniture
resource: files/models/furniture/kenney__red-sofa__cc0
tags: [sofa, furniture]
`);
  writeRecord(join(library, "catalog", "models", "furniture", "blue-chair.md"), `
type: Game Asset
asset_id: kenney__blue-chair__cc0
title: Blue Chair
description: Chair prop.
kind: model
origin: sourced
license: CC0
pack: furniture
resource: files/models/furniture/kenney__blue-chair__cc0
tags: [chair, furniture]
`);
  return { root, library };
}

function source(library) {
  return { id: "global-library", type: "library", label: "All Assets", path: library, mode: "library" };
}

function scanSource(path) {
  return { id: "template", type: "template", label: "Template", path, mode: "scan" };
}

test("rebuildAssetIndex persists OKF catalog assets and pack summaries", async (t) => {
  const { root, library } = tempLibrary();
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const result = await rebuildAssetIndex(root, source(library));

  assert.equal(result.assetCount, 2);
  assert.equal(existsSync(assetIndexPath(root, source(library))), true);

  const packs = await listIndexedPacks(root, source(library));
  assert.equal(packs.length, 1);
  assert.equal(packs[0].pack, "furniture");
  assert.equal(packs[0].count, 2);
  assert.equal(packs[0].covers.length, 2);
});

test("queryIndexedAssets supports paging, search, filters, facets, and model lookup", async (t) => {
  const { root, library } = tempLibrary();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  await rebuildAssetIndex(root, source(library));

  const page = await queryIndexedAssets(root, source(library), {
    q: "sofa",
    filters: { kind: ["model"] },
    offset: 0,
    limit: 24,
  });

  assert.equal(page.total, 1);
  assert.equal(page.assets[0].id, "kenney__red-sofa__cc0");
  assert.equal(page.assets[0].thumb, "lib/previews/kenney__red-sofa__cc0/preview.webp");
  assert.ok(page.facets.kind.some((facet) => facet.value === "model" && facet.count === 1));
  assert.ok(page.facets.tags.some((facet) => facet.value === "sofa" && facet.count === 1));

  const model = await resolveIndexedModel(root, source(library), "kenney__red-sofa__cc0");
  assert.equal(model.model, "lib/files/models/furniture/kenney__red-sofa__cc0/sofa.glb");
});

test("rebuildAssetIndex indexes folder scan sources with the same query API", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-index-scan-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets");
  mkdirSync(join(assets, "models"), { recursive: true });
  mkdirSync(join(assets, "ui"), { recursive: true });
  writeFileSync(join(assets, "models", "robot.glb"), "glb", "utf8");
  writeFileSync(join(assets, "ui", "button.png"), "png", "utf8");

  const src = scanSource(assets);
  const result = await rebuildAssetIndex(root, src);
  assert.equal(result.assetCount, 2);
  assert.equal(existsSync(assetIndexPath(root, src)), true);

  const page = await queryIndexedAssets(root, src, { q: "robot", offset: 0, limit: 24 });
  assert.equal(page.total, 1);
  assert.equal(page.assets[0].kind, "model");

  const model = await resolveIndexedModel(root, src, "template__assets__models__robot.glb");
  assert.equal(model.model, "lib/models/robot.glb");
});
