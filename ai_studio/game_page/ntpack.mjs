// Studio-side ntpack reader. Mirrors the engine's shared/include/nt_pack_format.h
// (pack v2) and nt_builder_dump's derived facts (texture format sniff, gzip
// size estimate, duplicate detection) without touching the engine.
import { gzipSync } from "node:zlib";

const NT_PACK_MAGIC = 0x4b41504e; // "NPAK"
const NT_PACK_VERSION = 2;
const PACK_HEADER_SIZE = 32;
const ASSET_ENTRY_SIZE = 24;

const TEXTURE_MAGIC = 0x58455454; // "TTEX"
const TEXTURE_HEADER_SIZE = 28;
const BASIS_SIG = 0x4273; // 'sB'

export const ASSET_TYPE_NAMES = {
  1: "MESH",
  2: "TEXTURE",
  3: "SHADER",
  4: "BLOB",
  5: "FONT",
  6: "ATLAS",
};

function textureFormatTag(view, offset, size) {
  if (size < TEXTURE_HEADER_SIZE || view.getUint32(offset, true) !== TEXTURE_MAGIC) return "TEX";
  const compression = view.getUint8(offset + 18);
  if (compression === 0) return "TEX|RAW";
  if (compression !== 1) return "TEX";
  const basisOffset = offset + TEXTURE_HEADER_SIZE;
  if (size < TEXTURE_HEADER_SIZE + 23 || view.getUint16(basisOffset, true) !== BASIS_SIG) return "TEX|BASIS";
  const mode = view.getUint8(basisOffset + 20);
  if (mode === 0) return "TEX|ETC1S";
  if (mode === 1) return "TEX|UASTC";
  return "TEX|BASIS";
}

export function textureInfo(view, offset, size) {
  if (size < TEXTURE_HEADER_SIZE || view.getUint32(offset, true) !== TEXTURE_MAGIC) return null;
  return {
    version: view.getUint16(offset + 4, true),
    format: view.getUint16(offset + 6, true),
    width: view.getUint32(offset + 8, true),
    height: view.getUint32(offset + 12, true),
    mipCount: view.getUint16(offset + 16, true),
    compression: view.getUint8(offset + 18),
  };
}

export function hashHex(value) {
  return value.toString(16).toUpperCase().padStart(16, "0");
}

// Generated asset-id headers pair each hash with its source path:
//   #define ASSET_X ((nt_hash64_t){0x...ULL}) /* assets/path.png */
export function parseNameHeader(text) {
  const names = new Map();
  const pattern = /\{0x([0-9A-Fa-f]+)ULL\}\)\s*\/\*\s*(.*?)\s*\*\//g;
  for (const match of String(text || "").matchAll(pattern)) {
    names.set(match[1].toUpperCase().padStart(16, "0"), match[2]);
  }
  return names;
}

export function parseNtpack(buffer, options = {}) {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  if (bytes.length < PACK_HEADER_SIZE) throw new Error("pack file is smaller than the pack header");
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const magic = view.getUint32(0, true);
  if (magic !== NT_PACK_MAGIC) throw new Error("not an ntpack file (bad magic)");
  const header = {
    metaCount: view.getUint32(4, true),
    version: view.getUint16(8, true),
    assetCount: view.getUint16(10, true),
    headerSize: view.getUint32(12, true),
    totalSize: view.getUint32(16, true),
    checksum: view.getUint32(20, true),
    metaOffset: view.getUint32(24, true),
  };
  if (header.version !== NT_PACK_VERSION) {
    throw new Error(`unsupported pack version ${header.version} (expected ${NT_PACK_VERSION}); rebuild packs`);
  }
  const names = options.names || new Map();
  const entries = [];
  for (let index = 0; index < header.assetCount; index += 1) {
    const at = PACK_HEADER_SIZE + index * ASSET_ENTRY_SIZE;
    if (at + ASSET_ENTRY_SIZE > bytes.length) throw new Error("asset entry table exceeds file size");
    const resourceId = hashHex(view.getBigUint64(at, true));
    const offset = view.getUint32(at + 8, true);
    const size = view.getUint32(at + 12, true);
    const type = view.getUint8(at + 18);
    const inBounds = offset + size <= bytes.length;
    const entry = {
      index,
      resourceId,
      name: names.get(resourceId) || `...${resourceId.slice(-12)}`,
      named: names.has(resourceId),
      offset,
      size,
      formatVersion: view.getUint16(at + 16, true),
      type,
      typeName: ASSET_TYPE_NAMES[type] || "UNKNOWN",
      typeTag: type === 2 && inBounds ? textureFormatTag(view, offset, size) : (ASSET_TYPE_NAMES[type] || "UNKNOWN"),
      metaOffset: view.getUint32(at + 20, true),
      inBounds,
    };
    if (options.gzip !== false && inBounds && size > 0) {
      entry.gzBytes = gzipSync(bytes.subarray(offset, offset + size)).length;
    }
    entries.push(entry);
  }
  const byLocation = new Map();
  for (const entry of entries) {
    const key = `${entry.offset}:${entry.size}`;
    if (byLocation.has(key)) entry.dupOfIndex = byLocation.get(key);
    else byLocation.set(key, entry.index);
  }
  const summary = {};
  for (const entry of entries) {
    const key = entry.typeName;
    const row = summary[key] || (summary[key] = { count: 0, bytes: 0, dupCount: 0 });
    row.count += 1;
    if (entry.dupOfIndex == null) row.bytes += entry.size;
    else row.dupCount += 1;
  }
  return { header, entries, summary, fileBytes: bytes.length };
}
