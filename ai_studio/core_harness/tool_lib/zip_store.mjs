import { readFileSync, writeFileSync } from "node:fs";

const LOCAL = 0x04034b50;
const CENTRAL = 0x02014b50;
const END = 0x06054b50;
const UTF8 = 0x0800;

const CRC_TABLE = (() => {
  const table = new Uint32Array(256);
  for (let index = 0; index < 256; index += 1) {
    let value = index;
    for (let bit = 0; bit < 8; bit += 1) value = (value & 1) ? (0xedb88320 ^ (value >>> 1)) : (value >>> 1);
    table[index] = value >>> 0;
  }
  return table;
})();

export function crc32(input) {
  const bytes = Buffer.isBuffer(input) ? input : Buffer.from(input);
  let value = 0xffffffff;
  for (const byte of bytes) value = CRC_TABLE[(value ^ byte) & 0xff] ^ (value >>> 8);
  return (value ^ 0xffffffff) >>> 0;
}

function normalizedPath(raw) {
  const path = String(raw || "");
  if (!path || path.startsWith("/") || path.endsWith("/") || path.includes("\0")
      || path.includes("\\")
      || path.split("/").some((part) => !part || part === "." || part === "..")
      || /^[A-Za-z]:/.test(path)) throw new Error(`invalid ZIP entry path: ${raw}`);
  return path;
}

function assertUniquePaths(paths) {
  const exact = new Set();
  const folded = new Map();
  for (const raw of paths) {
    const path = normalizedPath(raw);
    if (exact.has(path)) throw new Error(`duplicate ZIP entry path: ${path}`);
    const key = path.toLowerCase();
    if (folded.has(key)) throw new Error(`case-colliding ZIP entry paths: ${folded.get(key)} and ${path}`);
    exact.add(path);
    folded.set(key, path);
  }
}

function localHeader(name, bytes, crc) {
  const out = Buffer.alloc(30 + name.length);
  out.writeUInt32LE(LOCAL, 0);
  out.writeUInt16LE(20, 4);
  out.writeUInt16LE(UTF8, 6);
  out.writeUInt16LE(0, 8);
  out.writeUInt16LE(0, 10);
  out.writeUInt16LE(0, 12);
  out.writeUInt32LE(crc, 14);
  out.writeUInt32LE(bytes.length, 18);
  out.writeUInt32LE(bytes.length, 22);
  out.writeUInt16LE(name.length, 26);
  out.writeUInt16LE(0, 28);
  name.copy(out, 30);
  return out;
}

function centralHeader(name, bytes, crc, offset) {
  const out = Buffer.alloc(46 + name.length);
  out.writeUInt32LE(CENTRAL, 0);
  out.writeUInt16LE(20, 4);
  out.writeUInt16LE(20, 6);
  out.writeUInt16LE(UTF8, 8);
  out.writeUInt16LE(0, 10);
  out.writeUInt16LE(0, 12);
  out.writeUInt16LE(0, 14);
  out.writeUInt32LE(crc, 16);
  out.writeUInt32LE(bytes.length, 20);
  out.writeUInt32LE(bytes.length, 24);
  out.writeUInt16LE(name.length, 28);
  out.writeUInt16LE(0, 30);
  out.writeUInt16LE(0, 32);
  out.writeUInt16LE(0, 34);
  out.writeUInt16LE(0, 36);
  out.writeUInt32LE(0, 38);
  out.writeUInt32LE(offset, 42);
  name.copy(out, 46);
  return out;
}

export function createStoreZip(entries) {
  const sorted = [...entries].map((entry) => ({
    path: normalizedPath(entry.path),
    bytes: Buffer.isBuffer(entry.bytes) ? Buffer.from(entry.bytes) : Buffer.from(entry.bytes),
  })).sort((left, right) => left.path < right.path ? -1 : left.path > right.path ? 1 : 0);
  assertUniquePaths(sorted.map((entry) => entry.path));
  if (sorted.length > 0xffff) throw new Error("ZIP has too many entries");

  const bodies = [];
  const directory = [];
  let offset = 0;
  for (const entry of sorted) {
    if (entry.bytes.length > 0xffffffff) throw new Error(`ZIP entry is too large: ${entry.path}`);
    const name = Buffer.from(entry.path, "utf8");
    const crc = crc32(entry.bytes);
    const header = localHeader(name, entry.bytes, crc);
    bodies.push(header, entry.bytes);
    directory.push(centralHeader(name, entry.bytes, crc, offset));
    offset += header.length + entry.bytes.length;
  }
  const directoryBytes = Buffer.concat(directory);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(END, 0);
  end.writeUInt16LE(0, 4);
  end.writeUInt16LE(0, 6);
  end.writeUInt16LE(sorted.length, 8);
  end.writeUInt16LE(sorted.length, 10);
  end.writeUInt32LE(directoryBytes.length, 12);
  end.writeUInt32LE(offset, 16);
  end.writeUInt16LE(0, 20);
  return Buffer.concat([...bodies, directoryBytes, end]);
}

function requireRange(buffer, offset, length, label) {
  if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(length) || offset < 0 || length < 0 || offset + length > buffer.length) {
    throw new Error(`truncated ZIP ${label}`);
  }
}

export function readStoreZip(input) {
  const buffer = Buffer.isBuffer(input) ? input : readFileSync(input);
  if (buffer.length < 22 || buffer.readUInt32LE(buffer.length - 22) !== END) throw new Error("ZIP end record is missing or commented");
  const end = buffer.length - 22;
  if (buffer.readUInt16LE(end + 4) !== 0 || buffer.readUInt16LE(end + 6) !== 0 || buffer.readUInt16LE(end + 20) !== 0) {
    throw new Error("multi-disk or commented ZIP is unsupported");
  }
  const count = buffer.readUInt16LE(end + 10);
  if (buffer.readUInt16LE(end + 8) !== count) throw new Error("ZIP entry count mismatch");
  const directorySize = buffer.readUInt32LE(end + 12);
  const directoryOffset = buffer.readUInt32LE(end + 16);
  if (count === 0xffff || directorySize === 0xffffffff || directoryOffset === 0xffffffff) throw new Error("ZIP64 is unsupported");
  if (directoryOffset + directorySize !== end) throw new Error("ZIP central directory bounds mismatch");

  const entries = new Map();
  const folded = new Map();
  let expectedLocalOffset = 0;
  let previousName = "";
  let cursor = directoryOffset;
  for (let index = 0; index < count; index += 1) {
    requireRange(buffer, cursor, 46, "central header");
    if (buffer.readUInt32LE(cursor) !== CENTRAL) throw new Error("ZIP central header signature mismatch");
    const flags = buffer.readUInt16LE(cursor + 8);
    const method = buffer.readUInt16LE(cursor + 10);
    const crc = buffer.readUInt32LE(cursor + 16);
    const compressed = buffer.readUInt32LE(cursor + 20);
    const size = buffer.readUInt32LE(cursor + 24);
    const nameLength = buffer.readUInt16LE(cursor + 28);
    const extraLength = buffer.readUInt16LE(cursor + 30);
    const commentLength = buffer.readUInt16LE(cursor + 32);
    const disk = buffer.readUInt16LE(cursor + 34);
    const localOffset = buffer.readUInt32LE(cursor + 42);
    if (flags !== UTF8 || method !== 0 || compressed !== size || extraLength !== 0 || commentLength !== 0 || disk !== 0) {
      throw new Error("ZIP must use deterministic UTF-8 STORE entries without extras");
    }
    requireRange(buffer, cursor + 46, nameLength, "central name");
    const name = normalizedPath(buffer.subarray(cursor + 46, cursor + 46 + nameLength).toString("utf8"));
    if (previousName && previousName >= name) throw new Error("ZIP entries must be in deterministic path order");
    previousName = name;
    if (entries.has(name)) throw new Error(`duplicate ZIP entry path: ${name}`);
    const foldedName = name.toLowerCase();
    if (folded.has(foldedName)) throw new Error(`case-colliding ZIP entry paths: ${folded.get(foldedName)} and ${name}`);
    folded.set(foldedName, name);
    if (localOffset !== expectedLocalOffset) throw new Error(`ZIP has hidden, overlapping, or reordered local data before ${name}`);

    requireRange(buffer, localOffset, 30, `local header for ${name}`);
    if (buffer.readUInt32LE(localOffset) !== LOCAL) throw new Error(`ZIP local header signature mismatch: ${name}`);
    const localNameLength = buffer.readUInt16LE(localOffset + 26);
    const localExtraLength = buffer.readUInt16LE(localOffset + 28);
    if (buffer.readUInt16LE(localOffset + 6) !== flags || buffer.readUInt16LE(localOffset + 8) !== method
        || buffer.readUInt32LE(localOffset + 14) !== crc || buffer.readUInt32LE(localOffset + 18) !== compressed
        || buffer.readUInt32LE(localOffset + 22) !== size || localExtraLength !== 0
        || buffer.readUInt16LE(localOffset + 10) !== 0 || buffer.readUInt16LE(localOffset + 12) !== 0
        || buffer.readUInt16LE(cursor + 12) !== 0 || buffer.readUInt16LE(cursor + 14) !== 0) {
      throw new Error(`ZIP local/central metadata mismatch: ${name}`);
    }
    requireRange(buffer, localOffset + 30, localNameLength, `local name for ${name}`);
    const localName = buffer.subarray(localOffset + 30, localOffset + 30 + localNameLength).toString("utf8");
    if (localName !== name) throw new Error(`ZIP local/central path mismatch: ${name}`);
    const dataOffset = localOffset + 30 + localNameLength;
    requireRange(buffer, dataOffset, size, `data for ${name}`);
    const bytes = Buffer.from(buffer.subarray(dataOffset, dataOffset + size));
    if (crc32(bytes) !== crc) throw new Error(`ZIP CRC mismatch: ${name}`);
    entries.set(name, bytes);
    expectedLocalOffset = dataOffset + size;
    cursor += 46 + nameLength;
  }
  if (cursor !== end) throw new Error("ZIP central directory entry bounds mismatch");
  if (expectedLocalOffset !== directoryOffset) throw new Error("ZIP has hidden data before its central directory");
  return entries;
}

export function writeStoreZip(path, entries) {
  writeFileSync(path, createStoreZip(entries));
}
