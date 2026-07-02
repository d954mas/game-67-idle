import test from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdtempSync, mkdirSync, readFileSync, writeFileSync, existsSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { localRecord, parseArgs, resolvePullTarget } from "../pull.mjs";
import { isPublishable } from "../../backlog/storage/license/restricted.mjs";

const PULL_MJS = resolve(dirname(fileURLToPath(import.meta.url)), "..", "pull.mjs");

function makeLibrary(root, { withFile = true } = {}) {
  const packDir = join(root, "library", "packs", "test-pack");
  mkdirSync(join(packDir, "files"), { recursive: true });
  writeFileSync(join(packDir, "pack.json"), JSON.stringify({
    pack: "test-pack",
    title: "Test Pack",
    origin: "sourced",
    license: "CC0-1.0",
    license_kind: "cc0",
  }));
  writeFileSync(join(packDir, "assets.jsonl"), JSON.stringify({
    asset_id: "test__icon__cc0-1-0",
    title: "Icon",
    kind: "ui",
    resource: "files/icon.png",
    license: "CC0-1.0",
    license_kind: "cc0",
    origin: "sourced",
  }) + "\n");
  if (withFile) writeFileSync(join(packDir, "files", "icon.png"), Buffer.from("png-bytes-for-pull-test"));
  return join(root, "library");
}

function runPull(cwd, library, extra = []) {
  return spawnSync(process.execPath, [
    PULL_MJS,
    "--ids", "test__icon__cc0-1-0",
    "--library", library,
    "--to", "gameassets",
    ...extra,
  ], { cwd, encoding: "utf8" });
}

test("pull --apply copies non-model files into the target pack", () => {
  const root = mkdtempSync(join(tmpdir(), "pull-e2e-"));
  try {
    const library = makeLibrary(root);
    const result = runPull(root, library, ["--apply"]);
    assert.equal(result.status, 0, result.stderr);

    const dst = join(root, "gameassets", "packs", "library-pulls", "files", "test__icon__cc0-1-0", "icon.png");
    assert.ok(existsSync(dst), "pulled file must exist in the game pack");
    assert.equal(readFileSync(dst, "utf8"), "png-bytes-for-pull-test");

    const rows = readFileSync(join(root, "gameassets", "packs", "library-pulls", "assets.jsonl"), "utf8")
      .split(/\r?\n/).filter(Boolean).map((line) => JSON.parse(line));
    assert.equal(rows.length, 1);
    assert.equal(rows[0].asset_id, "test__icon__cc0-1-0");
    assert.equal(rows[0].resource, "files/test__icon__cc0-1-0/icon.png");
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("pull --apply fails loudly when the library file is missing", () => {
  const root = mkdtempSync(join(tmpdir(), "pull-e2e-missing-"));
  try {
    const library = makeLibrary(root, { withFile: false });
    const result = runPull(root, library, ["--apply"]);
    assert.notEqual(result.status, 0, "must exit non-zero when the source file is absent");
    assert.match(result.stderr, /library file missing for test__icon__cc0-1-0/);
    assert.ok(!existsSync(join(root, "gameassets", "packs", "library-pulls", "assets.jsonl")),
      "no manifest row may be written for a missing file");
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("parseArgs requires ids and target asset root", () => {
  assert.throws(() => parseArgs(["--to", "templates/template/assets"]), /missing --ids/);
  assert.throws(() => parseArgs(["--ids", "crate"]), /missing --to/);
  assert.deepEqual(parseArgs(["--ids", "crate", "--to", "templates/template/assets"]).ids, "crate");
});

test("resolvePullTarget keeps game/template asset targets inside the repository", () => {
  const root = resolve("C:/repo");

  assert.equal(resolvePullTarget(root, "templates/template/assets"), resolve(root, "templates/template/assets"));
  assert.equal(resolvePullTarget(root, resolve(root, "games/demo/assets")), resolve(root, "games/demo/assets"));
  assert.throws(() => resolvePullTarget(root, "../outside/assets"), /inside the repository/);
  assert.throws(() => resolvePullTarget(root, "C:/outside/assets"), /inside the repository/);
  assert.throws(() => resolvePullTarget(root, "."), /not the repository root/);
});

test("localRecord preserves custom publishability metadata for public pulls", () => {
  const record = {
    asset_id: "studio__tree__custom",
    title: "Tree",
    kind: "model",
    origin: "sourced",
    source_id: "studio__tree__custom",
    source_page: "https://example.test/tree",
    author_vendor: "Studio Vendor",
    license: "Studio Custom",
    license_url: "https://example.test/license",
    license_kind: "custom",
    attribution_required: "true",
    notice_required: "true",
    credit_text: "Tree by Studio Vendor",
    commercial_use: "true",
    modification_allowed: "true",
    redistribution_allowed: "true",
    publish: "true",
    tags: ["tree"],
  };

  const local = localRecord(record, "files/studio__tree__custom/tree.glb", "", "2026-07-01T00:00:00.000Z");

  assert.equal(local.license_kind, "custom");
  assert.equal(local.source_page, "https://example.test/tree");
  assert.equal(local.author_vendor, "Studio Vendor");
  assert.equal(local.commercial_use, "true");
  assert.equal(local.modification_allowed, "true");
  assert.equal(local.redistribution_allowed, "true");
  assert.equal(local.publish, "true");
  assert.equal(isPublishable(local), true);
});
