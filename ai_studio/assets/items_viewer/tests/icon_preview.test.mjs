// icon_preview.mjs tests (T0316 spec §3b/§5/§6).
// Run: node --test ai_studio/assets/items_viewer/tests/
//
// Fixture (spec §5): a REAL two-atlas pack (`ui` + `icons`) captured from a
// native-debug build -- tests/fixtures/icon_pack/{game.ntpack,icons_page0.png,
// game_assets.h.slice} -- copied into a throwaway temp folder per test so the
// module's own preset/file-layout lookup (build/<preset>/pack/...,
// src/generated/game_assets.h) is exercised unmodified, exactly as it runs
// against templates/template.
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, copyFileSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { inflateSync } from "node:zlib";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildIconPreview, parseIconRegionNames, parsePackBuffer, readPngDims } from "../icon_preview.mjs";

const FIXTURE_DIR = fileURLToPath(new URL("./fixtures/icon_pack", import.meta.url)).replace(/[\\/]$/, "");

function tempFixtureDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "items-viewer-icon-preview-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

// Lays out a throwaway catalog folder exactly like templates/template:
// build/<preset>/pack/{game.ntpack,icons_page0.png} + src/generated/game_assets.h
// -- copied from the real fixture, never synthesized, per spec §5.
function layoutFixture(dir, { preset = "native-debug", withPack = true, withPng = true, withHdr = true } = {}) {
  const packDir = join(dir, "build", preset, "pack");
  mkdirSync(packDir, { recursive: true });
  if (withPack) copyFileSync(join(FIXTURE_DIR, "game.ntpack"), join(packDir, "game.ntpack"));
  if (withPng) copyFileSync(join(FIXTURE_DIR, "icons_page0.png"), join(packDir, "icons_page0.png"));
  if (withHdr) {
    const genDir = join(dir, "src", "generated");
    mkdirSync(genDir, { recursive: true });
    copyFileSync(join(FIXTURE_DIR, "game_assets.h.slice"), join(genDir, "game_assets.h"));
  }
}

const ICON_NAMES = ["gold", "xp", "energy", "potion", "sword", "wood"];

test("buildIconPreview: real two-atlas fixture resolves all 6 icons/* regions, ui/* never leaks in", (t) => {
  const dir = tempFixtureDir(t);
  layoutFixture(dir);

  const preview = buildIconPreview(dir);
  assert.equal(preview.reason, undefined);
  assert.equal(preview.page_w, 512);
  assert.equal(preview.page_h, 256);
  assert.ok(preview.page_data_uri.startsWith("data:image/png;base64,"));

  const keys = Object.keys(preview.regions).sort();
  assert.deepEqual(keys, ICON_NAMES.map((n) => `icons/${n}`).sort());
  for (const name of ICON_NAMES) {
    const region = preview.regions[`icons/${name}`];
    assert.equal(region.page_index, 0);
    // Every region is a positive-area rect strictly inside the 512x256 page
    // (the 2px debug-outline inset, spec §5/§8, is already applied).
    assert.ok(region.x > 0 && region.y > 0, `${name}: inset applied`);
    assert.ok(region.w > 20 && region.h > 20, `${name}: sane crop size`);
    assert.ok(region.x + region.w <= preview.page_w && region.y + region.h <= preview.page_h, `${name}: inside page bounds`);
  }
  // ui/* regions exist in the SAME header (game_assets.h.slice carries both
  // sections) but must never surface in the icons preview (spec §3b step 4 /
  // §8 "ДВА атласа" risk).
  for (const key of keys) assert.ok(key.startsWith("icons/"), `${key} must not be a ui/* leak`);
});

test("buildIconPreview: no build at all -> reason distinguishes 'pack not built'", (t) => {
  const dir = tempFixtureDir(t); // empty -- no build/, no src/generated/
  const preview = buildIconPreview(dir);
  assert.equal(preview.page_data_uri, null);
  assert.deepEqual(preview.regions, {});
  assert.match(preview.reason, /pack not built/);
});

test("buildIconPreview: pack + header built but debug PNG missing -> a DISTINCT reason from 'pack not built'", (t) => {
  const dir = tempFixtureDir(t);
  layoutFixture(dir, { withPng: false });
  const preview = buildIconPreview(dir);
  assert.deepEqual(preview.regions, {});
  assert.match(preview.reason, /page PNG missing/);
  assert.doesNotMatch(preview.reason, /pack not built/, "must not be conflated with the 'pack not built' reason");
});

test("buildIconPreview: devapi-debug preset resolves too when native-debug has no pack (preset order fallback)", (t) => {
  const dir = tempFixtureDir(t);
  layoutFixture(dir, { preset: "devapi-debug" });
  const preview = buildIconPreview(dir);
  assert.equal(preview.reason, undefined);
  assert.equal(Object.keys(preview.regions).length, 6);
});

test("parsePackBuffer: version-assert FIRST -- a pack version mismatch is caught before touching the entry table", () => {
  const realBuf = readFileSync(join(FIXTURE_DIR, "game.ntpack"));
  const mutated = Buffer.from(realBuf); // copy -- never mutate the fixture buffer in place
  mutated.writeUInt16LE(99, 8); // NtPackHeader.version offset (nt_pack_format.h:60)
  const result = parsePackBuffer(mutated);
  assert.match(result.reason, /pack format newer than viewer/);
  assert.match(result.reason, /pack v99/);
});

test("parsePackBuffer: bad magic is rejected, not silently parsed as garbage", () => {
  const realBuf = readFileSync(join(FIXTURE_DIR, "game.ntpack"));
  const mutated = Buffer.from(realBuf);
  mutated.writeUInt32LE(0, 0); // corrupt NT_PACK_MAGIC
  const result = parsePackBuffer(mutated);
  assert.match(result.reason, /bad pack magic/);
});

test("parseIconRegionNames: BigInt hashes, icons/* only, ui/* filtered out of the SAME header text", () => {
  const hdrText = readFileSync(join(FIXTURE_DIR, "game_assets.h.slice"), "utf8");
  assert.ok(hdrText.includes("ASSET_ATLAS_REGION_UI_"), "fixture header carries both sections (sanity check on the fixture itself)");

  const nameToHash = parseIconRegionNames(hdrText);
  assert.deepEqual([...nameToHash.keys()].sort(), ICON_NAMES.map((n) => `icons/${n}`).sort());
  for (const hash of nameToHash.values()) {
    assert.equal(typeof hash, "bigint", "name_hash must be BigInt -- Number() silently loses precision above 2^53 (spec §3b step 5)");
    assert.ok(hash > 0xffffffffn, "a real xxh64 hash exceeds 32 bits -- a truncated/Number-cast hash would never legitimately be this large");
  }
});

test("readPngDims: matches the real fixture's known page size (512x256)", () => {
  const buf = readFileSync(join(FIXTURE_DIR, "icons_page0.png"));
  assert.deepEqual(readPngDims(buf), { width: 512, height: 256 });
});

// --- straight-alpha regression: the debug PNG must never be treated as
// premultiplied (spec §5/§8's "previous version required un-premultiply — this
// was a bug, fixed"). icon_preview.mjs performs NO pixel math at all -- proven
// here by asserting the returned page_data_uri round-trips the fixture PNG
// byte-for-byte -- and the fixture itself carries at least one genuinely
// semi-transparent (antialiased-edge) pixel inside a resolved icon's crop rect,
// so "no processing happened" is a meaningful claim, not a vacuous one.

// Minimal 8-bit RGBA (colorType 6), non-interlaced PNG decoder -- test-only.
// stb_image_write (the builder's debug-PNG writer, nt_builder_atlas.c) never
// emits anything else, so this is sufficient to inspect the fixture's raw
// alpha channel without depending on a canvas/DOM stack in Node.
function decodePngRgba(buf) {
  assert.equal(buf.readUInt32BE(0), 0x89504e47, "not a PNG signature");
  let offset = 8;
  let width = 0;
  let height = 0;
  let bitDepth = 0;
  let colorType = 0;
  const idatChunks = [];
  while (offset < buf.length) {
    const len = buf.readUInt32BE(offset);
    const type = buf.toString("ascii", offset + 4, offset + 8);
    const data = buf.subarray(offset + 8, offset + 8 + len);
    if (type === "IHDR") {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      bitDepth = data.readUInt8(8);
      colorType = data.readUInt8(9);
    } else if (type === "IDAT") {
      idatChunks.push(data);
    } else if (type === "IEND") {
      break;
    }
    offset += 8 + len + 4;
  }
  assert.equal(bitDepth, 8, "decoder only handles 8-bit PNGs");
  assert.equal(colorType, 6, "decoder only handles RGBA PNGs");

  const raw = inflateSync(Buffer.concat(idatChunks));
  const bpp = 4;
  const stride = width * bpp;
  const pixels = new Uint8Array(width * height * bpp);
  let prevRow = new Uint8Array(stride);
  let rawOffset = 0;
  for (let y = 0; y < height; y++) {
    const filterType = raw[rawOffset++];
    const row = raw.subarray(rawOffset, rawOffset + stride);
    rawOffset += stride;
    const outRow = new Uint8Array(stride);
    for (let x = 0; x < stride; x++) {
      const a = x >= bpp ? outRow[x - bpp] : 0;
      const b = prevRow[x];
      const c = x >= bpp ? prevRow[x - bpp] : 0;
      let value = row[x];
      if (filterType === 1) value = (value + a) & 0xff;
      else if (filterType === 2) value = (value + b) & 0xff;
      else if (filterType === 3) value = (value + Math.floor((a + b) / 2)) & 0xff;
      else if (filterType === 4) {
        const p = a + b - c;
        const pa = Math.abs(p - a);
        const pb = Math.abs(p - b);
        const pc = Math.abs(p - c);
        const pred = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
        value = (value + pred) & 0xff;
      } else if (filterType !== 0) {
        assert.fail(`unsupported PNG filter type ${filterType}`);
      }
      outRow[x] = value;
    }
    pixels.set(outRow, y * stride);
    prevRow = outRow;
  }
  return { width, height, pixels };
}

test("fixture sanity: the real icons page has at least one genuinely semi-transparent pixel inside a resolved crop rect", (t) => {
  const dir = tempFixtureDir(t);
  layoutFixture(dir);
  const preview = buildIconPreview(dir);

  const pngBuf = readFileSync(join(FIXTURE_DIR, "icons_page0.png"));
  const decoded = decodePngRgba(pngBuf);

  let sawSemiTransparent = false;
  outer: for (const [path, region] of Object.entries(preview.regions)) {
    for (let y = region.y; y < region.y + region.h && !sawSemiTransparent; y++) {
      for (let x = region.x; x < region.x + region.w; x++) {
        const alpha = decoded.pixels[(y * decoded.width + x) * 4 + 3];
        if (alpha > 0 && alpha < 255) {
          sawSemiTransparent = true;
          break outer;
        }
      }
    }
    void path;
  }
  assert.ok(sawSemiTransparent, "expected at least one antialiased (partial-alpha) edge pixel across the 6 icon crops");
});

test("buildIconPreview: page_data_uri is a byte-exact passthrough of the source PNG -- no alpha/pixel processing happens server-side", (t) => {
  const dir = tempFixtureDir(t);
  layoutFixture(dir);
  const preview = buildIconPreview(dir);

  const expected = readFileSync(join(FIXTURE_DIR, "icons_page0.png"));
  const actualBase64 = preview.page_data_uri.slice("data:image/png;base64,".length);
  const actual = Buffer.from(actualBase64, "base64");
  assert.ok(expected.equals(actual), "page_data_uri must be the exact source PNG bytes -- any RGB/alpha transform here would risk un-premultiply burning (spec §5/§8)");
});
