#!/usr/bin/env node
import { existsSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";

import { listWorkspaceMounts } from "./catalog.mjs";

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
  const checkNestedGitRoots = Object.hasOwn(state, "nestedGitRoots");
  const trackedTextFiles = Array.isArray(state.trackedTextFiles) ? state.trackedTextFiles : [];
  const unscannedTextFiles = Array.isArray(state.unscannedTextFiles) ? state.unscannedTextFiles : [];
  const violations = [];

  for (const mount of privateMounts) {
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
  return { ok: violations.length === 0, violations };
}

function git(root, args) {
  return spawnSync("git", args, { cwd: root, encoding: "utf8", shell: false, maxBuffer: 64 * 1024 * 1024 });
}

function textBuffer(buffer) {
  if (!buffer || buffer.includes(0)) return null;
  return buffer.toString("utf8");
}

function matchingTrackedPaths(root, tokens, cached) {
  if (!tokens.size) return { paths: [], error: null };
  const args = ["grep", "-z", "-I", "-l", "-F"];
  if (cached) args.push("--cached");
  for (const token of tokens.keys()) args.push("-e", token);
  args.push("--", ".");
  const result = git(root, args);
  if (!result.error && (result.status === 0 || result.status === 1)) {
    return { paths: result.stdout.split("\0").map(slash).filter(Boolean), error: null };
  }
  return { paths: [], error: result.error?.message || result.stderr.trim() || `git grep exited ${result.status}` };
}

function trackedTextFiles(root, tokens) {
  const files = [];
  const unscanned = [];
  const worktree = matchingTrackedPaths(root, tokens, false);
  const index = matchingTrackedPaths(root, tokens, true);
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
    const staged = git(root, ["show", `:${path}`]);
    if (!staged.error && staged.status === 0 && !String(staged.stdout).includes("\0")) {
      files.push({ path, text: staged.stdout, source: "index" });
    } else {
      unscanned.push({ path, reason: staged.error?.message || staged.stderr.trim() || "index read failed" });
    }
  }
  return { files, unscanned };
}

function nestedGitRoots(root, mounts) {
  const valid = [];
  for (const mount of mounts) {
    const gameRoot = resolve(root, mount.gitRoot);
    if (!existsSync(join(gameRoot, ".git"))) continue;
    const result = git(gameRoot, ["rev-parse", "--show-toplevel"]);
    if (result.error || result.status !== 0) continue;
    if (comparable(resolve(result.stdout.trim())) === comparable(gameRoot)) valid.push(mount.gitRoot);
  }
  return valid;
}

export function runPrivateGamePreflight(root, options = {}) {
  const repoRoot = resolve(root || process.cwd());
  const mounts = options.mounts || listWorkspaceMounts(repoRoot, {
    includePrivate: true, kinds: ["game"], warnings: [],
  }).filter((mount) => mount.visibility === "private");
  if (!mounts.length) return { ok: true, violations: [] };
  const tracked = trackedTextFiles(repoRoot, privateLeakTokens(mounts));
  return auditPrivateGamePreflight(mounts, {
    nestedGitRoots: nestedGitRoots(repoRoot, mounts),
    trackedTextFiles: tracked.files,
    unscannedTextFiles: tracked.unscanned,
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
