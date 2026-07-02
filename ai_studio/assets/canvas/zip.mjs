// Minimal STORE-mode ZIP writer (no npm deps — node built-ins only).
//
// Canvas exports produce PNG/JPG/WebP files that are ALREADY compressed, so the
// archive uses STORE (method 0, no deflate): the entry bytes are copied verbatim.
// That keeps this writer tiny (no compressor) and the CPU cost near zero while the
// archive size is essentially the sum of the files. The output is a standard .zip a
// Windows/macOS/Linux extractor opens.
//
// CRC-32 comes from node:zlib (`zlib.crc32`, available since Node 22.2) when present,
// with a table-based fallback so the writer still works on an older interpreter — the
// task's "verify the Node version supports it, else implement CRC32" requirement.
//
// This module is pure (no canvas/store knowledge): callers hand it {name, data}
// entries and get back the archive Buffer. ops.zipExport is the canvas-side caller.
import zlib from "node:zlib";

const LOCAL_SIG = 0x04034b50; // local file header
const CENTRAL_SIG = 0x02014b50; // central directory header
const EOCD_SIG = 0x06054b50; // end of central directory
const VERSION = 20; // 2.0 — the STORE/deflate baseline
const FLAG_UTF8 = 0x0800; // general-purpose bit 11: filenames are UTF-8

let crcTable = null;
function tableCrc32(buf) {
  if (!crcTable) {
    crcTable = new Uint32Array(256);
    for (let n = 0; n < 256; n += 1) {
      let c = n;
      for (let k = 0; k < 8; k += 1) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
      crcTable[n] = c >>> 0;
    }
  }
  let crc = 0xffffffff;
  for (let i = 0; i < buf.length; i += 1) crc = (crc >>> 8) ^ crcTable[(crc ^ buf[i]) & 0xff];
  return (crc ^ 0xffffffff) >>> 0;
}

// Prefer the native zlib.crc32 (fast, table-based fallback for older Node).
function crc32(buf) {
  return typeof zlib.crc32 === "function" ? zlib.crc32(buf, 0) >>> 0 : tableCrc32(buf);
}

// DOS date/time fields (2-second time resolution; year clamped to the 1980 epoch).
function dosDateTime(date) {
  const year = Math.max(1980, date.getFullYear());
  const dosDate = ((year - 1980) << 9) | ((date.getMonth() + 1) << 5) | date.getDate();
  const dosTime = (date.getHours() << 11) | (date.getMinutes() << 5) | Math.floor(date.getSeconds() / 2);
  return { dosDate: dosDate & 0xffff, dosTime: dosTime & 0xffff };
}

// Build a STORE-mode zip from [{name, data}] entries (data is a Buffer/Uint8Array).
// Returns the archive as a Buffer. Duplicate names are the caller's responsibility;
// the canvas export naming already yields unique file names.
export function zipStore(entries, { date = new Date() } = {}) {
  if (!Array.isArray(entries)) throw new Error("zipStore requires an entries array");
  const { dosDate, dosTime } = dosDateTime(date);
  const fileParts = [];
  const centralParts = [];
  let offset = 0;

  for (const entry of entries) {
    if (!entry || typeof entry.name !== "string" || !entry.name) throw new Error("zip entry requires a non-empty name");
    const nameBuf = Buffer.from(entry.name, "utf8");
    const data = Buffer.isBuffer(entry.data) ? entry.data : Buffer.from(entry.data || []);
    const crc = crc32(data);

    const local = Buffer.alloc(30);
    local.writeUInt32LE(LOCAL_SIG, 0);
    local.writeUInt16LE(VERSION, 4);
    local.writeUInt16LE(FLAG_UTF8, 6);
    local.writeUInt16LE(0, 8); // method 0 = store
    local.writeUInt16LE(dosTime, 10);
    local.writeUInt16LE(dosDate, 12);
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(data.length, 18); // compressed size == stored size
    local.writeUInt32LE(data.length, 22); // uncompressed size
    local.writeUInt16LE(nameBuf.length, 26);
    local.writeUInt16LE(0, 28); // extra field length
    fileParts.push(local, nameBuf, data);

    const central = Buffer.alloc(46);
    central.writeUInt32LE(CENTRAL_SIG, 0);
    central.writeUInt16LE(VERSION, 4); // version made by
    central.writeUInt16LE(VERSION, 6); // version needed
    central.writeUInt16LE(FLAG_UTF8, 8);
    central.writeUInt16LE(0, 10); // method
    central.writeUInt16LE(dosTime, 12);
    central.writeUInt16LE(dosDate, 14);
    central.writeUInt32LE(crc, 16);
    central.writeUInt32LE(data.length, 20);
    central.writeUInt32LE(data.length, 24);
    central.writeUInt16LE(nameBuf.length, 28);
    central.writeUInt16LE(0, 30); // extra length
    central.writeUInt16LE(0, 32); // comment length
    central.writeUInt16LE(0, 34); // disk number start
    central.writeUInt16LE(0, 36); // internal attributes
    central.writeUInt32LE(0, 38); // external attributes
    central.writeUInt32LE(offset, 42); // relative offset of local header
    centralParts.push(central, nameBuf);

    offset += local.length + nameBuf.length + data.length;
  }

  const centralBuf = Buffer.concat(centralParts);
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(EOCD_SIG, 0);
  eocd.writeUInt16LE(0, 4); // this disk number
  eocd.writeUInt16LE(0, 6); // disk with central directory
  eocd.writeUInt16LE(entries.length, 8); // entries on this disk
  eocd.writeUInt16LE(entries.length, 10); // total entries
  eocd.writeUInt32LE(centralBuf.length, 12); // central directory size
  eocd.writeUInt32LE(offset, 16); // central directory offset
  eocd.writeUInt16LE(0, 20); // comment length

  return Buffer.concat([...fileParts, centralBuf, eocd]);
}

export { crc32 };
