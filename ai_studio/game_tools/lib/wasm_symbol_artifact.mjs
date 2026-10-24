import { createHash, randomUUID } from "node:crypto";
import { lstatSync, mkdirSync, readFileSync, readdirSync, realpathSync, renameSync, rmdirSync, rmSync, writeFileSync } from "node:fs";
import { uptime } from "node:os";
import { isAbsolute, join, relative, sep } from "node:path";

const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");

function regularFile(path) {
  const stat = lstatSync(path, { throwIfNoEntry: false });
  if (stat && (!stat.isFile() || stat.isSymbolicLink())) {
    throw new Error(`artifact path must be a regular file: ${path}`);
  }
  return Boolean(stat);
}

export function retainWasmSymbols({ emittedPath, retainedPath, wasmPath, digestPath }) {
  const pendingPath = `${digestPath}.pending`;
  if (!regularFile(emittedPath)) {
    if (!regularFile(pendingPath)) return false;
    if (!regularFile(retainedPath) || !regularFile(wasmPath)
        || readFileSync(pendingPath, "utf8") !== symbolDigest(retainedPath, wasmPath)) {
      throw new Error("interrupted WASM symbol retention cannot be recovered");
    }
    renameSync(pendingPath, digestPath);
    return true;
  }
  if (!regularFile(wasmPath)) throw new Error(`symbol map requires linked WASM: ${wasmPath}`);
  regularFile(pendingPath);
  writeFileSync(pendingPath, symbolDigest(emittedPath, wasmPath));
  renameSync(emittedPath, retainedPath);
  renameSync(pendingPath, digestPath);
  return true;
}

function symbolDigest(symbolPath, wasmPath) {
  return `${sha256(readFileSync(wasmPath))} ${sha256(readFileSync(symbolPath))}\n`;
}

export function writeWasmSymbolDigest({ symbolPath, wasmPath, digestPath }) {
  if (!regularFile(symbolPath) || !regularFile(wasmPath)) throw new Error("symbol digest requires map and linked WASM");
  const pendingPath = `${digestPath}.pending`;
  regularFile(pendingPath);
  writeFileSync(pendingPath, symbolDigest(symbolPath, wasmPath));
  renameSync(pendingPath, digestPath);
}

function confined(path, buildDir) {
  const rel = relative(realpathSync(buildDir), realpathSync(path));
  if (rel === ".." || rel.startsWith(`..${sep}`) || isAbsolute(rel)) {
    throw new Error(`WASM symbol source leaves the build directory: ${path}`);
  }
}

export function readWasmSymbols({ symbolPath, linkedWasmPath, packagedWasm, digestPath, buildDir, required = false }) {
  const present = regularFile(symbolPath);
  if (!present) {
    if (required || (digestPath && regularFile(digestPath))) throw new Error(`package requires the WASM symbol map: ${symbolPath}`);
    return null;
  }
  if (buildDir) confined(symbolPath, buildDir);
  if (!regularFile(linkedWasmPath) || !readFileSync(linkedWasmPath).equals(packagedWasm)) {
    throw new Error("WASM symbol map requires the exact linked WASM staged in the release site");
  }
  if (buildDir) confined(linkedWasmPath, buildDir);
  if (digestPath) {
    if (!regularFile(digestPath) || readFileSync(digestPath, "utf8") !== symbolDigest(symbolPath, linkedWasmPath)) {
      throw new Error(`WASM symbol map does not match the linked WASM digest: ${digestPath}`);
    }
    if (buildDir) confined(digestPath, buildDir);
  }
  const symbols = readFileSync(symbolPath);
  if (!/^0:[^\r\n]+\r?\n/.test(symbols.toString("utf8"))) throw new Error(`invalid WASM symbol map: ${symbolPath}`);
  return symbols;
}

function publishVerified(tempPath, finalPath) {
  if (regularFile(finalPath)) {
    if (!readFileSync(finalPath).equals(readFileSync(tempPath))) throw new Error(`refusing to replace a different deterministic artifact: ${finalPath}`);
    rmSync(tempPath, { force: true });
    return false;
  }
  try {
    renameSync(tempPath, finalPath);
    return true;
  } catch (error) {
    if (regularFile(finalPath) && readFileSync(finalPath).equals(readFileSync(tempPath))) {
      rmSync(tempPath, { force: true });
      return false;
    }
    throw error;
  }
}

function alive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch (error) {
    return error.code !== "ESRCH";
  }
}

function acquirePublishLock(lockPath) {
  for (let attempt = 0; attempt < 3; attempt += 1) {
    try {
      mkdirSync(lockPath);
      try {
        writeFileSync(join(lockPath, "owner.json"), JSON.stringify({ pid: process.pid, createdAt: Date.now() }));
      } catch (error) {
        rmSync(lockPath, { recursive: true, force: true });
        throw error;
      }
      return;
    } catch (error) {
      if (error.code !== "EEXIST") throw error;
    }
    const stat = lstatSync(lockPath, { throwIfNoEntry: false });
    if (!stat) continue;
    if (!stat.isDirectory() || stat.isSymbolicLink()) throw new Error(`invalid release publication lock: ${lockPath}`);
    const ownerPath = join(lockPath, "owner.json");
    const ownerStat = lstatSync(ownerPath, { throwIfNoEntry: false });
    let owner;
    if (ownerStat?.isFile() && !ownerStat.isSymbolicLink()) {
      try { owner = JSON.parse(readFileSync(ownerPath, "utf8")); } catch { /* A partial owner record is recovered after its grace period. */ }
    }
    const validOwner = Number.isInteger(owner?.pid) && owner.pid > 0 && Number.isFinite(owner.createdAt);
    const stale = validOwner
      ? !alive(owner.pid) || owner.createdAt < Date.now() - uptime() * 1000 - 60000
      : Date.now() - stat.birthtimeMs > 5000;
    if (!stale) throw new Error(`release artifact is already being published: ${lockPath}`);
    const names = readdirSync(lockPath);
    if (names.some((name) => name !== "owner.json")) throw new Error(`invalid release publication lock: ${lockPath}`);
    if (ownerStat) rmSync(ownerPath, { force: true });
    try { rmdirSync(lockPath); } catch (error) { if (error.code !== "ENOENT") throw error; }
  }
  throw new Error(`release artifact is already being published: ${lockPath}`);
}

export function publishArtifactSet(files, publisher = publishVerified) {
  const lockPath = `${files.at(-1).finalPath}.publish.lock`;
  acquirePublishLock(lockPath);
  try {
    const preexisting = new Map();
    for (const { tempPath, finalPath } of files) {
      const exists = regularFile(finalPath);
      if (exists && !readFileSync(finalPath).equals(readFileSync(tempPath))) {
        throw new Error(`refusing to replace a different deterministic artifact: ${finalPath}`);
      }
      preexisting.set(finalPath, exists);
    }
    try {
      for (const { tempPath, finalPath } of files) publisher(tempPath, finalPath);
    } catch (error) {
      for (const { finalPath } of [...files].reverse()) {
        if (!preexisting.get(finalPath) && regularFile(finalPath)) rmSync(finalPath);
      }
      throw error;
    }
  } finally {
    const closingPath = `${lockPath}.closing-${process.pid}-${randomUUID()}`;
    renameSync(lockPath, closingPath);
    rmSync(closingPath, { recursive: true, force: true });
  }
}
