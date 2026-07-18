import { createHash, randomUUID } from "node:crypto";
import {
  closeSync,
  copyFileSync,
  existsSync,
  lstatSync,
  mkdirSync,
  openSync,
  readFileSync,
  realpathSync,
  renameSync,
  rmSync,
  rmdirSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, extname, join, resolve, sep } from "node:path";

import { validateLicenseRecord } from "../../licenses/ops.mjs";
import { resolveAcceptedGameStyleLock } from "../../style_lock/generation_origin.mjs";
import { canvasLocalCacheRoot } from "../config.mjs";
import { getProject, resolveProjectFile } from "../store.mjs";
import { requireAcceptedStyleDecision } from "./style_decision.mjs";

const PACK_ID = "canvas-promotions";
const ASSET_ID = /^[a-z0-9]+(?:[a-z0-9_-]*[a-z0-9])?$/;
const ORIGINS = new Set(["mine", "ai", "sourced"]);
const LICENSE_KINDS = new Set(["cc", "spdx", "custom"]);
const CANVAS_IMAGE_EXTENSIONS = new Set([".png", ".jpg", ".jpeg", ".gif", ".webp"]);
const BOOLEAN_TEXT = new Set(["true", "false"]);
const NON_VALUES = new Set(["", "unknown", "todo", "tbd", "unresolved"]);
const LOCK_STALE_MS = 30000;
const LOCK_RETRY_TOTAL_MS = 2000;
const LOCK_RETRY_INTERVAL_MS = 100;
const REQUIRED_METADATA_KEYS = [
  "asset_id",
  "author_vendor",
  "commercial_use",
  "credit_text",
  "description",
  "kind",
  "license",
  "license_kind",
  "license_url",
  "modification_allowed",
  "notice_required",
  "origin",
  "provenance",
  "publish",
  "redistribution_allowed",
  "source_page",
  "tags",
  "title",
  "attribution_required",
].sort();

function isNonValue(value) {
  const text = String(value || "").trim().toLowerCase();
  return NON_VALUES.has(text) || text.startsWith("pending");
}

function exactMetadata(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error("asset promotion requires a metadata object");
  }
  const keys = Object.keys(value).sort();
  if (JSON.stringify(keys) !== JSON.stringify(REQUIRED_METADATA_KEYS)) {
    throw new Error(`asset promotion metadata must contain exactly: ${REQUIRED_METADATA_KEYS.join(", ")}`);
  }
  if (!ASSET_ID.test(value.asset_id || "")) throw new Error("asset promotion metadata.asset_id must be a lowercase asset slug");
  for (const key of ["title", "kind", "license", "license_url", "source_page", "author_vendor", "provenance"]) {
    if (typeof value[key] !== "string" || !value[key].trim()) {
      throw new Error(`asset promotion metadata.${key} must be a non-empty string`);
    }
  }
  for (const key of ["license", "source_page", "author_vendor", "provenance"]) {
    if (isNonValue(value[key])) throw new Error(`asset promotion metadata.${key} must be resolved before promotion`);
  }
  for (const key of ["description", "credit_text"]) {
    if (typeof value[key] !== "string") throw new Error(`asset promotion metadata.${key} must be a string`);
  }
  if (!ORIGINS.has(value.origin)) throw new Error("asset promotion metadata.origin must be mine|ai|sourced");
  if (!LICENSE_KINDS.has(value.license_kind)) {
    throw new Error("asset promotion metadata.license_kind must be cc|spdx|custom; private/unknown assets cannot enter a public pack");
  }
  for (const key of ["attribution_required", "notice_required", "commercial_use", "modification_allowed", "redistribution_allowed", "publish"]) {
    if (!BOOLEAN_TEXT.has(value[key])) throw new Error(`asset promotion metadata.${key} must be true|false text`);
  }
  if (!Array.isArray(value.tags) || value.tags.length > 32
      || !value.tags.every((tag) => typeof tag === "string" && tag.trim() && tag.length <= 80)) {
    throw new Error("asset promotion metadata.tags must be an array of at most 32 non-empty strings");
  }
  const clean = structuredClone(value);
  const license = validateLicenseRecord(clean, { forPublicBinary: true, forRelease: true });
  if (!license.ok) {
    throw new Error(`asset promotion requires release-ready publishable license metadata: ${license.issues.join("; ")}`);
  }
  return clean;
}

function ownedGameId(project) {
  const ownership = project?.ownership;
  if (!ownership || ownership.kind !== "game" || !/^[a-z][a-z0-9-]*$/.test(ownership.gameId || "")) {
    throw new Error("asset promotion requires a game-owned Canvas project");
  }
  return ownership.gameId;
}

function samePhysicalPath(left, right) {
  const normalize = (value) => process.platform === "win32" ? resolve(value).toLowerCase() : resolve(value);
  return normalize(left) === normalize(right);
}

function isInside(parent, child) {
  const normalize = (value) => process.platform === "win32" ? resolve(value).toLowerCase() : resolve(value);
  const base = normalize(parent);
  const target = normalize(child);
  return target === base || target.startsWith(base + sep);
}

function requirePlainDirectory(path, label) {
  const lexical = resolve(path);
  const info = lstatSync(lexical);
  if (info.isSymbolicLink() || !info.isDirectory() || !samePhysicalPath(realpathSync(lexical), lexical)) {
    throw new Error(`asset promotion ${label} must be a physical directory, not a symlink or junction`);
  }
  return lexical;
}

function resolveOwnedGameRoot(root, gameId) {
  const workspace = realpathSync(resolve(root));
  const candidates = [
    { path: join(workspace, "games", gameId), privateStore: false },
    { path: join(workspace, "games", "private", gameId), privateStore: true },
  ].filter((candidate) => existsSync(candidate.path));
  if (candidates.length !== 1) {
    throw new Error(candidates.length
      ? `asset promotion found ambiguous public/private game roots for ${gameId}`
      : `asset promotion could not find game root for ${gameId}`);
  }
  const gameRoot = requirePlainDirectory(candidates[0].path, "game root");
  if (!isInside(workspace, gameRoot)) throw new Error("asset promotion game root escaped the workspace");
  return { gameRoot, privateStore: candidates[0].privateStore };
}

function ensurePlainChild(parent, name, createdDirectories) {
  const base = requirePlainDirectory(parent, "parent directory");
  const child = resolve(base, name);
  if (!isInside(base, child) || child === base) throw new Error("asset promotion directory escaped its parent");
  if (!existsSync(child)) {
    mkdirSync(child);
    createdDirectories.push(child);
  }
  return requirePlainDirectory(child, name);
}

function promotedFileName(element) {
  const sourceExtension = extname(element.src || "").toLowerCase();
  if (!CANVAS_IMAGE_EXTENSIONS.has(sourceExtension)) {
    throw new Error("asset promotion source extension must be a Canvas png|jpg|jpeg|gif|webp image");
  }
  const extension = sourceExtension;
  const stem = basename(element.name || element.id, extname(element.name || ""))
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 80) || "asset";
  return `${stem}${extension}`;
}

function readRows(path) {
  if (!existsSync(path)) return [];
  return readFileSync(path, "utf8").split(/\r?\n/).map((line) => line.trim()).filter(Boolean).map((line) => JSON.parse(line));
}

const TRANSACTION_PREPARED = ".asset-promotion-transaction.prepared.json";
const TRANSACTION_COMMITTED = ".asset-promotion-transaction.committed.json";

function transactionPaths(packDir, marker) {
  if (marker?.schema !== "ai_studio.canvas.asset_promotion_transaction.v1"
      || !["prepared", "committed"].includes(marker.state)
      || !ASSET_ID.test(marker.asset_id || "")
      || typeof marker.file_name !== "string"
      || !/^[a-z0-9][a-z0-9.-]{0,96}$/.test(marker.file_name)
      || typeof marker.suffix !== "string"
      || !/^[0-9a-f-]{36}$/.test(marker.suffix)
      || typeof marker.manifest_existed !== "boolean"
      || typeof marker.pack_existed !== "boolean") {
    throw new Error("asset promotion transaction marker is malformed; refusing unsafe recovery");
  }
  const destination = join(packDir, "files", marker.asset_id, marker.file_name);
  const manifestPath = join(packDir, "assets.jsonl");
  const packPath = join(packDir, "pack.json");
  return {
    destination,
    assetDir: dirname(destination),
    manifestPath,
    packPath,
    fileTemp: `${destination}.${marker.suffix}.tmp`,
    manifestTemp: `${manifestPath}.${marker.suffix}.tmp`,
    packTemp: `${packPath}.${marker.suffix}.tmp`,
    manifestBackup: `${manifestPath}.${marker.suffix}.bak`,
  };
}

function writeTransactionMarker(packDir, marker) {
  const markerName = marker.state === "committed" ? TRANSACTION_COMMITTED : TRANSACTION_PREPARED;
  const markerPath = join(packDir, markerName);
  const markerTemp = `${markerPath}.tmp`;
  let fd;
  let created = false;
  try {
    fd = openSync(markerTemp, "wx");
    created = true;
    writeFileSync(fd, `${JSON.stringify(marker)}\n`, "utf8");
    closeSync(fd);
    fd = undefined;
    renameSync(markerTemp, markerPath);
  } catch (error) {
    if (fd !== undefined) closeSync(fd);
    if (created) rmSync(markerTemp, { force: true });
    throw error;
  }
}

function recoverPromotionTransaction(packDir) {
  const committedPath = join(packDir, TRANSACTION_COMMITTED);
  const preparedPath = join(packDir, TRANSACTION_PREPARED);
  const markerPath = existsSync(committedPath) ? committedPath : preparedPath;
  rmSync(`${committedPath}.tmp`, { force: true });
  rmSync(`${preparedPath}.tmp`, { force: true });
  if (!existsSync(markerPath)) return false;
  let marker;
  try {
    marker = JSON.parse(readFileSync(markerPath, "utf8"));
  } catch {
    throw new Error("asset promotion transaction marker is unreadable; refusing unsafe recovery");
  }
  const paths = transactionPaths(packDir, marker);
  requirePlainDirectory(packDir, "transaction pack directory");
  const recoveryFilesRoot = join(packDir, "files");
  if (existsSync(recoveryFilesRoot)) {
    requirePlainDirectory(recoveryFilesRoot, "transaction files directory");
    if (existsSync(paths.assetDir)) requirePlainDirectory(paths.assetDir, "transaction asset directory");
  }
  for (const path of [paths.fileTemp, paths.manifestTemp, paths.packTemp]) rmSync(path, { force: true });
  if (marker.state === "prepared") {
    rmSync(paths.destination, { force: true });
    if (existsSync(paths.manifestBackup)) {
      rmSync(paths.manifestPath, { force: true });
      renameSync(paths.manifestBackup, paths.manifestPath);
    } else if (!marker.manifest_existed) {
      rmSync(paths.manifestPath, { force: true });
    }
    if (!marker.pack_existed) rmSync(paths.packPath, { force: true });
    try { rmdirSync(paths.assetDir); } catch { /* keep non-empty/pre-existing directory */ }
    rmSync(preparedPath, { force: true });
    rmSync(committedPath, { force: true });
  } else {
    // Once committed exists it remains the authoritative marker until every
    // other recovery artifact is gone. A crash at any point can only resume
    // finalization, never reinterpret the transaction as prepared.
    rmSync(preparedPath, { force: true });
    rmSync(paths.manifestBackup, { force: true });
    rmSync(committedPath, { force: true });
  }
  return true;
}

function hasPromotionTransaction(packDir) {
  return existsSync(join(packDir, TRANSACTION_PREPARED)) || existsSync(join(packDir, TRANSACTION_COMMITTED));
}

const promotionQueues = new Map();
const heldPromotionLocks = new Map();
let exitCleanupRegistered = false;

function promotionLockPath(root, gameId) {
  return join(canvasLocalCacheRoot(root), "asset-promotion-locks", `${gameId}.lock`);
}

function readLockInfo(path) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch {
    return null;
  }
}

function ensureExitCleanup() {
  if (exitCleanupRegistered) return;
  exitCleanupRegistered = true;
  process.on("exit", () => {
    for (const [path, token] of heldPromotionLocks) {
      try { releasePromotionLock({ path, token }); } catch { /* best effort */ }
    }
  });
}

function processIsAlive(pid) {
  if (!Number.isInteger(pid) || pid <= 0) return false;
  try {
    process.kill(pid, 0);
    return true;
  } catch (error) {
    return error.code === "EPERM";
  }
}

function releasePromotionLock(lock) {
  if (!lock) return;
  const current = readLockInfo(lock.path);
  heldPromotionLocks.delete(lock.path);
  if (current?.token === lock.token) rmSync(lock.path, { force: true });
}

async function acquirePromotionLock(root, gameId, options = {}) {
  const path = promotionLockPath(root, gameId);
  mkdirSync(dirname(path), { recursive: true });
  const staleMs = options.staleMs ?? LOCK_STALE_MS;
  const retryTotalMs = options.retryTotalMs ?? LOCK_RETRY_TOTAL_MS;
  const retryIntervalMs = options.retryIntervalMs ?? LOCK_RETRY_INTERVAL_MS;
  const deadline = Date.now() + retryTotalMs;
  let brokeStaleOnce = false;
  for (;;) {
    let fd;
    let created = false;
    try {
      fd = openSync(path, "wx");
      created = true;
      const token = randomUUID();
      writeFileSync(fd, JSON.stringify({ pid: process.pid, startedAt: Date.now(), token }));
      const lock = { path, token };
      heldPromotionLocks.set(path, token);
      ensureExitCleanup();
      return lock;
    } catch (error) {
      if (created && error.code !== "EEXIST") rmSync(path, { force: true });
      if (error.code !== "EEXIST") throw error;
      const info = readLockInfo(path);
      const mtimeAge = (() => {
        try { return Date.now() - lstatSync(path).mtimeMs; } catch { return 0; }
      })();
      const age = info && Number.isFinite(info.startedAt) ? Date.now() - info.startedAt : mtimeAge;
      const holderAlive = info ? processIsAlive(info.pid) : false;
      if (age > staleMs && !holderAlive && !brokeStaleOnce) {
        console.warn(`canvas: breaking stale asset-promotion lock for ${gameId} at ${path}`);
        rmSync(path, { force: true });
        brokeStaleOnce = true;
        continue;
      }
      if (Date.now() >= deadline) throw new Error(`asset promotion pack for ${gameId} is locked by another process`);
      await new Promise((resolveWait) => setTimeout(resolveWait, retryIntervalMs));
    } finally {
      if (fd !== undefined) closeSync(fd);
    }
  }
}

function withPromotionLock(root, gameId, fn, options = {}) {
  const key = promotionLockPath(root, gameId);
  const previous = promotionQueues.get(key) || Promise.resolve();
  const run = previous.then(async () => {
    const lock = await acquirePromotionLock(root, gameId, options);
    try {
      return await fn();
    } finally {
      releasePromotionLock(lock);
    }
  }, async () => {
    const lock = await acquirePromotionLock(root, gameId, options);
    try { return await fn(); } finally { releasePromotionLock(lock); }
  });
  promotionQueues.set(key, run.then(() => {}, () => {}));
  return run;
}

async function promoteAssetToGameImpl(root, { projectId, elementId, metadata } = {}, dependencies = {}) {
  if (!projectId) throw new Error("promoteAssetToGame requires projectId");
  if (!elementId) throw new Error("promoteAssetToGame requires elementId");
  const cleanMetadata = exactMetadata(metadata);
  const initialProject = getProject(root, projectId);
  const gameId = ownedGameId(initialProject);
  return withPromotionLock(root, gameId, async () => {
    const project = getProject(root, projectId);
    if (ownedGameId(project) !== gameId) throw new Error("asset promotion Canvas ownership changed while waiting for the game pack lock");
    const element = (project.elements || []).find((item) => item.id === elementId);
    if (!element) throw new Error(`element not found: ${elementId}`);
    if (element.type !== "image" || !element.src) throw new Error(`element ${elementId} asset promotion is image-only`);
    const resolveStyleLock = dependencies.resolveStyleLock || resolveAcceptedGameStyleLock;
    const resolved = resolveStyleLock(root, project);
    if (resolved?.gameId !== gameId || !resolved?.lock) throw new Error("asset promotion style lock must match Canvas game ownership");
    const { decision } = requireAcceptedStyleDecision(project, element, resolved.lock);
    const resolveGameRoot = dependencies.resolveGameRoot || resolveOwnedGameRoot;
    const rootResult = resolveGameRoot(root, gameId);
    const { gameRoot, privateStore = false } = typeof rootResult === "string" ? { gameRoot: rootResult } : rootResult;
    const source = resolveProjectFile(root, projectId, element.src);
    const sourceFilesRoot = requirePlainDirectory(dirname(source), "Canvas project files directory");
    const sourceInfo = lstatSync(source);
    if (sourceInfo.isSymbolicLink() || !sourceInfo.isFile() || !isInside(sourceFilesRoot, realpathSync(source))) {
      throw new Error("asset promotion source must be a physical regular file inside the Canvas project files directory");
    }
    const expectedSourceHash = basename(element.src, extname(element.src)).toLowerCase();
    if (!/^[0-9a-f]{64}$/.test(expectedSourceHash)) {
      throw new Error("asset promotion source must use the Canvas content-addressed filename contract");
    }

    const createdDirectories = [];
    let destination;
    let manifestPath;
    let packPath;
    let fileTemp;
    let manifestTemp;
    let packTemp;
    let manifestBackup;
    let packDir;
    let transaction;
    try {
      const assetsRoot = ensurePlainChild(gameRoot, "assets", createdDirectories);
      const packsRoot = ensurePlainChild(assetsRoot, "packs", createdDirectories);
      packDir = ensurePlainChild(packsRoot, PACK_ID, createdDirectories);
      recoverPromotionTransaction(packDir);
      const filesRoot = ensurePlainChild(packDir, "files", createdDirectories);
      const assetDir = ensurePlainChild(filesRoot, cleanMetadata.asset_id, createdDirectories);
      manifestPath = join(packDir, "assets.jsonl");
      packPath = join(packDir, "pack.json");
      const fileName = promotedFileName(element);
      const resource = `files/${cleanMetadata.asset_id}/${fileName}`;
      destination = join(assetDir, fileName);
      const rows = readRows(manifestPath);
      if (rows.some((row) => row.asset_id === cleanMetadata.asset_id) || existsSync(destination)) {
        throw new Error(`asset promotion destination already exists for ${cleanMetadata.asset_id}`);
      }
      if (dependencies.insideLock) await dependencies.insideLock({ projectId, gameId });

      const suffix = randomUUID();
      fileTemp = `${destination}.${suffix}.tmp`;
      manifestTemp = `${manifestPath}.${suffix}.tmp`;
      packTemp = `${packPath}.${suffix}.tmp`;
      manifestBackup = `${manifestPath}.${suffix}.bak`;
      transaction = {
        schema: "ai_studio.canvas.asset_promotion_transaction.v1",
        state: "prepared",
        asset_id: cleanMetadata.asset_id,
        file_name: fileName,
        suffix,
        manifest_existed: existsSync(manifestPath),
        pack_existed: existsSync(packPath),
      };
      writeTransactionMarker(packDir, transaction);
      copyFileSync(source, fileTemp);
      const bytes = statSync(fileTemp).size;
      const sha256 = createHash("sha256").update(readFileSync(fileTemp)).digest("hex");
      if (sha256 !== expectedSourceHash) {
        throw new Error("asset promotion source bytes no longer match the accepted content-addressed Canvas reference");
      }
      const canvasRef = privateStore
        ? `canvas://game/${gameId}/${projectId}/element/${elementId}`
        : `canvas://${projectId}/element/${elementId}`;
      const record = {
        ...cleanMetadata,
        resource,
        classification: "product-asset",
        sha256,
        bytes,
        canvas_ref: canvasRef,
        canvas_source_ref: element.src,
        style_lock_id: resolved.lock.id,
        style_decision: {
          decision: decision.decision,
          decided_at: decision.decided_at,
          reason: decision.reason,
          advisory_verdict: decision.advisory_verdict,
          advisory_checked_at: decision.advisory_checked_at,
        },
        promoted_at: new Date().toISOString(),
      };
      writeFileSync(manifestTemp, [...rows, record].map((row) => JSON.stringify(row)).join("\n") + "\n", "utf8");
      if (!existsSync(packPath)) {
        writeFileSync(packTemp, `${JSON.stringify({
          pack: PACK_ID,
          title: "Canvas Promotions",
          source: "canvas",
          description: "Lead-accepted game-local assets promoted from AI Studio Canvas.",
        }, null, 2)}\n`, "utf8");
      }
      if (dependencies.beforeCommit) await dependencies.beforeCommit({ fileTemp, manifestTemp, packTemp, destination });
      renameSync(fileTemp, destination);
      if (dependencies.afterDestinationCommit) await dependencies.afterDestinationCommit({ destination });
      if (existsSync(packTemp)) {
        renameSync(packTemp, packPath);
      }
      if (existsSync(manifestPath)) renameSync(manifestPath, manifestBackup);
      renameSync(manifestTemp, manifestPath);
      writeTransactionMarker(packDir, { ...transaction, state: "committed" });
      // The committed marker makes cleanup idempotent after a crash. A cleanup
      // failure is safe to defer to the next promotion; the manifest+binary are
      // already committed and recovery will preserve them.
      try {
        rmSync(join(packDir, TRANSACTION_PREPARED), { force: true });
        if (dependencies.afterPreparedMarkerCleanup) await dependencies.afterPreparedMarkerCleanup({ destination, manifestPath });
        rmSync(manifestBackup, { force: true });
        rmSync(join(packDir, TRANSACTION_COMMITTED), { force: true });
      } catch { /* committed recovery finishes this on the next invocation */ }
      return { projectId, elementId, gameId, source, destination, manifestPath, packPath, record };
    } catch (error) {
      if (packDir && hasPromotionTransaction(packDir)) {
        recoverPromotionTransaction(packDir);
      } else {
        for (const path of [fileTemp, manifestTemp, packTemp]) if (path) rmSync(path, { force: true });
      }
      for (const path of createdDirectories.reverse()) {
        try { rmdirSync(path); } catch { /* preserve non-empty/pre-existing state */ }
      }
      throw error;
    }
  });
}

export function promoteAssetToGame(root, args = {}) {
  return promoteAssetToGameImpl(root, args);
}

export function __promoteAssetToGameForTest(root, args = {}, dependencies = {}) {
  return promoteAssetToGameImpl(root, args, dependencies);
}

export function __withPromotionLockForTest(root, gameId, fn, options = {}) {
  return withPromotionLock(root, gameId, fn, options);
}
