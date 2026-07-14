import { createHash, randomUUID } from "node:crypto";
import {
  existsSync,
  linkSync,
  lstatSync,
  mkdirSync,
  readdirSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { basename, join } from "node:path";

import { createStoreZip, readStoreZip } from "../core_harness/tool_lib/zip_store.mjs";

const BATCH_NAME = /^[a-z0-9](?:[a-z0-9-]{0,78}[a-z0-9])?$/;

function taskEntryMatches(entryPath, id) {
  const name = basename(entryPath);
  return name === `${id}.md` || name.startsWith(`${id}-`) && name.endsWith(".md");
}

function verifiedZipBytes(entries) {
  const bytes = createStoreZip(entries);
  const reopened = readStoreZip(bytes);
  if (reopened.size !== entries.length) throw new Error("sealed ZIP verification count mismatch");
  for (const entry of entries) {
    const actual = reopened.get(entry.path);
    if (!actual || !actual.equals(entry.bytes)) throw new Error(`sealed ZIP verification failed: ${entry.path}`);
  }
  return bytes;
}

export function sealedArchiveFiles(archiveRoot) {
  if (!existsSync(archiveRoot)) return [];
  return readdirSync(archiveRoot)
    .filter((name) => name.endsWith(".zip"))
    .sort()
    .map((name) => {
      const path = join(archiveRoot, name);
      const stat = lstatSync(path);
      if (stat.isSymbolicLink() || !stat.isFile()) throw new Error(`sealed archive must be a regular file, not a link: ${path}`);
      return path;
    });
}

export function sealArchiveBatch({ archiveRoot, name, entries, sourceFiles }) {
  const batch = String(name || "").trim();
  if (!BATCH_NAME.test(batch)) throw new Error("archive batch name must use lowercase letters, digits, and hyphens");
  if (!Array.isArray(entries) || entries.length < 2) throw new Error("archive batch needs task entries and MANIFEST.md");
  if (!Array.isArray(sourceFiles) || sourceFiles.length !== entries.length - 1) {
    throw new Error("archive source count must match task entry count");
  }
  mkdirSync(archiveRoot, { recursive: true });
  const target = join(archiveRoot, `${batch}.zip`);
  if (existsSync(target)) throw new Error(`archive batch already exists: ${target}`);

  const normalized = entries.map((entry) => ({
    path: String(entry.path),
    bytes: Buffer.isBuffer(entry.bytes) ? entry.bytes : Buffer.from(entry.bytes),
  }));
  const bytes = verifiedZipBytes(normalized);
  const temporary = join(archiveRoot, `.${batch}.${randomUUID()}.tmp`);
  try {
    writeFileSync(temporary, bytes, { flag: "wx" });
    readStoreZip(temporary);
    linkSync(temporary, target);
    rmSync(temporary);
  } catch (error) {
    rmSync(temporary, { force: true });
    throw error;
  }

  for (const file of sourceFiles) rmSync(file);
  return {
    file: target,
    entries: sourceFiles.length,
    bytes: bytes.length,
    sha256: createHash("sha256").update(bytes).digest("hex"),
  };
}

export function findSealedTask(archiveRoot, id) {
  const matches = [];
  for (const archivePath of sealedArchiveFiles(archiveRoot)) {
    const entries = readStoreZip(archivePath);
    for (const [entryPath, bytes] of entries) {
      if (entryPath !== "MANIFEST.md" && taskEntryMatches(entryPath, id)) {
        matches.push({ archivePath, entryPath, bytes });
      }
    }
  }
  if (matches.length > 1) throw new Error(`${id} appears in multiple sealed Taskboard archives`);
  return matches[0] || null;
}

export function listSealedTasks(archiveRoot) {
  const tasks = [];
  for (const archivePath of sealedArchiveFiles(archiveRoot)) {
    for (const [entryPath, bytes] of readStoreZip(archivePath)) {
      if (entryPath !== "MANIFEST.md" && basename(entryPath).startsWith("T") && entryPath.endsWith(".md")) {
        tasks.push({ archivePath, entryPath, bytes });
      }
    }
  }
  return tasks;
}
