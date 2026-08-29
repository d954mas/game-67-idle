import assert from "node:assert/strict";
import test from "node:test";

import { analyzeNtpackBuffer, formatAnalysisText, parseArgs } from "../ntpack_cli.mjs";

const PACK_HEADER_SIZE = 32;
const ENTRY_SIZE = 24;

function buildPack(assets) {
  const dataStart = PACK_HEADER_SIZE + assets.length * ENTRY_SIZE;
  let cursor = Math.ceil(dataStart / 8) * 8;
  const placed = assets.map((asset) => {
    if (asset.aliasOf != null) return asset;
    const offset = cursor;
    cursor = Math.ceil((offset + asset.data.length) / 4) * 4;
    return { ...asset, offset };
  });
  const bytes = new Uint8Array(cursor);
  const view = new DataView(bytes.buffer);
  view.setUint32(0, 0x4b41504e, true);
  view.setUint16(8, 2, true);
  view.setUint16(10, placed.length, true);
  view.setUint32(12, dataStart, true);
  view.setUint32(16, bytes.length, true);
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

test("analyzeNtpackBuffer reports unique entries, exact raw totals, and isolated Brotli", () => {
  const pack = buildPack([
    { id: 0x1n, type: 4, data: new Uint8Array(100).fill(7) },
    { id: 0x2n, type: 3, data: new Uint8Array(40).fill(3) },
    { id: 0x3n, type: 4, aliasOf: 0 },
  ]);
  const names = new Map([
    ["0000000000000001", "assets/large.mesh"],
    ["0000000000000002", "assets/small.shader"],
    ["0000000000000003", "assets/large-alias.mesh"],
  ]);

  const result = analyzeNtpackBuffer(pack, { names, quality: 4 });

  assert.equal(result.fileBytes, pack.length);
  assert.equal(result.entryCount, 3);
  assert.equal(result.uniqueEntryCount, 2);
  assert.equal(result.payloadBytes, 140);
  assert.equal(result.overheadBytes, pack.length - 140);
  assert.ok(result.packBrotliBytes > 0);
  assert.deepEqual(result.entries.map((entry) => entry.name), [
    "assets/large.mesh",
    "assets/small.shader",
  ]);
  assert.ok(result.entries.every((entry) => entry.brotliBytes > 0));
  assert.deepEqual(result.byType.map((row) => [row.type, row.count, row.rawBytes]), [
    ["BLOB", 1, 100],
    ["SHADER", 1, 40],
  ]);
});

test("CLI args expose reusable JSON and human report controls", () => {
  assert.deepEqual(
    parseArgs(["build/pack/cosmetics.ntpack", "--names", "src/generated/cosmetics.h", "--quality", "9", "--limit", "7", "--json"]),
    {
      packPath: "build/pack/cosmetics.ntpack",
      namesPath: "src/generated/cosmetics.h",
      quality: 9,
      limit: 7,
      json: true,
    },
  );
  assert.throws(() => parseArgs([]), /usage:/);
  assert.throws(() => parseArgs(["a.ntpack", "--quality", "12"]), /quality/);
});

test("human report labels per-entry Brotli as an isolated estimate", () => {
  const report = formatAnalysisText({
    packPath: "cosmetics.ntpack",
    quality: 11,
    fileBytes: 1000,
    packBrotliBytes: 300,
    payloadBytes: 900,
    overheadBytes: 100,
    entryCount: 1,
    uniqueEntryCount: 1,
    byType: [{ type: "MESH", count: 1, rawBytes: 900, brotliBytes: 250 }],
    entries: [{ name: "assets/hat.glb", type: "MESH", rawBytes: 900, brotliBytes: 250, detail: "" }],
  }, { limit: 20 });

  assert.match(report, /Brotli q11 per entry is isolated/);
  assert.match(report, /assets\/hat\.glb/);
});
