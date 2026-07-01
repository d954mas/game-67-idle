import test from "node:test";
import assert from "node:assert/strict";
import { resolve } from "node:path";
import { localRecord, parseArgs, resolvePullTarget } from "../pull.mjs";
import { isPublishable } from "../../storage/license/restricted.mjs";

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
