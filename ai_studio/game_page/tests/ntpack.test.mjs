import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";

import { atlasInfo, fontInfo, hashHex, parseNameHeader, parseNtpack, textureInfo } from "../ntpack.mjs";
import { getPackDump, getPackEntryData, getPackEntryDetail } from "../ops.mjs";

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

function syntheticAtlas() {
  const bytes = new Uint8Array(28 + 8 + 48 + 4 * 8 + 6 * 2);
  const view = new DataView(bytes.buffer);
  view.setUint32(0, 0x534c5441, true);
  view.setUint16(4, 7, true);
  view.setUint16(6, 1, true);
  view.setUint16(8, 1, true);
  view.setUint32(12, 28 + 8 + 48, true);
  view.setUint32(16, 4, true);
  view.setUint32(20, 28 + 8 + 48 + 32, true);
  view.setUint32(24, 6, true);
  view.setBigUint64(28, 0xaabbn, true);
  const region = 36;
  view.setBigUint64(region, 0x1234n, true);
  view.setUint16(region + 8, 32, true);
  view.setUint16(region + 10, 16, true);
  view.setFloat32(region + 16, 0.5, true);
  view.setFloat32(region + 20, 0.25, true);
  view.setUint8(region + 32, 4);
  view.setUint8(region + 33, 0);
  view.setUint8(region + 35, 6);
  view.setUint16(region + 38, 3, true);
  const vertexBase = region + 48;
  [[0, 0, 0, 0], [32, 0, 65535, 0], [32, 16, 65535, 65535], [0, 16, 0, 65535]].forEach(([x, y, u, v], index) => {
    const at = vertexBase + index * 8;
    view.setInt16(at, x, true);
    view.setInt16(at + 2, y, true);
    view.setUint16(at + 4, u, true);
    view.setUint16(at + 6, v, true);
  });
  return bytes;
}

test("atlasInfo reads pages, regions, and normalized vertices", () => {
  const bytes = syntheticAtlas();
  const view = new DataView(bytes.buffer);
  const names = new Map([[hashHex(0x1234n), "ui/button"], [hashHex(0xaabbn), "ui/tex0"]]);
  const atlas = atlasInfo(view, 0, bytes.length, names);
  assert.equal(atlas.version, 7);
  assert.deepEqual(atlas.pages, [{ resourceId: hashHex(0xaabbn), name: "ui/tex0" }]);
  assert.equal(atlas.regions[0].name, "ui/button");
  assert.deepEqual([atlas.regions[0].sourceW, atlas.regions[0].sourceH], [32, 16]);
  assert.deepEqual(atlas.regions[0].slice9, [3, 0, 0, 0]);
  assert.equal(atlas.vertices.length, 4);
  assert.equal(atlas.vertices[2].u, 1);
});

function syntheticFont() {
  // One glyph "A": one square contour, 4 on-curve points, no kerns.
  const data = new Uint8Array([
    1, 0, // contour_count = 1
    4, 0, // point_count = 4
    0x0f, 0, // on-curve mask, 2-byte aligned
    0, 0, 0, 0, // first point (0, 0)
    20, 0, // delta (20, 0)
    0, 20, // delta (0, 20)
    0x80, 0xec, 0xff, 0, // delta x via sentinel: -20, delta y 0
  ]);
  const bytes = new Uint8Array(24 + 24 + data.length);
  const view = new DataView(bytes.buffer);
  view.setUint32(0, 0x544e4f46, true);
  view.setUint16(4, 5, true);
  view.setUint16(6, 1, true);
  view.setUint16(8, 1000, true);
  view.setInt16(10, 800, true);
  view.setInt16(12, -200, true);
  view.setUint32(24, 65, true); // codepoint 'A'
  view.setUint32(28, 48, true); // data_offset
  view.setInt16(32, 500, true); // advance
  bytes.set(data, 48);
  return bytes;
}

test("fontInfo decodes charset and varlen contour points", () => {
  const bytes = syntheticFont();
  const font = fontInfo(new DataView(bytes.buffer), 0, bytes.length);
  assert.equal(font.glyphCount, 1);
  assert.equal(font.charset, "A");
  assert.deepEqual(font.glyphs[0].contours, [[
    { x: 0, y: 0, on: 1 },
    { x: 20, y: 0, on: 1 },
    { x: 20, y: 20, on: 1 },
    { x: 0, y: 20, on: 1 },
  ]]);
});

test("getPackEntryDetail and data serve typed details from a pack", (t) => {
  const root = mkdtempSync(join(tmpdir(), "game-page-entry-"));
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
  const pack = buildPack([
    { id: 0x10n, type: 2, data: rawTexture(2, 2) },
    { id: 0x20n, type: 5, data: syntheticFont() },
    { id: 0x30n, type: 6, data: syntheticAtlas() },
  ]);
  write(`${gameRel}/build/dev/pack/game.ntpack`, pack);

  const texture = getPackEntryDetail(root, "alpha-game", "build/dev/pack/game.ntpack", 0);
  assert.equal(texture.kind, "texture");
  assert.deepEqual([texture.texture.width, texture.texture.height], [2, 2]);
  const font = getPackEntryDetail(root, "alpha-game", "build/dev/pack/game.ntpack", 1);
  assert.equal(font.font.charset, "A");
  const atlas = getPackEntryDetail(root, "alpha-game", "build/dev/pack/game.ntpack", 2);
  assert.equal(atlas.atlas.regions.length, 1);

  const data = getPackEntryData(root, "alpha-game", "build/dev/pack/game.ntpack", 0);
  assert.equal(data.bytes.length, rawTexture(2, 2).length);
  assert.equal(getPackEntryDetail(root, "alpha-game", "build/dev/pack/game.ntpack", 9), null);
});

test("textureInfo reads the 28-byte texture header", () => {
  const bytes = rawTexture(4, 2);
  const info = textureInfo(new DataView(bytes.buffer), 0, bytes.length);
  assert.deepEqual([info.width, info.height, info.compression, info.format], [4, 2, 0, 1]);
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
