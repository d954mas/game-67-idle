#!/usr/bin/env node
import { createHash } from "node:crypto";
import { existsSync, readFileSync } from "node:fs";
import { join, relative, resolve, sep } from "node:path";
import { pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { TextDecoder } from "node:util";

import { BINARY_EXTENSIONS } from "../assets/manifests/integrity.mjs";
import { listWorkspaceMounts } from "./catalog.mjs";

const UTF8_DECODER = new TextDecoder("utf-8", { fatal: true });

function slash(value) {
  return String(value || "").replace(/\\/g, "/");
}

function comparable(value) {
  const text = slash(value).replace(/\/+$/, "");
  return process.platform === "win32" ? text.toLowerCase() : text;
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
  for (const file of binaryScanErrors) {
    violations.push({
      path: slash(file.path || ""),
      reason: `tracked binary privacy scan failed (${file.reason || "unreadable"})`,
    });
  }
  for (const leak of binaryLeaks) {
    violations.push({
      path: slash(leak.path || ""),
      reason: `tracked binary is byte-identical to private binary ${leak.gameId}:${slash(leak.privatePath || "")}`,
    });
  }
  return { ok: violations.length === 0, violations };
}

function git(root, args, spawnGit = spawnSync, encoding = "utf8") {
  const cwd = resolve(root);
  return spawnGit("git", ["-c", `safe.directory=${slash(cwd)}`, ...args], {
    cwd,
    encoding,
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
  return { view: { path, source: "index", digest: sha256(bytes) }, error: null };
}

function worktreeBinaryView(root, path) {
  const absolute = join(root, path);
  if (!existsSync(absolute)) return { view: null, error: null };
  try {
    const bytes = readFileSync(absolute);
    if (!isBinaryView(path, bytes)) return { view: null, error: null };
    return { view: { path, source: "worktree", digest: sha256(bytes) }, error: null };
  } catch (error) {
    return { view: null, error: error.message };
  }
}

function changedTrackedPaths(root, spawnGit) {
  const paths = { worktree: new Set(), index: new Set() };
  const errors = [];
  const commands = [
    ["worktree", ["diff", "--name-only", "-z", "--diff-filter=ACMR", "--", "."]],
    ["index", ["diff", "--cached", "--name-only", "-z", "--diff-filter=ACMR", "--", "."]],
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

function privateBinaryViews(root, mount, spawnGit) {
  const confined = confinedMountRoot(root, mount.gitRoot);
  if (confined.error) return { views: [], errors: [{ path: mount.gitRoot, reason: confined.error }] };
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
  const selections = [
    { args: ["ls-files", "-z", "--cached", ...pathspec], tracked: true },
    { args: ["ls-files", "-z", "--others", "--exclude-standard", ...pathspec], tracked: false },
    { args: ["ls-files", "-z", "--others", "--ignored", "--exclude-standard", ...pathspec], tracked: false },
  ];
  const paths = new Set();
  const trackedPaths = new Set();
  const views = [];
  const errors = [];
  for (const selection of selections) {
    const result = git(confined.root, selection.args, spawnGit);
    const setupError = commandError(result, "git ls-files");
    if (setupError) {
      errors.push({ path: mount.gitRoot, reason: setupError });
      continue;
    }
    for (const path of nulPaths(result.stdout)) {
      paths.add(path);
      if (selection.tracked) trackedPaths.add(path);
    }
  }
  if (errors.length) return { views: [], errors };
  for (const path of paths) {
    const worktree = worktreeBinaryView(confined.root, path);
    if (worktree.view) views.push({ ...worktree.view, gameId: mount.gameId });
    if (worktree.error) errors.push({ path: `${mount.gitRoot}/${path}`, reason: worktree.error });
    if (trackedPaths.has(path)) {
      const index = indexBinaryView(confined.root, path, spawnGit);
      if (index.view) views.push({ ...index.view, gameId: mount.gameId });
      if (index.error) errors.push({ path: `${mount.gitRoot}/${path}`, reason: index.error });
    }
  }
  return { views, errors };
}

function trackedBinaryLeaks(root, mounts, spawnGit) {
  const parent = parentBinaryViews(root, spawnGit);
  if (!parent.views.length) return { leaks: [], errors: parent.errors };
  const privateByDigest = new Map();
  const errors = [...parent.errors];
  for (const mount of mounts) {
    const scanned = privateBinaryViews(root, mount, spawnGit);
    errors.push(...scanned.errors);
    for (const view of scanned.views) {
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
      });
    }
  }
  return { leaks, errors };
}

export function runPrivateGamePreflight(root, options = {}) {
  const repoRoot = resolve(root || process.cwd());
  const spawnGit = options.spawnGit || spawnSync;
  const mounts = options.mounts || listWorkspaceMounts(repoRoot, {
    includePrivate: true, kinds: ["game"], warnings: [],
  }).filter((mount) => mount.visibility === "private");
  if (!mounts.length) return { ok: true, violations: [] };
  const tracked = trackedTextFiles(repoRoot, privateLeakTokens(mounts), spawnGit);
  const nested = nestedGitRoots(repoRoot, mounts, spawnGit);
  const binaries = trackedBinaryLeaks(repoRoot, mounts, spawnGit);
  return auditPrivateGamePreflight(mounts, {
    nestedGitRoots: nested.roots,
    nestedGitErrors: nested.errors,
    trackedTextFiles: tracked.files,
    unscannedTextFiles: tracked.unscanned,
    binaryLeaks: binaries.leaks,
    binaryScanErrors: binaries.errors,
  });
}

function parseArgs(argv) {
  const args = { command: "preflight", root: process.cwd(), json: false, includePrivate: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (["list", "preflight"].includes(arg)) args.command = arg;
    else if (arg === "--root") args.root = argv[++index];
    else if (arg === "--json") args.json = true;
    else if (arg === "--include-private") args.includePrivate = true;
    else if (arg === "--help" || arg === "-h") args.help = true;
    else throw new Error(`unknown argument: ${arg}`);
  }
  return args;
}

function usageText() {
  return `usage:
  node ai_studio/workspace/games.mjs list [--root <repo>] [--include-private] [--json]
  node ai_studio/workspace/games.mjs preflight [--root <repo>] [--json]`;
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
    const result = runPrivateGamePreflight(resolve(args.root));
    if (args.json) console.log(JSON.stringify(result, null, 2));
    else if (result.ok) console.log("ok: private game preflight");
    else console.error(formatPreflightError(result));
    process.exitCode = result.ok ? 0 : 1;
  } catch (error) {
    console.error(error && error.message ? error.message : String(error));
    process.exitCode = 1;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) main();
