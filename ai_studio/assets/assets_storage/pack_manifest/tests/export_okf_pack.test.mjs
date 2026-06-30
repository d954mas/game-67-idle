import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { readFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { exportOkfPackToManifest } from "../export_okf_pack.mjs";
import { scanPackManifestSource } from "../pack_manifest.mjs";

test("exportOkfPackToManifest copies one OKF pack into pack manifest layout", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "okf-pack-export-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const library = join(root, "library");
  const out = join(root, "out");
  const assetId = "polypizza__rat__cc0-1-0";
  mkdirSync(join(library, "catalog", "models", "animated-enemies"), { recursive: true });
  mkdirSync(join(library, "files", "models", "animated-enemies", assetId), { recursive: true });
  mkdirSync(join(library, "previews", assetId), { recursive: true });
  writeFileSync(join(library, "files", "models", "animated-enemies", assetId, "rat.glb"), "glb", "utf8");
  writeFileSync(join(library, "previews", assetId, "preview.webp"), "webp", "utf8");
  writeFileSync(join(library, "catalog", "models", "animated-enemies", "_pack.md"), [
    "---",
    "pack: animated-enemies",
    "title: Animated Enemies",
    "source: Poly Pizza",
    "kind: model",
    "origin: sourced",
    "license: CC0-1.0",
    "tags: [enemy, animated]",
    "genre: [fantasy]",
    "style: [low-poly]",
    "description: Small animated enemy models.",
    "---",
    "",
    "Source page: https://poly.pizza/bundle/Animated-Enemies-a53OJwHrhh",
    "",
  ].join("\n"), "utf8");
  writeFileSync(join(library, "catalog", "models", "animated-enemies", "rat.md"), [
    "---",
    `asset_id: ${assetId}`,
    "title: Rat",
    "description: Low-poly animated rat enemy.",
    "kind: model",
    "origin: sourced",
    "license: CC0-1.0",
    "pack: animated-enemies",
    "packs: [animated-enemies, animals]",
    "tags: [rat, enemy]",
    `resource: files/models/animated-enemies/${assetId}`,
    "---",
    "",
    "Source page: https://poly.pizza/m/rat-example",
    "",
  ].join("\n"), "utf8");

  const result = await exportOkfPackToManifest(library, "animated-enemies", out);

  assert.equal(result.assets, 1);
  assert.deepEqual(result.missing_resources, []);
  assert.deepEqual(result.missing_previews, []);
  const packDir = join(out, "packs", "animated-enemies");
  assert.equal(existsSync(join(packDir, "pack.json")), true);
  assert.equal(existsSync(join(packDir, "assets.jsonl")), true);
  assert.equal(existsSync(join(packDir, "files", assetId, "rat.glb")), true);
  assert.equal(existsSync(join(packDir, "previews", assetId, "preview.webp")), true);

  const pack = JSON.parse(await readFile(join(packDir, "pack.json"), "utf8"));
  assert.equal(pack.title, "Animated Enemies");
  assert.equal(pack.source_page, "https://poly.pizza/bundle/Animated-Enemies-a53OJwHrhh");

  const { records, packs } = await scanPackManifestSource(out);
  assert.equal(packs.length, 1);
  assert.equal(records.length, 1);
  assert.equal(records[0].asset_id, assetId);
  assert.deepEqual(records[0].packs, ["animated-enemies", "animals"]);
  assert.equal(records[0].modelPath, join(packDir, "files", assetId, "rat.glb"));
  assert.equal(records[0].preview, join(packDir, "previews", assetId, "preview.webp"));
});
