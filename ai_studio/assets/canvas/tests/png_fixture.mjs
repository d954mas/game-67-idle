// Tiny dependency-free PNG encoder for tests. node:zlib provides deflate; CRC32
// is a small inline table. Produces 8-bit RGB PNGs whose header bytes match the
// store's pure imageSize() parser, and whose flat-magenta background + solid
// colored blobs give the raster2d detector real regions to find.
import { deflateSync } from "node:zlib";

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

// pixel(x, y) => [r, g, b]
export function encodePng(width, height, pixel) {
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 2; // color type: truecolor RGB
  const raw = Buffer.alloc(height * (1 + width * 3));
  let p = 0;
  for (let y = 0; y < height; y += 1) {
    raw[p] = 0; // filter type: none
    p += 1;
    for (let x = 0; x < width; x += 1) {
      const [r, g, b] = pixel(x, y);
      raw[p] = r;
      raw[p + 1] = g;
      raw[p + 2] = b;
      p += 3;
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

// A trivial solid 4x3 PNG for store round-trip tests where content does not matter.
export function solidPng(width = 4, height = 3, color = [10, 20, 30]) {
  return encodePng(width, height, () => color);
}
