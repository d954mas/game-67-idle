import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { dirname, extname, join, relative } from "node:path";

const indexedExt = new Set([
  ".png", ".jpg", ".jpeg", ".webp", ".gif",
  ".obj", ".glb", ".gltf", ".fbx",
  ".ttf", ".otf", ".woff", ".woff2",
  ".wav", ".mp3", ".ogg",
]);
const manifestExt = new Set([".json", ".jsonl"]);

function safeSlug(value) {
  return String(value || "assets").replace(/[^a-zA-Z0-9_.-]+/g, "_").replace(/^_+|_+$/g, "") || "assets";
}

function relPosix(root, abs) {
  return relative(root, abs).replace(/\\/g, "/");
}

function walkSync(dir) {
  const out = [];
  let entries = [];
  try {
    entries = readdirSync(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const entry of entries) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walkSync(path));
    else out.push(path);
  }
  return out;
}

function hashParts(parts) {
  let hash = 2166136261;
  for (const part of parts) {
    for (const ch of String(part)) {
      hash ^= ch.charCodeAt(0);
      hash = Math.imul(hash, 16777619);
    }
  }
  return String(hash >>> 0);
}

function sourceKey(source) {
  return `${source.id}|${source.path}|${source.type || "source"}`;
}

function trackedRoots(source) {
  return [{ label: "source", path: source.path }];
}

function isTrackedFile(source, path) {
  const ext = extname(path).toLowerCase();
  if (indexedExt.has(ext)) return true;
  if (manifestExt.has(ext)) return true;
  return /(^|[\\/])licen[cs]e(\.|$)/i.test(path);
}

export function sourceSnapshotPath(root, source) {
  return join(root, "tmp", "ai_studio", "assets", "snapshots", `${safeSlug(source.id)}.json`);
}

export function buildSourceSnapshot(root, source) {
  const files = [];
  for (const trackedRoot of trackedRoots(source)) {
    for (const file of walkSync(trackedRoot.path).filter((path) => isTrackedFile(source, path)).sort()) {
      let stat;
      try {
        stat = statSync(file);
      } catch {
        continue;
      }
      files.push({
        scope: trackedRoot.label,
        rel: relPosix(trackedRoot.path, file),
        size: stat.size,
        mtimeMs: Math.round(stat.mtimeMs),
      });
    }
  }
  const parts = [sourceKey(source), source.type || "source", source.path];
  let totalSize = 0;
  let maxMtimeMs = 0;
  for (const file of files) {
    totalSize += file.size;
    maxMtimeMs = Math.max(maxMtimeMs, file.mtimeMs);
    parts.push(file.scope, file.rel, file.size, file.mtimeMs);
  }
  return {
    version: 1,
    sourceKey: sourceKey(source),
    sourceId: source.id,
    type: source.type || "source",
    path: source.path,
    count: files.length,
    totalSize,
    maxMtimeMs,
    hash: hashParts(parts),
    files,
  };
}

export function sourceSnapshotSignature(snapshot) {
  if (!snapshot) return null;
  return {
    version: snapshot.version || 1,
    sourceKey: snapshot.sourceKey,
    type: snapshot.type || "source",
    path: snapshot.path,
    count: snapshot.count,
    totalSize: snapshot.totalSize,
    maxMtimeMs: snapshot.maxMtimeMs,
    hash: snapshot.hash,
  };
}

function fileKey(file) {
  return `${file.scope}:${file.rel}`;
}

export function diffSourceSnapshots(before, after) {
  const previous = new Map((before?.files || []).map((file) => [fileKey(file), file]));
  const current = new Map((after?.files || []).map((file) => [fileKey(file), file]));
  const added = [];
  const changed = [];
  const deleted = [];
  for (const [key, file] of current) {
    const old = previous.get(key);
    if (!old) added.push(file);
    else if (old.size !== file.size || old.mtimeMs !== file.mtimeMs) changed.push(file);
  }
  for (const [key, file] of previous) {
    if (!current.has(key)) deleted.push(file);
  }
  return { added, changed, deleted };
}

export function readSourceSnapshot(root, source) {
  const path = sourceSnapshotPath(root, source);
  if (!existsSync(path)) return null;
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch {
    return null;
  }
}

export function writeSourceSnapshot(root, source, snapshot) {
  const path = sourceSnapshotPath(root, source);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify(snapshot, null, 2), "utf8");
  return path;
}
