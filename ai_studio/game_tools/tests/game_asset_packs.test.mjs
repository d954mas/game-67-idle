import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";
import { readGameAssetPacks, stageGameAssetPacks, validateGameAssetPacks } from "../lib/game_asset_packs.mjs";

const declaration = (packs) => ({ schema: "ai_studio.game.asset_packs.v1", packs });
function write(path, value) { mkdirSync(dirname(path), { recursive: true }); writeFileSync(path, value); }
function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "game-packs-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  return { root, sourceDir: join(root, "host", "bin"), artifactDir: join(root, "web", "bin") };
}

test("asset pack declaration is absent by default and strictly bounds flat pack paths", (t) => {
  const { root } = fixture(t);
  assert.deepEqual(readGameAssetPacks(root), []);
  assert.deepEqual(validateGameAssetPacks(declaration(["assets/forest.ntpack"])), ["assets/forest.ntpack"]);
  for (const packs of [["assets/game.ntpack"], ["assets/a.ntpack", "assets/a.ntpack"], ["assets/A.ntpack"],
    ["assets/../a.ntpack"], ["assets/a/b.ntpack"], ["assets\\a.ntpack"], ["assets/*.ntpack"], ["/assets/a.ntpack"],
    ["assets/a.ntpack\u0000"], Array.from({ length: 65 }, (_, i) => `assets/p${i}.ntpack`)]) {
    assert.throws(() => validateGameAssetPacks(declaration(packs)), /asset pack/i);
  }
  assert.throws(() => validateGameAssetPacks({ ...declaration([]), ignored: true }), /asset pack/i);
  write(join(root, "assets", "runtime-packs.json"), JSON.stringify(declaration(["assets/ruins.ntpack"])));
  assert.deepEqual(readGameAssetPacks(root), ["assets/ruins.ntpack"]);
});

test("declared packs are required and stale cleanup removes only recorded paths", (t) => {
  const item = fixture(t);
  write(join(item.sourceDir, "assets", "forest.ntpack"), "forest");
  write(join(item.artifactDir, "assets", "unmanaged.ntpack"), "keep");
  stageGameAssetPacks({ ...item, packs: ["assets/forest.ntpack"] });
  assert.equal(readFileSync(join(item.artifactDir, "assets", "forest.ntpack"), "utf8"), "forest");
  assert.throws(() => stageGameAssetPacks({ ...item, packs: ["assets/missing.ntpack"] }), /missing/i);
  assert.ok(existsSync(join(item.artifactDir, "assets", "forest.ntpack")));
  stageGameAssetPacks({ ...item, packs: [] });
  assert.ok(!existsSync(join(item.artifactDir, "assets", "forest.ntpack")));
  assert.ok(existsSync(join(item.artifactDir, "assets", "unmanaged.ntpack")));
});

test("declaration and staged pack paths reject symbolic links", (t) => {
  const item = fixture(t);
  write(join(item.root, "outside", "forest.ntpack"), "outside");
  mkdirSync(item.sourceDir, { recursive: true });
  symlinkSync(join(item.root, "outside"), join(item.sourceDir, "assets"), process.platform === "win32" ? "junction" : "dir");
  assert.throws(() => stageGameAssetPacks({ ...item, packs: ["assets/forest.ntpack"] }), /symbolic link/i);
  symlinkSync(join(item.root, "outside"), join(item.root, "assets"), process.platform === "win32" ? "junction" : "dir");
  assert.throws(() => readGameAssetPacks(item.root), /symbolic link/i);
});

test("invalid staging records cannot expand the deletion set and linked destinations are rejected", (t) => {
  const item = fixture(t);
  write(join(item.artifactDir, "assets", "game.ntpack"), "keep core");
  write(join(item.root, "web", "game-asset-packs.json"), JSON.stringify(declaration(["assets/game.ntpack"])));
  assert.throws(() => stageGameAssetPacks({ ...item, packs: [] }), /asset pack/i);
  assert.equal(readFileSync(join(item.artifactDir, "assets", "game.ntpack"), "utf8"), "keep core");
  write(join(item.root, "web", "game-asset-packs.json"), JSON.stringify(declaration([])));
  write(join(item.sourceDir, "assets", "forest.ntpack"), "forest");
  rmSync(join(item.artifactDir, "assets"), { recursive: true });
  mkdirSync(join(item.root, "outside"));
  symlinkSync(join(item.root, "outside"), join(item.artifactDir, "assets"), process.platform === "win32" ? "junction" : "dir");
  assert.throws(() => stageGameAssetPacks({ ...item, packs: ["assets/forest.ntpack"] }), /symbolic link/i);
  assert.ok(!existsSync(join(item.root, "outside", "forest.ntpack")));
});
