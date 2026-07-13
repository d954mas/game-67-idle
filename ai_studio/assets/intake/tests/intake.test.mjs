import test from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { stageAsset } from "../stage.mjs";
import { acceptStagedAsset } from "../accept.mjs";
import { rejectStagedAsset } from "../reject.mjs";
import { scanPackManifestSource } from "../../manifests/ops.mjs";

test("stage + accept writes a publishable pack manifest asset", async () => {
  const root = await mkdtemp(join(tmpdir(), "asset-intake-"));
  try {
    const sourceRoot = join(root, "source");
    const input = join(root, "crate.glb");
    await writeFile(input, "glb bytes");

    await stageAsset([
      "--source-root", sourceRoot,
      "--input", input,
      "--source", "Local Source",
      "--slug", "Crate",
      "--license", "CC0-1.0",
    ]);

    const result = await acceptStagedAsset([
      "--source-root", sourceRoot,
      "--source", "local-source",
      "--slug", "crate",
      "--file", "crate.glb",
      "--pack", "starter-props",
      "--asset-id", "local__crate__cc0",
      "--kind", "model",
      "--title", "Crate",
      "--description", "Small crate.",
      "--tags", "crate,prop",
      "--license", "CC0-1.0",
      "--license-url", "https://creativecommons.org/publicdomain/zero/1.0/",
    ]);

    assert.equal(result.publish, "true");
    assert.equal(existsSync(join(sourceRoot, "packs", "starter-props", "files", "crate.glb")), true);
    assert.equal(existsSync(join(sourceRoot, "_incoming", "local-source", "crate")), false);
    assert.equal(existsSync(join(sourceRoot, "_accepted", "local-source", "crate")), true);
    assert.match(result.accepted_dir, /_accepted/);
    const assetRows = (await readFile(join(sourceRoot, "packs", "starter-props", "assets.jsonl"), "utf8")).trim().split(/\r?\n/);
    assert.equal(assetRows.length, 1);
    const row = JSON.parse(assetRows[0]);
    assert.equal(row.asset_id, "local__crate__cc0");
    assert.equal(row.publish, "true");

    const { records } = await scanPackManifestSource(sourceRoot);
    assert.equal(records.length, 1);
    assert.equal(records[0].asset_id, "local__crate__cc0");
    assert.match(records[0].resource, /packs\/starter-props\/files\/crate\.glb$/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("custom license without publish proof accepts into restricted packs", async () => {
  const root = await mkdtemp(join(tmpdir(), "asset-intake-restricted-"));
  try {
    const sourceRoot = join(root, "source");
    const input = join(root, "paid.glb");
    await writeFile(input, "paid bytes");
    await stageAsset(["--source-root", sourceRoot, "--input", input, "--source", "Vendor", "--slug", "Paid Model", "--license", "Vendor Custom"]);

    const result = await acceptStagedAsset([
      "--source-root", sourceRoot,
      "--source", "vendor",
      "--slug", "paid-model",
      "--file", "paid.glb",
      "--pack", "vendor-pack",
      "--asset-id", "vendor__paid__custom",
      "--kind", "model",
      "--license", "Vendor Custom",
      "--license-url", "https://example.test/license",
    ]);

    assert.equal(result.publish, "false");
    assert.equal(existsSync(join(sourceRoot, "restricted", "packs", "vendor-pack", "files", "paid.glb")), true);
    assert.equal(existsSync(join(sourceRoot, "_incoming", "vendor", "paid-model")), false);
    assert.equal(existsSync(join(sourceRoot, "_accepted", "vendor", "paid-model")), true);
    const { records } = await scanPackManifestSource(sourceRoot);
    assert.equal(records.length, 1);
    assert.equal(records[0].asset_id, "vendor__paid__custom");
    assert.match(records[0].resource, /restricted\/packs\/vendor-pack\/files\/paid\.glb$/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("custom publishable asset records provenance, rights, and release-debt metadata", async () => {
  const root = await mkdtemp(join(tmpdir(), "asset-intake-custom-publish-"));
  try {
    const sourceRoot = join(root, "source");
    const input = join(root, "tree.glb");
    await writeFile(input, "tree bytes");
    await stageAsset([
      "--source-root", sourceRoot,
      "--input", input,
      "--source", "Studio Vendor",
      "--slug", "Tree Model",
      "--license", "Studio Custom",
      "--source-page-url", "https://example.test/tree",
    ]);

    const result = await acceptStagedAsset([
      "--source-root", sourceRoot,
      "--source", "studio-vendor",
      "--slug", "tree-model",
      "--file", "tree.glb",
      "--pack", "studio-nature",
      "--asset-id", "studio__tree__custom",
      "--kind", "model",
      "--license", "Studio Custom",
      "--license-url", "https://example.test/license",
      "--license-kind", "custom",
      "--source-page-url", "https://example.test/tree",
      "--author-vendor", "Studio Vendor",
      "--attribution-required", "true",
      "--notice-required", "true",
      "--credit-text", "Tree by Studio Vendor",
      "--commercial-use", "true",
      "--modification-allowed", "true",
      "--redistribution-allowed", "true",
      "--publish", "true",
    ]);

    assert.equal(result.publish, "true");
    assert.equal(existsSync(join(sourceRoot, "packs", "studio-nature", "files", "tree.glb")), true);
    assert.equal(existsSync(join(sourceRoot, "restricted", "packs", "studio-nature", "files", "tree.glb")), false);
    const assetRows = (await readFile(join(sourceRoot, "packs", "studio-nature", "assets.jsonl"), "utf8")).trim().split(/\r?\n/);
    assert.equal(assetRows.length, 1);
    const row = JSON.parse(assetRows[0]);
    assert.equal(row.asset_id, "studio__tree__custom");
    assert.equal(row.license_kind, "custom");
    assert.equal(row.source_page, "https://example.test/tree");
    assert.equal(row.author_vendor, "Studio Vendor");
    assert.equal(row.attribution_required, "true");
    assert.equal(row.notice_required, "true");
    assert.equal(row.credit_text, "Tree by Studio Vendor");
    assert.equal(row.commercial_use, "true");
    assert.equal(row.modification_allowed, "true");
    assert.equal(row.redistribution_allowed, "true");
    assert.equal(row.publish, "true");

    const { records } = await scanPackManifestSource(sourceRoot);
    assert.equal(records.length, 1);
    assert.equal(records[0].asset_id, "studio__tree__custom");
    assert.equal(records[0].license_kind, "custom");
    assert.equal(records[0].source_page, "https://example.test/tree");
    assert.equal(records[0].author_vendor, "Studio Vendor");
    assert.equal(records[0].publish, "true");
    assert.match(records[0].resource, /packs\/studio-nature\/files\/tree\.glb$/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("reject moves staged item out of _incoming", async () => {
  const root = await mkdtemp(join(tmpdir(), "asset-intake-reject-"));
  try {
    const sourceRoot = join(root, "source");
    const input = join(root, "bad.png");
    await writeFile(input, "bad image");
    await stageAsset(["--source-root", sourceRoot, "--input", input, "--source", "Bad", "--slug", "Nope"]);
    const result = await rejectStagedAsset(["--source-root", sourceRoot, "--source", "bad", "--slug", "nope", "--reason", "not useful"]);
    assert.equal(existsSync(join(sourceRoot, "_incoming", "bad", "nope")), false);
    assert.equal(existsSync(result.rejected), true);
    assert.match(result.rejected, /_rejected/);
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});

test("accept rejects staged file paths that escape into same-prefix sibling folders", async () => {
  const root = await mkdtemp(join(tmpdir(), "asset-intake-boundary-"));
  try {
    const sourceRoot = join(root, "source");
    const input = join(root, "crate.glb");
    await writeFile(input, "crate bytes");
    await stageAsset([
      "--source-root", sourceRoot,
      "--input", input,
      "--source", "Local Source",
      "--slug", "Crate",
      "--license", "CC0-1.0",
    ]);

    const sibling = join(sourceRoot, "_incoming", "local-source", "crate2");
    await mkdir(sibling, { recursive: true });
    await writeFile(join(sibling, "outside.glb"), "outside bytes");

    await assert.rejects(
      acceptStagedAsset([
        "--source-root", sourceRoot,
        "--source", "local-source",
        "--slug", "crate",
        "--file", "../crate2/outside.glb",
        "--pack", "starter-props",
        "--asset-id", "local__outside__cc0",
        "--kind", "model",
        "--license", "CC0-1.0",
      ]),
      /escapes staged directory/,
    );
  } finally {
    await rm(root, { recursive: true, force: true });
  }
});
