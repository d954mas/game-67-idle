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
    unsupported: view.getUint16(offset + 4, true) !== 3 || undefined,
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

const ATLAS_MAGIC = 0x534c5441; // "ATLS"
const ATLAS_HEADER_SIZE = 28;
const ATLAS_REGION_SIZE = 48;
const FONT_MAGIC = 0x544e4f46; // "FONT"
const FONT_HEADER_SIZE = 24;
const FONT_GLYPH_ENTRY_SIZE = 24;

export function atlasInfo(view, offset, size, names = new Map()) {
  if (size < ATLAS_HEADER_SIZE || view.getUint32(offset, true) !== ATLAS_MAGIC) return null;
  const header = {
    version: view.getUint16(offset + 4, true),
    regionCount: view.getUint16(offset + 6, true),
    pageCount: view.getUint16(offset + 8, true),
    vertexOffset: view.getUint32(offset + 12, true),
    totalVertexCount: view.getUint32(offset + 16, true),
    indexOffset: view.getUint32(offset + 20, true),
    totalIndexCount: view.getUint32(offset + 24, true),
  };
  if (header.version !== 7) return { ...header, unsupported: true, pages: [], regions: [], vertices: [] };
  const pages = [];
  let cursor = offset + ATLAS_HEADER_SIZE;
  for (let page = 0; page < header.pageCount; page += 1) {
    if (cursor + 8 > offset + size) break;
    const id = hashHex(view.getBigUint64(cursor, true));
    pages.push({ resourceId: id, name: names.get(id) || "" });
    cursor += 8;
  }
  const regions = [];
  for (let index = 0; index < header.regionCount; index += 1) {
    if (cursor + ATLAS_REGION_SIZE > offset + size) break;
    const nameHash = hashHex(view.getBigUint64(cursor, true));
    regions.push({
      name: names.get(nameHash) || `...${nameHash.slice(-12)}`,
      sourceW: view.getUint16(cursor + 8, true),
      sourceH: view.getUint16(cursor + 10, true),
      trimX: view.getInt16(cursor + 12, true),
      trimY: view.getInt16(cursor + 14, true),
      originX: view.getFloat32(cursor + 16, true),
      originY: view.getFloat32(cursor + 20, true),
      vertexStart: view.getUint32(cursor + 24, true),
      indexStart: view.getUint32(cursor + 28, true),
      vertexCount: view.getUint8(cursor + 32),
      pageIndex: view.getUint8(cursor + 33),
      transform: view.getUint8(cursor + 34),
      indexCount: view.getUint8(cursor + 35),
      slice9: [38, 40, 42, 44].map((at) => view.getUint16(cursor + at, true)),
    });
    cursor += ATLAS_REGION_SIZE;
  }
  const vertices = [];
  const vertexBase = offset + header.vertexOffset;
  for (let index = 0; index < header.totalVertexCount; index += 1) {
    const at = vertexBase + index * 8;
    if (at + 8 > offset + size) break;
    vertices.push({
      x: view.getInt16(at, true),
      y: view.getInt16(at + 2, true),
      u: view.getUint16(at + 4, true) / 65535,
      v: view.getUint16(at + 6, true) / 65535,
    });
  }
  return { ...header, pages, regions, vertices };
}

// v5 contour decode: bitmask of on-curve flags, absolute first point, varlen
// deltas after it (0x80 sentinel switches one delta to int16).
function decodeGlyphContours(view, base, end) {
  if (base + 2 > end) return [];
  const contours = [];
  let cursor = base;
  const contourCount = view.getUint16(cursor, true);
  cursor += 2;
  for (let contour = 0; contour < contourCount; contour += 1) {
    if (cursor + 2 > end) break;
    const pointCount = view.getUint16(cursor, true);
    cursor += 2;
    // NT_FONT_MAX_POINTS_PER_CONTOUR guards against a corrupt count walking
    // past the buffer.
    if (pointCount > 4096) break;
    const maskBytes = (Math.ceil(pointCount / 8) + 1) & ~1;
    const maskAt = cursor;
    if (maskAt + maskBytes > end) break;
    cursor += maskBytes;
    const points = [];
    let x = 0;
    let y = 0;
    for (let point = 0; point < pointCount; point += 1) {
      if (point === 0) {
        if (cursor + 4 > end) return contours;
        x = view.getInt16(cursor, true);
        y = view.getInt16(cursor + 2, true);
        cursor += 4;
      } else {
        for (const axis of ["x", "y"]) {
          if (cursor + 1 > end) return contours;
          const head = view.getUint8(cursor);
          let delta;
          if (head === 0x80) {
            if (cursor + 3 > end) return contours;
            delta = view.getInt16(cursor + 1, true);
            cursor += 3;
          } else {
            delta = view.getInt8(cursor);
            cursor += 1;
          }
          if (axis === "x") x += delta;
          else y += delta;
        }
      }
      const maskByte = view.getUint8(maskAt + (point >> 3));
      points.push({ x, y, on: (maskByte >> (point & 7)) & 1 ? 1 : 0 });
    }
    contours.push(points);
  }
  return contours;
}

export function fontInfo(view, offset, size, options = {}) {
  if (size < FONT_HEADER_SIZE || view.getUint32(offset, true) !== FONT_MAGIC) return null;
  const header = {
    version: view.getUint16(offset + 4, true),
    glyphCount: view.getUint16(offset + 6, true),
    unitsPerEm: view.getUint16(offset + 8, true),
    ascent: view.getInt16(offset + 10, true),
    descent: view.getInt16(offset + 12, true),
    lineGap: view.getInt16(offset + 14, true),
  };
  if (header.version !== 5) return { ...header, unsupported: true, glyphs: [], charset: "" };
  const end = offset + size;
  const glyphs = [];
  const withContours = options.contours !== false;
  for (let index = 0; index < header.glyphCount; index += 1) {
    const at = offset + FONT_HEADER_SIZE + index * FONT_GLYPH_ENTRY_SIZE;
    if (at + FONT_GLYPH_ENTRY_SIZE > end) break;
    const glyph = {
      codepoint: view.getUint32(at, true),
      advance: view.getInt16(at + 8, true),
      bbox: [
        view.getInt16(at + 10, true),
        view.getInt16(at + 12, true),
        view.getInt16(at + 14, true),
        view.getInt16(at + 16, true),
      ],
      curveCount: view.getUint16(at + 18, true),
    };
    if (withContours) {
      const dataOffset = view.getUint32(at + 4, true);
      const kernCount = view.getUint16(at + 20, true);
      glyph.contours = decodeGlyphContours(view, offset + dataOffset + kernCount * 4, end);
    }
    glyphs.push(glyph);
  }
  const charset = glyphs
    .map((glyph) => glyph.codepoint)
    .filter((cp) => cp >= 0x20)
    .map((cp) => String.fromCodePoint(cp))
    .join("");
  return { ...header, glyphs, charset };
}

export function entryDetail(buffer, entry, names = new Map()) {
  const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  if (!entry || entry.offset + entry.size > bytes.length) return null;
  if (entry.type === 2) return { kind: "texture", texture: textureInfo(view, entry.offset, entry.size) };
  if (entry.type === 6) return { kind: "atlas", atlas: atlasInfo(view, entry.offset, entry.size, names) };
  if (entry.type === 5) return { kind: "font", font: fontInfo(view, entry.offset, entry.size) };
  return { kind: "binary" };
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
    entries.push(entry);
  }
  const byLocation = new Map();
  for (const entry of entries) {
    const key = `${entry.offset}:${entry.size}`;
    if (byLocation.has(key)) entry.dupOfIndex = byLocation.get(key);
    else byLocation.set(key, entry.index);
  }
  // gzip AFTER dedup, once per unique location, with a total budget: a crafted
  // pack whose entries all alias the whole file must not hang the server.
  if (options.gzip !== false) {
    const gzByLocation = new Map();
    let budget = 256 * 1024 * 1024;
    for (const entry of entries) {
      if (!entry.inBounds || entry.size === 0) continue;
      const key = `${entry.offset}:${entry.size}`;
      if (!gzByLocation.has(key)) {
        if (entry.size > budget) {
          gzByLocation.set(key, null);
          continue;
        }
        budget -= entry.size;
        gzByLocation.set(key, gzipSync(bytes.subarray(entry.offset, entry.offset + entry.size)).length);
      }
      const gz = gzByLocation.get(key);
      if (gz != null) entry.gzBytes = gz;
      else entry.gzSkipped = true;
    }
  }
  const summary = {};
  for (const entry of entries) {
    const key = entry.typeName;
    const row = summary[key] || (summary[key] = { count: 0, bytes: 0, dupCount: 0 });
    row.count += 1;
    if (entry.dupOfIndex == null) row.bytes += entry.size;
    else row.dupCount += 1;
  }
  return {
    header,
    entries,
    summary,
    fileBytes: bytes.length,
    truncated: header.totalSize !== bytes.length,
  };
}
