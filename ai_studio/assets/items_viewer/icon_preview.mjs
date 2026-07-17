// Icon preview — parses a BUILT template/game asset pack (game.ntpack) + its
// debug-PNG atlas page + the generated game_assets.h to produce pixel-rect crops
// for the items-viewer's item cards. Pure, no HTTP, no subprocess -- a small,
// version-guarded binary reader over the engine's ntpack/atlas formats
// (nt_pack_format.h, nt_atlas_format.h). Deliberately NOT the engine's runtime
// atlas reader (nt_atlas_find_region/nt_atlas_get_region) -- that would need a
// native studio target; the current module keeps this code in the JS tool layer.
import { existsSync, readdirSync } from "node:fs";
import { open, readFile, stat } from "node:fs/promises";
import { join } from "node:path";

// Presets tried in order (spec §3b). game_assets.h is written to ONE shared
// path regardless of which preset triggered the build (CMakeLists' HEADER_DIR
// is source-tree-relative, not binary-dir-relative), so its presence only
// means "some build ran"; the per-preset game.ntpack under build/<preset>/pack/
// is what actually decides which preset's pack gets read.
export const ICON_PRESETS = ["native-debug", "devapi-debug"];

const NT_PACK_MAGIC = 0x4b41504e; // "NPAK" LE (nt_pack_format.h)
const NT_PACK_VERSION = 2;
const NT_ATLAS_MAGIC = 0x534c5441; // "ATLS" LE (nt_atlas_format.h)
const NT_ATLAS_VERSION = 6;
const NT_ASSET_ATLAS = 6; // nt_asset_type_t

const PACK_HEADER_SIZE = 32; // NtPackHeader
const ASSET_ENTRY_SIZE = 24; // NtAssetEntry
const ATLAS_HEADER_SIZE = 28; // NtAtlasHeader
const ATLAS_REGION_SIZE = 48; // NtAtlasRegion (v6)
const ATLAS_VERTEX_SIZE = 8; // NtAtlasVertex

export const ICON_LIMITS = Object.freeze({
  maxPackBytes: 64 * 1024 * 1024,
  maxHeaderBytes: 4 * 1024 * 1024,
  maxPngBytes: 16 * 1024 * 1024,
  maxAssetCount: 4096,
  maxAtlasCount: 64,
  maxPageCount: 64,
  maxRegionCount: 10_000,
  maxRegionVertices: 16,
  maxVertexCount: 160_000,
  maxImageDimension: 8192,
  maxImagePixels: 32 * 1024 * 1024,
});

// Parse NtPackHeader + NtAssetEntry[] and, for EVERY asset_type==NT_ASSET_ATLAS
// entry (spec §3b step 2 -- the template pack has TWO atlases, `ui` then
// `icons`; stopping at the first entry would silently drop every icon region),
// the NtAtlasRegion[]/NtAtlasVertex[] rect data. Version-assert FIRST (spec §3b
// step 0): a pack/atlas version this parser was not written against returns a
// `reason` instead of guessing at a since-changed byte layout.
export function parsePackBuffer(buf) {
  if (buf.length < PACK_HEADER_SIZE) return { reason: "pack file too small to contain a header" };
  const dv = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);

  const magic = dv.getUint32(0, true);
  const packVersion = dv.getUint16(8, true);
  const assetCount = dv.getUint16(10, true);
  if (magic !== NT_PACK_MAGIC) return { reason: "pack format newer than viewer (bad pack magic)" };
  if (packVersion !== NT_PACK_VERSION) {
    return { reason: `pack format newer than viewer (pack v${packVersion}, viewer supports v${NT_PACK_VERSION})` };
  }
  if (assetCount > ICON_LIMITS.maxAssetCount) return { reason: "pack asset count exceeds viewer cap" };
  if (PACK_HEADER_SIZE + assetCount * ASSET_ENTRY_SIZE > buf.length) {
    return { reason: "pack asset table exceeds file bounds" };
  }

  const atlasOffsets = [];
  for (let i = 0; i < assetCount; i++) {
    const off = PACK_HEADER_SIZE + i * ASSET_ENTRY_SIZE;
    if (dv.getUint8(off + 18) !== NT_ASSET_ATLAS) continue;
    atlasOffsets.push(dv.getUint32(off + 8, true));
  }
  if (atlasOffsets.length > ICON_LIMITS.maxAtlasCount) return { reason: "pack atlas count exceeds viewer cap" };
  if (atlasOffsets.length === 0) return { reason: "pack has no atlas assets" };

  const regionsByHash = new Map();
  let totalVertices = 0;
  for (const base of atlasOffsets) {
    if (base + ATLAS_HEADER_SIZE > buf.length) return { reason: "atlas header exceeds file bounds" };
    const atlasMagic = dv.getUint32(base, true);
    const atlasVersion = dv.getUint16(base + 4, true);
    if (atlasMagic !== NT_ATLAS_MAGIC || atlasVersion !== NT_ATLAS_VERSION) {
      return { reason: `pack format newer than viewer (pack v${packVersion}, atlas v${atlasVersion})` };
    }
    const regionCount = dv.getUint16(base + 6, true);
    const pageCount = dv.getUint16(base + 8, true);
    if (pageCount > ICON_LIMITS.maxPageCount) return { reason: "atlas page count exceeds viewer cap" };
    if (regionCount > ICON_LIMITS.maxRegionCount) return { reason: "atlas region count exceeds viewer cap" };
    const vertexOffset = dv.getUint32(base + 12, true);
    const regionsStart = base + ATLAS_HEADER_SIZE + pageCount * 8; // skip texture_resource_ids[page_count]
    if (regionsStart + regionCount * ATLAS_REGION_SIZE > buf.length) {
      return { reason: "atlas region section exceeds file bounds" };
    }
    for (let r = 0; r < regionCount; r++) {
      const rOff = regionsStart + r * ATLAS_REGION_SIZE;
      // BigInt ALWAYS: name_hash is a 64-bit xxh64, well past
      // Number.MAX_SAFE_INTEGER (spec §3b step 5) -- getBigUint64 is the only
      // lossless read; Number()/parseInt() would silently truncate and the
      // name<->hash join below would silently miss.
      const nameHash = dv.getBigUint64(rOff, true);
      const vertexStart = dv.getUint32(rOff + 24, true);
      const vertexCount = dv.getUint8(rOff + 32);
      if (vertexCount < 1 || vertexCount > ICON_LIMITS.maxRegionVertices) {
        return { reason: "atlas region vertex count exceeds viewer cap" };
      }
      totalVertices += vertexCount;
      if (totalVertices > ICON_LIMITS.maxVertexCount) return { reason: "atlas vertex count exceeds viewer cap" };
      const pageIndex = dv.getUint8(rOff + 33);
      let minU = Infinity;
      let minV = Infinity;
      let maxU = -Infinity;
      let maxV = -Infinity;
      for (let v = 0; v < vertexCount; v++) {
        const vOff = base + vertexOffset + (vertexStart + v) * ATLAS_VERTEX_SIZE;
        if (vOff + ATLAS_VERTEX_SIZE > buf.length) return { reason: "atlas vertex section exceeds file bounds" };
        const u = dv.getUint16(vOff + 4, true);
        const vv = dv.getUint16(vOff + 6, true);
        if (u < minU) minU = u;
        if (u > maxU) maxU = u;
        if (vv < minV) minV = vv;
        if (vv > maxV) maxV = vv;
      }
      // allow_transform=false (build_packs.c icons_opts) -> transform==0,
      // vertex_count==4, an axis-aligned rect -- min/max UV IS the crop rect.
      regionsByHash.set(nameHash, { minU, minV, maxU, maxV, pageIndex });
    }
  }
  return { regionsByHash };
}

// Parse game_assets.h's `#define ASSET_ATLAS_REGION_* ((nt_hash64_t){0x...ULL})
// /* icons/name */` lines (same shape nt_builder_codegen.c:228 writes; the dump
// tool's parse_header_file, nt_builder_dump.c:22-59, reads the identical
// pattern), filtered to the `icons/` atlas (spec §3b step 4 -- `ui/*` regions
// live in the SAME header and must not leak into the icon preview). The
// macro's hash and NtAtlasRegion.name_hash both hash ONLY the sprite name (e.g.
// "gold"), never the full "icons/gold" path (nt_builder_atlas.c:1943) -- the
// join below is correct by construction; the map is keyed by the full comment
// path because that string IS the item.icon contract ("icons/gold").
const HDR_DEFINE_RE = /#define\s+ASSET_ATLAS_REGION_\S+\s+\(\(nt_hash64_t\)\{0x([0-9A-Fa-f]+)ULL\}\)\s*\/\*\s*(\S+)\s*\*\//g;

export function parseIconRegionNames(hdrText) {
  const nameToHash = new Map();
  for (const m of hdrText.matchAll(HDR_DEFINE_RE)) {
    const [, hex, path] = m;
    if (!path.startsWith("icons/")) continue;
    if (nameToHash.size >= ICON_LIMITS.maxRegionCount) {
      throw new Error("icon region name count exceeds viewer cap");
    }
    nameToHash.set(path, BigInt(`0x${hex}`));
  }
  return nameToHash;
}

// PNG IHDR width/height (big-endian per the PNG spec, bytes 16-23) -- reading
// two 4-byte integers directly avoids pulling in an image-decoding dependency
// just for page dimensions (spec §3b step 3).
export function readPngDims(buf) {
  if (buf.length < 24 || buf.readUInt32BE(0) !== 0x89504e47 || buf.toString("ascii", 12, 16) !== "IHDR") {
    throw new Error("icon page is not a valid PNG header");
  }
  const width = buf.readUInt32BE(16);
  const height = buf.readUInt32BE(20);
  if (
    width < 1 || height < 1
    || width > ICON_LIMITS.maxImageDimension || height > ICON_LIMITS.maxImageDimension
    || width * height > ICON_LIMITS.maxImagePixels
  ) {
    throw new Error("icon page dimensions exceed viewer cap");
  }
  return { width, height };
}

function findDebugPagePng(packDir) {
  // Tolerant glob (spec §3b): debug-PNG naming is `<atlas-name>_page<N>.png`
  // (nt_builder_atlas.c:1539) -- match anything "icons*...page*.png" rather
  // than hardcoding "icons_page0.png" exactly.
  let files;
  try {
    files = readdirSync(packDir);
  } catch {
    return null;
  }
  const matches = files.filter((f) => /^icons.*page.*\.png$/i.test(f)).sort();
  return matches.length ? join(packDir, matches[0]) : null;
}

// Locate the first preset (spec order: native-debug, devapi-debug) with BOTH a
// built pack and the generated header. Honest, DISTINCT reasons (spec §3b
// "различать причины деградации"): no preset has a pack (or the header was
// never generated) is a different problem than "pack+header exist but the
// debug-PNG page is missing" (e.g. debug_png got turned off) -- the caller
// should not have to guess which one happened from a single generic message.
function findPackArtifacts(folderAbs) {
  const hdrPath = join(folderAbs, "src", "generated", "game_assets.h");
  const hdrExists = existsSync(hdrPath);
  for (const preset of ICON_PRESETS) {
    const packDir = join(folderAbs, "build", preset, "pack");
    const packPath = join(packDir, "game.ntpack");
    if (!existsSync(packPath) || !hdrExists) continue;
    const pngPath = findDebugPagePng(packDir);
    if (!pngPath) return { reason: "atlas built but page PNG missing (debug_png off?)" };
    return { packPath, hdrPath, pngPath };
  }
  return { reason: `pack not built (cmake --build ${join(folderAbs, "build", ICON_PRESETS[0])})` };
}

function degraded(reason) {
  return { page_available: false, page_w: 0, page_h: 0, regions: {}, reason };
}

async function readBounded(path, maxBytes, label) {
  const info = await stat(path);
  if (info.size > maxBytes) throw new Error(`${label} file exceeds ${maxBytes} byte cap`);
  const data = await readFile(path);
  if (data.length > maxBytes) throw new Error(`${label} file exceeds ${maxBytes} byte cap`);
  return data;
}

async function readPngMetadata(path) {
  const info = await stat(path);
  if (info.size > ICON_LIMITS.maxPngBytes) {
    throw new Error(`icon page file exceeds ${ICON_LIMITS.maxPngBytes} byte cap`);
  }
  const handle = await open(path, "r");
  try {
    const header = Buffer.alloc(24);
    const { bytesRead } = await handle.read(header, 0, header.length, 0);
    if (bytesRead !== header.length) throw new Error("icon page is truncated");
    return readPngDims(header);
  } finally {
    await handle.close();
  }
}

// debug_png draws a 2px magenta {255,0,255,255} outline AT the region boundary
// (nt_builder_atlas.c:245-278, landing in the extrude gutter for these atlas
// opts) -- a crop of the raw rect would show that outline as a magenta ring
// around every icon. A 2px inner inset is one of the two mitigations the spec
// allows (the other: canvas key-out of exact 0xFF00FF); inset was chosen here
// because it needs no per-pixel canvas work in the page (spec §5/§8).
const DEBUG_OUTLINE_INSET = 2;

// The view.icons contract: bounded page metadata plus a name->rect map, or a
// `reason` when the pack is not in a previewable state. PNG bytes are loaded
// only by the focused icon-page route, never embedded in catalog JSON.
// Never throws -- any failure degrades to `reason` (mirrors ops.mjs's
// readLockRaw: a broken icon preview must not turn the whole catalog view into
// a 500).
export async function buildIconPreview(folderAbs) {
  const found = findPackArtifacts(folderAbs);
  if (found.reason) return degraded(found.reason);

  try {
    const [packBuf, hdrBuf, dimensions] = await Promise.all([
      readBounded(found.packPath, ICON_LIMITS.maxPackBytes, "pack"),
      readBounded(found.hdrPath, ICON_LIMITS.maxHeaderBytes, "asset header"),
      readPngMetadata(found.pngPath),
    ]);
    const parsed = parsePackBuffer(packBuf);
    if (parsed.reason) return degraded(parsed.reason);

    const hdrText = hdrBuf.toString("utf8");
    const nameToHash = parseIconRegionNames(hdrText);
    const { width: pageW, height: pageH } = dimensions;

    const regions = {};
    for (const [path, hash] of nameToHash) {
      const region = parsed.regionsByHash.get(hash);
      // page_index is READ, not assumed (spec §3b step 3): a region packed
      // onto a later page than the single page loaded here has no rect in
      // THIS preview and is left out -- an honest miss (placeholder in the
      // UI), not a crash or a wrong crop.
      if (!region || region.pageIndex !== 0) continue;
      const x0 = Math.round((region.minU / 65535) * pageW);
      const y0 = Math.round((region.minV / 65535) * pageH);
      const x1 = Math.round((region.maxU / 65535) * pageW);
      const y1 = Math.round((region.maxV / 65535) * pageH);
      regions[path] = {
        x: x0 + DEBUG_OUTLINE_INSET,
        y: y0 + DEBUG_OUTLINE_INSET,
        w: x1 - x0 - DEBUG_OUTLINE_INSET * 2,
        h: y1 - y0 - DEBUG_OUTLINE_INSET * 2,
        page_index: region.pageIndex,
      };
    }

    return {
      page_available: true,
      page_w: pageW,
      page_h: pageH,
      regions,
    };
  } catch (err) {
    return degraded(`icon preview failed to parse pack: ${err.message}`);
  }
}

export async function loadIconPage(folderAbs) {
  const found = findPackArtifacts(folderAbs);
  if (found.reason) return { reason: found.reason };
  try {
    const data = await readBounded(found.pngPath, ICON_LIMITS.maxPngBytes, "icon page");
    readPngDims(data);
    return { data };
  } catch (error) {
    return { reason: `icon preview failed to read page: ${error.message}` };
  }
}
