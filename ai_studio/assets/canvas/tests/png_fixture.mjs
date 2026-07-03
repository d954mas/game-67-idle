// Tiny dependency-free PNG encoder for tests. node:zlib provides deflate; CRC32
// is a small inline table. Produces 8-bit RGB PNGs whose header bytes match the
// store's pure imageSize() parser, and whose flat-magenta background + solid
// colored blobs give the raster2d detector real regions to find.
import { deflateSync, inflateSync } from "node:zlib";

const CRC_TABLE = (() => {
  const table = new Uint32Array(256);
  for (let n = 0; n < 256; n += 1) {
    let c = n;
    for (let k = 0; k < 8; k += 1) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    table[n] = c >>> 0;
  }
  return table;
})();

function crc32(buffer) {
  let c = 0xffffffff;
  for (let i = 0; i < buffer.length; i += 1) c = CRC_TABLE[(c ^ buffer[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function chunk(type, data) {
  const body = Buffer.concat([Buffer.from(type, "ascii"), data]);
  const length = Buffer.alloc(4);
  length.writeUInt32BE(data.length, 0);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body), 0);
  return Buffer.concat([length, body, crc]);
}

// pixel(x, y) => [r, g, b] (RGB) or [r, g, b, a] with {alpha: true} (RGBA)
export function encodePng(width, height, pixel, { alpha = false } = {}) {
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  const channels = alpha ? 4 : 3;
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = alpha ? 6 : 2; // color type: truecolor / truecolor+alpha
  const raw = Buffer.alloc(height * (1 + width * channels));
  let p = 0;
  for (let y = 0; y < height; y += 1) {
    raw[p] = 0; // filter type: none
    p += 1;
    for (let x = 0; x < width; x += 1) {
      const value = pixel(x, y);
      raw[p] = value[0];
      raw[p + 1] = value[1];
      raw[p + 2] = value[2];
      if (alpha) raw[p + 3] = value.length > 3 ? value[3] : 255;
      p += channels;
    }
  }
  const idat = deflateSync(raw);
  return Buffer.concat([
    signature,
    chunk("IHDR", ihdr),
    chunk("IDAT", idat),
    chunk("IEND", Buffer.alloc(0)),
  ]);
}

// A 64x48 sheet: flat magenta (#ff00ff) background with two solid colored blobs
// (each well above the 256px min area), placed away from the border so the
// border-connected background normalization keeps them intact.
export function magentaSheetPng() {
  const width = 64;
  const height = 48;
  const blobs = [
    { x0: 8, y0: 8, x1: 28, y1: 28, color: [220, 40, 40] },
    { x0: 36, y0: 16, x1: 56, y1: 40, color: [40, 180, 60] },
  ];
  return encodePng(width, height, (x, y) => {
    for (const b of blobs) {
      if (x >= b.x0 && x < b.x1 && y >= b.y0 && y < b.y1) return b.color;
    }
    return [255, 0, 255];
  });
}

// A 64x48 "soft glow" sheet: magenta background with a blob whose color sits INSIDE the
// router's partial-alpha band (distance 40 of tolerance 80 from the key => alpha 0.5, no
// opaque core). soft_metrics scores it 1.0 soft => route_cutout deterministically routes
// to dual_plate, which the canvas alpha op refuses on method=auto.
export function softGlowPng() {
  return encodePng(64, 48, (x, y) =>
    x >= 16 && x < 48 && y >= 12 && y < 36 ? [255, 40, 255] : [255, 0, 255],
  );
}

// A 64x48 polygon-sliced crop lookalike: magenta background + red blob like
// magentaSheetPng, but the top-left corner is a HIDDEN orange chunk (alpha 0 with
// bright RGB garbage underneath) — exactly what crop_regions leaves outside the
// polygon. The alpha op must neither resurrect it nor let it skew the key estimate.
export function slicedCropPng() {
  return encodePng(
    64,
    48,
    (x, y) => {
      if (x < 20 && y < 16) return [234, 175, 98, 0]; // hidden garbage (neighbour sprite)
      if (x >= 24 && x < 44 && y >= 16 && y < 36) return [220, 40, 40, 255]; // subject blob
      return [255, 0, 255, 255]; // magenta key background
    },
    { alpha: true },
  );
}

// A trivial solid 4x3 PNG for store round-trip tests where content does not matter.
export function solidPng(width = 4, height = 3, color = [10, 20, 30]) {
  return encodePng(width, height, () => color);
}

// A white-plate + black-plate dual-plate PAIR fixture (40x30): the SAME solid blob at the
// SAME position (offset:0, default) on a flat white background and a flat black
// background — exactly what ops.alphaDualPlate expects as its 2-element input. Hand-verified
// against dual_plate_pair_gate's math: background pixels diff to a uniform (255,255,255)
// (alpha 0, not "opaque"), blob pixels diff to (0,0,0) (alpha 1, chroma 0) — so
// inconsistent_fraction is exactly 0.0, comfortably inside PASS_FRACTION (0.05).
// A nonzero `offset` shifts the BLACK plate's blob by that many px (misaligned pair): with
// offset:10 (blob width 16), the overlap/mismatch bands push inconsistent_fraction to
// ~0.77 — well past ALIGN_FRACTION (0.20) into the gate's "regenerate" refusal.
export function dualPlatePairPng({ offset = 0 } = {}) {
  const width = 40;
  const height = 30;
  const blob = { x0: 12, y0: 8, x1: 28, y1: 22, color: [200, 60, 60] };
  const white = encodePng(width, height, (x, y) =>
    x >= blob.x0 && x < blob.x1 && y >= blob.y0 && y < blob.y1 ? blob.color : [255, 255, 255],
  );
  const black = encodePng(width, height, (x, y) =>
    x >= blob.x0 + offset && x < blob.x1 + offset && y >= blob.y0 && y < blob.y1 ? blob.color : [0, 0, 0],
  );
  return { white, black };
}

// Minimal PNG decoder for pixel assertions on render output. Handles 8-bit
// truecolor (RGB, type 2) and truecolor+alpha (RGBA, type 6), no interlace —
// which covers both this file's fixtures and PIL's renderGroup output. Reverses
// the five PNG scanline filters after zlib inflate. Returns { width, height,
// channels, at(x, y) -> [r, g, b, a] }.
function paeth(a, b, c) {
  const p = a + b - c;
  const pa = Math.abs(p - a);
  const pb = Math.abs(p - b);
  const pc = Math.abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

export function decodePng(buffer) {
  let offset = 8; // skip 8-byte signature
  let header = null;
  const idat = [];
  while (offset + 12 <= buffer.length) {
    const length = buffer.readUInt32BE(offset);
    const type = buffer.toString("ascii", offset + 4, offset + 8);
    const data = buffer.subarray(offset + 8, offset + 8 + length);
    if (type === "IHDR") {
      header = { width: data.readUInt32BE(0), height: data.readUInt32BE(4), bitDepth: data[8], colorType: data[9], interlace: data[12] };
    } else if (type === "IDAT") {
      idat.push(Buffer.from(data));
    } else if (type === "IEND") {
      break;
    }
    offset += 12 + length;
  }
  if (!header) throw new Error("decodePng: no IHDR");
  if (header.bitDepth !== 8 || ![2, 6].includes(header.colorType) || header.interlace !== 0) {
    throw new Error(`decodePng: unsupported PNG (bitDepth ${header.bitDepth}, colorType ${header.colorType}, interlace ${header.interlace})`);
  }
  const channels = header.colorType === 6 ? 4 : 3;
  const { width, height } = header;
  const stride = width * channels;
  const raw = inflateSync(Buffer.concat(idat));
  const out = new Uint8Array(height * stride);
  let pos = 0;
  for (let y = 0; y < height; y += 1) {
    const filter = raw[pos];
    pos += 1;
    for (let i = 0; i < stride; i += 1) {
      const value = raw[pos];
      pos += 1;
      const a = i >= channels ? out[y * stride + i - channels] : 0;
      const b = y > 0 ? out[(y - 1) * stride + i] : 0;
      const c = i >= channels && y > 0 ? out[(y - 1) * stride + i - channels] : 0;
      let recon = value;
      if (filter === 1) recon = value + a;
      else if (filter === 2) recon = value + b;
      else if (filter === 3) recon = value + ((a + b) >> 1);
      else if (filter === 4) recon = value + paeth(a, b, c);
      else if (filter !== 0) throw new Error(`decodePng: bad filter ${filter}`);
      out[y * stride + i] = recon & 0xff;
    }
  }
  return {
    width,
    height,
    channels,
    at(x, y) {
      const idx = (y * width + x) * channels;
      return channels === 4
        ? [out[idx], out[idx + 1], out[idx + 2], out[idx + 3]]
        : [out[idx], out[idx + 1], out[idx + 2], 255];
    },
  };
}
