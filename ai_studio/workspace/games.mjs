#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, isAbsolute, join, posix, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";

import { listRegisteredGames } from "../assets/backlog/storage/sources/games.mjs";

const LOCAL_GAMES_SCHEMA = "ai_studio.workspace.games.local.v1";
const MOUNT_SCHEMA_VERSION = 1;
const DEFAULT_PRIVATE_STORES = ["assets", "taskboard", "canvas", "evidence"];

function slash(value) {
  return String(value || "").replace(/\\/g, "/");
}

function stripTrailingSlash(value) {
  return slash(value).replace(/\/+$/, "");
}

function normalizeRelPath(value, label = "path") {
  const text = stripTrailingSlash(String(value || "").trim().replace(/^\.?\//, ""));
  if (!text) return "";
  if (isAbsolute(text) || /^[a-zA-Z]:\//.test(text) || text.startsWith("//") || text.split("/").includes("..")) {
    throw new Error(`${label} must be repo-relative and stay inside the repository`);
  }
  return text;
}

function normalizeId(value, label = "game id") {
  const id = String(value || "").trim();
  if (!/^[a-z][a-z0-9-]*$/.test(id)) {
    throw new Error(`${label} must be lowercase kebab-case`);
  }
  return id;
}

function readJson(path, fallback) {
  if (!existsSync(path)) return fallback;
  return JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
}

function asStringArray(value, label, fallback = []) {
  if (value === undefined || value === null) return [...fallback];
  if (!Array.isArray(value)) {
    throw new Error(`${label} must be an array`);
  }
  return value.map((item) => String(item).trim()).filter(Boolean);
}

function localRegistryPath(root) {
  return join(root, "ai_studio", "workspace", "games.local.json");
}

function publicMountFromGame(game) {
  return {
    schemaVersion: MOUNT_SCHEMA_VERSION,
    storeId: `game:${game.id}`,
    kind: "game",
    gameId: game.id,
    title: game.title || game.id,
    root: normalizeRelPath(game.folder || `games/${game.id}`, "game root"),
    visibility: "public",
    gitRoot: "",
    commitPolicy: "parent-public",
    enabledStores: ["assets"],
    assetRoot: normalizeRelPath(game.assets || `games/${game.id}/assets`, "game asset root"),
    status: game.status || "active",
    source: "public",
  };
}

function readPublicGameMounts(root) {
  return listRegisteredGames(root).map((game) => publicMountFromGame(game));
}

function readLocalRegistry(root) {
  const registry = readJson(localRegistryPath(root), { schema: LOCAL_GAMES_SCHEMA, games: [] });
  if (registry.schema !== LOCAL_GAMES_SCHEMA) {
    throw new Error(`${localGameRegistryRelPath()}: expected schema ${LOCAL_GAMES_SCHEMA}`);
  }
  return {
    schema: registry.schema,
    games: Array.isArray(registry.games) ? registry.games : [],
  };
}

function writeLocalRegistry(root, registry) {
  const path = localRegistryPath(root);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(registry, null, 2)}\n`, "utf8");
}

function normalizeLocalMount(raw, index = 0) {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
    throw new Error(`${localGameRegistryRelPath()}: games[${index}] must be an object`);
  }
  const gameId = normalizeId(raw.gameId || raw.id, `games[${index}].gameId`);
  const root = normalizeRelPath(raw.root || `games/${gameId}`, `games[${index}].root`);
  if (root !== `games/${gameId}`) {
    throw new Error(`${localGameRegistryRelPath()}: games[${index}].root must be games/${gameId}`);
  }
  const kind = String(raw.kind || "game").trim();
  if (kind !== "game") {
    throw new Error(`${localGameRegistryRelPath()}: games[${index}].kind must be game`);
  }
  const visibility = String(raw.visibility || "private").trim();
  if (!["private", "local"].includes(visibility)) {
    throw new Error(`${localGameRegistryRelPath()}: games[${index}].visibility must be private or local`);
  }
  const schemaVersion = Number(raw.schemaVersion || MOUNT_SCHEMA_VERSION);
  if (schemaVersion !== MOUNT_SCHEMA_VERSION) {
    throw new Error(`${localGameRegistryRelPath()}: games[${index}].schemaVersion must be ${MOUNT_SCHEMA_VERSION}`);
  }
  const gitRoot = normalizeRelPath(raw.gitRoot || root, `games[${index}].gitRoot`);
  if (gitRoot !== root) {
    throw new Error(`${localGameRegistryRelPath()}: games[${index}].gitRoot must match root for nested private games`);
  }
  const commitPolicy = String(raw.commitPolicy || "nested-private").trim();
  if (!["nested-private", "local-only"].includes(commitPolicy)) {
    throw new Error(`${localGameRegistryRelPath()}: games[${index}].commitPolicy must be nested-private or local-only`);
  }
  const mount = {
    schemaVersion,
    storeId: String(raw.storeId || `game:${gameId}`).trim(),
    kind,
    gameId,
    title: String(raw.title || gameId),
    root,
    visibility,
    gitRoot,
    commitPolicy,
    enabledStores: asStringArray(raw.enabledStores, `games[${index}].enabledStores`, DEFAULT_PRIVATE_STORES),
    assetRoot: normalizeRelPath(raw.assetRoot || `${root}/assets`, `games[${index}].assetRoot`),
    publicAlias: raw.publicAlias ? String(raw.publicAlias).trim() : "",
    aliases: asStringArray(raw.aliases, `games[${index}].aliases`),
    remoteHints: asStringArray(raw.remoteHints, `games[${index}].remoteHints`),
    overridesPublicFixture: raw.overridesPublicFixture === true,
    source: "local",
  };
  if (mount.storeId !== `game:${gameId}`) {
    throw new Error(`${localGameRegistryRelPath()}: games[${index}].storeId must be game:${gameId}`);
  }
  if (!mount.enabledStores.length) {
    throw new Error(`${localGameRegistryRelPath()}: games[${index}].enabledStores must not be empty`);
  }
  return mount;
}

function readLocalGameMounts(root) {
  return readLocalRegistry(root).games.map((game, index) => normalizeLocalMount(game, index));
}

export function upsertLocalGameMount(root, options = {}) {
  const gameId = normalizeId(options.gameId || options.id, "game id");
  const next = normalizeLocalMount({
    schemaVersion: MOUNT_SCHEMA_VERSION,
    storeId: `game:${gameId}`,
    kind: "game",
    gameId,
    root: `games/${gameId}`,
    visibility: options.visibility || "private",
    gitRoot: `games/${gameId}`,
    commitPolicy: options.commitPolicy || "nested-private",
    enabledStores: options.enabledStores || DEFAULT_PRIVATE_STORES,
    assetRoot: `games/${gameId}/assets`,
    publicAlias: options.publicAlias || "",
    aliases: options.aliases || [],
    remoteHints: options.remoteHints || [],
    overridesPublicFixture: options.overridesPublicFixture === true,
  });
  const registry = readLocalRegistry(root);
  const existing = registry.games.findIndex((game) => game && (game.gameId === gameId || game.id === gameId));
  const raw = {
    schemaVersion: next.schemaVersion,
    storeId: next.storeId,
    kind: next.kind,
    gameId: next.gameId,
    root: next.root,
    visibility: next.visibility,
    gitRoot: next.gitRoot,
    commitPolicy: next.commitPolicy,
    enabledStores: next.enabledStores,
    assetRoot: next.assetRoot,
    aliases: next.aliases,
    remoteHints: next.remoteHints,
  };
  if (next.publicAlias) raw.publicAlias = next.publicAlias;
  if (next.overridesPublicFixture) raw.overridesPublicFixture = true;
  if (existing >= 0) registry.games[existing] = raw;
  else registry.games.push(raw);
  registry.games.sort((a, b) => String(a.gameId || a.id).localeCompare(String(b.gameId || b.id)));
  writeLocalRegistry(root, registry);
  return next;
}

function localMountIsIncluded(mount, options) {
  if (options.includePrivate === true) return true;
  if (options.activeGameId && options.activeGameId === mount.gameId) return true;
  if (options.activeStoreId && options.activeStoreId === mount.storeId) return true;
  return false;
}

function mergeMounts(publicMounts, localMounts) {
  const byGameId = new Map(publicMounts.map((mount, index) => [mount.gameId, { mount, index }]));
  const out = [...publicMounts];
  for (const local of localMounts) {
    const existing = byGameId.get(local.gameId);
    if (!existing) {
      out.push(local);
      byGameId.set(local.gameId, { mount: local, index: out.length - 1 });
      continue;
    }
    if (local.overridesPublicFixture === true && existing.mount.status === "fixture") {
      out[existing.index] = local;
      byGameId.set(local.gameId, { mount: local, index: existing.index });
      continue;
    }
    throw new Error(`duplicate game id '${local.gameId}' in local private registry; only public fixture overrides are allowed`);
  }
  return out.sort((a, b) => visibilityRank(a) - visibilityRank(b) || a.storeId.localeCompare(b.storeId));
}

function visibilityRank(mount) {
  return mount.visibility === "public" ? 0 : 1;
}

function formatPreflightError(result) {
  const lines = result.violations.map((item) => `- ${item.path}: ${item.reason}`);
  return `private game preflight failed:\n${lines.join("\n")}`;
}

export function localGameRegistryRelPath() {
  return "ai_studio/workspace/games.local.json";
}

export function publicGameRegistryRelPath() {
  return "games/games.json";
}

export function listGameMounts(root, options = {}) {
  const publicMounts = readPublicGameMounts(root);
  if (options.includePrivate !== true && !options.activeGameId && !options.activeStoreId) {
    return publicMounts;
  }
  const localMounts = readLocalGameMounts(root).filter((mount) => localMountIsIncluded(mount, options));
  if (localMounts.length && options.skipPreflight !== true) {
    const preflight = runPrivateGamePreflight(root, { mounts: localMounts });
    if (!preflight.ok) {
      throw new Error(formatPreflightError(preflight));
    }
  }
  return mergeMounts(publicMounts, localMounts);
}

function pathMatchesPrefix(path, prefix) {
  const p = comparablePath(path);
  const pre = comparablePath(prefix);
  return p === pre || p.startsWith(`${pre}/`);
}

function comparablePath(path) {
  const text = stripTrailingSlash(path);
  return process.platform === "win32" ? text.toLowerCase() : text;
}

function pathMayMatchLiteral(path, literal) {
  if (!pathHasGlob(path)) return pathMatchesPrefix(path, literal) || pathMatchesPrefix(literal, path);
  const prefix = globLiteralPrefix(path);
  if (prefix === ".") return true;
  const normalizedLiteral = comparablePath(literal);
  const normalizedPrefix = comparablePath(prefix);
  return (
    pathMatchesPrefix(normalizedLiteral, normalizedPrefix) ||
    pathMatchesPrefix(normalizedPrefix, normalizedLiteral) ||
    normalizedLiteral.startsWith(normalizedPrefix)
  );
}

function pathHasGlob(path) {
  return /[*?\[\]{}]/.test(String(path || ""));
}

function globLiteralPrefix(path) {
  const text = stripTrailingSlash(path);
  const index = text.search(/[*?\[\]{}]/);
  if (index < 0) return text;
  return stripTrailingSlash(text.slice(0, index).replace(/\/+$/, "")) || ".";
}

function normalizedSet(values) {
  const out = new Set();
  for (const value of values || []) {
    const text = comparablePath(value);
    if (text) out.add(text);
  }
  return out;
}

function setHasPathOrPrefix(set, path) {
  const normalized = comparablePath(path);
  return set.has(normalized) || set.has(`${normalized}/`);
}

function privateLeakTokens(mounts) {
  const tokens = new Map();
  for (const mount of mounts) {
    const candidates = [
      ["gameId", mount.gameId],
      ["title", mount.title],
      ["storeId", mount.storeId],
      ["root", mount.root],
      ["gitRoot", mount.gitRoot],
      ["assetRoot", mount.assetRoot],
      ["taskStore", `${mount.root}/.ai_studio/taskboard`],
      ["canvasStore", `${mount.root}/.ai_studio/canvas`],
      ["evidenceStore", `${mount.root}/.ai_studio/evidence`],
      ...asStringArray(mount.aliases, "aliases").map((value) => ["alias", value]),
      ...asStringArray(mount.remoteHints, "remoteHints").map((value) => ["remote", value]),
    ];
    for (const [kind, raw] of candidates) {
      addLeakToken(tokens, raw, { kind, gameId: mount.gameId });
    }
  }
  return tokens;
}

function addLeakToken(tokens, raw, meta) {
  const token = String(raw || "").trim();
  if (!token) return;
  tokens.set(token, meta);
  if (token.includes("/")) {
    tokens.set(token.replace(/\//g, "\\"), meta);
  }
}

function firstTrackedUnder(paths, root) {
  for (const path of paths) {
    if (pathMatchesPrefix(path, root)) return path;
  }
  return "";
}

export function auditPrivateGamePreflight(mounts, state = {}) {
  const privateMounts = (mounts || []).filter((mount) => mount && mount.visibility !== "public");
  const ignoredPaths = normalizedSet(state.ignoredPaths || []);
  const trackedPaths = normalizedSet(state.trackedPaths || []);
  const stagedPaths = normalizedSet(state.stagedPaths || []);
  const gitlinks = normalizedSet(state.gitlinks || []);
  const nestedGitRoots = normalizedSet(state.nestedGitRoots || []);
  const checkNestedGitRoots = Object.hasOwn(state, "nestedGitRoots");
  const trackedTextFiles = Array.isArray(state.trackedTextFiles) ? state.trackedTextFiles : [];
  const unscannedTextFiles = Array.isArray(state.unscannedTextFiles) ? state.unscannedTextFiles : [];
  const localRegistry = localGameRegistryRelPath();
  const violations = [];

  const localRegistryTracked = trackedPaths.has(localRegistry) || stagedPaths.has(localRegistry);
  if ((privateMounts.length || localRegistryTracked) && !setHasPathOrPrefix(ignoredPaths, localRegistry)) {
    violations.push({
      path: localRegistry,
      reason: "local private registry is not ignored by the parent repository",
    });
  }
  if (trackedPaths.has(localRegistry) || stagedPaths.has(localRegistry)) {
    violations.push({
      path: localRegistry,
      reason: "local private registry is tracked or staged by the parent repository",
    });
  }
  if (!privateMounts.length) {
    return { ok: violations.length === 0, violations };
  }

  for (const mount of privateMounts) {
    if (!setHasPathOrPrefix(ignoredPaths, mount.root)) {
      violations.push({
        path: mount.root,
        reason: "private root is not ignored by the parent repository; add it to .git/info/exclude or another local ignore",
      });
    }
    const tracked = firstTrackedUnder(trackedPaths, mount.root);
    if (tracked) {
      violations.push({
        path: tracked,
        reason: "private root is tracked by the parent repository",
      });
    }
    const staged = firstTrackedUnder(stagedPaths, mount.root);
    if (staged) {
      violations.push({
        path: staged,
        reason: "private root has staged parent-repo paths",
      });
    }
    const gitlink = firstTrackedUnder(gitlinks, mount.root);
    if (gitlink) {
      violations.push({
        path: gitlink,
        reason: "parent repository records a gitlink for the private nested repo; keep it ignored/local instead of a public submodule",
      });
    }
    if (checkNestedGitRoots && mount.commitPolicy === "nested-private" && !setHasPathOrPrefix(nestedGitRoots, mount.gitRoot)) {
      violations.push({
        path: `${mount.gitRoot}/.git`,
        reason: "nested-private game root is missing nested git metadata",
      });
    }
  }

  const tokens = privateLeakTokens(privateMounts);
  for (const file of unscannedTextFiles) {
    violations.push({
      path: slash(file.path || ""),
      reason: `tracked text leak scan could not inspect this file (${file.reason || "unreadable"}); unstage it or run a narrower check`,
    });
  }
  for (const file of trackedTextFiles) {
    const path = slash(file.path || "");
    const text = String(file.text || "");
    for (const [token, meta] of tokens) {
      if (text.includes(token)) {
        violations.push({
          path,
          reason: `tracked file leaks private token (${meta.kind}) for ${meta.gameId}`,
        });
        break;
      }
    }
  }

  return { ok: violations.length === 0, violations };
}

function git(root, args, options = {}) {
  return spawnSync("git", args, {
    cwd: root,
    encoding: options.encoding || "utf8",
    input: options.input,
    shell: false,
  });
}

function gitListStage(root) {
  const result = git(root, ["ls-files", "--stage", "-z"]);
  if (result.error || result.status !== 0) return { paths: [], gitlinks: [] };
  const paths = [];
  const gitlinks = [];
  for (const record of result.stdout.split("\0")) {
    if (!record) continue;
    const tab = record.indexOf("\t");
    if (tab < 0) continue;
    const meta = record.slice(0, tab);
    const path = slash(record.slice(tab + 1));
    paths.push(path);
    if (meta.startsWith("160000 ")) gitlinks.push(path);
  }
  return { paths, gitlinks };
}

function gitListStaged(root) {
  const result = git(root, ["diff", "--cached", "--name-only", "-z"]);
  if (result.error || result.status !== 0) return [];
  return result.stdout.split("\0").map((path) => slash(path)).filter(Boolean);
}

function gitIgnoredPaths(root, paths) {
  if (!paths.length) return [];
  const result = git(root, ["check-ignore", "--stdin"], { input: `${paths.join("\n")}\n` });
  if (result.error || result.status > 1) return [];
  return result.stdout.split(/\r?\n/).map((path) => slash(path)).filter(Boolean);
}

function readableTextFile(root, relPath) {
  const path = join(root, relPath);
  let stat;
  try {
    stat = statSync(path);
  } catch {
    return null;
  }
  if (!stat.isFile()) return null;
  let data;
  try {
    data = readFileSync(path);
  } catch {
    return null;
  }
  if (data.includes(0)) return null;
  return data.toString("utf8");
}

function worktreeTextFiles(root, paths) {
  return paths.flatMap((path) => {
    const text = readableTextFile(root, path);
    return text === null ? [] : [{ path, text, source: "worktree" }];
  });
}

function stagedLeakTextFiles(root, paths, mounts) {
  const tokens = [...privateLeakTokens(mounts).keys()];
  if (!paths.length || !tokens.length) return { matches: [], unscanned: [] };
  const args = ["grep", "--cached", "-I", "-n", "-F"];
  for (const token of tokens) {
    args.push("-e", token);
  }
  args.push("--", ...paths);
  const result = spawnSync("git", args, {
    cwd: root,
    encoding: "utf8",
    shell: false,
    maxBuffer: 64 * 1024 * 1024,
  });
  if (result.status === 1) return { matches: [], unscanned: [] };
  if (result.error || result.status !== 0) {
    return {
      matches: [],
      unscanned: paths.map((path) => ({ path, reason: result.error ? result.error.message : `git grep exited ${result.status}` })),
    };
  }
  return { matches: [{ path: "<staged index>", text: result.stdout, source: "index" }], unscanned: [] };
}

function validNestedGitRoots(root, mounts) {
  const out = [];
  for (const mount of mounts) {
    if (!existsSync(join(root, mount.gitRoot, ".git"))) continue;
    const result = spawnSync("git", ["-C", join(root, mount.gitRoot), "rev-parse", "--show-toplevel"], {
      cwd: root,
      encoding: "utf8",
      shell: false,
    });
    const actual = slash(result.stdout || "").trim().toLowerCase();
    const expected = slash(resolve(root, mount.gitRoot)).toLowerCase();
    if (!result.error && result.status === 0 && actual === expected) out.push(mount.gitRoot);
  }
  return out;
}

export function runPrivateGamePreflight(root, options = {}) {
  const repoRoot = resolve(root || process.cwd());
  const mounts = options.mounts || readLocalGameMounts(repoRoot);
  const privateMounts = mounts.filter((mount) => mount.visibility !== "public");
  const stage = gitListStage(repoRoot);
  const stagedPaths = gitListStaged(repoRoot);
  if (
    !privateMounts.length &&
    !normalizedSet(stage.paths).has(localGameRegistryRelPath()) &&
    !normalizedSet(stagedPaths).has(localGameRegistryRelPath())
  ) {
    return { ok: true, violations: [] };
  }
  const ignoreProbePaths = [
    localGameRegistryRelPath(),
    ...privateMounts.map((mount) => mount.root),
  ];
  const stagedLeaks = stagedLeakTextFiles(repoRoot, stagedPaths, privateMounts);
  return auditPrivateGamePreflight(privateMounts, {
    ignoredPaths: gitIgnoredPaths(repoRoot, ignoreProbePaths),
    trackedPaths: stage.paths,
    stagedPaths,
    gitlinks: stage.gitlinks,
    nestedGitRoots: validNestedGitRoots(repoRoot, privateMounts),
    trackedTextFiles: [
      ...worktreeTextFiles(repoRoot, stage.paths),
      ...stagedLeaks.matches,
    ],
    unscannedTextFiles: stagedLeaks.unscanned,
  });
}

function splitCommandChunks(commandText) {
  const chunks = [];
  let current = "";
  let quote = "";
  for (let i = 0; i < String(commandText || "").length; i += 1) {
    const ch = commandText[i];
    if (quote) {
      current += ch;
      if (ch === quote && commandText[i - 1] !== "\\") quote = "";
      continue;
    }
    if (ch === "\"" || ch === "'") {
      quote = ch;
      current += ch;
      continue;
    }
    if (/[;&|\n\r]/.test(ch)) {
      if (current.trim()) chunks.push(current.trim());
      current = "";
      continue;
    }
    current += ch;
  }
  if (current.trim()) chunks.push(current.trim());
  return chunks;
}

function shellTokens(chunk) {
  const matches = chunk.match(/"[^"]*"|'[^']*'|\S+/g) || [];
  return matches.map((token) => unquoteShellToken(token));
}

function unquoteShellToken(token) {
  return String(token || "").replace(/^"(.*)"$/, "$1").replace(/^'(.*)'$/, "$1");
}

function isGitToken(token) {
  const name = basename(String(token || "").replace(/\\/g, "/")).toLowerCase();
  return ["git", "git.exe", "git.cmd"].includes(name);
}

function parseGitOperations(commandText) {
  const operations = [];
  for (const chunk of splitCommandChunks(commandText)) {
    const tokens = shellTokens(chunk);
    operations.push(...parseGitOperationsFromTokens(tokens, chunk));
    operations.push(...parseWrappedGitOperations(tokens, chunk));
  }
  return operations;
}

function parseGitOperationsFromTokens(tokens, chunk) {
  const operations = [];
  for (let i = 0; i < tokens.length; i += 1) {
    if (!isGitToken(tokens[i])) continue;
    let j = i + 1;
    let cwd = "";
    while (j < tokens.length && tokens[j].startsWith("-")) {
      const option = tokens[j];
      j += 1;
      if (option === "-C" && j < tokens.length) {
        cwd = combineGitCwd(cwd, tokens[j]);
        j += 1;
        continue;
      }
      if (["-c", "--git-dir", "--work-tree", "--namespace", "--config-env", "--exec-path"].includes(option) && j < tokens.length) {
        j += 1;
      }
    }
    const rawOp = String(tokens[j] || "").toLowerCase();
    const op = rawOp === "stage" ? "add" : rawOp;
    if (["add", "clean", "commit"].includes(op)) {
      operations.push({ op, args: tokens.slice(j + 1), chunk, cwd });
    }
  }
  return operations;
}

function combineGitCwd(current, next) {
  const text = slash(String(next || "").trim().replace(/^"\s*|\s*"$/g, ""));
  if (!text || text === ".") return current || ".";
  if (isCommandAbsolutePath(text) || !current || current === ".") return text;
  return `${current}/${text}`;
}

function parseWrappedGitOperations(tokens, chunk) {
  const operations = [];
  for (let i = 0; i < tokens.length; i += 1) {
    const lower = basename(String(tokens[i] || "").replace(/\\/g, "/")).toLowerCase();
    if (["bash", "bash.exe", "sh", "sh.exe"].includes(lower) && ["-c", "-lc"].includes(tokens[i + 1])) {
      operations.push(...parseGitOperations(tokens[i + 2] || ""));
    }
    if (["powershell", "powershell.exe", "pwsh", "pwsh.exe"].includes(lower)) {
      const commandIndex = tokens.findIndex((token, index) => index > i && ["-command", "-c"].includes(String(token).toLowerCase()));
      if (commandIndex >= 0) {
        operations.push(...parseGitOperations(tokens[commandIndex + 1] || ""));
      }
    }
  }
  return operations.map((operation) => ({ ...operation, chunk }));
}

function pathArgs(args, root = "", cwd = "") {
  const out = [];
  let afterDashDash = false;
  for (const arg of args) {
    if (arg === "--") {
      afterDashDash = true;
      continue;
    }
    if (!afterDashDash && arg.startsWith("-")) continue;
    out.push(normalizeCommandPathArg(arg, root, cwd));
  }
  return out.filter(Boolean);
}

function normalizeCommandPathArg(value, root = "", cwd = "") {
  const unquoted = String(value || "").trim().replace(/^"\s*|\s*"$/g, "");
  const raw = slash(unquoted);
  const rooted = pathspecUsesRepoRoot(raw) || isCommandAbsolutePath(raw);
  const normalized = normalizeGitPathspecTop(raw);
  const repoPath = normalizeSlashPath(normalized);
  const repoRoot = normalizeCommandPathSegments(resolve(root || process.cwd()));
  let rel = repoRelativeCommandPath(repoPath, repoRoot);
  const cwdRel = normalizeGitCwd(root, cwd);
  if (cwdRel && !rooted) {
    rel = normalizeSlashPath(`${cwdRel}/${rel}`);
  }
  return normalizeCommandPathSegments(rel || ".");
}

function pathspecUsesRepoRoot(value) {
  if (value === ":" || value.startsWith(":/") || value.startsWith(":!") || value.startsWith(":^")) return true;
  const magic = value.match(/^:\(([^)]*)\)/);
  if (!magic) return false;
  const flags = magic[1].split(",").map((flag) => flag.trim().toLowerCase());
  return flags.includes("top") || flags.includes("exclude") || flags.includes("!");
}

function isCommandAbsolutePath(value) {
  const text = slash(value);
  return /^[a-zA-Z]:\//.test(text) || text.startsWith("/") || text.startsWith("//");
}

function normalizeGitPathspecTop(value) {
  if (value === ":" || value === ":/") return ".";
  if (value.startsWith(":/")) return value.slice(2) || ".";
  if (value.startsWith(":!") || value.startsWith(":^")) return ".";
  const magic = value.match(/^:\(([^)]*)\)(.*)$/);
  if (!magic) return value;
  const rest = magic[2] || ".";
  const flags = magic[1].split(",").map((flag) => flag.trim().toLowerCase());
  if (flags.includes("exclude") || flags.includes("!")) return ".";
  return rest;
}

function repoRelativeCommandPath(path, repoRoot) {
  const normalizedPath = stripTrailingSlash(path);
  const normalizedRoot = stripTrailingSlash(repoRoot);
  if (!normalizedPath || !normalizedRoot) return normalizedPath;
  const comparablePath = process.platform === "win32" ? normalizedPath.toLowerCase() : normalizedPath;
  const comparableRoot = process.platform === "win32" ? normalizedRoot.toLowerCase() : normalizedRoot;
  if (comparablePath === comparableRoot) return ".";
  if (comparablePath.startsWith(`${comparableRoot}/`)) {
    return normalizedPath.slice(normalizedRoot.length + 1);
  }
  return normalizedPath;
}

function normalizeCommandPathSegments(path) {
  const repoPath = normalizeSlashPath(path);
  if (!repoPath || repoPath === "." || repoPath === ".." || repoPath.startsWith("../")) return ".";
  return repoPath;
}

function normalizeSlashPath(path) {
  const normalized = stripTrailingSlash(posix.normalize(slash(path || ".")));
  return normalized.replace(/^\.?\//, "") || ".";
}

function normalizeGitCwd(root, cwd) {
  if (!cwd) return "";
  const cwdPath = normalizeSlashPath(cwd);
  const repoRoot = normalizeCommandPathSegments(resolve(root || process.cwd()));
  const rel = repoRelativeCommandPath(cwdPath, repoRoot);
  const normalized = normalizeCommandPathSegments(rel || ".");
  return normalized === "." ? "" : normalized;
}

function gitAddOptionIncludes(args, shortName, longName) {
  return args.some((arg) => arg === `-${shortName}` || arg === `--${longName}` || shortOptionBundleIncludes(arg, shortName));
}

function shortOptionBundleIncludes(arg, shortName) {
  const text = String(arg || "");
  return /^-[^-]/.test(text) && text.length > 2 && text.slice(1).includes(shortName);
}

function hasBroadGitScope(op, args, root = "", cwd = "") {
  if (op === "add") {
    const paths = pathArgs(args, root, cwd);
    const hasPathspecFile = args.some((arg) => arg === "--pathspec-from-file" || String(arg).startsWith("--pathspec-from-file="));
    const hasAll = gitAddOptionIncludes(args, "A", "all") && paths.length === 0;
    const hasUpdateAll = gitAddOptionIncludes(args, "u", "update") && paths.length === 0;
    return (
      hasPathspecFile ||
      hasAll ||
      hasUpdateAll ||
      paths.some((arg) => ["", ".", "games"].includes(arg))
    );
  }
  if (op === "clean") {
    const paths = pathArgs(args, root, cwd);
    return paths.length === 0 || paths.some((arg) => ["", ".", "games"].includes(arg));
  }
  return false;
}

function commandTouchesPrivateRoot(args, mounts, root = "", cwd = "") {
  const paths = pathArgs(args, root, cwd);
  return mounts.some((mount) => paths.some((path) => pathMayMatchLiteral(path, mount.root)));
}

function commandTouchesLocalRegistry(args, root = "", cwd = "") {
  return pathArgs(args, root, cwd).some((path) => pathMayMatchLiteral(path, localGameRegistryRelPath()));
}

export function auditParentGitCommand(root, commandText, options = {}) {
  const repoRoot = resolve(root || process.cwd());
  const mounts = options.mounts || readLocalGameMounts(repoRoot);
  const privateMounts = mounts.filter((mount) => mount.visibility !== "public");
  const operations = parseGitOperations(commandText);
  const violations = [];
  if (!operations.length) return { ok: true, violations, operations };

  for (const operation of operations) {
    if (operation.op === "commit") {
      const preflight = runPrivateGamePreflight(repoRoot, { mounts: privateMounts });
      for (const violation of preflight.violations) {
        violations.push({
          path: violation.path,
          reason: `git commit blocked by private game preflight: ${violation.reason}`,
        });
      }
      continue;
    }
    if (commandTouchesLocalRegistry(operation.args, repoRoot, operation.cwd)) {
      violations.push({
        path: localGameRegistryRelPath(),
        reason: `parent git ${operation.op} command can affect the local private registry; keep it ignored/local`,
      });
      continue;
    }
    if (
      privateMounts.length &&
      (hasBroadGitScope(operation.op, operation.args, repoRoot, operation.cwd) ||
        commandTouchesPrivateRoot(operation.args, privateMounts, repoRoot, operation.cwd))
    ) {
      violations.push({
        path: privateMounts.map((mount) => mount.root).join(", "),
        reason: `parent git ${operation.op} command can affect private game roots; use scoped public paths and keep private roots ignored/local`,
      });
    }
  }

  return { ok: violations.length === 0, violations, operations };
}

function formatGitGuardError(result) {
  const lines = result.violations.map((item) => `- ${item.path}: ${item.reason}`);
  return `private game git guard blocked this command:\n${lines.join("\n")}`;
}

function readStdin() {
  try {
    return readFileSync(0, "utf8");
  } catch {
    return "";
  }
}

function hookCommandText(payload) {
  const input = (payload && (payload.tool_input || payload.toolInput)) || {};
  return String(input.command || input.cmd || "");
}

function parseArgs(argv) {
  const args = { command: "preflight", root: process.cwd(), json: false, includePrivate: false, gitCommand: "" };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (["list", "preflight", "git-guard", "hook-guard"].includes(arg)) args.command = arg;
    else if (arg === "--root") args.root = argv[++i];
    else if (arg === "--json") args.json = true;
    else if (arg === "--include-private") args.includePrivate = true;
    else if (arg === "--command") args.gitCommand = argv[++i];
    else if (arg === "--help" || arg === "-h") args.help = true;
    else throw new Error(`unknown argument: ${arg}`);
  }
  return args;
}

function usageText() {
  return `usage:
  node ai_studio/workspace/games.mjs list [--root <repo>] [--include-private] [--json]
  node ai_studio/workspace/games.mjs preflight [--root <repo>] [--json]
  node ai_studio/workspace/games.mjs git-guard --command "<git command>" [--root <repo>] [--json]
  node ai_studio/workspace/games.mjs hook-guard [--root <repo>]`;
}

function main() {
  try {
    const args = parseArgs(process.argv.slice(2));
    if (args.help) {
      console.log(usageText());
      return;
    }
    if (args.command === "list") {
      const mounts = listGameMounts(resolve(args.root), { includePrivate: args.includePrivate });
      if (args.json) console.log(JSON.stringify({ schema: "ai_studio.workspace.game_mounts.v1", mounts }, null, 2));
      else console.log(mounts.map((mount) => `${mount.visibility}\t${mount.storeId}\t${mount.root}`).join("\n"));
      return;
    }
    if (args.command === "git-guard") {
      const result = auditParentGitCommand(resolve(args.root), args.gitCommand);
      if (args.json) console.log(JSON.stringify(result, null, 2));
      else if (result.ok) console.log("ok: private game git guard");
      else console.error(formatGitGuardError(result));
      process.exitCode = result.ok ? 0 : 1;
      return;
    }
    if (args.command === "hook-guard") {
      let payload = {};
      const stdin = readStdin();
      if (stdin.trim()) payload = JSON.parse(stdin);
      const result = auditParentGitCommand(resolve(args.root), hookCommandText(payload));
      if (!result.ok) {
        console.error(formatGitGuardError(result));
        process.exitCode = 1;
      }
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

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  main();
}

export {
  LOCAL_GAMES_SCHEMA,
  MOUNT_SCHEMA_VERSION,
};
