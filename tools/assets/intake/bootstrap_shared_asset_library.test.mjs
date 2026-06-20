import { strict as assert } from "node:assert";
import { existsSync } from "node:fs";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import test from "node:test";

const execFileAsync = promisify(execFile);

test("bootstrap_shared_asset_library creates OKF folders and templates", async () => {
  const root = await mkdtemp(join(tmpdir(), "shared-asset-library-"));
  try {
    const library = join(root, "library");
    const { stdout } = await execFileAsync(process.execPath, [
      "tools/assets/intake/bootstrap_shared_asset_library.mjs",
      "--library",
      library,
    ]);

    const result = JSON.parse(stdout);
    assert.equal(result.library, library);
    assert.equal(existsSync(join(library, "_incoming")), true);
    assert.equal(existsSync(join(library, "_quarantine")), true);
    assert.equal(existsSync(join(library, "catalog", "models")), true);
    assert.equal(existsSync(join(library, "catalog", "textures")), true);
    assert.equal(existsSync(join(library, "files", "models")), true);
    assert.equal(existsSync(join(library, "licenses")), true);
    assert.equal(existsSync(join(library, "previews")), true);

    const textureTemplate = await readFile(join(library, "_templates", "texture-record.md"), "utf8");
    assert.match(textureTemplate, /tileable: unknown/);
    assert.match(textureTemplate, /wrap_mode: repeat/);
    assert.match(textureTemplate, /Source page or generation prompt/);
    assert.match(textureTemplate, /License or no-seed reason/);
    assert.match(textureTemplate, /Seam audit/);

    const assetTemplate = await readFile(join(library, "_templates", "asset-record.md"), "utf8");
    assert.match(assetTemplate, /shipping_decision: reference-only/);
    assert.match(assetTemplate, /commercial_use: unknown/);
    assert.match(assetTemplate, /redistribution_allowed: unknown/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});
