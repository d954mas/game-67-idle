import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import { hashHex, parseNameHeader, parseNtpack } from "../ntpack.mjs";
import { getPackDump } from "../ops.mjs";

const PACK_HEADER_SIZE = 32;
const ENTRY_SIZE = 24;

// Synthesizes a v2 pack per shared/include/nt_pack_format.h: header, entry
// table, then each asset's bytes at its recorded offset.
function buildPack(assets) {
  const dataStart = PACK_HEADER_SIZE + assets.length * ENTRY_SIZE;
  let cursor = Math.ceil(dataStart / 8) * 8;
  const placed = assets.map((asset) => {
    if (asset.aliasOf != null) return asset;
    const at = cursor;
    cursor = Math.ceil((at + asset.data.length) / 4) * 4;
    return { ...asset, offset: at };
  });
  const total = cursor;
  const bytes = new Uint8Array(total);
  const view = new DataView(bytes.buffer);
  view.setUint32(0, 0x4b41504e, true);
  view.setUint32(4, 0, true);
  view.setUint16(8, 2, true);
  view.setUint16(10, placed.length, true);
  view.setUint32(12, dataStart, true);
  view.setUint32(16, total, true);
  placed.forEach((asset, index) => {
    const source = asset.aliasOf != null ? placed[asset.aliasOf] : asset;
    const at = PACK_HEADER_SIZE + index * ENTRY_SIZE;
    view.setBigUint64(at, asset.id, true);
    view.setUint32(at + 8, source.offset, true);
    view.setUint32(at + 12, source.data.length, true);
    view.setUint16(at + 16, 1, true);
    view.setUint8(at + 18, asset.type);
    if (asset.aliasOf == null) bytes.set(asset.data, asset.offset);
  });
  return bytes;
}

function rawTexture(width, height) {
  const header = new Uint8Array(28 + width * height * 4);
  const view = new DataView(header.buffer);
  view.setUint32(0, 0x58455454, true);
  view.setUint16(4, 3, true);
  view.setUint16(6, 1, true);
  view.setUint32(8, width, true);
  view.setUint32(12, height, true);
  view.setUint16(16, 1, true);
  view.setUint8(18, 0);
  view.setUint32(24, width * height * 4, true);
  return header;
}

test("parseNtpack reads entries, texture tags, duplicates, and the type summary", () => {
  const pack = buildPack([
    { id: 0x1111n, type: 2, data: rawTexture(2, 2) },
    { id: 0x2222n, type: 4, data: new Uint8Array(100).fill(7) },
    { id: 0x3333n, type: 4, aliasOf: 1 },
  ]);
  const names = new Map([[hashHex(0x1111n), "assets/textures/tile.png"]]);
  const dump = parseNtpack(pack, { names });

  assert.equal(dump.header.version, 2);
  assert.equal(dump.header.assetCount, 3);
  assert.equal(dump.entries[0].name, "assets/textures/tile.png");
  assert.equal(dump.entries[0].typeTag, "TEX|RAW");
  assert.equal(dump.entries[1].typeTag, "BLOB");
  assert.equal(dump.entries[1].name, `...${hashHex(0x2222n).slice(-12)}`);
  assert.equal(dump.entries[2].dupOfIndex, 1);
  assert.ok(dump.entries[0].gzBytes > 0);
  assert.deepEqual(dump.summary.BLOB, { count: 2, bytes: 100, dupCount: 1 });
});

test("parseNtpack rejects wrong magic and wrong version", () => {
  assert.throws(() => parseNtpack(new Uint8Array(64)), /bad magic/);
  const pack = buildPack([{ id: 1n, type: 4, data: new Uint8Array(4) }]);
  new DataView(pack.buffer).setUint16(8, 9, true);
  assert.throws(() => parseNtpack(pack), /unsupported pack version 9/);
});

test("parseNameHeader maps generated hash defines to asset paths", () => {
  const names = parseNameHeader([
    "#define ASSET_TEX ((nt_hash64_t){0x4B6889C73ADCB47AULL}) /* assets/tex.png */",
    "#define NOT_AN_ASSET 7",
  ].join("\n"));
  assert.equal(names.get("4B6889C73ADCB47A"), "assets/tex.png");
  assert.equal(names.size, 1);
});

test("getPackDump serves a confined pack with names and rejects escapes", (t) => {
  const root = mkdtempSync(join(tmpdir(), "game-page-pack-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  mkdirSync(join(root, "games", "private"), { recursive: true });
  const gameRel = "games/alpha-game";
  const write = (rel, content) => {
    mkdirSync(dirname(join(root, rel)), { recursive: true });
    writeFileSync(join(root, rel), content);
  };
  write(`${gameRel}/game.json`, JSON.stringify({
    schema: "ai_studio.game.v1", id: "alpha-game", title: "Alpha", storageNamespace: "alpha-game",
  }));
  write(`${gameRel}/dependencies.json`, JSON.stringify({
    schema: "ai_studio.game.dependencies.v3",
    engine: { source: "engine", version: "0.1.0", revision: "0".repeat(40), compatibility: "test" },
    features: [], compatibility: "test",
  }));
  const pack = buildPack([{ id: 0x4b6889c73adcb47an, type: 4, data: new Uint8Array(16).fill(3) }]);
  write(`${gameRel}/build/dev/pack/game.ntpack`, pack);
  write(`${gameRel}/src/generated/game.h`, "#define A ((nt_hash64_t){0x4B6889C73ADCB47AULL}) /* assets/blob.bin */");
  write("secret.ntpack", pack);

  const dump = getPackDump(root, "alpha-game", "build/dev/pack/game.ntpack");
  assert.equal(dump.schema, "ai_studio.game_page.pack.v1");
  assert.equal(dump.entries[0].name, "assets/blob.bin");
  assert.equal(dump.pack.namesFrom, "src/generated/game.h");

  assert.equal(getPackDump(root, "alpha-game", "../../secret.ntpack"), null);
  assert.equal(getPackDump(root, "alpha-game", "game.json"), null);
  assert.equal(getPackDump(root, "missing", "build/dev/pack/game.ntpack"), null);
});
