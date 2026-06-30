import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { scanPackManifestSource } from "../pack_manifest.mjs";

test("scanPackManifestSource reads pack.json and assets.jsonl records", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "pack-manifest-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));

  const packDir = join(root, "packs", "starter-props");
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
    genre: ["prototype"],
    style: ["low-poly"],
    tags: ["props", "starter"],
    description: "Small reusable starter props.",
  }, null, 2), "utf8");
  writeFileSync(join(packDir, "assets.jsonl"), [
    JSON.stringify({
      asset_id: "starter__crate__cc0",
      title: "Crate",
      description: "Reusable wooden crate.",
      kind: "model",
      resource: "files/crate.glb",
      preview: "previews/crate.webp",
      tags: ["crate", "wood"],
    }),
    "",
  ].join("\n"), "utf8");

  const { records, packs } = await scanPackManifestSource(root);

  assert.equal(packs.length, 1);
  assert.equal(packs[0].pack, "starter-props");
  assert.equal(packs[0].title, "Starter Props");
  assert.equal(records.length, 1);
  assert.equal(records[0].asset_id, "starter__crate__cc0");
  assert.equal(records[0].pack, "starter-props");
  assert.equal(records[0].origin, "mine");
  assert.deepEqual(records[0].genre, ["prototype"]);
  assert.deepEqual(records[0].style, ["low-poly"]);
  assert.deepEqual(records[0].tags, ["props", "starter", "crate", "wood"]);
  assert.equal(records[0].resource, "packs/starter-props/files/crate.glb");
  assert.equal(records[0].modelPath, join(packDir, "files", "crate.glb"));
  assert.equal(records[0].preview, join(packDir, "previews", "crate.webp"));
});
