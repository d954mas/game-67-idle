import { TextDecoder } from "node:util";

const UTF8 = new TextDecoder("utf-8", { fatal: true });
export const MAX_BATCH_BYTES = 64 * 1024 * 1024 - 4096;

function bytes(value) {
  return Buffer.isBuffer(value) ? value : Buffer.from(value || "");
}

function decodePath(value) {
  try {
    const path = UTF8.decode(value);
    if (!path || path.includes("\0")) throw new Error("empty path");
    return path;
  } catch (error) {
    throw new Error(`invalid UTF-8 index path (${error.message})`);
  }
}

export function assertGitRelativePath(path) {
  const value = String(path || "");
  const parts = value.split("/");
  if (
    !value
    || value.startsWith("/")
    || value.includes("\\")
    || /^[a-zA-Z]:/.test(value)
    || parts.some((part) => !part || part === "." || part === "..")
  ) {
    throw new Error(`unsafe Git-relative path: ${value || "<empty>"}`);
  }
  return value;
}

export function parseUntrackedGitPath(path) {
  const value = String(path || "");
  return assertGitRelativePath(value.endsWith("/") ? value.slice(0, -1) : value);
}

function records(output, description) {
  const source = bytes(output);
  if (!source.length) return [];
  if (source[source.length - 1] !== 0) throw new Error(`truncated ${description}`);
  const rows = [];
  let offset = 0;
  while (offset < source.length) {
    const end = source.indexOf(0, offset);
    rows.push(source.subarray(offset, end));
    offset = end + 1;
  }
  return rows;
}

export function parseIndexEntries(output, { includeGitlinks = false } = {}) {
  const paths = new Set();
  const entries = [];
  for (const row of records(output, "index entry list")) {
    const tab = row.indexOf(9);
    if (tab < 1) throw new Error("invalid index entry");
    const header = row.subarray(0, tab).toString("ascii");
    const match = /^(100644|100755|120000|160000) ([0-9a-f]{40}|[0-9a-f]{64}) ([0-3])$/.exec(header);
    if (!match) throw new Error("invalid index entry");
    const [, mode, oid, stage] = match;
    const path = assertGitRelativePath(decodePath(row.subarray(tab + 1)));
    if (paths.has(path)) throw new Error(`duplicate or unmerged index entry: ${path}`);
    paths.add(path);
    if (stage !== "0") throw new Error(`unmerged index entry: ${path}`);
    // A gitlink points at a commit. It has no file bytes to scan.
    if (mode !== "160000" || includeGitlinks) entries.push({ mode, oid, path });
  }
  return entries;
}

function lines(output, description) {
  const source = bytes(output);
  if (!source.length) return [];
  if (source[source.length - 1] !== 10) throw new Error(`truncated ${description}`);
  return source.subarray(0, -1).toString("ascii").split("\n");
}

function checkedHeader(line, oid, maxBlobBytes) {
  const match = /^([0-9a-f]{40}|[0-9a-f]{64}) blob ([0-9]+)$/.exec(line);
  if (!match || match[1] !== oid) throw new Error(`invalid batch header for ${oid}`);
  const size = Number(match[2]);
  if (!Number.isSafeInteger(size) || size < 0) throw new Error(`invalid batch blob size for ${oid}`);
  if (size > maxBlobBytes) throw new Error(`batch blob exceeds limit for ${oid}`);
  return size;
}

export function parseBatchCheck(output, oids, { maxBlobBytes = MAX_BATCH_BYTES } = {}) {
  const headers = lines(output, "batch check output");
  if (headers.length !== oids.length) throw new Error("batch check output count does not match index blobs");
  const metadata = new Map();
  for (let index = 0; index < oids.length; index += 1) {
    const oid = oids[index];
    if (metadata.has(oid)) throw new Error(`duplicate requested object id: ${oid}`);
    metadata.set(oid, checkedHeader(headers[index], oid, maxBlobBytes));
  }
  return metadata;
}

function batchWireBytes(oid, size) {
  return Buffer.byteLength(`${oid} blob ${size}\n`, "ascii") + size + 1;
}

export function splitBatchOids(oids, metadata, maxBytes = MAX_BATCH_BYTES) {
  const chunks = [];
  let chunk = [];
  let used = 0;
  for (const oid of oids) {
    const size = metadata.get(oid);
    if (size === undefined) throw new Error(`missing checked size for ${oid}`);
    const required = batchWireBytes(oid, size);
    if (required > maxBytes) throw new Error(`batch blob exceeds bounded output size for ${oid}`);
    if (chunk.length && used + required > maxBytes) {
      chunks.push(chunk);
      chunk = [];
      used = 0;
    }
    chunk.push(oid);
    used += required;
  }
  if (chunk.length) chunks.push(chunk);
  return chunks;
}

export function batchInput(oids) {
  return Buffer.from(`${oids.join("\n")}\n`, "ascii");
}

export function parseBatchBodies(output, oids, metadata) {
  const source = bytes(output);
  const blobs = new Map();
  let offset = 0;
  for (const oid of oids) {
    const headerEnd = source.indexOf(10, offset);
    if (headerEnd < 0) throw new Error(`truncated batch header for ${oid}`);
    const header = source.subarray(offset, headerEnd).toString("ascii");
    const size = checkedHeader(header, oid, Number.MAX_SAFE_INTEGER);
    if (size !== metadata.get(oid)) throw new Error(`batch blob size does not match checked size for ${oid}`);
    const bodyStart = headerEnd + 1;
    const bodyEnd = bodyStart + size;
    if (bodyEnd >= source.length || source[bodyEnd] !== 10) throw new Error(`truncated batch blob for ${oid}`);
    blobs.set(oid, source.subarray(bodyStart, bodyEnd));
    offset = bodyEnd + 1;
  }
  if (offset !== source.length) throw new Error("unexpected trailing batch blob output");
  return blobs;
}
