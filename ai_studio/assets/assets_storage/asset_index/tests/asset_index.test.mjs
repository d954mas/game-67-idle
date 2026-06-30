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
  refreshAssetIndex,
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

test("refreshAssetIndex skips full rebuild when the source signature is unchanged", async (t) => {
  const { root, library } = tempLibrary();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const src = source(library);

  await rebuildAssetIndex(root, src);
  const unchanged = await refreshAssetIndex(root, src);
  assert.equal(unchanged.unchanged, true);
  assert.equal(unchanged.assetCount, 2);

  mkdirSync(join(library, "files", "models", "furniture", "kenney__green-table__cc0"), { recursive: true });
  writeFileSync(join(library, "files", "models", "furniture", "kenney__green-table__cc0", "table.glb"), "glb", "utf8");
  writeRecord(join(library, "catalog", "models", "furniture", "green-table.md"), `
type: Game Asset
asset_id: kenney__green-table__cc0
title: Green Table
description: Table prop.
kind: model
origin: sourced
license: CC0
pack: furniture
resource: files/models/furniture/kenney__green-table__cc0
tags: [table, furniture]
`);

  const rebuilt = await refreshAssetIndex(root, src);
  assert.equal(Boolean(rebuilt.unchanged), false);
  assert.equal(rebuilt.assetCount, 3);
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

test("queryIndexedAssets resolves secondary pack memberships", async (t) => {
  const { root, library } = tempLibrary();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(library, "catalog", "models", "showcase"), { recursive: true });
  writeRecord(join(library, "catalog", "models", "showcase", "_pack.md"), `
type: Asset Pack
pack: showcase
title: Showcase Pack
source: Kenney
kind: model
license: CC0
origin: sourced
tags: [showcase]
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
packs: [furniture, showcase]
resource: files/models/furniture/kenney__red-sofa__cc0
tags: [sofa, furniture]
`);

  await rebuildAssetIndex(root, source(library));
  const packs = await listIndexedPacks(root, source(library));
  const showcase = packs.find((pack) => pack.pack === "showcase");
  assert.equal(showcase.count, 1);

  const page = await queryIndexedAssets(root, source(library), { pack: "showcase", offset: 0, limit: 24 });
  assert.equal(page.total, 1);
  assert.equal(page.assets[0].id, "kenney__red-sofa__cc0");
  assert.equal(page.assets[0].pack, "showcase");
  assert.equal(page.assets[0].primaryPack, "furniture");
  assert.deepEqual(page.assets[0].packs, ["furniture", "showcase"]);
  assert.ok(page.facets.pack.some((facet) => facet.value === "showcase" && facet.count === 1));
});

test("rebuildAssetIndex keeps unregistered files visible for OKF library sources", async (t) => {
  const { root, library } = tempLibrary();
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(library, "files", "models", "furniture", "loose-asset"), { recursive: true });
  writeFileSync(join(library, "files", "models", "furniture", "loose-asset", "loose-crate.glb"), "glb", "utf8");

  const result = await rebuildAssetIndex(root, source(library));
  assert.equal(result.assetCount, 3);

  const page = await queryIndexedAssets(root, source(library), { q: "loose-crate", offset: 0, limit: 24 });
  assert.equal(page.total, 1);
  assert.equal(page.assets[0].origin, "unregistered");
  assert.equal(page.assets[0].license, "unknown");
  assert.ok(page.assets[0].tags.includes("unregistered"));
});

test("rebuildAssetIndex marks raw folder scan files as unregistered", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-index-raw-unregistered-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets");
  mkdirSync(join(assets, "ui"), { recursive: true });
  writeFileSync(join(assets, "ui", "button.png"), "png", "utf8");

  const src = scanSource(assets);
  await rebuildAssetIndex(root, src);
  const page = await queryIndexedAssets(root, src, { q: "button", offset: 0, limit: 24 });
  assert.equal(page.total, 1);
  assert.equal(page.assets[0].origin, "unregistered");
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

test("rebuildAssetIndex prefers pack manifests over raw folder scans", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-index-manifest-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets");
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
    tags: ["crate"],
  }) + "\n", "utf8");

  const src = scanSource(assets);
  const result = await rebuildAssetIndex(root, src);
  assert.equal(result.assetCount, 1);
  assert.equal(result.packCount, 1);

  const packs = await listIndexedPacks(root, src);
  assert.equal(packs.length, 1);
  assert.equal(packs[0].pack, "starter-props");
  assert.equal(packs[0].count, 1);

  const page = await queryIndexedAssets(root, src, { q: "crate", offset: 0, limit: 24 });
  assert.equal(page.total, 1);
  assert.equal(page.assets[0].id, "starter__crate__cc0");
  assert.equal(page.assets[0].thumb, "lib/packs/starter-props/previews/crate.webp");

  const model = await resolveIndexedModel(root, src, "starter__crate__cc0");
  assert.equal(model.model, "lib/packs/starter-props/files/crate.glb");
});

test("refreshAssetIndex rebuilds when pack manifest metadata changes", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-index-manifest-refresh-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets");
  const packDir = join(assets, "packs", "starter-props");
  const assetsJsonl = join(packDir, "assets.jsonl");
  mkdirSync(join(packDir, "files"), { recursive: true });
  writeFileSync(join(packDir, "files", "crate.glb"), "glb", "utf8");
  writeFileSync(join(packDir, "pack.json"), JSON.stringify({
    pack: "starter-props",
    title: "Starter Props",
    kind: "model",
    origin: "mine",
    license: "CC0",
  }, null, 2), "utf8");
  writeFileSync(assetsJsonl, JSON.stringify({
    asset_id: "starter__crate__cc0",
    title: "Crate",
    kind: "model",
    resource: "files/crate.glb",
  }) + "\n", "utf8");

  const src = scanSource(assets);
  await rebuildAssetIndex(root, src);
  assert.equal((await queryIndexedAssets(root, src, { q: "big", offset: 0, limit: 24 })).total, 0);

  writeFileSync(assetsJsonl, JSON.stringify({
    asset_id: "starter__crate__cc0",
    title: "Big Crate",
    kind: "model",
    resource: "files/crate.glb",
  }) + "\n", "utf8");

  const refreshed = await refreshAssetIndex(root, src);
  assert.equal(Boolean(refreshed.unchanged), false);
  assert.ok(refreshed.snapshotDiff.changed.some((file) => file.rel === "packs/starter-props/assets.jsonl"));
  assert.equal((await queryIndexedAssets(root, src, { q: "big", offset: 0, limit: 24 })).total, 1);
});

test("rebuildAssetIndex keeps unregistered files visible for manifest sources", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "asset-index-unregistered-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const assets = join(root, "template", "assets");
  const packDir = join(assets, "packs", "starter-props");
  mkdirSync(join(packDir, "files"), { recursive: true });
  writeFileSync(join(packDir, "files", "crate.glb"), "glb", "utf8");
  writeFileSync(join(packDir, "files", "forgotten.png"), "png", "utf8");
  writeFileSync(join(packDir, "pack.json"), JSON.stringify({
    pack: "starter-props",
    title: "Starter Props",
    kind: "model",
    origin: "mine",
    license: "CC0",
  }, null, 2), "utf8");
  writeFileSync(join(packDir, "assets.jsonl"), JSON.stringify({
    asset_id: "starter__crate__cc0",
    title: "Crate",
    kind: "model",
    resource: "files/crate.glb",
  }) + "\n", "utf8");

  const src = scanSource(assets);
  const result = await rebuildAssetIndex(root, src);
  assert.equal(result.assetCount, 2);

  const page = await queryIndexedAssets(root, src, { q: "forgotten", offset: 0, limit: 24 });
  assert.equal(page.total, 1);
  assert.equal(page.assets[0].kind, "texture");
  assert.equal(page.assets[0].origin, "unregistered");
  assert.equal(page.assets[0].license, "unknown");
  assert.equal(page.assets[0].previewStatus, "missing");
});
