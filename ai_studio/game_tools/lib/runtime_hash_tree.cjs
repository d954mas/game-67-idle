const { createHash } = require("node:crypto");
const {
  closeSync,
  fstatSync,
  lstatSync,
  openSync,
  readSync,
  readdirSync,
  realpathSync,
} = require("node:fs");
const { isAbsolute, join, relative, resolve, sep } = require("node:path");

const CHUNK_BYTES = 1024 * 1024;

function nativeRealpath(path) {
  return (realpathSync.native || realpathSync)(path);
}

function escapes(root, path) {
  const rel = relative(root, path);
  return rel === ".." || rel.startsWith(`..${sep}`) || isAbsolute(rel);
}

function relativeParts(value, label) {
  if (typeof value !== "string" || !value || value.includes("\0") || value.includes("\\")
      || value.includes(":") || isAbsolute(value)) {
    throw new Error(`${label} relative path is invalid`);
  }
  const parts = value.split("/");
  if (parts.some((part) => !part || part === "." || part === "..")) {
    throw new Error(`${label} relative path is invalid`);
  }
  return parts;
}

function fileHash(path, rel, state) {
  const descriptor = openSync(path, "r");
  try {
    const info = fstatSync(descriptor);
    if (!info.isFile()) throw new Error(`runtime build input has the wrong type: ${rel}`);
    const digest = createHash("sha256");
    const chunk = state.chunk || (state.chunk = Buffer.allocUnsafe(CHUNK_BYTES));
    let size = 0;
    for (;;) {
      const bytes = readSync(descriptor, chunk, 0, chunk.length, null);
      if (bytes === 0) break;
      digest.update(chunk.subarray(0, bytes));
      size += bytes;
    }
    return { path: rel, size, sha256: digest.digest("hex") };
  } finally {
    closeSync(descriptor);
  }
}

function selectedPath(root, rel, expectedType, required) {
  const parts = relativeParts(rel, "runtime build input");
  let path = root;
  for (const [index, component] of parts.entries()) {
    path = join(path, component);
    let info;
    try {
      info = lstatSync(path);
    } catch (error) {
      if (!required && error.code === "ENOENT") return null;
      throw error;
    }
    if (info.isSymbolicLink()) throw new Error(`runtime build input must not contain a symbolic link: ${rel}`);
    if (escapes(root, nativeRealpath(path))) throw new Error(`runtime build input escapes its root: ${rel}`);
    const isFinal = index + 1 === parts.length;
    if ((isFinal && expectedType === "file" && !info.isFile()) || (!isFinal && !info.isDirectory())
        || (isFinal && expectedType === "directory" && !info.isDirectory())) {
      throw new Error(`runtime build input has the wrong type: ${rel}`);
    }
  }
  return path;
}

// Runtime records need a synchronous, confined tree walk because package and
// capture callers consume the record before launching a child process.
function hashRuntimeBuildTree({
  root,
  ignoredRoots = new Set(),
  ignoredAnywhere = new Set(),
  extraRoots = [],
  extraFiles = [],
  shouldIgnoreEntry = null,
}) {
  if (typeof shouldIgnoreEntry !== "function" && shouldIgnoreEntry !== null) {
    throw new Error("runtime build input exclusion must be a function");
  }
  const realRoot = nativeRealpath(resolve(root));
  const rows = [];
  const state = { chunk: null };
  function visit(dir, prefix = "") {
    for (const name of readdirSync(dir).sort()) {
      if ((!prefix && ignoredRoots.has(name)) || ignoredAnywhere.has(name)
          || shouldIgnoreEntry?.({ prefix, name })) continue;
      const path = join(dir, name);
      const rel = prefix ? `${prefix}/${name}` : name;
      const info = lstatSync(path);
      if (info.isSymbolicLink()) throw new Error(`runtime build input must not contain a symbolic link: ${rel}`);
      if (escapes(realRoot, nativeRealpath(path))) throw new Error(`runtime build input escapes its root: ${rel}`);
      if (info.isDirectory()) visit(path, rel);
      else if (info.isFile()) {
        if (/\.md$/i.test(name) || /(?:\.test\.mjs|_test\.py|\.pyc)$/i.test(name)) continue;
        rows.push(fileHash(path, rel, state));
      } else throw new Error(`unsupported runtime build input: ${rel}`);
    }
  }
  visit(realRoot);
  for (const relRoot of extraRoots) {
    const path = selectedPath(realRoot, relRoot, "directory", false);
    if (path) visit(path, relRoot);
  }
  for (const relFile of extraFiles) {
    const path = selectedPath(realRoot, relFile, "file", true);
    rows.push(fileHash(path, relFile, state));
  }
  const digest = createHash("sha256");
  for (const row of rows) digest.update(row.path).update("\0").update(String(row.size)).update("\0").update(row.sha256).update("\n");
  return { files: rows.length, sha256: digest.digest("hex") };
}

module.exports = { hashRuntimeBuildTree };
