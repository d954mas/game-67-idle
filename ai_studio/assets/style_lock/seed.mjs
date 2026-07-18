#!/usr/bin/env node

import { createHash, randomUUID } from "node:crypto";
import {
  closeSync,
  existsSync,
  linkSync,
  lstatSync,
  mkdirSync,
  openSync,
  readFileSync,
  realpathSync,
  renameSync,
  rmSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, extname, isAbsolute, join, relative, resolve, sep } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import {
  addImage,
  assignToGroup,
  createGroup,
  createProject,
  createStyleCard,
  deleteProject,
  getProject,
  patchStyle,
  resolveProjectFile,
  listProjects,
} from "../canvas/ops.mjs";
import { canvasLocalCacheRoot } from "../canvas/config.mjs";
import { selectCanvasStore, studioCanvasStore, withCanvasStore } from "../canvas/stores.mjs";
import { validateStyleLockFile } from "./validate.mjs";

const DEFAULT_WORKSPACE_ROOT = resolve(fileURLToPath(new URL("../../../", import.meta.url)));
const GAME_ID = /^[a-z][a-z0-9-]*$/;
const CANVAS_IMAGE_EXTENSIONS = new Set([".png", ".jpg", ".jpeg", ".gif", ".webp"]);
const SEED_LOCK_STALE_MS = 30000;
const SEED_LOCK_RETRY_MS = 2000;
const SEED_LOCK_INTERVAL_MS = 100;

function samePath(left, right) {
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
  if (info.isSymbolicLink() || !info.isDirectory() || !samePath(realpathSync(lexical), lexical)) {
    throw new Error(`${label} must be a physical directory, not a symlink or junction`);
  }
  return lexical;
}

function resolveTargetGame(root, gameId) {
  const workspace = realpathSync(resolve(root));
  const candidates = [
    { gameRoot: join(workspace, "games", gameId), privateStore: false },
    { gameRoot: join(workspace, "games", "private", gameId), privateStore: true },
  ].filter((entry) => existsSync(entry.gameRoot));
  if (candidates.length !== 1) {
    throw new Error(candidates.length
      ? `style lock seed found ambiguous public/private game roots for ${gameId}`
      : `style lock seed could not find target game ${gameId}`);
  }
  const gameRoot = requirePlainDirectory(candidates[0].gameRoot, "target game root");
  if (!isInside(workspace, gameRoot)) throw new Error("target game root escaped the workspace");
  const designRoot = requirePlainDirectory(join(gameRoot, "design"), "target design directory");
  return { workspace, gameRoot, designRoot, privateStore: candidates[0].privateStore };
}

function canvasRef(value, kind) {
  const pattern = kind === "group"
    ? /^canvas:\/\/(?:(?:game\/([a-z][a-z0-9-]*)\/)?)([^/]+)\/group\/([^/]+)$/
    : /^canvas:\/\/(?:(?:game\/([a-z][a-z0-9-]*)\/)?)([^/]+)\/element\/([^/]+)$/;
  const match = pattern.exec(value || "");
  if (!match) throw new Error(`style lock seed requires a valid Canvas ${kind} ref`);
  return { gameId: match[1] || "", projectId: match[2], nodeId: match[3] };
}

function sourceStore(root, sourceLock) {
  const parsed = canvasRef(sourceLock.canvas_ref, "group");
  return parsed.gameId ? selectCanvasStore(root, { game: parsed.gameId }) : studioCanvasStore(root);
}

function readSourceExemplars(root, sourceLock, sourcePath) {
  const groupRef = canvasRef(sourceLock.canvas_ref, "group");
  const store = sourceStore(root, sourceLock);
  return withCanvasStore(store, () => {
    const project = getProject(root, groupRef.projectId);
    if (project.ownership?.kind !== "game" || project.ownership.gameId !== sourceLock.game_id) {
      throw new Error("past-game style lock Canvas project ownership does not match its game_id");
    }
    if (!(project.groups || []).some((group) => group.id === groupRef.nodeId)) {
      throw new Error("past-game style lock Canvas style group does not exist");
    }
    return sourceLock.exemplar_refs.map((entry) => {
      const ref = canvasRef(entry.ref, "element");
      if (ref.projectId !== project.id) throw new Error("past-game exemplar escaped the style Canvas project");
      const element = (project.elements || []).find((item) => item.id === ref.nodeId);
      if (!element || element.type !== "image" || !element.src) throw new Error("past-game exemplar must identify an image");
      const path = resolveProjectFile(root, project.id, element.src);
      const filesRoot = requirePlainDirectory(dirname(path), "past-game Canvas files directory");
      const info = lstatSync(path);
      if (info.isSymbolicLink() || !info.isFile() || !isInside(filesRoot, realpathSync(path))) {
        throw new Error("past-game exemplar must be a physical Canvas file");
      }
      const extension = extname(element.src).toLowerCase();
      const expectedHash = basename(element.src, extension).toLowerCase();
      if (!CANVAS_IMAGE_EXTENSIONS.has(extension) || !/^[0-9a-f]{64}$/.test(expectedHash)) {
        throw new Error("past-game exemplar must use a Canvas content-addressed image filename");
      }
      const bytes = readFileSync(path);
      const actualHash = createHash("sha256").update(bytes).digest("hex");
      if (actualHash !== expectedHash) {
        throw new Error("past-game exemplar bytes no longer match the accepted content-addressed Canvas reference");
      }
      return {
        bytes,
        name: element.name,
        domain: entry.domain,
        sourceCanvasRef: entry.ref,
        sourceFileRef: element.src,
        sourceLock: relative(realpathSync(root), realpathSync(sourcePath)).replaceAll("\\", "/"),
      };
    });
  });
}

function targetCanvasStore(root, gameId, privateStore) {
  return privateStore ? selectCanvasStore(root, { game: gameId }) : studioCanvasStore(root);
}

function targetCanvasBase(privateStore, gameId, projectId) {
  return privateStore ? `canvas://game/${gameId}/${projectId}` : `canvas://${projectId}`;
}

function writeExclusiveJson(path, value) {
  let fd;
  let created = false;
  try {
    fd = openSync(path, "wx");
    created = true;
    writeFileSync(fd, `${JSON.stringify(value, null, 2)}\n`, "utf8");
    closeSync(fd);
    fd = undefined;
  } catch (error) {
    if (fd !== undefined) closeSync(fd);
    if (created) rmSync(path, { force: true });
    throw error;
  }
}

const heldSeedLocks = new Map();
const heldSeedClaims = new Set();
let seedExitCleanupRegistered = false;

function seedCachePaths(root, gameId) {
  const workspaceKey = createHash("sha256").update(realpathSync(root)).digest("hex").slice(0, 16);
  const dir = join(canvasLocalCacheRoot(root), "style-lock-seed", workspaceKey, gameId);
  return {
    dir,
    lock: join(dir, ".lock"),
    reclaim: join(dir, ".lock.reclaim"),
    reclaimOwner: join(dir, ".lock.reclaim", "owner.json"),
    reclaimLock: join(dir, ".lock.reclaim", "lock"),
    marker: join(dir, "transaction.json"),
    markerTemp: join(dir, "transaction.json.tmp"),
  };
}

function readJsonOrNull(path) {
  try { return JSON.parse(readFileSync(path, "utf8")); } catch { return null; }
}

function processIsAlive(pid) {
  if (!Number.isInteger(pid) || pid <= 0) return false;
  try { process.kill(pid, 0); return true; } catch (error) { return error.code === "EPERM"; }
}

function releaseSeedLock(lock) {
  if (!lock) return;
  const current = readJsonOrNull(lock.path);
  heldSeedLocks.delete(lock.path);
  if (current?.token === lock.token) rmSync(lock.path, { force: true });
}

function ensureSeedExitCleanup() {
  if (seedExitCleanupRegistered) return;
  seedExitCleanupRegistered = true;
  process.on("exit", () => {
    for (const [path, token] of heldSeedLocks) {
      try { releaseSeedLock({ path, token }); } catch { /* best effort during process exit */ }
    }
    for (const path of heldSeedClaims) {
      try { rmSync(path, { recursive: true, force: true }); } catch { /* best effort during process exit */ }
    }
  });
}

function sleepSync(milliseconds) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, milliseconds);
}

function releaseSeedClaim(paths) {
  heldSeedClaims.delete(paths.reclaim);
  rmSync(paths.reclaim, { recursive: true, force: true });
}

function recoverAbandonedSeedClaim(paths) {
  const owner = readJsonOrNull(paths.reclaimOwner);
  const age = owner && Number.isFinite(owner.startedAt)
    ? Date.now() - owner.startedAt
    : (() => { try { return Date.now() - lstatSync(paths.reclaim).mtimeMs; } catch { return 0; } })();
  if (processIsAlive(owner?.pid) || (!owner && age <= SEED_LOCK_STALE_MS)) return false;

  // Rename the exact claim directory before deleting it. A new breaker may
  // immediately create the fixed path without an old recovery deleting it.
  const abandoned = `${paths.reclaim}.${randomUUID()}.abandoned`;
  try {
    renameSync(paths.reclaim, abandoned);
  } catch (error) {
    if (error.code === "ENOENT" || error.code === "EEXIST") return true;
    throw error;
  }
  rmSync(abandoned, { recursive: true, force: true });
  return true;
}

function acquireSeedLock(root, gameId, hooks = {}) {
  const paths = seedCachePaths(root, gameId);
  mkdirSync(paths.dir, { recursive: true });
  const deadline = Date.now() + SEED_LOCK_RETRY_MS;
  let ownsReclaim = false;
  for (;;) {
    if (existsSync(paths.reclaim) && !ownsReclaim) {
      if (recoverAbandonedSeedClaim(paths)) continue;
      if (Date.now() >= deadline) throw new Error(`style lock seed reclaim for ${gameId} is already in progress`);
      sleepSync(SEED_LOCK_INTERVAL_MS);
      continue;
    }
    let fd;
    let created = false;
    try {
      fd = openSync(paths.lock, "wx");
      created = true;
      const token = randomUUID();
      writeFileSync(fd, JSON.stringify({ pid: process.pid, startedAt: Date.now(), token }));
      const lock = { path: paths.lock, token };
      heldSeedLocks.set(lock.path, token);
      if (ownsReclaim) {
        releaseSeedClaim(paths);
        ownsReclaim = false;
      }
      ensureSeedExitCleanup();
      return lock;
    } catch (error) {
      if (error.code !== "EEXIST") {
        if (created) rmSync(paths.lock, { force: true });
        if (ownsReclaim) {
          releaseSeedClaim(paths);
          ownsReclaim = false;
        }
      }
      if (error.code !== "EEXIST") throw error;
      const info = readJsonOrNull(paths.lock);
      const age = info && Number.isFinite(info.startedAt)
        ? Date.now() - info.startedAt
        : (() => { try { return Date.now() - lstatSync(paths.lock).mtimeMs; } catch { return 0; } })();
      if (age > SEED_LOCK_STALE_MS && !processIsAlive(info?.pid) && !ownsReclaim) {
        try {
          // The directory is the atomic breaker election; the hard link inside
          // fixes the exact old inode and removes the read-decide-delete ABA.
          mkdirSync(paths.reclaim);
          ownsReclaim = true;
          heldSeedClaims.add(paths.reclaim);
          ensureSeedExitCleanup();
          writeExclusiveJson(paths.reclaimOwner, {
            pid: process.pid,
            startedAt: Date.now(),
            token: randomUUID(),
            staleToken: info?.token || null,
          });
          linkSync(paths.lock, paths.reclaimLock);
          const claimed = readJsonOrNull(paths.reclaimLock);
          const lockStat = statSync(paths.lock);
          const claimStat = statSync(paths.reclaimLock);
          if (claimed?.token !== info?.token || lockStat.dev !== claimStat.dev || lockStat.ino !== claimStat.ino) {
            releaseSeedClaim(paths);
            ownsReclaim = false;
            continue;
          }
          if (hooks.afterReclaimLinked) hooks.afterReclaimLinked({ reclaimPath: paths.reclaim });
          rmSync(paths.lock, { force: true });
          if (hooks.afterReclaimUnlink) hooks.afterReclaimUnlink({ reclaimPath: paths.reclaim });
          continue;
        } catch (claimError) {
          if (ownsReclaim) {
            releaseSeedClaim(paths);
            ownsReclaim = false;
          }
          if (claimError.code !== "EEXIST" && claimError.code !== "ENOENT") throw claimError;
        }
      }
      if (Date.now() >= deadline) {
        if (ownsReclaim) {
          releaseSeedClaim(paths);
          ownsReclaim = false;
        }
        throw new Error(`style lock seed for ${gameId} is locked by another process`);
      }
      sleepSync(SEED_LOCK_INTERVAL_MS);
    } finally {
      if (fd !== undefined) closeSync(fd);
    }
  }
}

function withSeedLock(root, gameId, fn, hooks = {}) {
  const lock = acquireSeedLock(root, gameId, hooks);
  try { return fn(); } finally { releaseSeedLock(lock); }
}

function writeSeedMarker(paths, marker) {
  let fd;
  let created = false;
  try {
    fd = openSync(paths.markerTemp, "wx");
    created = true;
    writeFileSync(fd, `${JSON.stringify(marker)}\n`, "utf8");
    closeSync(fd);
    fd = undefined;
    renameSync(paths.markerTemp, paths.marker);
  } catch (error) {
    if (fd !== undefined) closeSync(fd);
    if (created) rmSync(paths.markerTemp, { force: true });
    throw error;
  }
}

function validateSeedMarker(marker, gameId) {
  if (marker?.schema !== "ai_studio.style_lock.seed_transaction.v1"
      || marker.game_id !== gameId
      || typeof marker.project_title !== "string"
      || !marker.project_title.includes(marker.token)
      || !/^[0-9a-f-]{36}$/.test(marker.token || "")
      || typeof marker.lock_temp_name !== "string"
      || !/^style_lock\.json\.[0-9a-f-]{36}\.tmp$/.test(marker.lock_temp_name)) {
    throw new Error("style lock seed transaction marker is malformed; refusing unsafe recovery");
  }
  return marker;
}

function recoverSeedTransaction(root, target, store, gameId, lockPath, options = {}) {
  const paths = seedCachePaths(root, gameId);
  rmSync(paths.markerTemp, { force: true });
  if (!existsSync(paths.marker)) return false;
  const marker = validateSeedMarker(readJsonOrNull(paths.marker), gameId);
  const lockTemp = join(target.designRoot, marker.lock_temp_name);
  if (!isInside(target.designRoot, lockTemp)) throw new Error("style lock seed temp escaped target design");
  if (existsSync(lockPath) && options.preparedOnly !== true) {
    const lock = validateStyleLockFile(lockPath, { workspaceRoot: root });
    if (lock.game_id !== gameId) throw new Error("style lock seed recovery found a foreign target lock");
    const ref = canvasRef(lock.canvas_ref, "group");
    const project = withCanvasStore(store, () => getProject(root, ref.projectId));
    if (project.title !== marker.project_title || project.ownership?.gameId !== gameId) {
      throw new Error("style lock seed recovery found a target lock that does not own the transaction project");
    }
  } else {
    const matches = withCanvasStore(store, () => listProjects(root).filter((project) =>
      project.title === marker.project_title
      && project.ownership?.kind === "game"
      && project.ownership.gameId === gameId));
    if (matches.length > 1) throw new Error("style lock seed recovery found multiple transaction projects");
    if (matches.length === 1) withCanvasStore(store, () => deleteProject(root, { projectId: matches[0].id }));
  }
  rmSync(lockTemp, { force: true });
  rmSync(paths.marker, { force: true });
  return true;
}

export function seedStyleLock(root, { gameId, from } = {}, dependencies = {}) {
  const targetGameId = String(gameId || "").trim();
  if (!GAME_ID.test(targetGameId)) throw new Error("style lock seed --game must be a lowercase game id");
  if (!from || typeof from !== "string") throw new Error("style lock seed requires --from <past-game-lock>");
  const workspace = realpathSync(resolve(root));
  const sourcePath = isAbsolute(from) ? resolve(from) : resolve(workspace, from);
  const sourceLock = validateStyleLockFile(sourcePath, { workspaceRoot: workspace });
  if (sourceLock.status !== "accepted") throw new Error("style lock seed requires an accepted past-game style lock");
  if (sourceLock.game_id === targetGameId) throw new Error("style lock seed requires a different target game");

  const target = resolveTargetGame(workspace, targetGameId);
  const lockPath = join(target.designRoot, "style_lock.json");
  const targetArtContract = resolve(target.gameRoot, sourceLock.art_contract_ref);
  if (!existsSync(targetArtContract) || !statSync(targetArtContract).isFile()
      || !isInside(target.designRoot, realpathSync(targetArtContract))) {
    throw new Error(`target game must provide ${sourceLock.art_contract_ref} before style seeding`);
  }
  const exemplars = readSourceExemplars(workspace, sourceLock, sourcePath);
  const store = targetCanvasStore(workspace, targetGameId, target.privateStore);
  return withSeedLock(workspace, targetGameId, () => {
    recoverSeedTransaction(workspace, target, store, targetGameId, lockPath);
    if (existsSync(lockPath)) throw new Error(`target style lock already exists: ${lockPath}`);
    const token = randomUUID();
    const projectTitle = `${targetGameId} style seed from ${sourceLock.id} [${token}]`;
    const lockTempName = `style_lock.json.${token}.tmp`;
    const transactionPaths = seedCachePaths(workspace, targetGameId);
    writeSeedMarker(transactionPaths, {
      schema: "ai_studio.style_lock.seed_transaction.v1",
      game_id: targetGameId,
      token,
      project_title: projectTitle,
      lock_temp_name: lockTempName,
    });
    let committed = false;
    try {
      const seeded = withCanvasStore(store, () => {
      const project = createProject(workspace, {
        title: projectTitle,
        ownership: { kind: "game", gameId: targetGameId },
      });
      if (dependencies.afterProjectCreated) dependencies.afterProjectCreated({ projectId: project.id, projectTitle });
      const elements = exemplars.map((entry, index) => addImage(workspace, project.id, {
        name: entry.name || `${entry.domain}-exemplar-${index + 1}.png`,
        bytes: entry.bytes,
        x: index * 112,
        y: 0,
        meta: {
          style_seed: {
            schema: "ai_studio.canvas.style_seed.v1",
            source_lock: entry.sourceLock,
            source_style_id: sourceLock.id,
            source_canvas_ref: entry.sourceCanvasRef,
            source_file_ref: entry.sourceFileRef,
            domain: entry.domain,
          },
        },
      }).element);
      const group = createGroup(workspace, {
        projectId: project.id,
        name: "style",
        fromElements: elements.map((element) => element.id),
      }).group;
      const passport = createStyleCard(workspace, {
        projectId: project.id,
        name: "passport",
        parentId: group.id,
        x: group.x,
        y: group.y,
        w: Math.max(group.w, 360),
        h: Math.max(group.h, 280),
      }).group;
      assignToGroup(workspace, { projectId: project.id, elementIds: elements.map((element) => element.id), groupId: passport.id });
      patchStyle(workspace, {
        projectId: project.id,
        groupId: passport.id,
        patch: { prompt: sourceLock.prompt_preamble, ref: elements[0].id },
      });
      for (const [name, x] of [["palette", 400], ["references", 680], ["do-dont", 960]]) {
        createGroup(workspace, { projectId: project.id, name, parentId: group.id, x, y: 0, w: 240, h: 180 });
      }
        return { project: getProject(workspace, project.id), group, passport, elements };
      });
      const base = targetCanvasBase(target.privateStore, targetGameId, seeded.project.id);
      const lock = {
        ...structuredClone(sourceLock),
        id: `${targetGameId}-style-v1`,
        game_id: targetGameId,
        status: "draft",
        canvas_ref: `${base}/group/${seeded.group.id}`,
        exemplar_refs: seeded.elements.map((element, index) => ({
          ref: `${base}/element/${element.id}`,
          origin: "owned",
          domain: exemplars[index].domain,
        })),
      };
      const lockTemp = join(target.designRoot, lockTempName);
      writeExclusiveJson(lockTemp, lock);
      if (dependencies.afterLockTempWritten) dependencies.afterLockTempWritten({ projectId: seeded.project.id, lockTemp });
      linkSync(lockTemp, lockPath);
      committed = true;
      rmSync(lockTemp, { force: true });
      if (dependencies.afterLockCommitted) dependencies.afterLockCommitted({ projectId: seeded.project.id, lockPath });
      validateStyleLockFile(lockPath, { workspaceRoot: workspace });
      rmSync(transactionPaths.marker, { force: true });
      return {
        gameId: targetGameId,
        sourceGameId: sourceLock.game_id,
        sourceLock: relative(workspace, realpathSync(sourcePath)).replaceAll("\\", "/"),
        lockPath,
        projectId: seeded.project.id,
        groupId: seeded.group.id,
        passportId: seeded.passport.id,
        copiedExemplars: seeded.elements.length,
      };
    } catch (error) {
      try {
        recoverSeedTransaction(workspace, target, store, targetGameId, lockPath, { preparedOnly: !committed });
      } catch (cleanupError) {
        throw new AggregateError([error, cleanupError], `style lock seed failed and transaction cleanup also failed: ${cleanupError.message}`);
      }
      throw error;
    }
  }, dependencies.lockHooks || {});
}

export function parseArgs(argv) {
  const result = { gameId: "", from: "", root: "", help: false };
  for (let index = 0; index < argv.length; index += 1) {
    const flag = argv[index];
    if (flag === "--help" || flag === "-h") result.help = true;
    else if (["--game", "--from", "--root"].includes(flag)) {
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) throw new Error(`${flag} requires a value`);
      index += 1;
      if (flag === "--game") result.gameId = value;
      else if (flag === "--from") result.from = value;
      else result.root = value;
    } else throw new Error(`unknown argument: ${flag}`);
  }
  return result;
}

export function __seedCachePathsForTest(root, gameId) {
  return seedCachePaths(root, gameId);
}

export function usageText() {
  return "usage: node ai_studio/assets/style_lock/seed.mjs --game <new-game-id> --from <games/<past-id>/design/style_lock.json> [--root <repo>]";
}

export function main(argv = process.argv.slice(2)) {
  try {
    const args = parseArgs(argv);
    if (args.help) {
      console.log(usageText());
      return 0;
    }
    const root = args.root ? resolve(args.root) : DEFAULT_WORKSPACE_ROOT;
    console.log(JSON.stringify(seedStyleLock(root, args), null, 2));
    return 0;
  } catch (error) {
    console.error(error?.message || String(error));
    return 1;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  process.exitCode = main();
}
