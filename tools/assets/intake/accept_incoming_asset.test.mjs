import { strict as assert } from "node:assert";
import { mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import test from "node:test";

const execFileAsync = promisify(execFile);

test("accept_incoming_asset writes catalog, file copy, and license note", async () => {
  const root = await mkdtemp(join(tmpdir(), "asset-accept-"));
  try {
    const library = join(root, "library");
    const sourceFile = join(root, "asset.bin");
    await writeFile(sourceFile, "asset payload");
    await execFileAsync(process.execPath, [
      "tools/assets/intake/download_source_asset.mjs",
      "--url", sourceFile,
      "--source", "Example",
      "--slug", "Tiny Model",
      "--library", library,
      "--license-name", "CC0",
    ]);

    const { stdout } = await execFileAsync(process.execPath, [
      "tools/assets/intake/accept_incoming_asset.mjs",
      "--library", library,
      "--source", "example",
      "--slug", "tiny-model",
      "--asset-id", "example__tiny-model__cc0",
      "--kind", "model",
      "--title", "Tiny Model",
      "--description", "Small accepted model fixture.",
      "--license-name", "CC0",
      "--license-url", "https://creativecommons.org/publicdomain/zero/1.0/",
      "--tags", "model,cc0,test",
      "--source-page-url", "https://example.test/tiny-model",
      "--author-vendor", "Example Vendor",
      "--notes", "fixture",
    ]);

    const result = JSON.parse(stdout);
    assert.equal(result.asset_id, "example__tiny-model__cc0");
    assert.equal(existsSync(join(library, "files", "models", "example__tiny-model__cc0", "asset.bin")), true);
    const catalog = await readFile(join(library, "catalog", "models", "example__tiny-model__cc0.md"), "utf8");
    assert.match(catalog, /shipping_decision: allowed/);
    assert.match(catalog, /tags: \[model, cc0, test\]/);
    assert.match(catalog, /Source page: https:\/\/example\.test\/tiny-model/);
    assert.match(catalog, /Author\/vendor: Example Vendor/);
    assert.match(catalog, /SHA256:/);
    const license = await readFile(join(library, "licenses", "example__tiny-model__cc0", "license.md"), "utf8");
    assert.match(license, /Commercial use: true/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("accept_incoming_asset preserves texture proof links", async () => {
  const root = await mkdtemp(join(tmpdir(), "asset-texture-accept-"));
  try {
    const library = join(root, "library");
    const sourceFile = join(root, "tile.png");
    await writeFile(sourceFile, "png fixture");
    await execFileAsync(process.execPath, [
      "tools/assets/intake/download_source_asset.mjs",
      "--url", sourceFile,
      "--source", "Example",
      "--slug", "Tiny Tile",
      "--library", library,
      "--license-name", "CC0",
    ]);

    await execFileAsync(process.execPath, [
      "tools/assets/intake/accept_incoming_asset.mjs",
      "--library", library,
      "--source", "example",
      "--slug", "tiny-tile",
      "--asset-id", "example__tiny-tile__cc0",
      "--kind", "texture",
      "--title", "Tiny Tile",
      "--description", "Small accepted texture fixture.",
      "--license-name", "CC0",
      "--license-url", "https://creativecommons.org/publicdomain/zero/1.0/",
      "--tags", "texture,tileable,cc0,test",
      "--tileable", "true",
      "--wrap-mode", "repeat",
      "--preview-2x2", "previews/example__tiny-tile__cc0/tile_2x2.png",
      "--seam-audit", "previews/example__tiny-tile__cc0/tile_audit.json",
    ]);

    const catalog = await readFile(join(library, "catalog", "textures", "example__tiny-tile__cc0.md"), "utf8");
    assert.match(catalog, /tileable: true/);
    assert.match(catalog, /wrap_mode: repeat/);
    assert.match(catalog, /preview_2x2: previews\/example__tiny-tile__cc0\/tile_2x2\.png/);
    assert.match(catalog, /seam_audit: previews\/example__tiny-tile__cc0\/tile_audit\.json/);
    assert.match(catalog, /Texture proof: previews\/example__tiny-tile__cc0\/tile_2x2\.png, previews\/example__tiny-tile__cc0\/tile_audit\.json/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});
