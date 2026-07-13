import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

import {
  BINARY_EXTENSIONS,
  ISSUE_CODES,
  auditAssetIntegrity,
  collectTrackedEntries,
  loadAndAuditRepository,
  normalizeRepoPath,
} from "../integrity.mjs";

async function fixture(t) {
  const root = await mkdtemp(join(tmpdir(), "asset-integrity-"));
  t.after(async () => { await import("node:fs/promises").then(({ rm }) => rm(root, { recursive: true, force: true })); });
  await mkdir(join(root, "templates", "one", "assets", "packs", "fixture"), { recursive: true });
  await mkdir(join(root, "templates", "one", "assets", "ui"), { recursive: true });
  await writeFile(join(root, "templates", "one", "assets", "ui", "button.png"), "button");
  await writeFile(join(root, "templates", "one", "assets", "packs", "fixture", "pack.json"), JSON.stringify({
    pack: "fixture", license: "CC0-1.0", license_url: "https://creativecommons.org/publicdomain/zero/1.0/",
    origin: "sourced", source_page: "https://example.test/source", author_vendor: "Fixture Author",
    publish: "true", redistribution_allowed: "true", commercial_use: "true", modification_allowed: "true",
  }));
  await writeFile(join(root, "templates", "one", "assets", "packs", "fixture", "assets.jsonl"), `${JSON.stringify({
    asset_id: "button", source_resource: "ui/button.png", classification: "product-asset", provenance: "fixture source",
    sha256: "c3e2d78f3ff335724b4029f06505b511e4f8f01e258828de29849b36b40b55d0", bytes: 6,
  })}\n`);
  const inventory = {
    schema: "ai_studio.asset_integrity_inventory.v1",
    metadata_sources: [{ type: "pack-manifest", root: "templates/one/assets", scope: "template:one" }],
    entries: [{ path: "templates/one/assets/ui/button.png", classification: "product-asset" }],
    boundaries: [],
  };
  return { root, inventory, tracked: [{ path: "templates/one/assets/ui/button.png", mode: "100644" }] };
}

test("valid tracked binary is resolved through Pack Manifest, license registry, and SHA", async (t) => {
  const f = await fixture(t);
  const result = await auditAssetIntegrity(f);
  assert.equal(result.ok, true, JSON.stringify(result.issues));
  assert.equal(result.schema, "ai_studio.asset_integrity_report.v1");
  assert.equal(result.exitCode, 0);
  assert.equal(result.summary.trackedBinaryBlobs, 1);
  assert.equal(result.summary.verified, 1);
});

test("audit uses stable issue codes for metadata, integrity, inventory, and path failures", async (t) => {
  const f = await fixture(t);
  const pack = join(f.root, "templates", "one", "assets", "packs", "fixture", "assets.jsonl");
  await writeFile(pack, `${JSON.stringify({ asset_id: "button", source_resource: "ui/button.png", classification: "product-asset", origin: "pending", license: "pending", provenance: "pending", sha256: "bad", bytes: 5 })}\n`);
  f.inventory.entries.push({ path: "templates/one/assets/missing.png", classification: "product-asset" });
  f.inventory.entries.push({ path: "../escape.png", classification: "product-asset" });
  const result = await auditAssetIntegrity(f);
  const codes = new Set(result.issues.map((issue) => issue.code));
  for (const code of [
    ISSUE_CODES.MISSING_LICENSE, ISSUE_CODES.MISSING_PROVENANCE, ISSUE_CODES.MISSING_ORIGIN,
    ISSUE_CODES.MALFORMED_SHA256, ISSUE_CODES.MISSING_FILE,
    ISSUE_CODES.PATH_ESCAPE,
  ]) assert.ok(codes.has(code), `${code}: ${JSON.stringify(result.issues)}`);
});

test("missing SHA and valid-but-wrong SHA are distinct failures", async (t) => {
  const f = await fixture(t);
  const manifest = join(f.root, "templates", "one", "assets", "packs", "fixture", "assets.jsonl");
  const base = { asset_id: "button", source_resource: "ui/button.png", classification: "product-asset", provenance: "fixture" };
  await writeFile(manifest, `${JSON.stringify(base)}\n`);
  let result = await auditAssetIntegrity(f);
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.MISSING_SHA256));
  await writeFile(manifest, `${JSON.stringify({ ...base, sha256: "0".repeat(64) })}\n`);
  result = await auditAssetIntegrity(f);
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.HASH_MISMATCH));
});

test("malformed structured metadata fails closed", async (t) => {
  const f = await fixture(t);
  f.inventory.metadata_sources.push({ type: "records-json", path: "broken.json", scope: "broken" });
  await writeFile(join(f.root, "broken.json"), "{");
  const result = await auditAssetIntegrity(f);
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.MALFORMED_METADATA));
});

test("missing classification, untracked, unexpected, duplicate file and conflicting source-local id fail", async (t) => {
  const f = await fixture(t);
  f.inventory.entries[0].classification = "";
  f.inventory.entries.push({ path: "templates/one/assets/ui/untracked.png", classification: "product-asset" });
  f.tracked.push({ path: "templates/one/assets/ui/unexpected.png", mode: "100644" });
  await writeFile(join(f.root, "templates", "one", "assets", "ui", "untracked.png"), "x");
  await writeFile(join(f.root, "templates", "one", "assets", "ui", "unexpected.png"), "x");
  const manifest = join(f.root, "templates", "one", "assets", "packs", "fixture", "assets.jsonl");
  const row = { asset_id: "button", source_resource: "ui/button.png", classification: "product-asset", provenance: "x", sha256: "c3e2d78f3ff335724b4029f06505b511e4f8f01e258828de29849b36b40b55d0", bytes: 6 };
  await writeFile(manifest, `${JSON.stringify(row)}\n${JSON.stringify({ ...row, source_resource: "ui/untracked.png" })}\n${JSON.stringify({ ...row, asset_id: "other" })}\n`);
  const result = await auditAssetIntegrity(f);
  const codes = new Set(result.issues.map((issue) => issue.code));
  for (const code of [
    ISSUE_CODES.MISSING_CLASSIFICATION, ISSUE_CODES.UNTRACKED_FILE, ISSUE_CODES.UNEXPECTED_FILE,
    ISSUE_CODES.TWO_RECORDS_ONE_FILE, ISSUE_CODES.CONFLICTING_DUPLICATE_ID,
  ]) assert.ok(codes.has(code), `${code}: ${JSON.stringify(result.issues)}`);
});

test("IDs are source-scoped and Windows paths normalize without case ambiguity", async (t) => {
  const f = await fixture(t);
  assert.equal(normalizeRepoPath("templates\\one\\assets\\ui\\button.png"), "templates/one/assets/ui/button.png");
  f.inventory.metadata_sources.push({ type: "records-json", path: "owner.json", scope: "other:owner" });
  await writeFile(join(f.root, "owner.json"), JSON.stringify({ schema: "ai_studio.asset_records.v1", assets: [{
    asset_id: "button", path: "other/button.png", classification: "test-fixture", license: "CC0-1.0",
    license_url: "https://creativecommons.org/publicdomain/zero/1.0/", provenance: "fixture", origin: "mine",
    source_page: "https://example.test", author_vendor: "Owner", publish: "true", redistribution_allowed: "true",
    commercial_use: "true", modification_allowed: "true", sha256: "2d711642b726b04401627ca9fbac32f5c8530fb1903cc4db02258717921a4881", bytes: 1,
  }] }));
  await mkdir(join(f.root, "other"), { recursive: true });
  await writeFile(join(f.root, "other", "button.png"), "x");
  f.inventory.entries.push({ path: "other/button.png", classification: "test-fixture" });
  f.tracked.push({ path: "other/button.png", mode: "100644" });
  const result = await auditAssetIntegrity(f);
  assert.equal(result.ok, true, JSON.stringify(result.issues));

  f.inventory.entries.push({ path: "OTHER/BUTTON.PNG", classification: "test-fixture" });
  const collision = await auditAssetIntegrity(f);
  assert.ok(collision.issues.some((issue) => issue.code === ISSUE_CODES.CASE_COLLISION));
});

test("git collection fails closed with setup exit semantics", () => {
  const failed = collectTrackedEntries("x", { spawn: () => ({ status: 128, stdout: Buffer.alloc(0), stderr: Buffer.from("fatal") }) });
  assert.equal(failed.ok, false);
  assert.equal(failed.exitCode, 2);
  assert.equal(failed.issue.code, ISSUE_CODES.GIT_SETUP);
});

test("git index parsing is NUL-safe and malformed rows fail setup", () => {
  const output = Buffer.from("100644 0123456789012345678901234567890123456789 0\tweird\nname.png\0");
  const parsed = collectTrackedEntries("x", { spawn: (_command, args) => {
    assert.deepEqual(args, ["ls-files", "-s", "-z"]);
    return { status: 0, stdout: output, stderr: Buffer.alloc(0) };
  } });
  assert.equal(parsed.ok, true);
  assert.equal(parsed.entries[0].path, "weird\nname.png");
  const malformed = collectTrackedEntries("x", { spawn: () => ({ status: 0, stdout: Buffer.from("bad\0"), stderr: Buffer.alloc(0) }) });
  assert.equal(malformed.ok, false);
  assert.equal(malformed.exitCode, 2);
});

test("actual ntpack fixture is binary inventory content while textual slice is not", () => {
  assert.equal(BINARY_EXTENSIONS.has(".ntpack"), true);
  assert.equal(BINARY_EXTENSIONS.has(".slice"), false);
});

test("boundary rows cannot hide regular files or swap with inventory entries", async (t) => {
  const f = await fixture(t);
  f.inventory.boundaries = [{ path: "templates/one/assets/ui/button.png", classification: "external-boundary", kind: "gitlink" }];
  f.inventory.entries = [];
  const regular = await auditAssetIntegrity(f);
  assert.ok(regular.issues.some((entry) => entry.code === ISSUE_CODES.BOUNDARY_MISMATCH));
  f.tracked[0].mode = "160000";
  f.inventory.entries = [{ path: "templates/one/assets/ui/button.png", classification: "product-asset" }];
  f.inventory.boundaries = [];
  const gitlink = await auditAssetIntegrity(f);
  assert.ok(gitlink.issues.some((entry) => entry.code === ISSUE_CODES.BOUNDARY_MISMATCH));
});

test("duplicate inventory and stale public manifest rows fail 1:1 coverage", async (t) => {
  const f = await fixture(t);
  f.inventory.entries.push({ ...f.inventory.entries[0] });
  const manifest = join(f.root, "templates", "one", "assets", "packs", "fixture", "assets.jsonl");
  const valid = JSON.parse((await import("node:fs/promises").then(({ readFile }) => readFile(manifest, "utf8"))).trim());
  await writeFile(manifest, `${JSON.stringify(valid)}\n${JSON.stringify({ ...valid, asset_id: "stale", source_resource: "ui/stale.png" })}\n`);
  const result = await auditAssetIntegrity(f);
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.DUPLICATE_INVENTORY));
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.UNEXPECTED_METADATA && entry.path.endsWith("ui/stale.png")));
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.MISSING_FILE && entry.path.endsWith("ui/stale.png")));
});

test("origin is an exact enum and asset_id is mandatory", async (t) => {
  const f = await fixture(t);
  const manifest = join(f.root, "templates", "one", "assets", "packs", "fixture", "assets.jsonl");
  await writeFile(manifest, `${JSON.stringify({ source_resource: "ui/button.png", classification: "product-asset", provenance: "fixture", origin: "home-made", license: "CC0-1.0", sha256: "c3e2d78f3ff335724b4029f06505b511e4f8f01e258828de29849b36b40b55d0", bytes: 6 })}\n`);
  const result = await auditAssetIntegrity(f);
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.MISSING_ASSET_ID));
  await writeFile(manifest, `${JSON.stringify({ asset_id: "button", source_resource: "ui/button.png", classification: "product-asset", provenance: "fixture", origin: "home-made", license: "CC0-1.0", sha256: "c3e2d78f3ff335724b4029f06505b511e4f8f01e258828de29849b36b40b55d0", bytes: 6 })}\n`);
  const invalidOrigin = await auditAssetIntegrity(f);
  assert.ok(invalidOrigin.issues.some((entry) => entry.code === ISSUE_CODES.INVALID_LICENSE_ORIGIN && /origin must/.test(entry.message)));
});

test("scope filters work before verification and report scoped accounting", async (t) => {
  const f = await fixture(t);
  f.inventory.entries[0].metadata_scope = "template:one";
  f.inventory.metadata_sources[0].scope = "template:one";
  f.inventory.entries.push({ path: "templates/other/assets/ui/missing.png", classification: "product-asset", metadata_scope: "broken" });
  f.inventory.metadata_sources.push({ type: "records-json", path: "broken.json", scope: "broken" });
  await writeFile(join(f.root, "broken.json"), "{");
  const scoped = await auditAssetIntegrity({ ...f, scope: "templates/one/assets/ui/button.png" });
  assert.equal(scoped.issues.length, 0, JSON.stringify(scoped.issues));
  assert.equal(scoped.summary.inventoryEntries, 1);
  assert.equal(scoped.summary.trackedBinaryBlobs, 1);
});

test("invalid, escaping, and typo scopes are setup errors", async (t) => {
  const f = await fixture(t);
  const inventoryPath = "inventory.json";
  await writeFile(join(f.root, inventoryPath), JSON.stringify(f.inventory));
  const spawn = () => ({ status: 0, stdout: Buffer.from("100644 0123456789012345678901234567890123456789 0\ttemplates/one/assets/ui/button.png\0"), stderr: Buffer.alloc(0) });
  for (const scope of ["../escape", "templates/typo"]) {
    const result = await loadAndAuditRepository(f.root, { inventoryPath, scope, spawn });
    assert.equal(result.exitCode, 2);
    assert.equal(result.schema, "ai_studio.asset_integrity_report.v1");
    assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.INVALID_SCOPE));
  }
});

test("unsupported inventory schema is a setup error", async (t) => {
  const f = await fixture(t);
  const inventoryPath = "inventory.json";
  await writeFile(join(f.root, inventoryPath), JSON.stringify({ ...f.inventory, schema: "future.v9" }));
  const result = await loadAndAuditRepository(f.root, { inventoryPath, spawn: () => { throw new Error("git must not run before schema validation"); } });
  assert.equal(result.exitCode, 2);
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.MALFORMED_METADATA));
});

test("pack metadata scopes select every inventory row owned by that metadata_scope", async (t) => {
  const f = await fixture(t);
  f.inventory.entries[0].metadata_scope = "template:fixture";
  f.inventory.metadata_sources[0].scope = "template:fixture";
  f.inventory.metadata_sources[0].packs = ["fixture"];
  const inventoryPath = "inventory.json";
  await writeFile(join(f.root, inventoryPath), JSON.stringify(f.inventory));
  const spawn = () => ({ status: 0, stdout: Buffer.from("100644 0123456789012345678901234567890123456789 0\ttemplates/one/assets/ui/button.png\0"), stderr: Buffer.alloc(0) });
  for (const scope of [
    "templates/one/assets/packs/fixture",
    "templates/one/assets/packs/fixture/pack.json",
    "templates/one/assets/packs/fixture/assets.jsonl",
  ]) {
    const result = await loadAndAuditRepository(f.root, { inventoryPath, scope, spawn });
    assert.equal(result.ok, true, `${scope}: ${JSON.stringify(result.issues)}`);
    assert.equal(result.summary.inventoryEntries, 1);
    assert.equal(result.summary.verified, 1);
    assert.equal(result.summary.scope, scope);
  }
});

test("records-json metadata scope selects its owned binary", async (t) => {
  const f = await fixture(t);
  await mkdir(join(f.root, "owner"), { recursive: true });
  await writeFile(join(f.root, "owner", "studio.hdr"), "x");
  const metadataPath = "owner/studio.asset.json";
  await writeFile(join(f.root, metadataPath), JSON.stringify({ schema: "ai_studio.asset_records.v1", assets: [{
    asset_id: "studio-hdr", path: "owner/studio.hdr", classification: "generated-procedural-output",
    license: "CC0-1.0", license_url: "https://creativecommons.org/publicdomain/zero/1.0/", provenance: "fixture",
    origin: "mine", source_page: "owner/generator.mjs", author_vendor: "Fixture", publish: "true",
    redistribution_allowed: "true", commercial_use: "true", modification_allowed: "true",
    sha256: "2d711642b726b04401627ca9fbac32f5c8530fb1903cc4db02258717921a4881", bytes: 1,
  }] }));
  f.inventory.metadata_sources.push({ type: "records-json", path: metadataPath, scope: "owner:hdr" });
  f.inventory.entries.push({ path: "owner/studio.hdr", classification: "generated-procedural-output", metadata_scope: "owner:hdr" });
  const inventoryPath = "inventory.json";
  await writeFile(join(f.root, inventoryPath), JSON.stringify(f.inventory));
  const stdout = Buffer.from([
    "100644 0123456789012345678901234567890123456789 0\ttemplates/one/assets/ui/button.png",
    "100644 1123456789012345678901234567890123456789 0\towner/studio.hdr",
    "",
  ].join("\0"));
  const result = await loadAndAuditRepository(f.root, { inventoryPath, scope: metadataPath, spawn: () => ({ status: 0, stdout, stderr: Buffer.alloc(0) }) });
  assert.equal(result.ok, true, JSON.stringify(result.issues));
  assert.equal(result.summary.inventoryEntries, 1);
  assert.equal(result.summary.verified, 1);
});

test("license_file evidence is repo-confined, existing, and declared for the same change", async (t) => {
  const f = await fixture(t);
  await writeFile(join(f.root, "OWNER_LICENSE.md"), "approved");
  const manifest = join(f.root, "templates", "one", "assets", "packs", "fixture", "assets.jsonl");
  const row = {
    asset_id: "button", source_resource: "ui/button.png", classification: "product-asset", provenance: "fixture", origin: "mine",
    license: "Studio-Owned-Public-1.0", license_kind: "custom", license_file: "OWNER_LICENSE.md", publish: "true",
    redistribution_allowed: "true", commercial_use: "true", modification_allowed: "true",
    sha256: "c3e2d78f3ff335724b4029f06505b511e4f8f01e258828de29849b36b40b55d0", bytes: 6,
  };
  await writeFile(manifest, `${JSON.stringify(row)}\n`);
  f.inventory.evidence_files = ["OWNER_LICENSE.md"];
  let result = await auditAssetIntegrity(f);
  assert.equal(result.ok, true, JSON.stringify(result.issues));

  await writeFile(manifest, `${JSON.stringify({ ...row, license_file: "../escape.md" })}\n`);
  result = await auditAssetIntegrity(f);
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.INVALID_LICENSE_EVIDENCE));

  await writeFile(manifest, `${JSON.stringify({ ...row, license_file: "MISSING.md" })}\n`);
  result = await auditAssetIntegrity(f);
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.INVALID_LICENSE_EVIDENCE));
});

test("typo below a valid manifest source is a zero-selection setup error", async (t) => {
  const f = await fixture(t);
  f.inventory.entries[0].metadata_scope = "template:fixture";
  f.inventory.metadata_sources[0].scope = "template:fixture";
  f.inventory.metadata_sources[0].packs = ["fixture"];
  const inventoryPath = "inventory.json";
  await writeFile(join(f.root, inventoryPath), JSON.stringify(f.inventory));
  const result = await loadAndAuditRepository(f.root, {
    inventoryPath,
    scope: "templates/one/assets/packs/fixture/typo.json",
    spawn: () => { throw new Error("git must not run for zero-selected scope"); },
  });
  assert.equal(result.exitCode, 2);
  assert.ok(result.issues.some((entry) => entry.code === ISSUE_CODES.INVALID_SCOPE));
});

test("real repository inventory is globally clean after the approved owner disposition", async () => {
  const root = fileURLToPath(new URL("../../../../", import.meta.url)).replace(/[\\/]$/, "");
  const inventory = JSON.parse(await import("node:fs/promises").then(({ readFile }) => readFile(join(root, "ai_studio/assets/manifests/tracked_binary_inventory.json"), "utf8")));
  const collected = collectTrackedEntries(root, {
    spawn: (command, args, options) => spawnSync(command, ["-c", `safe.directory=${normalizeRepoPath(root)}`, ...args], options),
  });
  assert.equal(collected.ok, true, JSON.stringify(collected.issue));
  const result = await auditAssetIntegrity({ root, tracked: collected.entries, inventory });
  assert.equal(result.summary.trackedBinaryBlobs, 27);
  assert.equal(result.summary.externalBoundaries, 1);
  assert.equal(result.summary.inventoryEntries, 27);
  assert.equal(result.summary.metadataRecords, 27);
  assert.equal(result.summary.verified, 27);
  assert.equal(result.ok, true, JSON.stringify(result.issues));
  assert.equal(result.exitCode, 0);
  assert.deepEqual(result.issues, []);
  const inventoryPaths = new Set(inventory.entries.map((entry) => entry.path));
  for (const required of [
    "ai_studio/assets/items_viewer/tests/fixtures/icon_pack/game.ntpack",
    "ai_studio/assets/items_viewer/tests/fixtures/icon_pack/icons_page0.png",
    "ai_studio/assets/previews/studio_env.hdr",
    "ai_studio/core_harness/profiling/hook_record_fast.exe",
  ]) assert.ok(inventoryPaths.has(required), required);
  assert.equal(inventoryPaths.has("ai_studio/assets/items_viewer/tests/fixtures/icon_pack/game_assets.h.slice"), false);
  const packNames = inventory.metadata_sources.flatMap((source) => source.packs || []);
  assert.equal(packNames.some((name) => name.includes("pending")), false);
  const studioOwnedFiles = [
    "ai_studio/assets/previews/studio_env.asset.json",
    "ai_studio/core_harness/profiling/hook_record_fast.asset.json",
  ];
  for (const path of studioOwnedFiles) {
    const text = await import("node:fs/promises").then(({ readFile }) => readFile(join(root, path), "utf8"));
    assert.doesNotMatch(text, /"license_url"\s*:\s*"(?!https?:)/, path);
    assert.match(text, /"license_file"\s*:/, path);
  }
  const templateCube = (await import("node:fs/promises").then(({ readFile }) => readFile(join(root, "templates/template/assets/packs/template-starter-meshes/assets.jsonl"), "utf8"))).trim();
  assert.match(templateCube, /b21a81e96/);
  assert.match(templateCube, /68cfaabcf/);
  assert.match(templateCube, /f8be370c2/);
  const template = await auditAssetIntegrity({ root, tracked: collected.entries, inventory, scope: "templates/template/assets" });
  assert.equal(template.ok, true, JSON.stringify(template.issues));
});
