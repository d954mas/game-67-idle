// Canvas project persistence.
//
// One project is one folder under the configured canvas projects root:
//   <projectsRoot>/<project-id>/
//     project.json           schema ai_studio.canvas.project.v1
//     files/<file>           immutable, content-addressed image bytes
//
// Every write is atomic (temp file + rename). Image files are never mutated or
// deleted from disk: removeElement only drops the element from project.json so
// prior tool runs and references stay valid. All ids/filenames are path-confined.
//
// The projects root is resolved lazily via studio config (CANVAS_PROJECTS_ROOT
// env overrides for tests) and is only created on first project create.
import { randomUUID } from "node:crypto";
import {
  appendFileSync,
  closeSync,
  copyFileSync,
  existsSync,
  fstatSync,
  mkdirSync,
  openSync,
  readFileSync,
  readSync,
  readdirSync,
  renameSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, extname, join, resolve, sep } from "node:path";
import { canvasLocalCacheRoot, canvasProjectsRoot } from "../../core_harness/tool_lib/studio_config.mjs";
import { sha256Hex } from "../../core_harness/tool_lib/hash.mjs";

export const CANVAS_PROJECT_SCHEMA = "ai_studio.canvas.project.v1";

const IMAGE_EXTENSIONS = ["png", "jpg", "jpeg", "gif", "webp"];

function projectsRoot(root) {
  return canvasProjectsRoot(root);
}

function nowIso() {
  return new Date().toISOString();
}

function slugify(value) {
  const slug = String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 48);
  return slug || "project";
}

// Reject anything that could escape the projects root (separators, traversal,
// dotfiles) and confine the resolved path under `parent`.
function confineChild(parent, name, label) {
  if (!name || typeof name !== "string" || /[\\/]/.test(name) || name.includes("..") || name.startsWith(".")) {
    throw new Error(`unsafe ${label}: ${JSON.stringify(name)}`);
  }
  const base = resolve(parent);
  const full = resolve(base, name);
  if (full !== base && !full.startsWith(base + sep)) {
    throw new Error(`${label} escapes its root: ${JSON.stringify(name)}`);
  }
  return full;
}

function projectDir(root, id) {
  return confineChild(projectsRoot(root), id, "project id");
}

function projectJsonPath(root, id) {
  return join(projectDir(root, id), "project.json");
}

// ---- local cache location (T0259) --------------------------------------------
//
// The per-gesture history subsystem — journal.jsonl, sidecar snapshots/, the compaction
// archive + fat-journal backup, and the cross-process .lock — lives in a LOCAL, per-machine
// cache OFF the (cloud-synced) projects folder, so a gesture no longer churns ~0.5MB of sync
// traffic. project.json and files/ STAY in the synced project dir (current state + assets
// still travel); undo history deliberately does not. The cache is keyed by a short hash of
// the RESOLVED projects root so two different store roots — including parallel tests reusing
// one project id under different temp roots — can NEVER share a cache entry: collision is
// impossible by construction, not by test discipline.
function cacheProjectDir(root, id) {
  const base = join(canvasLocalCacheRoot(root), sha256Hex(projectsRoot(root)).slice(0, 16));
  return confineChild(base, id, "project id");
}

// Absolute cache paths for a project's per-machine history subsystem. Exported so tooling
// and tests read the relocated layout through the store instead of re-deriving the rootHash
// by hand — journal/snapshot/lock path knowledge stays in this one module.
export function projectCachePaths(root, id) {
  const dir = cacheProjectDir(root, id);
  return {
    dir,
    journal: join(dir, "journal.jsonl"),
    backup: join(dir, "journal.jsonl.bak"),
    archive: join(dir, "journal.archive.jsonl"),
    snapshots: join(dir, "snapshots"),
    lock: join(dir, ".lock"),
  };
}

// One-time move-on-first-access of any PRE-T0259 in-project history (journal.jsonl, sidecar
// snapshots/, the archive, and the fat-journal .bak) into the local cache. Chosen over a
// read-fallback because the journal is a SINGLE append-only file: split across two locations
// it could neither seq-continue nor undo coherently. Idempotent + crash-safe + cross-volume
// safe (the project dir may sit on a different drive than the cache): each file is copied
// atomically (tmp+rename on the cache volume) and only THEN removed from the project dir; a
// crash mid-move re-runs on next access (the O(1) gate still sees the leftover legacy files),
// and an existing cache file is NEVER overwritten — a completed atomic copy or already-live
// cache history wins, the stale legacy copy is just dropped. The .lock is deliberately not
// migrated (locks are per-machine + transient; a synced one is meaningless here).
const relocatedCacheDirs = new Set(); // cacheProjectDir paths reconciled with legacy layout this process

function copyFileAtomic(from, to) {
  const tmp = `${to}.tmp-${randomUUID().slice(0, 8)}`;
  copyFileSync(from, tmp);
  renameSync(tmp, to);
}

function relocateLegacyFile(from, to) {
  if (!existsSync(from)) return;
  // Never clobber an existing cache file: it is either this move's own completed atomic copy
  // (crash retry) or newer live cache history — the legacy copy is stale either way.
  if (!existsSync(to)) {
    mkdirSync(dirname(to), { recursive: true });
    copyFileAtomic(from, to);
  }
  rmSync(from, { force: true });
}

function relocateLegacyHistory(root, id) {
  const cacheDir = cacheProjectDir(root, id);
  if (relocatedCacheDirs.has(cacheDir)) return;
  const pdir = projectDir(root, id);
  const legacyJournal = join(pdir, "journal.jsonl");
  const legacySnapshots = join(pdir, "snapshots");
  // O(1) gate: no pre-T0259 history in the (synced) project dir → nothing to relocate. Never
  // mkdir the cache here for an empty project — a lock/read on an id with no history must
  // leave zero trace (mirrors acquireFileLock's "creates no folder" contract).
  if (existsSync(legacyJournal) || existsSync(legacySnapshots)) {
    mkdirSync(cacheDir, { recursive: true });
    relocateLegacyFile(legacyJournal, join(cacheDir, "journal.jsonl"));
    relocateLegacyFile(`${legacyJournal}.bak`, join(cacheDir, "journal.jsonl.bak"));
    relocateLegacyFile(join(pdir, "journal.archive.jsonl"), join(cacheDir, "journal.archive.jsonl"));
    if (existsSync(legacySnapshots)) {
      const dest = join(cacheDir, "snapshots");
      mkdirSync(dest, { recursive: true });
      for (const name of readdirSync(legacySnapshots)) relocateLegacyFile(join(legacySnapshots, name), join(dest, name));
      rmSync(legacySnapshots, { recursive: true, force: true });
    }
  }
  relocatedCacheDirs.add(cacheDir);
}

function writeAtomic(filePath, contents) {
  const tmp = `${filePath}.tmp-${randomUUID().slice(0, 8)}`;
  writeFileSync(tmp, contents);
  renameSync(tmp, filePath);
}

function writeProjectFile(root, project) {
  const dir = projectDir(root, project.id);
  mkdirSync(dir, { recursive: true });
  writeAtomic(join(dir, "project.json"), `${JSON.stringify(project, null, 2)}\n`);
  return project;
}

function readProjectFile(root, id) {
  const path = projectJsonPath(root, id);
  if (!existsSync(path)) throw new Error(`canvas project not found: ${id}`);
  return JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
}

// PNG/JPEG/GIF intrinsic dimensions from header bytes only. Pure, no deps.
export function imageSize(buffer) {
  if (
    buffer.length >= 24 &&
    buffer[0] === 0x89 && buffer[1] === 0x50 && buffer[2] === 0x4e && buffer[3] === 0x47
  ) {
    return { width: buffer.readUInt32BE(16), height: buffer.readUInt32BE(20) };
  }
  if (buffer.length >= 10 && buffer.toString("ascii", 0, 3) === "GIF") {
    return { width: buffer.readUInt16LE(6), height: buffer.readUInt16LE(8) };
  }
  if (buffer.length >= 4 && buffer[0] === 0xff && buffer[1] === 0xd8) {
    let offset = 2;
    while (offset + 9 < buffer.length) {
      if (buffer[offset] !== 0xff) {
        offset += 1;
        continue;
      }
      const marker = buffer[offset + 1];
      const isSof = marker >= 0xc0 && marker <= 0xcf && ![0xc4, 0xc8, 0xcc].includes(marker);
      if (isSof) {
        return { width: buffer.readUInt16BE(offset + 7), height: buffer.readUInt16BE(offset + 5) };
      }
      const length = buffer.readUInt16BE(offset + 2);
      if (length < 2) break;
      offset += 2 + length;
    }
  }
  throw new Error("unsupported image: expected PNG, JPEG, or GIF header bytes");
}

function sniffExtension(buffer) {
  if (buffer.length >= 4 && buffer[0] === 0x89 && buffer[1] === 0x50) return "png";
  if (buffer.length >= 3 && buffer.toString("ascii", 0, 3) === "GIF") return "gif";
  if (buffer.length >= 2 && buffer[0] === 0xff && buffer[1] === 0xd8) return "jpg";
  if (buffer.length >= 12 && buffer.toString("ascii", 8, 12) === "WEBP") return "webp";
  return "png";
}

function imageExtension(name, buffer) {
  const fromName = extname(String(name || "")).toLowerCase().replace(/^\./, "");
  if (IMAGE_EXTENSIONS.includes(fromName)) return fromName === "jpeg" ? "jpg" : fromName;
  return sniffExtension(buffer);
}

export function createProject(root, { title } = {}) {
  const cleanTitle = String(title || "").trim() || "Untitled canvas";
  const id = `${slugify(cleanTitle)}-${randomUUID().slice(0, 6)}`;
  const now = nowIso();
  const project = {
    schema: CANVAS_PROJECT_SCHEMA,
    id,
    title: cleanTitle,
    created: now,
    updated: now,
    // history_seq is the undo/redo head: the seq of the currently applied journal
    // mutation (0 = base/empty). The op layer owns the journal semantics; the
    // store just carries this pointer through updateProject like any other field.
    history_seq: 0,
    // Groups are Figma-frame-like named screen regions (one level, no nesting).
    // Additive to ai_studio.canvas.project.v1: older projects load with no groups
    // and every reader tolerates a missing/empty groups array.
    groups: [],
    elements: [],
    tool_runs: [],
  };
  // mkdir is recursive so this also creates the projects root on first use.
  mkdirSync(join(projectDir(root, id), "files"), { recursive: true });
  return writeProjectFile(root, project);
}

export function listProjects(root) {
  const base = projectsRoot(root);
  if (!existsSync(base)) return [];
  const projects = [];
  for (const entry of readdirSync(base, { withFileTypes: true })) {
    if (!entry.isDirectory() || entry.name.startsWith(".")) continue;
    try {
      projects.push(readProjectFile(root, entry.name));
    } catch {
      // Tolerate broken/foreign project folders: skip rather than fail the list.
    }
  }
  projects.sort((a, b) => String(b.updated || "").localeCompare(String(a.updated || "")));
  return projects;
}

export function getProject(root, id) {
  return readProjectFile(root, id);
}

// Merge a shallow patch onto the project, always bumping `updated`. Immutable
// fields (schema/id/created) are never overwritten.
export function updateProject(root, id, patch = {}) {
  const project = readProjectFile(root, id);
  const { schema, id: _id, created, ...rest } = patch;
  const next = { ...project, ...rest, schema: project.schema, id: project.id, created: project.created, updated: nowIso() };
  return writeProjectFile(root, next);
}

function finiteOr(value, fallback) {
  return value !== undefined && value !== null && Number.isFinite(Number(value)) ? Number(value) : fallback;
}

// ---- per-project write lock ---------------------------------------------------
//
// T0254 Tier 1 #1: page + chat (same Node server process) and the CLI (a SEPARATE
// process) can now all mutate the same project.json concurrently — previously this
// was a "single-writer local tool" (see appendJournalLine's old comment above); that
// assumption is gone. withProjectLock(root, projectId, fn) serializes the CALLER-GIVEN
// critical section per project with two layers:
//   - in-process: a promise-chain mutex (projectLockQueues: projectDir -> tail
//     promise) so two concurrent callers IN THIS PROCESS (a page request and a chat
//     agent request, which share one server) queue instead of interleaving.
//   - cross-process: an advisory lockfile <project>/.lock ({pid, startedAt}), created
//     with O_EXCL so only one process can hold it at a time; a second process retries
//     with a short backoff (LOCK_RETRY_TOTAL_MS) before refusing loudly (naming the
//     holder pid + age); a lock older than LOCK_STALE_MS is treated as abandoned (its
//     holder crashed without releasing it) and is broken with a loud console.warn
//     rather than wedging the project forever.
// Callers choose WHAT to lock: wrapping a whole (fast) op call queues it cleanly end
// to end. A slow codex/agy generation op instead locks only its OWN final commit
// section (see generateFromRecipe and friends in ops.mjs) so the lock is never held
// across a multi-minute external call — see those ops' own comments. commitMutation
// (ops.mjs) additionally re-checks the project's actual head against the op's
// `before` snapshot; that check is what actually catches a race for the ops that
// don't hold this lock the whole time (a loud refusal instead of a silent merge).
const LOCK_STALE_MS = 30000; // an abandoned lock (crashed holder) this old is broken
const LOCK_RETRY_TOTAL_MS = 2000; // total time a caller waits on a live lock before refusing
const LOCK_RETRY_INTERVAL_MS = 100;

function lockFilePath(root, id) {
  // Per-machine + transient, so the lock lives in the LOCAL cache (T0259): a synced lockfile
  // from another machine would otherwise cause false cross-process refusals here.
  return join(cacheProjectDir(root, id), ".lock");
}

function readLockInfo(path) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch {
    return null; // torn/foreign lock file: treat as an unknown holder, not a crash
  }
}

// Acquire <project>/.lock exclusively (O_EXCL — only one process can ever hold it),
// retrying a live lock with a short backoff and breaking an abandoned (stale) one with
// a loud warning. Returns the lock path on success, or null when the project folder
// doesn't exist — deliberately NEVER mkdir's one into existence (a locked call on an
// unresolvable id must leave the same zero trace on disk it always did; the op itself
// throws "not found" moments later for the identical reason, so there is nothing here
// to protect). Throws a loud, holder-naming error if the deadline passes with a REAL
// lock still live.
async function acquireFileLock(root, id) {
  relocateLegacyHistory(root, id); // fold any pre-cache in-project history in before locking in the cache
  const dir = projectDir(root, id);
  if (!existsSync(dir)) return null; // no project folder to lock → leave zero trace (no cache dir either)
  mkdirSync(cacheProjectDir(root, id), { recursive: true }); // the .lock lives in the cache; ensure it exists
  const path = lockFilePath(root, id);
  const deadline = Date.now() + LOCK_RETRY_TOTAL_MS;
  let brokeStaleOnce = false; // never break a lock more than once per acquire: a lock
  // that keeps reappearing stale is a bug worth a loud refusal, not an infinite loop.
  for (;;) {
    let fd;
    try {
      fd = openSync(path, "wx"); // O_EXCL: throws EEXIST iff another holder is live
      writeFileSync(fd, JSON.stringify({ pid: process.pid, startedAt: Date.now() }));
      ensureExitCleanup();
      heldLockPaths.add(path);
      return path;
    } catch (error) {
      if (error.code !== "EEXIST") throw error;
      const info = readLockInfo(path);
      const age = info && Number.isFinite(info.startedAt) ? Date.now() - info.startedAt : Infinity;
      if (age > LOCK_STALE_MS && !brokeStaleOnce) {
        console.warn(
          `canvas: breaking stale project lock for ${id} (pid ${info && info.pid}, held ${Math.round(age / 1000)}s) at ${path}`,
        );
        rmSync(path, { force: true });
        brokeStaleOnce = true;
        continue;
      }
      if (Date.now() >= deadline) {
        const holder = info ? `pid ${info.pid}, held ${Math.round(age / 1000)}s` : "unknown holder";
        throw new Error(
          `canvas project ${id} is locked by another process (${holder}) — retry, or delete ${path} if it is stale`,
        );
      }
      await new Promise((resolveWait) => setTimeout(resolveWait, LOCK_RETRY_INTERVAL_MS));
    } finally {
      if (fd !== undefined) closeSync(fd);
    }
  }
}

function releaseFileLock(path) {
  heldLockPaths.delete(path);
  rmSync(path, { force: true });
}

// Safety net for the CLI's `fail()` (tool_lib/cli.mjs): it calls `process.exit(1)`
// directly on a validation error, which — unlike a thrown Error — skips every pending
// `finally` block, including runLocked's own release below. Without this, a CLI
// command that holds the lock and then hits a validation `fail()` deep inside would
// leak its lockfile, wedging the NEXT invocation on that project for up to
// LOCK_RETRY_TOTAL_MS (or worse, confusingly, until LOCK_STALE_MS if that next call
// also fails fast). `process.exit()` DOES still run synchronous "exit" listeners
// before the process actually dies, so a synchronous rmSync here is reliable; this
// only matters for the CLI (a one-shot process) — the long-lived server never calls
// process.exit() mid-request.
const heldLockPaths = new Set();
let exitCleanupRegistered = false;
function ensureExitCleanup() {
  if (exitCleanupRegistered) return;
  exitCleanupRegistered = true;
  process.on("exit", () => {
    for (const path of heldLockPaths) {
      try {
        rmSync(path, { force: true });
      } catch {
        // best-effort on shutdown — nothing left to report to
      }
    }
  });
}

const projectLockQueues = new Map(); // projectDir -> tail promise (always resolves)

async function runLocked(root, projectId, fn) {
  const lockPath = await acquireFileLock(root, projectId); // null: no project folder to lock
  try {
    return await fn();
  } finally {
    if (lockPath) releaseFileLock(lockPath);
  }
}

// Run `fn` (sync or async) exclusively for this project: queued in-process, then
// gated by the cross-process lockfile. `projectId` may be falsy for project-less
// calls (list/create) — those run immediately, unlocked, since there is no project
// folder yet to confine a lock under. Returns/throws fn's own result/error; the lock
// is always released (runLocked's finally), so a thrown error never wedges the
// project for the next caller.
export function withProjectLock(root, projectId, fn) {
  if (!projectId) return Promise.resolve().then(fn);
  const key = projectDir(root, projectId);
  const previousTail = projectLockQueues.get(key) || Promise.resolve();
  const run = previousTail.then(
    () => runLocked(root, projectId, fn),
    () => runLocked(root, projectId, fn), // an earlier caller's failure must not block this one
  );
  // The stored tail always resolves (never rejects) so it never poisons the queue.
  projectLockQueues.set(key, run.then(() => {}, () => {}));
  return run;
}

// Content-addressed write of image bytes into files/ WITHOUT creating an element.
// Returns { src, fileName, width, height }. Immutable + deduped exactly like addImage's
// own file write (identical bytes reuse the same file, so this never mutates or orphans
// an existing file). The alphaCutout op uses this to point an element at a NEW alpha PNG
// while the previous file stays on disk (the non-destructive src-swap pattern). Shared by
// addImage below so there is one content-addressing path.
export function addFile(root, id, { bytes, name } = {}) {
  const buffer = Buffer.isBuffer(bytes) ? bytes : Buffer.from(bytes || []);
  if (!buffer.length) throw new Error("addFile requires non-empty image bytes");
  const { width, height } = imageSize(buffer);
  const ext = imageExtension(name, buffer);
  const fileName = `${sha256Hex(buffer)}.${ext}`;
  const filesDir = join(projectDir(root, id), "files");
  mkdirSync(filesDir, { recursive: true });
  const filePath = join(filesDir, fileName);
  // Content-addressed and immutable: identical bytes reuse the same file.
  if (!existsSync(filePath)) writeAtomic(filePath, buffer);
  return { src: `files/${fileName}`, fileName, width, height };
}

export function addImage(root, id, { name, bytes, x = 0, y = 0, meta } = {}) {
  const { src, fileName, width, height } = addFile(root, id, { bytes, name });

  const project = readProjectFile(root, id);
  const element = {
    id: `el_${randomUUID().slice(0, 8)}`,
    type: "image",
    src,
    x: finiteOr(x, 0),
    y: finiteOr(y, 0),
    w: width,
    h: height,
    // Intrinsic source pixels, kept immutable so region overlays and future
    // resizes can scale by w/source_w even after the element box is resized.
    source_w: width,
    source_h: height,
    name: String(name || fileName),
    meta: meta && typeof meta === "object" ? meta : {},
  };
  project.elements = [...(project.elements || []), element];
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, element };
}

function findElement(project, elementId) {
  const element = (project.elements || []).find((item) => item.id === elementId);
  if (!element) throw new Error(`element not found: ${elementId}`);
  return element;
}

const NUMERIC_ELEMENT_FIELDS = ["x", "y", "w", "h"];

// Apply the mutable element fields from a patch onto an element IN PLACE: numeric
// x/y/w/h (finite values only), name (string), and the optional explicit visibility
// boolean (stored so renderGroup/the page can hide with `element.visible !== false`).
// For a TEXT element it also applies `content` (string) and `style` (a fully
// validated/normalized style OBJECT — the op layer does the fonts-manifest validation
// before this runs, so the store only stores the object verbatim). `rotation`/`flipH`/
// `flipV` (T0232 increment 3a — additive transform schema) are likewise pre-validated by
// the op layer (ops.sanitizeTransformPatch: finite degrees normalized to [0,360), real
// booleans, flip rejected on a text element) — this just stores them, mirroring
// `group.clip`'s "absent means the default" rule: `rotation === 0` and `flipH`/`flipV
// === false` are stored as an ABSENT field (an untouched/reset element stays clean; the
// stored shape never carries a redundant zero/false). Shared by patchElement and the
// batched patchElements so both honor identical rules.
function applyElementFields(element, patch = {}) {
  for (const field of NUMERIC_ELEMENT_FIELDS) {
    if (patch[field] !== undefined && patch[field] !== null && Number.isFinite(Number(patch[field]))) {
      element[field] = Number(patch[field]);
    }
  }
  if (patch.name !== undefined) element.name = String(patch.name);
  if (patch.visible !== undefined) {
    element.visible = !(patch.visible === false || patch.visible === "false");
  }
  if (patch.content !== undefined) element.content = String(patch.content);
  if (patch.style !== undefined && patch.style && typeof patch.style === "object") {
    element.style = patch.style;
  }
  if (patch.rotation !== undefined) {
    if (Number(patch.rotation) === 0) delete element.rotation;
    else element.rotation = Number(patch.rotation);
  }
  if (patch.flipH !== undefined) {
    if (patch.flipH === false) delete element.flipH;
    else element.flipH = true;
  }
  if (patch.flipV !== undefined) {
    if (patch.flipV === false) delete element.flipV;
    else element.flipV = true;
  }
  return element;
}

// Append a TEXT element (parallel to addImage, but no backing file: the content +
// style ARE the element). `style` is the fully validated/normalized style object and
// `name` the layer name — the op layer computes both before calling this. `w`/`h` are
// the nominal box (bookkeeping only; both renderers re-measure). Optional `groupId`
// drops the text straight into a group.
export function addText(root, id, { x = 0, y = 0, w = 0, h = 0, content = "Text", style = {}, name, groupId, meta } = {}) {
  const project = readProjectFile(root, id);
  const element = {
    id: `el_${randomUUID().slice(0, 8)}`,
    type: "text",
    x: finiteOr(x, 0),
    y: finiteOr(y, 0),
    w: finiteOr(w, 0),
    h: finiteOr(h, 0),
    content: String(content),
    style,
    name: String(name || "Text"),
    meta: meta && typeof meta === "object" ? meta : {},
  };
  if (groupId != null && groupId !== "") element.groupId = String(groupId);
  project.elements = [...(project.elements || []), element];
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, element };
}

export function patchElement(root, id, elementId, patch = {}) {
  const project = readProjectFile(root, id);
  const element = findElement(project, elementId);
  applyElementFields(element, patch);
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, element };
}

// Patch several elements in ONE project write. Every patch's `elementId` must
// resolve (findElement throws before updateProject, so a bad id aborts the batch
// with NO partial write). Returns the saved project and the touched elements.
export function patchElements(root, id, patches = []) {
  const project = readProjectFile(root, id);
  const touched = [];
  for (const patch of patches) {
    const element = findElement(project, patch.elementId);
    applyElementFields(element, patch);
    touched.push(element);
  }
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, elements: touched };
}

// Move a project folder to <projectsRoot>/.trash/<id>-<stamp>/ instead of deleting
// it (safety: the folder is recoverable, never rm'd). listProjects skips
// dot-prefixed entries, so a trashed project disappears from the list. This is a
// project-level action and is intentionally not journaled (the per-project journal
// lives inside the folder being moved).
export function deleteProject(root, id) {
  const dir = projectDir(root, id);
  if (!existsSync(dir)) throw new Error(`canvas project not found: ${id}`);
  const base = projectsRoot(root);
  const trashDir = join(base, ".trash");
  mkdirSync(trashDir, { recursive: true });
  const stamp = nowIso().replace(/[:.]/g, "-");
  const dest = join(trashDir, `${id}-${stamp}`);
  renameSync(dir, dest);
  // The undo history is per-machine and local (not recoverable from the synced trash), so
  // its cache entry is removed outright rather than trashed (T0259). Clearing the process
  // memo lets a future project reusing this id re-check for legacy history cleanly.
  const cacheDir = cacheProjectDir(root, id);
  rmSync(cacheDir, { recursive: true, force: true });
  relocatedCacheDirs.delete(cacheDir);
  return { id, trashed: dest };
}

// Remove the element from project.json. The backing file under files/ stays on
// disk on purpose (immutability + prior tool_runs may still reference it).
export function removeElement(root, id, elementId) {
  const project = readProjectFile(root, id);
  findElement(project, elementId);
  project.elements = (project.elements || []).filter((item) => item.id !== elementId);
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, removed: elementId };
}

// Remove several elements in ONE project write. Every id must exist (findElement
// throws before updateProject, so a bad id aborts the batch with NO partial delete);
// duplicate ids are de-duplicated. Backing files stay on disk (immutable storage).
export function removeElements(root, id, elementIds = []) {
  const project = readProjectFile(root, id);
  const wanted = new Set(elementIds.map(String));
  for (const elementId of wanted) findElement(project, elementId);
  project.elements = (project.elements || []).filter((item) => !wanted.has(item.id));
  const saved = updateProject(root, id, { elements: project.elements });
  return { project: saved, removed: [...wanted] };
}

// Confined absolute path for a project-relative file reference (e.g. an element
// `src` like "files/<hash>.png" or a bare file name). Used by the API to serve
// image bytes and by ops to read an element's source for pipeline steps.
export function resolveProjectFile(root, id, relName) {
  const filesDir = join(projectDir(root, id), "files");
  // Strip a single leading "files/" (element.src form); confineChild then rejects
  // any remaining separators or traversal instead of silently sanitizing them.
  const bare = String(relName || "").replace(/^files[\\/]/, "");
  return confineChild(filesDir, bare, "file name");
}

export function readElementBytes(root, id, elementId) {
  const project = readProjectFile(root, id);
  const element = findElement(project, elementId);
  if (element.type !== "image" || !element.src) {
    throw new Error(`element ${elementId} is not an image`);
  }
  const filePath = resolveProjectFile(root, id, element.src);
  if (!existsSync(filePath)) throw new Error(`image file missing for element ${elementId}`);
  return { buffer: readFileSync(filePath), fileName: basename(filePath), element };
}

// ---- operation journal -------------------------------------------------------
//
// One append-only journal.jsonl lives in the project's LOCAL cache dir (T0259 —
// cacheProjectDir, OFF the synced projects folder). Each line is a small JSON object of op
// METADATA only: mutation entries
// ({seq, at, op, args_summary, parent, duration_ms, has_snapshot:true}) and
// undo/redo markers ({seq, at, op, target_seq, duration_ms}). The fat before/after
// project snapshots live OUT of the line, one file per mutation under
// <cacheProjectDir>/snapshots/<seq>.json ({undo_patch, state}). This keeps every journal
// line O(1) in project size, so appendJournal, readHistory, and undo/redo scan
// only tiny lines and load exactly the one snapshot they need by seq.
//
// The store owns the plumbing: read, atomic append, O(1) monotonic seq (tail
// read), sidecar snapshot read/write/delete, thin-journal rewrite/archive, a
// transparent one-time migration of legacy fat lines, capped tool_runs with a
// spill sidecar, and the per-project errors.jsonl sink.

function journalPath(root, id) {
  return join(cacheProjectDir(root, id), "journal.jsonl");
}

function snapshotsDir(root, id) {
  return join(cacheProjectDir(root, id), "snapshots");
}

function snapshotPath(root, id, seq) {
  // seq is an integer we allocate, so it can never contain a path separator.
  return join(snapshotsDir(root, id), `${Number(seq)}.json`);
}

export function readJournal(root, id) {
  relocateLegacyHistory(root, id); // read-only history views must see relocated legacy history too
  const path = journalPath(root, id);
  if (!existsSync(path)) return [];
  const text = readFileSync(path, "utf8").replace(/^\uFEFF/, "");
  const entries = [];
  for (const line of text.split("\n")) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    try {
      entries.push(JSON.parse(trimmed));
    } catch {
      // Tolerate a torn/foreign line rather than losing the whole history.
    }
  }
  return entries;
}

// O(1) max-seq via a tail read: seq is monotonic and appends are physical-append
// only, so the last complete line always carries the max seq. Read only the file
// tail (not the whole journal), scan backward for the last parseable line, and
// fall back to a full parse only if the tail holds no complete line (e.g. a single
// giant legacy fat line larger than the tail window \u2014 rare and one-time).
const SEQ_TAIL_BYTES = 65536;

function lastJournalSeq(root, id) {
  relocateLegacyHistory(root, id);
  const path = journalPath(root, id);
  if (!existsSync(path)) return 0;
  const fd = openSync(path, "r");
  try {
    const size = fstatSync(fd).size;
    if (size === 0) return 0;
    const window = Math.min(size, SEQ_TAIL_BYTES);
    const buffer = Buffer.alloc(window);
    readSync(fd, buffer, 0, window, size - window);
    const lines = buffer.toString("utf8").split("\n");
    for (let i = lines.length - 1; i >= 0; i -= 1) {
      const trimmed = lines[i].trim();
      if (!trimmed) continue;
      try {
        const parsed = JSON.parse(trimmed);
        if (Number.isFinite(Number(parsed.seq))) return Number(parsed.seq);
      } catch {
        // Partial fragment at the chunk boundary or a torn tail: keep scanning back.
      }
    }
  } finally {
    closeSync(fd);
  }
  // Tail held no complete line: fall back to a full parse (correctness over speed).
  return readJournal(root, id).reduce((max, item) => Math.max(max, Number(item.seq) || 0), 0);
}

// The project's own head seq from project.json (0 when absent/unset). It is the FLOOR for
// the next journal seq: on a machine where project.json arrived via sync but the local cache
// journal is empty or stale, a fresh local mutation must continue from the SYNCED head, not
// restart at 1 — the local journal legitimately begins mid-history there (T0259 cross-machine
// semantics). Same-machine, the journal tail already dominates (lastSeq >= head after any
// mutation, and after an undo the redo tail's seq still dominates), so this floor is a no-op.
function projectHeadSeq(root, id) {
  try {
    const head = Number(readProjectFile(root, id).history_seq);
    return Number.isFinite(head) && head > 0 ? head : 0;
  } catch {
    return 0; // no project.json yet: no head to continue from (nextJournalSeq is only called once it exists)
  }
}

export function nextJournalSeq(root, id) {
  return Math.max(lastJournalSeq(root, id), projectHeadSeq(root, id)) + 1;
}

// Append one pre-built journal line verbatim (single O_APPEND write, atomic at the OS
// level regardless of writer count; the file is never rewritten here, so a crash at
// worst drops the last partial line \u2014 tolerated on read). Concurrent WRITERS across
// the seq-allocate + append + snapshot sequence are serialized by withProjectLock
// above (T0254) \u2014 this function itself makes no locking assumption.
export function appendJournalLine(root, id, line) {
  mkdirSync(cacheProjectDir(root, id), { recursive: true }); // the journal lives in the cache (T0259)
  appendFileSync(journalPath(root, id), `${JSON.stringify(line)}\n`);
  return line;
}

// Convenience append that allocates seq (O(1) tail read) + timestamp. Used for the
// undo/redo markers (which carry no snapshot).
export function appendJournal(root, id, entry) {
  const line = { seq: nextJournalSeq(root, id), at: nowIso(), ...entry };
  return appendJournalLine(root, id, line);
}

// ---- sidecar snapshots -------------------------------------------------------

export function writeSnapshot(root, id, seq, snapshot) {
  const dir = snapshotsDir(root, id);
  mkdirSync(dir, { recursive: true });
  writeAtomic(snapshotPath(root, id, seq), `${JSON.stringify(snapshot)}\n`);
}

export function readSnapshot(root, id, seq) {
  relocateLegacyHistory(root, id);
  const path = snapshotPath(root, id, seq);
  if (!existsSync(path)) return null;
  try {
    return JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
  } catch {
    return null;
  }
}

export function deleteSnapshot(root, id, seq) {
  rmSync(snapshotPath(root, id, seq), { force: true });
}

// Atomic rewrite of the whole journal (used only by compaction). Kept lines are
// serialized in order; the last kept line still carries the max seq, so the tail
// seq read stays correct afterward.
export function rewriteJournal(root, id, lines) {
  mkdirSync(cacheProjectDir(root, id), { recursive: true }); // journal lives in the cache (T0259)
  const body = lines.map((line) => JSON.stringify(line)).join("\n");
  writeAtomic(journalPath(root, id), body ? `${body}\n` : "");
}

// Append dropped journal lines to the compaction archive (append-only audit trail
// of what fell past the history horizon; the fat snapshots themselves are dropped).
export function appendArchive(root, id, lines) {
  if (!lines.length) return;
  const dir = cacheProjectDir(root, id); // the archive rides with the journal in the cache (T0259)
  mkdirSync(dir, { recursive: true });
  const body = lines.map((line) => JSON.stringify(line)).join("\n");
  appendFileSync(join(dir, "journal.archive.jsonl"), `${body}\n`);
}

// ---- transparent fat-journal migration --------------------------------------
//
// Legacy journals inlined {undo_patch, state} on every line (O(project) per line,
// O(n^2) per session). On the first mutating open we move those blobs into sidecar
// snapshots and rewrite the journal thin, keeping the ORIGINAL as journal.jsonl.bak
// (non-destructive: the lead's history is never deleted). The gate is O(1): once a
// snapshots/ dir exists the project is already v2, so this never re-scans. Migration
// is idempotent \u2014 a re-run finds no inline blobs and only ensures snapshots/ exists.

function lineHasInlineSnapshot(line) {
  return line && (line.undo_patch !== undefined || line.state !== undefined);
}

export function ensureThinJournal(root, id) {
  // T0259: pull any pre-cache in-project history into the local cache FIRST, so the fat->thin
  // gate below (and every read) sees it. A legacy fat journal is thus migrated in the cache,
  // never orphaned in the synced project dir.
  relocateLegacyHistory(root, id);
  // Fast O(1) gate: a snapshots/ dir means this project was already created or
  // migrated under the sidecar format; nothing to do.
  if (existsSync(snapshotsDir(root, id))) return;
  const jp = journalPath(root, id);
  if (!existsSync(jp)) return; // brand-new project: the first snapshot write creates snapshots/.

  const journal = readJournal(root, id);
  const fatLines = journal.filter(lineHasInlineSnapshot);
  if (!fatLines.length) {
    // Journal exists but is already thin (or markers-only): just mark it v2 so the
    // O(1) gate stops re-reading it every op.
    mkdirSync(snapshotsDir(root, id), { recursive: true });
    return;
  }

  // Back up the original fat journal once (never clobber an existing backup).
  const bak = `${jp}.bak`;
  if (!existsSync(bak)) copyFileSync(jp, bak);

  const thin = journal.map((line) => {
    if (!lineHasInlineSnapshot(line)) return line; // markers pass through unchanged.
    writeSnapshot(root, id, line.seq, { undo_patch: line.undo_patch, state: line.state });
    const { undo_patch, state, ...meta } = line;
    return { ...meta, has_snapshot: true };
  });
  mkdirSync(snapshotsDir(root, id), { recursive: true });
  rewriteJournal(root, id, thin);
}

// ---- tool_runs cap + spill ---------------------------------------------------
//
// tool_runs ride inside every snapshot and every project.json read/write, so an
// unbounded array quietly inflates P. Keep the last N in project.json and spill the
// overflow to an append-only <project>/tool_runs.jsonl provenance sidecar.
const DEFAULT_TOOL_RUNS_CAP = 50;

function toolRunsCap() {
  const raw = Number(process.env.CANVAS_TOOL_RUNS_CAP);
  return Number.isFinite(raw) && raw >= 0 ? raw : DEFAULT_TOOL_RUNS_CAP;
}

export function capToolRuns(root, id, runs) {
  const list = Array.isArray(runs) ? runs : [];
  const cap = toolRunsCap();
  if (list.length <= cap) return list;
  const spill = list.slice(0, list.length - cap);
  const dir = projectDir(root, id);
  mkdirSync(dir, { recursive: true });
  appendFileSync(join(dir, "tool_runs.jsonl"), `${spill.map((run) => JSON.stringify(run)).join("\n")}\n`);
  return list.slice(list.length - cap);
}

export function readToolRunsArchive(root, id) {
  const path = join(projectDir(root, id), "tool_runs.jsonl");
  if (!existsSync(path)) return [];
  const out = [];
  for (const line of readFileSync(path, "utf8").replace(/^\uFEFF/, "").split("\n")) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    try {
      out.push(JSON.parse(trimmed));
    } catch {
      // tolerate a torn line
    }
  }
  return out;
}

// ---- errors sink -------------------------------------------------------------
//
// Failed ops append one line to <project>/errors.jsonl. Only project-resolvable
// failures land here (a missing/unsafe project id can't be logged \u2014 there is no
// folder to write to), so this never throws for the caller's original error.
export function projectExists(root, id) {
  try {
    return existsSync(projectDir(root, id));
  } catch {
    return false; // unsafe id: confineChild threw
  }
}

export function appendError(root, id, entry) {
  if (!projectExists(root, id)) return false;
  try {
    const dir = projectDir(root, id);
    mkdirSync(dir, { recursive: true });
    const line = { at: nowIso(), ...entry };
    appendFileSync(join(dir, "errors.jsonl"), `${JSON.stringify(line)}\n`);
    return true;
  } catch {
    return false; // logging must never mask the real error
  }
}

export function readErrors(root, id) {
  const path = join(projectDir(root, id), "errors.jsonl");
  if (!existsSync(path)) return [];
  const out = [];
  for (const line of readFileSync(path, "utf8").replace(/^\uFEFF/, "").split("\n")) {
    const trimmed = line.trim();
    if (!trimmed) continue;
    try {
      out.push(JSON.parse(trimmed));
    } catch {
      // tolerate a torn line
    }
  }
  return out;
}

// Confined absolute path built from nested project-relative segments (e.g.
// "export", "<stamp>", "file.png"). Every segment is rejected if it contains a
// separator, traversal, or dotfile, so export writes stay under the project dir.
export function resolveProjectPath(root, id, ...segments) {
  let full = projectDir(root, id);
  for (const segment of segments) {
    full = confineChild(full, segment, "path segment");
  }
  return full;
}

// Atomic write of arbitrary bytes to a confined absolute path, creating parent
// dirs as needed. Used by the export op to copy immutable image files and write
// the manifest without ever leaving a half-written file.
export function writeProjectBytes(absPath, bytes) {
  mkdirSync(dirname(absPath), { recursive: true });
  writeAtomic(absPath, bytes);
}
