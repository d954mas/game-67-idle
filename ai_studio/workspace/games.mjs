#!/usr/bin/env node
import { createHash } from "node:crypto";
import { closeSync, existsSync, fstatSync, lstatSync, openSync, readFileSync, readdirSync, realpathSync } from "node:fs";
import { isAbsolute, join, relative, resolve, sep } from "node:path";
import { pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { TextDecoder } from "node:util";

import { BINARY_EXTENSIONS } from "../assets/manifests/integrity.mjs";
import { canvasPrivateGameProjectsRoots } from "../assets/canvas/config.mjs";
import { listWorkspaceMounts } from "./catalog.mjs";
import {
  assertGitRelativePath,
  batchInput,
  parseBatchBodies,
  parseBatchCheck,
  parseIndexEntries,
  splitBatchOids,
} from "./git_index_blobs.mjs";

const UTF8_DECODER = new TextDecoder("utf-8", { fatal: true });
const physicalPath = realpathSync.native || realpathSync;

function slash(value) {
  return String(value || "").replace(/\\/g, "/");
}

function comparable(value) {
  const text = slash(value).replace(/\/+$/, "");
  return process.platform === "win32" ? text.toLowerCase() : text;
}

function pathIsInside(parent, child) {
  const childPath = relative(resolve(parent), resolve(child));
  return childPath !== ".." && !childPath.startsWith(`..${sep}`) && !isAbsolute(childPath);
}

export const SHARED_BINARIES_MANIFEST = "ai_studio/workspace/shared_private_binaries.json";
const SHARED_BINARIES_SCHEMA = "ai_studio.workspace.shared_private_binaries.v1";
const WAIVER_FIELDS = ["path", "game_id", "private_path", "sha256", "reason", "approved_by", "approved_on"];
const CANVAS_PROJECTS_RELATIVE_ROOT = ".ai_studio/canvas/projects";

function waiverKey(path, gameId, privatePath, digest) {
  return [slash(path), gameId, slash(privatePath), String(digest || "").toLowerCase()].join("\0");
}

// A waiver is not "this path may match anything private": it names the exact
// bytes, at both ends, that one shared source produced. Swap different content
// into the path and the digest stops matching, so the waiver cannot be
// inherited silently -- which is the only reason an exception list is safe to
// have at all.
function parseSharedBinaryWaivers(content) {
  const rel = SHARED_BINARIES_MANIFEST;
  let document = null;
  try {
    document = JSON.parse(content);
  } catch (error) {
    return { waivers: [], errors: [{ path: rel, reason: `invalid JSON (${error.message})` }] };
  }
  if (document?.schema !== SHARED_BINARIES_SCHEMA) {
    return { waivers: [], errors: [{ path: rel, reason: `expected schema ${SHARED_BINARIES_SCHEMA}` }] };
  }
  if (!Array.isArray(document.entries)) {
    return { waivers: [], errors: [{ path: rel, reason: "entries must be an array" }] };
  }
  const errors = [];
  const waivers = [];
  const seen = new Set();
  document.entries.forEach((entry, index) => {
    const at = `${rel}#entries[${index}]`;
    const missing = WAIVER_FIELDS.filter((field) => !String(entry?.[field] || "").trim());
    if (missing.length) {
      errors.push({ path: at, reason: `shared-binary waiver is missing ${missing.join(", ")}` });
      return;
    }
    if (!/^[0-9a-f]{64}$/i.test(entry.sha256)) {
      errors.push({ path: at, reason: "shared-binary waiver sha256 must be a full hex digest" });
      return;
    }
    if (!/^\d{4}-\d{2}-\d{2}$/.test(entry.approved_on)) {
      errors.push({ path: at, reason: "shared-binary waiver approved_on must be YYYY-MM-DD" });
      return;
    }
    const key = waiverKey(entry.path, entry.game_id, entry.private_path, entry.sha256);
    if (seen.has(key)) {
      errors.push({ path: at, reason: "shared-binary waiver is a duplicate" });
      return;
    }
    seen.add(key);
    waivers.push({ ...entry, key });
  });
  return { waivers, errors };
}

export function readSharedBinaryWaivers(root, { readFile = readFileSync } = {}) {
  const absolute = join(resolve(root), SHARED_BINARIES_MANIFEST);
  if (!existsSync(absolute)) return { waivers: [], errors: [] };
  try {
    return parseSharedBinaryWaivers(readFile(absolute, "utf8"));
  } catch (error) {
    return { waivers: [], errors: [{ path: SHARED_BINARIES_MANIFEST, reason: `invalid JSON (${error.message})` }] };
  }
}

function formatPreflightError(result) {
  return `private game preflight failed:\n${result.violations.map((item) => `- ${item.path}: ${item.reason}`).join("\n")}`;
}

export function listGameMounts(root, options = {}) {
  return listWorkspaceMounts(root, { ...options, kinds: ["game"] });
}

function addToken(tokens, raw, meta) {
  const token = String(raw || "").trim();
  if (!token) return;
  tokens.set(token, meta);
  if (token.includes("/")) tokens.set(token.replace(/\//g, "\\"), meta);
}

function privateLeakTokens(mounts) {
  const tokens = new Map();
  for (const mount of mounts) {
    const candidates = [
      ["gameId", mount.gameId],
      ["title", mount.title],
      ["storeId", mount.storeId],
      ["root", mount.root],
      ["assetRoot", mount.assetRoot],
      ...mount.aliases.map((value) => ["alias", value]),
    ];
    for (const [kind, value] of candidates) addToken(tokens, value, { kind, gameId: mount.gameId });
  }
  return tokens;
}

export function auditPrivateGamePreflight(mounts, state = {}) {
  const privateMounts = (mounts || []).filter((mount) => mount?.visibility === "private");
  const nestedGitRoots = new Set((state.nestedGitRoots || []).map(comparable));
  const nestedGitErrors = new Map();
  for (const item of state.nestedGitErrors || []) {
    const key = comparable(item.gitRoot);
    const rows = nestedGitErrors.get(key) || [];
    rows.push(item);
    nestedGitErrors.set(key, rows);
  }
  const checkNestedGitRoots = Object.hasOwn(state, "nestedGitRoots");
  const trackedTextFiles = Array.isArray(state.trackedTextFiles) ? state.trackedTextFiles : [];
  const unscannedTextFiles = Array.isArray(state.unscannedTextFiles) ? state.unscannedTextFiles : [];
  const binaryLeaks = Array.isArray(state.binaryLeaks) ? state.binaryLeaks : [];
  const binaryScanErrors = Array.isArray(state.binaryScanErrors) ? state.binaryScanErrors : [];
  const violations = [];

  for (const mount of privateMounts) {
    const setupErrors = nestedGitErrors.get(comparable(mount.gitRoot)) || [];
    for (const item of setupErrors) {
      violations.push({
        path: `${mount.gitRoot}/.git`,
        reason: `nested git validation failed: nested git metadata is invalid (${item.reason || "unknown Git error"})`,
      });
    }
    if (setupErrors.length) continue;
    if (checkNestedGitRoots && !nestedGitRoots.has(comparable(mount.gitRoot))) {
      violations.push({
        path: `${mount.gitRoot}/.git`,
        reason: "private game root is missing nested git metadata",
      });
    }
  }

  for (const file of unscannedTextFiles) {
    violations.push({
      path: slash(file.path || ""),
      reason: `tracked text leak scan could not inspect this file (${file.reason || "unreadable"})`,
    });
  }

  const tokens = privateLeakTokens(privateMounts);
  for (const file of trackedTextFiles) {
    // The shared-binary manifest is the one tracked file whose JOB is to name a
    // private game: a waiver that could not say which game it covers would waive
    // nothing checkable. It lives in ai_studio/, not in a template, so the id
    // never rides along into a game created from one.
    if (slash(file.path || "") === SHARED_BINARIES_MANIFEST) continue;
    const text = String(file.text || "");
    for (const [token, meta] of tokens) {
      if (!text.includes(token)) continue;
      violations.push({
        path: slash(file.path || ""),
        reason: `tracked file leaks private token (${meta.kind}) for ${meta.gameId}`,
      });
      break;
    }
  }
  for (const file of state.sharedBinaryErrors || []) {
    violations.push({
      path: slash(file.path || ""),
      reason: file.reason || "shared-binary waiver is invalid",
    });
  }
  for (const file of binaryScanErrors) {
    violations.push({
      path: slash(file.path || ""),
      reason: `tracked binary privacy scan failed (${file.reason || "unreadable"})`,
    });
  }
  const waivers = new Map((state.sharedBinaryWaivers || []).map((entry) => [entry.key, entry]));
  const usedWaivers = new Set(state.validSharedBinaryWaiverKeys || []);
  for (const leak of binaryLeaks) {
    const key = waiverKey(leak.path, leak.gameId, leak.privatePath, leak.digest);
    if (waivers.has(key)) {
      usedWaivers.add(key);
      continue;
    }
    violations.push({
      path: slash(leak.path || ""),
      reason: `tracked binary is byte-identical to private binary ${leak.gameId}:${slash(leak.privatePath || "")}`,
    });
  }
  // A waiver that matches nothing has outlived its reason. Reporting it is what
  // keeps the list from growing quietly: an entry survives only while the
  // sharing it describes is still real.
  for (const [key, entry] of waivers) {
    if (usedWaivers.has(key)) continue;
    violations.push({
      path: `${SHARED_BINARIES_MANIFEST}: ${slash(entry.path)}`,
      reason: `shared-binary waiver matches no tracked binary (stale entry for ${entry.game_id}:${slash(entry.private_path)})`,
    });
  }
  return { ok: violations.length === 0, violations };
}

// A hook runs with GIT_DIR and GIT_INDEX_FILE exported for the repository being
// committed. Inheriting them would point every query below at THAT repository
// whatever cwd says, so a studio commit would read the studio's index while
// asking about a private game and report every waiver as stale.
const GIT_CONTEXT_ENV = ["GIT_DIR", "GIT_INDEX_FILE", "GIT_WORK_TREE",
                         "GIT_COMMON_DIR", "GIT_OBJECT_DIRECTORY", "GIT_PREFIX", "GIT_NO_REPLACE_OBJECTS"];

function gitEnv() {
  const env = { ...process.env };
  for (const name of GIT_CONTEXT_ENV) delete env[name];
  return env;
}

function git(root, args, spawnGit = spawnSync, encoding = "utf8", input) {
  const cwd = resolve(root);
  return spawnGit("git", ["--no-replace-objects", "-c", `safe.directory=${slash(cwd)}`, ...args], {
    cwd,
    encoding,
    input,
    env: gitEnv(),
    shell: false,
    maxBuffer: 64 * 1024 * 1024,
  });
}

function commandError(result, fallback) {
  if (!result?.error && result?.status === 0) return null;
  const stderr = Buffer.isBuffer(result?.stderr)
    ? result.stderr.toString("utf8")
    : String(result?.stderr || "");
  return result?.error?.message || stderr.trim().split(/\r?\n/, 1)[0] || `${fallback} exited ${result?.status}`;
}

function nulPaths(output) {
  const text = Buffer.isBuffer(output) ? output.toString("utf8") : String(output || "");
  return text.split("\0").map(slash).filter(Boolean);
}

function isBinaryPath(path) {
  const normalized = slash(path).toLowerCase();
  const dot = normalized.lastIndexOf(".");
  return dot >= 0 && BINARY_EXTENSIONS.has(normalized.slice(dot));
}

function isBinaryView(path, bytes) {
  if (isBinaryPath(path) || bytes.includes(0)) return true;
  try {
    const text = UTF8_DECODER.decode(bytes);
    return /[\u0001-\u0008\u000b\u000c\u000e-\u001f\u007f]/.test(text);
  } catch {
    return true;
  }
}

function confinedMountRoot(root, gitRoot) {
  const repoRoot = resolve(root);
  const candidate = resolve(repoRoot, gitRoot);
  const child = relative(repoRoot, candidate);
  if (!child || child === ".." || child.startsWith(`..${sep}`) || resolve(repoRoot, child) !== candidate) {
    return { root: null, error: "private game root escapes the parent repository" };
  }
  return { root: candidate, error: null };
}

function textBuffer(buffer) {
  if (!buffer || buffer.includes(0)) return null;
  return buffer.toString("utf8");
}

function matchingTrackedPaths(root, tokens, cached, spawnGit) {
  if (!tokens.size) return { paths: [], error: null };
  const args = ["grep", "-z", "-I", "-l", "-F"];
  if (cached) args.push("--cached");
  for (const token of tokens.keys()) args.push("-e", token);
  args.push("--", ".");
  const result = git(root, args, spawnGit);
  if (!result.error && (result.status === 0 || result.status === 1)) {
    return { paths: nulPaths(result.stdout), error: null };
  }
  return { paths: [], error: commandError(result, "git grep") };
}

function trackedTextFiles(root, tokens, spawnGit) {
  const files = [];
  const unscanned = [];
  const worktree = matchingTrackedPaths(root, tokens, false, spawnGit);
  const index = matchingTrackedPaths(root, tokens, true, spawnGit);
  if (worktree.error) unscanned.push({ path: "<tracked-worktree>", reason: worktree.error });
  if (index.error) unscanned.push({ path: "<tracked-index>", reason: index.error });
  for (const path of worktree.paths) {
    try {
      const text = textBuffer(readFileSync(join(root, path)));
      if (text !== null) files.push({ path, text, source: "worktree" });
    } catch (error) {
      unscanned.push({ path, reason: error.message });
    }
  }
  for (const path of index.paths) {
    const staged = git(root, ["show", `:${path}`], spawnGit);
    if (!staged.error && staged.status === 0 && !String(staged.stdout).includes("\0")) {
      files.push({ path, text: staged.stdout, source: "index" });
    } else {
      unscanned.push({ path, reason: commandError(staged, "git show") || "index read failed" });
    }
  }
  return { files, unscanned };
}

function nestedGitRoots(root, mounts, spawnGit) {
  const roots = [];
  const errors = [];
  for (const mount of mounts) {
    const confined = confinedMountRoot(root, mount.gitRoot);
    if (confined.error) {
      errors.push({ gitRoot: mount.gitRoot, reason: confined.error });
      continue;
    }
    const gameRoot = confined.root;
    if (!existsSync(join(gameRoot, ".git"))) continue;
    const result = git(gameRoot, ["rev-parse", "--is-inside-work-tree", "--show-prefix"], spawnGit);
    const error = commandError(result, "git rev-parse");
    if (error) {
      errors.push({ gitRoot: mount.gitRoot, reason: error });
      continue;
    }
    const [inside, ...prefixLines] = result.stdout.replace(/\r\n/g, "\n").split("\n");
    if (inside.trim() === "true" && prefixLines.join("\n").trim() === "") roots.push(mount.gitRoot);
    else errors.push({ gitRoot: mount.gitRoot, reason: "path is not an independent Git work tree root" });
  }
  return { roots, errors };
}

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex");
}

function indexBinaryView(root, path, spawnGit) {
  const result = git(root, ["show", `:${path}`], spawnGit, null);
  const error = commandError(result, "git show");
  if (error) return { view: null, error };
  const bytes = Buffer.isBuffer(result.stdout) ? result.stdout : Buffer.from(result.stdout || "");
  if (!isBinaryView(path, bytes)) return { view: null, error: null };
  return { view: { path, source: "index", digest: sha256(bytes), size: bytes.length }, error: null };
}

function indexBinaryViews(root, entries, spawnGit, candidateSizes = null) {
  const entryByOid = new Map();
  for (const entry of entries) {
    const rows = entryByOid.get(entry.oid) || [];
    rows.push(entry);
    entryByOid.set(entry.oid, rows);
  }
  const oids = [...entryByOid.keys()];
  if (!oids.length) return { views: [], error: null };
  const check = git(root, ["cat-file", "--batch-check"], spawnGit, null, batchInput(oids));
  const checkError = commandError(check, "git cat-file --batch-check");
  if (checkError) return { views: [], error: checkError };
  let metadata;
  try {
    metadata = parseBatchCheck(check.stdout, oids);
  } catch (error) {
    return { views: [], error: `invalid index blob metadata (${error.message})` };
  }
  const views = [];
  let batches;
  try {
    const candidates = candidateSizes ? oids.filter((oid) => candidateSizes.has(metadata.get(oid))) : oids;
    batches = splitBatchOids(candidates, metadata);
  } catch (error) {
    return { views: [], error: `invalid index blob batch (${error.message})` };
  }
  for (const batch of batches) {
    const result = git(root, ["cat-file", "--batch"], spawnGit, null, batchInput(batch));
    const batchError = commandError(result, "git cat-file --batch");
    if (batchError) return { views: [], error: batchError };
    let blobs;
    try {
      blobs = parseBatchBodies(result.stdout, batch, metadata);
    } catch (error) {
      return { views: [], error: `invalid index blob content (${error.message})` };
    }
    for (const oid of batch) {
      const bytes = blobs.get(oid);
      const rows = entryByOid.get(oid) || [];
      let digest = null;
      for (const entry of rows) {
        if (!isBinaryView(entry.path, bytes)) continue;
        digest ||= sha256(bytes);
        views.push({ path: entry.path, source: "index", digest, size: bytes.length });
      }
    }
  }
  return { views, error: null };
}

function sameFileState(left, right) {
  return left.dev === right.dev && left.ino === right.ino && left.size === right.size
    && left.mtimeNs === right.mtimeNs && left.ctimeNs === right.ctimeNs;
}

function readCandidateFile(path, entry, candidateSizes) {
  const descriptor = openSync(path, "r");
  try {
    const before = fstatSync(descriptor, { bigint: true });
    if (!before.isFile() || !sameFileState(before, entry)) {
      throw new Error("private file changed while opening its contents");
    }
    // Equal length is necessary for byte identity; matching lengths still require the full digest.
    if (candidateSizes && !candidateSizes.has(Number(before.size))) {
      if (!sameFileState(before, fstatSync(descriptor, { bigint: true }))) {
        throw new Error("private file changed while checking its size");
      }
      return null;
    }
    const bytes = readFileSync(descriptor);
    const after = fstatSync(descriptor, { bigint: true });
    if (!sameFileState(before, after) || BigInt(bytes.length) !== before.size) {
      throw new Error("private file changed while reading its contents");
    }
    return bytes;
  } finally {
    closeSync(descriptor);
  }
}

function canvasLogicalProjectsRoot(gameRoot) {
  return join(gameRoot, ...CANVAS_PROJECTS_RELATIVE_ROOT.split("/"));
}

function validateCanvasLogicalMount(gameRoot, registeredRoot) {
  const logicalRoot = canvasLogicalProjectsRoot(gameRoot);
  if (!existsSync(logicalRoot)) return null;
  try {
    const entry = lstatSync(logicalRoot);
    if (!entry.isDirectory() && !entry.isSymbolicLink()) return "private Canvas projects mount is not a directory";
    const physicalRoot = physicalPath(logicalRoot);
    if (!registeredRoot && !pathIsInside(physicalPath(gameRoot), physicalRoot)) {
      return "private Canvas projects mount resolves outside the game root without registration";
    }
    if (registeredRoot && comparable(physicalRoot) !== comparable(physicalPath(registeredRoot))) {
      return "private Canvas projects mount does not match its registered root";
    }
    return null;
  } catch {
    return "private Canvas projects mount cannot be resolved";
  }
}

function privateCanvasBinaryViews(mount, registeredRoot, candidateSizes = null) {
  try {
    const rootEntry = lstatSync(registeredRoot);
    if (!rootEntry.isDirectory() || rootEntry.isSymbolicLink()) {
      return { views: [], error: "registered private Canvas projects root is not a physical directory" };
    }
    const physicalRoot = physicalPath(registeredRoot);
    const views = [];
    const visit = (directory, relativePath = "") => {
      const physicalDirectory = physicalPath(directory);
      if (!pathIsInside(physicalRoot, physicalDirectory)) {
        throw new Error("registered private Canvas projects root contains a reparse escape");
      }
      for (const entry of readdirSync(directory, { withFileTypes: true })) {
        const childRelative = relativePath ? `${relativePath}/${entry.name}` : entry.name;
        assertGitRelativePath(childRelative);
        const child = join(directory, entry.name);
        const stat = lstatSync(child, { bigint: true });
        if (stat.isSymbolicLink()) {
          throw new Error("registered private Canvas projects root contains a symlink or reparse point");
        }
        if (stat.isDirectory()) {
          visit(child, childRelative);
          continue;
        }
        if (!stat.isFile()) {
          throw new Error("registered private Canvas projects root contains an unsupported entry");
        }
        const physical = physicalPath(child);
        if (!pathIsInside(physicalRoot, physical)) {
          throw new Error("registered private Canvas file resolves outside its root");
        }
        if (lstatSync(child).isSymbolicLink()) {
          throw new Error("registered private Canvas file changed to a symlink or reparse point");
        }
        const bytes = readCandidateFile(child, stat, candidateSizes);
        if (bytes === null) continue;
        const logicalPath = `${CANVAS_PROJECTS_RELATIVE_ROOT}/${slash(childRelative)}`;
        if (isBinaryView(logicalPath, bytes)) {
          views.push({ path: logicalPath, source: "canvas", digest: sha256(bytes), size: bytes.length, gameId: mount.gameId });
        }
      }
    };
    visit(registeredRoot);
    return { views, error: null };
  } catch (error) {
    return { views: [], error: error.message || "registered private Canvas projects root cannot be scanned" };
  }
}

function worktreeBinaryView(root, path, { indexedSymlink = false, candidateSizes = null, physicalRoot = null } = {}) {
  try {
    const rootEntry = lstatSync(root);
    if (rootEntry.isSymbolicLink()) return { view: null, error: "private game root is a symlink or reparse point" };
    const absolute = resolve(root, assertGitRelativePath(path));
    const logical = relative(root, absolute);
    if (!logical || logical === ".." || logical.startsWith(`..${sep}`)) {
      return { view: null, error: "private worktree path escapes the game root" };
    }
    let entry;
    try {
      entry = lstatSync(absolute, { bigint: true });
    } catch (error) {
      if (error.code === "ENOENT") return { view: null, error: null };
      throw error;
    }
    if (entry.isSymbolicLink()) {
      return indexedSymlink
        ? { view: null, error: null }
        : { view: null, error: "private worktree path is a symlink or reparse point" };
    }
    // A nested repository lists as a directory; it holds no bytes to compare.
    if (!entry.isFile()) return { view: null, error: null };
    const anchor = physicalRoot || physicalPath(root);
    const physical = physicalPath(absolute);
    const physicalRelative = relative(anchor, physical);
    if (!physicalRelative || physicalRelative === ".." || physicalRelative.startsWith(`..${sep}`)) {
      return { view: null, error: "private worktree file resolves outside the game root" };
    }
    if (lstatSync(absolute).isSymbolicLink()) return { view: null, error: "private worktree path changed to a symlink or reparse point" };
    const bytes = readCandidateFile(absolute, entry, candidateSizes);
    if (bytes === null) return { view: null, error: null };
    if (!isBinaryView(path, bytes)) return { view: null, error: null };
    return { view: { path, source: "worktree", digest: sha256(bytes), size: bytes.length }, error: null };
  } catch (error) {
    return { view: null, error: error.message };
  }
}

function changedTrackedPaths(root, spawnGit) {
  const paths = { worktree: new Set(), index: new Set() };
  const errors = [];
  // Submodules are excluded because a gitlink names a commit, not bytes: there
  // is nothing at that path for either scanner to read, and a changed pointer
  // would otherwise report as an unscannable file and block the commit.
  const commands = [
    ["worktree", ["diff", "--name-only", "-z", "--diff-filter=ACMR", "--ignore-submodules=all", "--", "."]],
    ["index", ["diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR", "--ignore-submodules=all", "--", "."]],
  ];
  for (const [source, args] of commands) {
    const result = git(root, args, spawnGit);
    const error = commandError(result, `git ${args[0]}`);
    if (error) errors.push({ path: "<parent-index>", reason: error });
    else for (const path of nulPaths(result.stdout)) paths[source].add(path);
  }
  return { paths, errors };
}

function parentBinaryViews(root, spawnGit) {
  const changed = changedTrackedPaths(root, spawnGit);
  const views = [];
  const errors = [...changed.errors];
  for (const path of changed.paths.worktree) {
    const worktree = worktreeBinaryView(root, path);
    if (worktree.view) views.push(worktree.view);
    if (worktree.error) errors.push({ path, reason: worktree.error });
  }
  for (const path of changed.paths.index) {
    const index = indexBinaryView(root, path, spawnGit);
    if (index.view) views.push(index.view);
    if (index.error) errors.push({ path, reason: index.error });
  }
  return { views, errors };
}

function privateBinaryViews(root, mount, spawnGit, registeredCanvasRoot = null, candidateSizes = null) {
  const confined = confinedMountRoot(root, mount.gitRoot);
  if (confined.error) return { views: [], errors: [{ path: mount.gitRoot, reason: confined.error }] };
  const mountError = validateCanvasLogicalMount(confined.root, registeredCanvasRoot);
  if (mountError) return { views: [], errors: [{ path: mount.gitRoot, reason: mountError }] };
  const pathspec = [
    "--", ".",
    ":(exclude).mypy_cache/**",
    ":(exclude).pytest_cache/**",
    ":(exclude)__pycache__/**",
    ":(exclude)build/**",
    ":(exclude)node_modules/**",
    ":(exclude)release/artifacts/**",
    ":(exclude)tmp/**",
  ];
  if (registeredCanvasRoot) pathspec.push(`:(exclude)${CANVAS_PROJECTS_RELATIVE_ROOT}/**`);
  const indexedSymlinkPaths = new Set();
  const gitlinkPaths = new Set();
  const views = [];
  const errors = [];
  const result = git(confined.root, ["ls-files", "--stage", "-z", "--cached", ...pathspec], spawnGit);
  const setupError = commandError(result, "git ls-files");
  if (setupError) return { views: [], errors: [{ path: mount.gitRoot, reason: setupError }] };
  try {
    const entries = parseIndexEntries(result.stdout, { includeGitlinks: true });
    for (const entry of entries) {
      if (entry.mode === "120000") indexedSymlinkPaths.add(entry.path);
      if (entry.mode === "160000") gitlinkPaths.add(entry.path);
    }
    const index = indexBinaryViews(confined.root, entries.filter((entry) => entry.mode !== "160000"), spawnGit, candidateSizes);
    if (index.error) return { views: [], errors: [{ path: mount.gitRoot, reason: index.error }] };
    views.push(...index.views.map((view) => ({ ...view, gameId: mount.gameId })));
  } catch (error) {
    return { views: [], errors: [{ path: mount.gitRoot, reason: `invalid index entries (${error.message})` }] };
  }
  let physicalRoot;
  try {
    physicalRoot = physicalPath(confined.root);
  } catch (error) {
    return { views: [], errors: [{ path: mount.gitRoot, reason: error.message }] };
  }
  const excludedRoots = new Set([".mypy_cache", ".pytest_cache", "__pycache__", "build", "node_modules", "tmp"]);
  const visit = (directory, prefix = "") => {
    const info = lstatSync(directory);
    if (info.isSymbolicLink() || !info.isDirectory()) throw new Error("private worktree directory is a symlink or reparse point");
    if (!pathIsInside(physicalRoot, physicalPath(directory))) throw new Error("private worktree directory resolves outside the game root");
    if (prefix && existsSync(join(directory, ".git"))) {
      // Git's untracked view treats an independent nested repository as a separate container.
      const nested = git(directory, ["rev-parse", "--is-inside-work-tree", "--show-prefix"], spawnGit);
      const lines = String(nested.stdout || "").split(/\r?\n/);
      if (!commandError(nested, "git rev-parse") && lines[0] === "true" && lines[1] === "") return;
    }
    for (const entry of readdirSync(directory, { withFileTypes: true })) {
      const path = prefix ? `${prefix}/${entry.name}` : entry.name;
      if (entry.name === ".git" || (!prefix && excludedRoots.has(entry.name)) || path === "release/artifacts"
          || (registeredCanvasRoot && path === CANVAS_PROJECTS_RELATIVE_ROOT) || gitlinkPaths.has(path)) continue;
      assertGitRelativePath(path);
      const absolute = join(directory, entry.name);
      if (entry.isDirectory() && !entry.isSymbolicLink()) {
        visit(absolute, path);
      } else {
        const worktree = worktreeBinaryView(confined.root, path, { indexedSymlink: indexedSymlinkPaths.has(path), candidateSizes, physicalRoot });
        if (worktree.view) views.push({ ...worktree.view, gameId: mount.gameId });
        if (worktree.error) errors.push({ path: `${mount.gitRoot}/${path}`, reason: worktree.error });
      }
    }
  };
  try {
    visit(confined.root);
  } catch (error) {
    errors.push({ path: mount.gitRoot, reason: error.message });
  }
  return { views, errors };
}

function registeredPrivateCanvasRoots(root, mounts, { ignoreUnselected = false } = {}) {
  let configured;
  try {
    configured = canvasPrivateGameProjectsRoots(root);
  } catch (error) {
    return { roots: new Map(), errors: [{ path: "<canvas-config>", reason: error.message }] };
  }
  const mountsByGameId = new Map();
  for (const mount of mounts) mountsByGameId.set(comparable(mount.gameId), mount);
  const roots = new Map();
  const errors = [];
  for (const [gameId, projectsRoot] of configured) {
    const mount = mountsByGameId.get(comparable(gameId));
    if (!mount) {
      if (!ignoreUnselected) {
        errors.push({ path: "<canvas-config>", reason: "registered private Canvas root has no canonical private game" });
      }
      continue;
    }
    roots.set(comparable(mount.gameId), projectsRoot);
  }
  return { roots, errors };
}

function trackedBinaryLeaks(root, mounts, spawnGit, canvasRoots = new Map()) {
  const parent = parentBinaryViews(root, spawnGit);
  if (!parent.views.length) return { leaks: [], errors: parent.errors };
  const candidateSizes = new Set(parent.views.map((view) => view.size));
  const privateByDigest = new Map();
  const errors = [...parent.errors];
  const scannedCanvasRoots = new Set();
  for (const mount of mounts) {
    const canvasRoot = canvasRoots.get(comparable(mount.gameId)) || null;
    const scanned = privateBinaryViews(root, mount, spawnGit, canvasRoot, candidateSizes);
    errors.push(...scanned.errors);
    for (const view of scanned.views) {
      const rows = privateByDigest.get(view.digest) || [];
      rows.push(view);
      privateByDigest.set(view.digest, rows);
    }
    const canvasKey = comparable(mount.gameId);
    if (!canvasRoot || scannedCanvasRoots.has(canvasKey)) continue;
    scannedCanvasRoots.add(canvasKey);
    const external = privateCanvasBinaryViews(mount, canvasRoot, candidateSizes);
    if (external.error) {
      errors.push({ path: mount.gitRoot, reason: external.error });
      continue;
    }
    for (const view of external.views) {
      const rows = privateByDigest.get(view.digest) || [];
      rows.push(view);
      privateByDigest.set(view.digest, rows);
    }
  }
  const leaks = [];
  const seen = new Set();
  for (const view of parent.views) {
    for (const privateView of privateByDigest.get(view.digest) || []) {
      const key = `${view.path}\0${privateView.gameId}\0${privateView.path}`;
      if (seen.has(key)) continue;
      seen.add(key);
      leaks.push({
        path: view.path,
        gameId: privateView.gameId,
        privatePath: privateView.path,
        digest: view.digest,
      });
    }
  }
  return { leaks, errors };
}

function trackedSharedBinaryWaivers(root, spawnGit) {
  const result = git(root, ["show", `:${SHARED_BINARIES_MANIFEST}`], spawnGit, null);
  if (commandError(result, "git show")) {
    const head = git(root, ["show", `HEAD:${SHARED_BINARIES_MANIFEST}`], spawnGit, null);
    if (!commandError(head, "git show")) {
      return {
        waivers: [],
        errors: [{
          path: SHARED_BINARIES_MANIFEST,
          reason: "shared-binary approval manifest is staged for deletion",
        }],
      };
    }
    if (!existsSync(join(root, SHARED_BINARIES_MANIFEST))) return { waivers: [], errors: [] };
    return {
      waivers: [],
      errors: [{
        path: SHARED_BINARIES_MANIFEST,
        reason: "shared-binary approval manifest must be tracked in the parent index",
      }],
    };
  }
  const content = Buffer.isBuffer(result.stdout) ? result.stdout.toString("utf8") : String(result.stdout || "");
  return parseSharedBinaryWaivers(content);
}

function validSharedBinaryWaiverKeys(root, auditMounts, waivers, spawnGit) {
  const mountsByGameId = new Map();
  for (const mount of auditMounts) {
    const key = comparable(mount.gameId);
    const roots = mountsByGameId.get(key) || [];
    roots.push(mount);
    mountsByGameId.set(key, roots);
  }
  const pathsByRoot = new Map();
  const addPath = (gitRoot, path) => {
    const paths = pathsByRoot.get(gitRoot) || new Set();
    paths.add(path);
    pathsByRoot.set(gitRoot, paths);
  };
  for (const waiver of waivers) {
    addPath(root, waiver.path);
    for (const mount of mountsByGameId.get(comparable(waiver.game_id)) || []) {
      const confined = confinedMountRoot(root, mount.gitRoot);
      if (!confined.error) addPath(confined.root, waiver.private_path);
    }
  }
  const viewsByRoot = new Map();
  for (const [gitRoot, paths] of pathsByRoot) {
    const result = git(gitRoot, ["ls-files", "--stage", "-z", "--", ...[...paths].map((path) => `:(literal)${path}`)], spawnGit);
    let views = [];
    if (!commandError(result, "git ls-files")) {
      try {
        const entries = parseIndexEntries(result.stdout).filter((entry) => paths.has(entry.path));
        const scanned = indexBinaryViews(gitRoot, entries, spawnGit);
        if (!scanned.error) views = scanned.views;
      } catch {
        // Invalid or missing index bytes cannot vouch for an approved digest.
      }
    }
    viewsByRoot.set(gitRoot, new Map(views.map((view) => [view.path, view])));
  }
  const valid = new Set();
  for (const waiver of waivers) {
    const mounts = mountsByGameId.get(comparable(waiver.game_id)) || [];
    if (!mounts.length) continue;
    const publicView = viewsByRoot.get(root)?.get(waiver.path);
    if (!publicView || publicView.digest !== waiver.sha256) continue;
    for (const mount of mounts) {
      const confined = confinedMountRoot(root, mount.gitRoot);
      if (confined.error) continue;
      const privateIndex = viewsByRoot.get(confined.root)?.get(waiver.private_path);
      if (privateIndex?.digest === waiver.sha256) {
        valid.add(waiver.key);
        break;
      }
      const privateWorktree = worktreeBinaryView(confined.root, waiver.private_path).view;
      if (privateWorktree?.digest === waiver.sha256) {
        valid.add(waiver.key);
        break;
      }
    }
  }
  return valid;
}

function privateAuditMounts(mounts) {
  const auditMounts = [];
  for (const mount of mounts) {
    const roots = Array.isArray(mount.auditRoots) && mount.auditRoots.length ? mount.auditRoots : [mount.gitRoot];
    for (const root of roots) {
      auditMounts.push({
        ...mount,
        root,
        gitRoot: root,
        folder: root,
        assetRoot: `${root}/assets`,
        assets: `${root}/assets`,
      });
    }
  }
  return auditMounts;
}

export function runPrivateGamePreflight(root, options = {}) {
  const repoRoot = resolve(root || process.cwd());
  const spawnGit = options.spawnGit || spawnSync;
  const measure = (phase, work) => {
    const start = performance.now();
    try { return work(); }
    finally { options.onTiming?.({ phase, durationMs: performance.now() - start }); }
  };
  const activeGameId = String(options.activeGameId || "").trim();
  const discovered = options.mounts || measure("discovery", () => listWorkspaceMounts(repoRoot, activeGameId
    ? { activeGameId, kinds: ["game"], warnings: [] }
    : { includePrivate: true, kinds: ["game"], warnings: [] }));
  const mounts = discovered.filter((mount) => mount.visibility === "private"
    && (!activeGameId || comparable(mount.gameId) === comparable(activeGameId)));
  if (activeGameId && mounts.length !== 1) {
    return {
      ok: false,
      violations: [{
        path: "<active-game>",
        reason: mounts.length ? `active private game ${activeGameId} resolved to multiple mounts` : `active private game ${activeGameId} was not found`,
      }],
    };
  }
  if (!mounts.length) return { ok: true, violations: [] };
  const auditMounts = privateAuditMounts(mounts);
  const privateCanvas = measure("canvas-roots", () => registeredPrivateCanvasRoots(repoRoot, mounts, { ignoreUnselected: Boolean(activeGameId) }));
  const tracked = measure("public-text", () => trackedTextFiles(repoRoot, privateLeakTokens(auditMounts), spawnGit));
  const nested = measure("nested-git", () => nestedGitRoots(repoRoot, auditMounts, spawnGit));
  const binaries = measure("binary-comparison", () => trackedBinaryLeaks(repoRoot, auditMounts, spawnGit, privateCanvas.roots));
  const shared = measure("waiver-manifest", () => trackedSharedBinaryWaivers(repoRoot, spawnGit));
  const waivers = activeGameId
    ? shared.waivers.filter((entry) => comparable(entry.game_id) === comparable(activeGameId))
    : shared.waivers;
  const validWaivers = measure("waiver-validation", () => validSharedBinaryWaiverKeys(repoRoot, auditMounts, waivers, spawnGit));
  return auditPrivateGamePreflight(auditMounts, {
    sharedBinaryWaivers: waivers,
    validSharedBinaryWaiverKeys: validWaivers,
    sharedBinaryErrors: shared.errors,
    nestedGitRoots: nested.roots,
    nestedGitErrors: nested.errors,
    trackedTextFiles: tracked.files,
    unscannedTextFiles: tracked.unscanned,
    binaryLeaks: binaries.leaks,
    binaryScanErrors: [...privateCanvas.errors, ...binaries.errors],
  });
}

function parseArgs(argv) {
  const args = { command: "preflight", root: process.cwd(), json: false, includePrivate: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (["list", "preflight"].includes(arg)) args.command = arg;
    else if (arg === "--root") args.root = argv[++index];
    else if (arg === "--json") args.json = true;
    else if (arg === "--timings") args.timings = true;
    else if (arg === "--include-private") args.includePrivate = true;
    else if (arg === "--help" || arg === "-h") args.help = true;
    else throw new Error(`unknown argument: ${arg}`);
  }
  return args;
}

function usageText() {
  return `usage:
  node ai_studio/workspace/games.mjs list [--root <repo>] [--include-private] [--json]
  node ai_studio/workspace/games.mjs preflight [--root <repo>] [--json] [--timings]`;
}

function main() {
  try {
    const args = parseArgs(process.argv.slice(2));
    if (args.help) return console.log(usageText());
    if (args.command === "list") {
      const mounts = listGameMounts(resolve(args.root), { includePrivate: args.includePrivate });
      if (args.json) console.log(JSON.stringify({ schema: "ai_studio.workspace.game_mounts.v1", mounts }, null, 2));
      else console.log(mounts.map((mount) => `${mount.visibility}\t${mount.storeId}\t${mount.root}`).join("\n"));
      return;
    }
    const timings = [];
    const result = runPrivateGamePreflight(resolve(args.root), args.timings ? { onTiming: (row) => timings.push(row) } : {});
    if (args.timings) result.timings = timings;
    if (args.json) console.log(JSON.stringify(result, null, 2));
    else if (result.ok) console.log("ok: private game preflight");
    else console.error(formatPreflightError(result));
    if (args.timings && !args.json) console.error(timings.map(({ phase, durationMs }) => `${phase}: ${durationMs.toFixed(1)} ms`).join("\n"));
    process.exitCode = result.ok ? 0 : 1;
  } catch (error) {
    console.error(error && error.message ? error.message : String(error));
    process.exitCode = 1;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) main();
