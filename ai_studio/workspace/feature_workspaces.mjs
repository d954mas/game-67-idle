#!/usr/bin/env node
import { execFileSync } from "node:child_process";
import { randomUUID } from "node:crypto";
import {
  existsSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  realpathSync,
  renameSync,
  rmSync,
  rmdirSync,
  writeFileSync,
} from "node:fs";
import { createServer } from "node:net";
import { hostname } from "node:os";
import { basename, dirname, join, relative, resolve, sep } from "node:path";
import { pathToFileURL } from "node:url";

import { parseDoc } from "../taskboard/store.mjs";
import { selectTaskboardStore, validateTaskboardStoresDetailed } from "../taskboard/stores.mjs";
import { listGameMounts, runPrivateGamePreflight } from "./games.mjs";

export const FEATURE_WORKSPACE_SCHEMA = "ai_studio.feature_workspace.v1";
export const FEATURE_WORKSPACE_RECORD_SCHEMA = "ai_studio.feature_workspace_record.v1";

const TASK_ID_PATTERN = /^T\d{4}$/;
const GAME_ID_PATTERN = /^[a-z][a-z0-9-]*$/;
const WORKSPACE_NAME_PATTERN = /^[a-z0-9](?:[a-z0-9-]{0,46}[a-z0-9])?$/;
const ELIGIBLE_TASK_STATUSES = new Set(["backlog", "todo", "doing"]);

function git(root, args, { trim = true, ...options } = {}) {
  try {
    const output = execFileSync("git", ["-c", `safe.directory=${String(resolve(root)).replace(/\\/g, "/")}`, ...args], {
      cwd: resolve(root),
      encoding: "utf8",
      maxBuffer: 64 * 1024 * 1024,
      stdio: ["ignore", "pipe", "pipe"],
      ...options,
    });
    return trim ? output.trim() : output;
  } catch (error) {
    const stderrLines = String(error?.stderr || "").trim().split(/\r?\n/).filter(Boolean);
    const stderr = stderrLines.findLast((line) => !line.startsWith("warning: unable to access")) || stderrLines.at(-1) || "";
    throw new Error(stderr || error.message || `git ${args[0]} failed`);
  }
}

function gitSucceeds(root, args) {
  try {
    git(root, args);
    return true;
  } catch {
    return false;
  }
}

function optionName(raw) {
  return raw.slice(2).replace(/-([a-z])/g, (_, letter) => letter.toUpperCase());
}

function normalizeTaskId(value) {
  const id = String(value || "").trim().toUpperCase();
  if (!TASK_ID_PATTERN.test(id)) throw new Error("task must use the T#### form");
  return id;
}

function normalizeGameId(value) {
  const id = String(value || "").trim();
  if (!GAME_ID_PATTERN.test(id)) throw new Error("game id must be lowercase kebab-case");
  return id;
}

function normalizePort(value, label) {
  const port = Number(value);
  if (!Number.isInteger(port) || port < 1024 || port > 65535) throw new Error(`${label} must be a port from 1024 to 65535`);
  return port;
}

export function normalizeWorkspaceName(value) {
  const name = String(value || "")
    .trim()
    .toLowerCase()
    .replace(/[ _]+/g, "-")
    .replace(/-+/g, "-");
  if (!WORKSPACE_NAME_PATTERN.test(name)) {
    throw new Error("workspace name must be 1-48 lowercase letters, digits, or internal hyphens");
  }
  return name;
}

export function defaultWorkspaceBase(studioRoot) {
  const root = resolve(studioRoot);
  return join(dirname(root), `${basename(root)}-workspaces`);
}

export function parseCommandLine(argv) {
  const args = [...argv];
  const command = args.shift();
  const commands = new Set(["new", "list", "check", "recover", "reallocate-ports", "remove"]);
  if (!commands.has(command)) throw new Error("command must be new, list, check, recover, reallocate-ports, or remove");
  const options = {};
  if (command !== "new" && command !== "list" && args[0] && !args[0].startsWith("--")) {
    options.name = normalizeWorkspaceName(args.shift());
  }
  const booleans = new Set(["json"]);
  const allowed = new Set(command === "new"
    ? ["game", "task", "name", "root", "base", "branch", "devapiPort", "webPort", "json"]
    : command === "reallocate-ports"
      ? ["base", "devapiPort", "webPort", "json"]
      : ["base", "json"]);
  while (args.length) {
    const token = args.shift();
    if (!token.startsWith("--")) throw new Error(`unexpected argument '${token}'`);
    const key = optionName(token);
    if (!allowed.has(key)) throw new Error(`unknown option '${token}'`);
    if (booleans.has(key)) {
      options[key] = true;
      continue;
    }
    if (!args.length || args[0].startsWith("--")) throw new Error(`missing value for '${token}'`);
    options[key] = args.shift();
  }
  if (command === "new") {
    for (const key of ["game", "task", "name"]) {
      if (!options[key]) throw new Error(`new requires --${key}`);
    }
    options.game = normalizeGameId(options.game);
    options.task = normalizeTaskId(options.task);
    options.name = normalizeWorkspaceName(options.name);
    if (options.devapiPort !== undefined) options.devapiPort = normalizePort(options.devapiPort, "devapi port");
    if (options.webPort !== undefined) options.webPort = normalizePort(options.webPort, "web port");
  } else if (command !== "list" && !options.name) {
    throw new Error(`${command} requires a workspace name`);
  }
  if (command === "reallocate-ports") {
    if (options.devapiPort !== undefined) options.devapiPort = normalizePort(options.devapiPort, "devapi port");
    if (options.webPort !== undefined) options.webPort = normalizePort(options.webPort, "web port");
  }
  return { command, options };
}

function dirtySummary(porcelain) {
  const dirty = { staged: 0, unstaged: 0, untracked: 0, ignored: 0 };
  for (const entry of porcelain.split("\0").filter(Boolean)) {
    const x = entry[0];
    const y = entry[1];
    if (x === "?" && y === "?") {
      dirty.untracked += 1;
      continue;
    }
    if (x === "!" && y === "!") {
      dirty.ignored += 1;
      continue;
    }
    if (x && x !== " " && x !== "?") dirty.staged += 1;
    if (y && y !== " " && y !== "?") dirty.unstaged += 1;
  }
  return dirty;
}

export function inspectRepository(root, { requireAttached = false } = {}) {
  const commit = git(root, ["rev-parse", "--verify", "HEAD"]);
  let ref = null;
  try {
    ref = git(root, ["symbolic-ref", "-q", "HEAD"]);
  } catch {
    if (requireAttached) throw new Error("source game must be on an attached branch");
  }
  const porcelain = git(root, ["status", "--porcelain=v1", "-z", "--untracked-files=all", "--ignored=matching"], { trim: false });
  return { root: resolve(root), commit, ref, dirty: dirtySummary(porcelain) };
}

function committedText(root, commit, path) {
  return git(root, ["show", `${commit}:${path}`]);
}

function committedDoc(root, commit, path, expectedId, kind) {
  let text;
  try {
    text = committedText(root, commit, path);
  } catch {
    throw new Error(`committed ${kind} ${expectedId} is missing`);
  }
  const { fields } = parseDoc(text);
  if (fields.id !== expectedId) throw new Error(`committed ${kind} filename and frontmatter id do not match for ${expectedId}`);
  return fields;
}

function committedDocById(root, commit, directory, expectedId, kind) {
  const filenamePattern = new RegExp(`^${expectedId}(?:-[a-z0-9][a-z0-9-]*)?\\.md$`, "i");
  const paths = git(root, ["ls-tree", "-r", "--name-only", commit, "--", directory])
    .split(/\r?\n/)
    .filter((path) => path.startsWith(`${directory}/`))
    .filter((path) => filenamePattern.test(path.slice(directory.length + 1)));
  if (paths.length !== 1) throw new Error(`expected one committed ${kind} ${expectedId}; found ${paths.length}`);
  return { fields: committedDoc(root, commit, paths[0], expectedId, kind), path: paths[0] };
}

export function inspectCommittedTask(root, commit, rawTaskId, gameId, { requireEligible = true } = {}) {
  const taskId = normalizeTaskId(rawTaskId);
  const prefix = ".ai_studio/taskboard/items/active";
  const paths = git(root, ["ls-tree", "-r", "--name-only", commit, "--", prefix])
    .split(/\r?\n/)
    .filter((path) => path.startsWith(`${prefix}/`))
    .filter((path) => new RegExp(`^${taskId}(?:-[a-z0-9][a-z0-9-]*)?\\.md$`, "i").test(path.slice(prefix.length + 1)));
  if (paths.length !== 1) throw new Error(`expected one committed active task ${taskId}; found ${paths.length}`);
  const task = committedDoc(root, commit, paths[0], taskId, "task");
  if (requireEligible && !ELIGIBLE_TASK_STATUSES.has(task.status)) {
    throw new Error(`task ${taskId} status '${task.status || "missing"}' is not eligible for implementation`);
  }
  if (!/^P\d{3}$/.test(String(task.project || "")) || !/^E\d{3}$/.test(String(task.epic || ""))) {
    throw new Error(`task ${taskId} must reference a project and epic`);
  }
  const project = committedDocById(root, commit, ".ai_studio/taskboard/items/projects", task.project, "project").fields;
  const epic = committedDocById(root, commit, ".ai_studio/taskboard/items/epics", task.epic, "epic").fields;
  if (epic.project !== task.project) throw new Error(`task ${taskId} epic belongs to another project`);
  if (project.kind !== "game" || project.target !== `games/private/${gameId}`) {
    throw new Error(`task ${taskId} project does not own private game ${gameId}`);
  }
  let counters;
  try {
    counters = JSON.parse(committedText(root, commit, ".ai_studio/taskboard/items/.counters.json"));
  } catch {
    throw new Error("committed Taskboard counter is missing or malformed");
  }
  const number = Number(taskId.slice(1));
  if (!Number.isInteger(counters.T) || counters.T < number) {
    throw new Error(`committed Taskboard counter T must cover ${taskId}`);
  }
  return { id: taskId, status: task.status, project: task.project, epic: task.epic, path: paths[0] };
}

function atomicWriteJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  const temporary = `${path}.${process.pid}.${randomUUID()}.tmp`;
  writeFileSync(temporary, `${JSON.stringify(value, null, 2)}\n`, "utf8");
  renameSync(temporary, path);
}

function readJson(path, label) {
  try {
    return JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
  } catch (error) {
    throw new Error(`${label}: ${error.message}`);
  }
}

function assertPhysicalDirectory(path, label) {
  if (!existsSync(path)) return;
  const stat = lstatSync(path);
  if (!stat.isDirectory() || stat.isSymbolicLink()) throw new Error(`${label} must be a physical directory`);
}

function canonicalPhysicalPath(path, label) {
  const requested = resolve(path);
  let cursor = requested;
  while (true) {
    if (existsSync(cursor) && lstatSync(cursor).isSymbolicLink()) {
      throw new Error(`${label} path contains a link or reparse point: ${cursor}`);
    }
    const parent = dirname(cursor);
    if (parent === cursor) break;
    cursor = parent;
  }
  assertPhysicalDirectory(requested, label);
  return existsSync(requested) ? realpathSync(requested) : requested;
}

function assertDirectChild(base, target, label) {
  const child = relative(resolve(base), resolve(target));
  if (!child || child === ".." || child.startsWith(`..${sep}`) || child.includes(sep)) {
    throw new Error(`${label} must be a direct child of the workspace base`);
  }
}

function registryPaths(base, name) {
  const registry = join(base, ".feature-workspaces");
  return {
    registry,
    activeDir: join(registry, "active"),
    tombstoneDir: join(registry, "tombstones"),
    active: join(registry, "active", `${name}.json`),
    tombstone: join(registry, "tombstones", `${name}.json`),
    lock: join(registry, "lock"),
  };
}

function assertRegistryLayout(paths) {
  for (const [path, label] of [
    [paths.registry, "workspace registry"],
    [paths.activeDir, "workspace active registry"],
    [paths.tombstoneDir, "workspace tombstone registry"],
    [paths.lock, "workspace registry lock"],
  ]) {
    if (existsSync(path)) assertPhysicalDirectory(path, label);
  }
}

function delay(milliseconds) {
  return new Promise((resolveDelay) => setTimeout(resolveDelay, milliseconds));
}

async function acquireRegistryLock(base, { allowStale = false, timeoutMs = 5000 } = {}) {
  const paths = registryPaths(base, "unused");
  assertRegistryLayout(paths);
  mkdirSync(paths.registry, { recursive: true });
  assertPhysicalDirectory(paths.registry, "workspace registry");
  const createLock = () => mkdirSync(paths.lock);
  const startedAt = Date.now();
  while (true) {
    try {
      createLock();
      break;
    } catch (error) {
      if (error.code !== "EEXIST") throw error;
      let owner;
      try { owner = readJson(join(paths.lock, "owner.json"), "workspace registry lock"); }
      catch {
        if (Date.now() - startedAt >= timeoutMs) {
          throw new Error(`workspace registry is locked with ambiguous ownership: ${paths.lock}`);
        }
        await delay(50);
        continue;
      }
      let alive = true;
      if (owner.hostname === hostname() && Number.isInteger(owner.pid) && owner.pid > 0) {
        try { process.kill(owner.pid, 0); }
        catch (probeError) { alive = probeError.code !== "ESRCH"; }
      }
      if (!alive && owner.hostname === hostname()) {
        if (!allowStale) throw new Error(`workspace registry has a stale lock; run recover for its workspace`);
        assertPhysicalDirectory(paths.lock, "workspace registry lock");
        const currentOwner = readJson(join(paths.lock, "owner.json"), "workspace registry lock");
        if (JSON.stringify(currentOwner) !== JSON.stringify(owner)) continue;
        rmSync(paths.lock, { recursive: true, force: true });
        continue;
      }
      if (Date.now() - startedAt >= timeoutMs) {
        throw new Error(`workspace registry is locked by ${owner.hostname || "unknown"}:${owner.pid || "unknown"}`);
      }
      await delay(50);
    }
  }
  const operationId = randomUUID();
  atomicWriteJson(join(paths.lock, "owner.json"), {
    operationId,
    pid: process.pid,
    hostname: hostname(),
    createdAt: new Date().toISOString(),
  });
  return () => {
    if (!existsSync(paths.lock)) return;
    assertPhysicalDirectory(paths.lock, "workspace registry lock");
    const owner = readJson(join(paths.lock, "owner.json"), "workspace registry lock");
    if (owner.operationId !== operationId) throw new Error("workspace registry lock ownership changed before release");
    rmSync(paths.lock, { recursive: true, force: true });
  };
}

function liveRecords(base) {
  const activeDir = registryPaths(base, "unused").activeDir;
  if (!existsSync(activeDir)) return [];
  assertPhysicalDirectory(activeDir, "workspace active registry");
  return readdirSync(activeDir)
    .filter((name) => name.endsWith(".json"))
    .map((name) => readJson(join(activeDir, name), `active record ${name}`));
}

function portAvailable(port) {
  return new Promise((resolveAvailability) => {
    const server = createServer();
    server.unref();
    server.once("error", () => resolveAvailability(false));
    server.listen({ host: "127.0.0.1", port, exclusive: true }, () => {
      server.close(() => resolveAvailability(true));
    });
  });
}

async function allocatePort(base, requested, range, field) {
  const leased = new Set(liveRecords(base)
    .filter((record) => record.portsReleased !== true)
    .flatMap((record) => [record.ports?.devapi, record.ports?.web])
    .filter(Number.isInteger));
  const candidates = requested === undefined
    ? Array.from({ length: range[1] - range[0] + 1 }, (_, index) => range[0] + index)
    : [normalizePort(requested, `${field} port`)];
  for (const port of candidates) {
    if (!leased.has(port) && await portAvailable(port)) return port;
  }
  throw new Error(`no available ${field} port`);
}

function engineGitlink(root, commit) {
  const output = git(root, ["ls-tree", commit, "--", "external/neotolis-engine"]);
  const match = output.match(/^160000 commit ([0-9a-f]{40,64})\t/);
  if (!match) throw new Error("Studio commit has no external/neotolis-engine gitlink");
  return match[1];
}

function manifestRecord(manifest) {
  return {
    ...manifest,
    schema: FEATURE_WORKSPACE_RECORD_SCHEMA,
    manifestSchema: manifest.schema,
  };
}

function updateTransaction(manifestPath, activePath, manifest, step) {
  if (step && !manifest.completedSteps.includes(step)) manifest.completedSteps.push(step);
  atomicWriteJson(manifestPath, manifest);
  atomicWriteJson(activePath, manifestRecord(manifest));
}

function injectCrash(input, point) {
  if (input.crashAt !== point) return;
  const error = new Error(`injected crash at ${point}`);
  error.simulatedCrash = true;
  throw error;
}

function discoverPrivateGame(studioRoot, gameId) {
  const mounts = listGameMounts(studioRoot, { activeGameId: gameId, warnings: [] });
  const mount = mounts.find((candidate) => candidate.id === gameId && candidate.visibility === "private");
  if (!mount) throw new Error(`private game '${gameId}' is not mounted in this Studio`);
  return realpathSync(join(studioRoot, mount.root));
}

function validateCreatedWorkspace(studioWorktree, gameId, taskId, { requireEligible = true } = {}) {
  const preflight = runPrivateGamePreflight(studioWorktree, { activeGameId: gameId });
  if (!preflight.ok) throw new Error(`private game preflight failed: ${preflight.violations[0]?.reason || "unknown violation"}`);
  const store = selectTaskboardStore(studioWorktree, { activeGameId: gameId });
  const problems = validateTaskboardStoresDetailed(studioWorktree, [store]);
  if (problems.length) throw new Error(`Taskboard validation failed: ${problems[0].message || problems[0]}`);
  const task = inspectCommittedTask(
    join(studioWorktree, "games/private", gameId),
    "HEAD",
    taskId,
    gameId,
    { requireEligible },
  );
  if (task.id !== taskId) throw new Error(`workspace task ${taskId} is missing`);
}

function rollbackCreate(manifest, manifestPath, activePath) {
  const errors = [];
  if (existsSync(manifest.gameWorktree)) {
    try { git(manifest.sourceGameRoot, ["worktree", "remove", manifest.gameWorktree]); }
    catch (error) { errors.push(`game worktree: ${error.message}`); }
  }
  if (errors.length) return errors;
  if (existsSync(join(manifest.studioWorktree, "external/neotolis-engine"))) {
    try { deinitializeEngine(manifest.studioWorktree); }
    catch (error) { errors.push(`engine submodule: ${error.message}`); }
  }
  if (errors.length) return errors;
  if (existsSync(manifest.studioWorktree)) {
    try { git(manifest.sourceStudioRoot, ["worktree", "remove", "--force", manifest.studioWorktree]); }
    catch (error) { errors.push(`Studio worktree: ${error.message}`); }
  }
  if (!errors.length) {
    if (existsSync(manifestPath)) rmSync(manifestPath);
    const workspaceRoot = dirname(manifestPath);
    if (existsSync(workspaceRoot)) rmdirSync(workspaceRoot);
    if (existsSync(activePath)) rmSync(activePath);
  }
  return errors;
}

export async function createFeatureWorkspace(input) {
  const name = normalizeWorkspaceName(input.name);
  const taskId = normalizeTaskId(input.task);
  const gameId = normalizeGameId(input.game);
  const studioRoot = realpathSync(resolve(input.root || process.cwd()));
  const base = canonicalPhysicalPath(input.base || defaultWorkspaceBase(studioRoot), "workspace base");
  if (base === studioRoot) throw new Error("workspace base cannot be the Studio root");
  mkdirSync(base, { recursive: true });
  const workspaceRoot = join(base, name);
  assertDirectChild(base, workspaceRoot, "workspace path");
  if (existsSync(workspaceRoot)) throw new Error(`workspace '${name}' already exists`);

  const sourceGameRoot = discoverPrivateGame(studioRoot, gameId);
  const studio = inspectRepository(studioRoot);
  const game = inspectRepository(sourceGameRoot, { requireAttached: true });
  const task = inspectCommittedTask(sourceGameRoot, game.commit, taskId, gameId);
  const engineCommit = engineGitlink(studioRoot, studio.commit);
  const sourceEngineRoot = realpathSync(join(studioRoot, "external/neotolis-engine"));
  if (!gitSucceeds(sourceEngineRoot, ["cat-file", "-e", `${engineCommit}^{commit}`])) {
    throw new Error(`engine object ${engineCommit} is unavailable locally`);
  }

  const branch = input.branch
    ? String(input.branch).trim()
    : `agent/${taskId.toLowerCase()}-${name}`;
  if (!/^agent\/t\d{4}-[a-z0-9][a-z0-9-]{0,63}$/.test(branch)) throw new Error("game branch must use agent/t####-name");
  if (gitSucceeds(sourceGameRoot, ["show-ref", "--verify", "--quiet", `refs/heads/${branch}`])) {
    throw new Error(`game branch '${branch}' already exists`);
  }

  const releaseLock = await acquireRegistryLock(base, { timeoutMs: input.lockTimeoutMs ?? 5000 });
  const paths = registryPaths(base, name);
  let manifest;
  let manifestPath;
  try {
    if (existsSync(paths.active) || existsSync(paths.tombstone)) throw new Error(`workspace name '${name}' is already registered`);
    const duplicate = liveRecords(base).find((record) => record.gameId === gameId && record.taskId === taskId);
    if (duplicate) throw new Error(`${gameId}:${taskId} is already assigned to workspace '${duplicate.name}'`);
    const ports = {
      devapi: await allocatePort(base, input.devapiPort, [17900, 17999], "devapi"),
      web: await allocatePort(base, input.webPort, [5190, 5290], "web"),
    };
    if (ports.devapi === ports.web) throw new Error("DevAPI and web ports must be different");
    const studioWorktree = join(workspaceRoot, "studio");
    const gameWorktree = join(studioWorktree, "games", "private", gameId);
    manifestPath = join(workspaceRoot, "workspace.json");
    manifest = {
      schema: FEATURE_WORKSPACE_SCHEMA,
      state: "creating",
      transactionMode: "create",
      operationId: randomUUID(),
      name,
      taskId,
      gameId,
      createdAt: new Date().toISOString(),
      sourceStudioRoot: studio.root,
      sourceStudioCommit: studio.commit,
      sourceGameRoot: game.root,
      sourceGameCommit: game.commit,
      integrationRef: game.ref,
      integrationTipAtCreate: game.commit,
      studioWorktree,
      gameWorktree,
      gameBranch: branch,
      ports,
      sourceDirty: { studio: studio.dirty, game: game.dirty },
      ownership: {
        studioCommonGitDir: resolve(studioRoot, git(studioRoot, ["rev-parse", "--git-common-dir"])),
        gameCommonGitDir: resolve(sourceGameRoot, git(sourceGameRoot, ["rev-parse", "--git-common-dir"])),
        engineGitlink: engineCommit,
      },
      completedSteps: [],
    };
    mkdirSync(paths.activeDir, { recursive: true });
    mkdirSync(paths.tombstoneDir, { recursive: true });
    injectCrash(input, "before-active-record");
    atomicWriteJson(paths.active, manifestRecord(manifest));
    injectCrash(input, "after-active-record");
    mkdirSync(workspaceRoot);
    injectCrash(input, "after-workspace-directory");
    updateTransaction(manifestPath, paths.active, manifest, "manifest-published");
    injectCrash(input, "after-manifest");

    git(studioRoot, ["worktree", "add", "--detach", studioWorktree, studio.commit]);
    updateTransaction(manifestPath, paths.active, manifest, "studio-worktree-added");
    injectCrash(input, "after-studio-worktree");
    git(studioWorktree, [
      "-c", "protocol.file.allow=always",
      "-c", `submodule.external/neotolis-engine.url=${sourceEngineRoot.replace(/\\/g, "/")}`,
      "submodule", "update", "--init", "--checkout", "--no-fetch", "--", "external/neotolis-engine",
    ]);
    updateTransaction(manifestPath, paths.active, manifest, "engine-initialized");
    injectCrash(input, "after-engine");
    mkdirSync(join(studioWorktree, "games", "private"), { recursive: true });
    git(sourceGameRoot, ["worktree", "add", "-b", branch, gameWorktree, game.commit]);
    updateTransaction(manifestPath, paths.active, manifest, "game-worktree-added");
    injectCrash(input, "after-game-worktree");
    validateCreatedWorkspace(studioWorktree, gameId, taskId);
    injectCrash(input, "before-ready");
    manifest.state = "ready";
    manifest.transactionMode = null;
    updateTransaction(manifestPath, paths.active, manifest, "validated");
    return manifest;
  } catch (error) {
    if (error.simulatedCrash) throw error;
    if (manifest) {
      const rollbackErrors = rollbackCreate(manifest, manifestPath, paths.active);
      if (rollbackErrors.length) {
        manifest.state = "recovery-required";
        manifest.recoveryErrors = rollbackErrors;
        if (manifestPath && existsSync(dirname(manifestPath))) atomicWriteJson(manifestPath, manifest);
        atomicWriteJson(paths.active, manifestRecord(manifest));
        throw new Error(`${error.message}; rollback incomplete: ${rollbackErrors.join("; ")}`);
      }
    }
    throw error;
  } finally {
    releaseLock();
  }
}

function comparablePath(value) {
  const normalized = resolve(String(value || "")).replace(/\\/g, "/").replace(/\/+$/, "");
  return process.platform === "win32" ? normalized.toLowerCase() : normalized;
}

function derivedWorkspace(base, name) {
  const normalizedName = normalizeWorkspaceName(name);
  const canonicalBase = canonicalPhysicalPath(base, "workspace base");
  const root = join(canonicalBase, normalizedName);
  assertDirectChild(canonicalBase, root, "workspace path");
  return {
    base: canonicalBase,
    name: normalizedName,
    root,
    manifestPath: join(root, "workspace.json"),
    studioWorktree: join(root, "studio"),
    paths: registryPaths(canonicalBase, normalizedName),
  };
}

function loadWorkspaceAuthority(base, name) {
  const derived = derivedWorkspace(base, name);
  assertRegistryLayout(derived.paths);
  if (existsSync(derived.paths.active)) {
    const record = readJson(derived.paths.active, `active workspace ${derived.name}`);
    if (record.schema !== FEATURE_WORKSPACE_RECORD_SCHEMA) throw new Error("active workspace record schema is invalid");
    return { ...derived, record, removed: false };
  }
  if (existsSync(derived.paths.tombstone)) {
    const record = readJson(derived.paths.tombstone, `workspace tombstone ${derived.name}`);
    return { ...derived, record, removed: true };
  }
  throw new Error(`workspace '${derived.name}' is not registered`);
}

function assertOwnedPaths(authority, { allowMissing = false } = {}) {
  normalizeGameId(authority.record.gameId);
  normalizeTaskId(authority.record.taskId);
  const expectedGame = join(authority.studioWorktree, "games", "private", authority.record.gameId);
  const comparisons = [
    [authority.record.name, authority.name, "name"],
    [authority.record.studioWorktree, authority.studioWorktree, "Studio worktree"],
    [authority.record.gameWorktree, expectedGame, "game worktree"],
  ];
  for (const [actual, expected, label] of comparisons) {
    const equal = label === "name"
      ? String(actual).toLowerCase() === String(expected).toLowerCase()
      : comparablePath(actual) === comparablePath(expected);
    if (!equal) throw new Error(`${label} does not match the registry-owned path`);
  }
  for (const [path, label] of [
    [authority.root, "workspace"],
    [authority.studioWorktree, "Studio worktree"],
    [expectedGame, "game worktree"],
  ]) {
    if (!allowMissing || existsSync(path)) assertPhysicalDirectory(path, label);
  }
  return expectedGame;
}

function registeredWorktree(repo, worktree) {
  const wanted = comparablePath(worktree);
  const lines = git(repo, ["worktree", "list", "--porcelain"]).split(/\r?\n/);
  return lines.some((line) => line.startsWith("worktree ") && comparablePath(line.slice(9)) === wanted);
}

function dirtyCount(dirty) {
  return dirty.staged + dirty.unstaged + dirty.untracked;
}

function lockDiagnostics(base) {
  const lock = registryPaths(base, "unused").lock;
  if (!existsSync(lock)) return null;
  try { return { path: lock, owner: readJson(join(lock, "owner.json"), "workspace registry lock") }; }
  catch (error) { return { path: lock, error: error.message }; }
}

function commonGitDir(root) {
  return realpathSync(resolve(root, git(root, ["rev-parse", "--git-common-dir"])));
}

function assertSourceIdentity(record) {
  assertPhysicalDirectory(record.sourceStudioRoot, "source Studio root");
  assertPhysicalDirectory(record.sourceGameRoot, "source game root");
  if (comparablePath(commonGitDir(record.sourceStudioRoot)) !== comparablePath(record.ownership?.studioCommonGitDir)) {
    throw new Error("source Studio common Git directory does not match workspace ownership");
  }
  if (comparablePath(commonGitDir(record.sourceGameRoot)) !== comparablePath(record.ownership?.gameCommonGitDir)) {
    throw new Error("source game common Git directory does not match workspace ownership");
  }
}

function ignoredPaths(root) {
  return git(root, ["ls-files", "--others", "-i", "--exclude-standard", "-z"], { trim: false })
    .split("\0")
    .filter(Boolean)
    .map((path) => path.replace(/\\/g, "/"));
}

function cleanReproducibleIgnored(root, protectedRoots = []) {
  const allowed = new Set(["build", "tmp", "out", ".cache", ".vite"]);
  const removable = new Set();
  const unknown = [];
  for (const ignored of ignoredPaths(root)) {
    const absolute = resolve(root, ignored);
    const child = relative(resolve(root), absolute);
    if (!child || child === ".." || child.startsWith(`..${sep}`)) throw new Error("ignored path escapes its repository");
    const candidate = comparablePath(absolute);
    if (protectedRoots.some((item) => {
      const protectedPath = comparablePath(item);
      return candidate === protectedPath || candidate.startsWith(`${protectedPath}/`) || protectedPath.startsWith(`${candidate}/`);
    })) continue;
    const top = child.split(/[\\/]/, 1)[0];
    if (!allowed.has(top) && !top.startsWith("cmake-build-")) unknown.push(ignored);
    else removable.add(join(root, top));
  }
  if (unknown.length) throw new Error(`unknown ignored data would be removed: ${unknown.slice(0, 3).join(", ")}`);
  for (const path of removable) {
    if (!existsSync(path)) continue;
    assertNoLinksRecursively(path);
    rmSync(path, { recursive: true, force: true });
  }
}

function assertNoLinksRecursively(path) {
  const stat = lstatSync(path);
  if (stat.isSymbolicLink()) throw new Error(`ignored output is a link or reparse point: ${path}`);
  if (!stat.isDirectory()) return;
  for (const entry of readdirSync(path)) assertNoLinksRecursively(join(path, entry));
}

function cleanOwnedIgnored(authority, record, safety) {
  if (existsSync(safety.gameWorktree)) cleanReproducibleIgnored(safety.gameWorktree);
  if (existsSync(join(safety.engineRoot, ".git"))) cleanReproducibleIgnored(safety.engineRoot);
  if (existsSync(authority.studioWorktree)) {
    cleanReproducibleIgnored(authority.studioWorktree, [safety.gameWorktree, safety.engineRoot]);
  }
}

function assertRemovalSafety(authority, record) {
  const gameWorktree = assertOwnedPaths({ ...authority, record }, { allowMissing: true });
  assertSourceIdentity(record);
  const engineRoot = join(authority.studioWorktree, "external/neotolis-engine");
  if (existsSync(authority.studioWorktree)) {
    if (!registeredWorktree(record.sourceStudioRoot, authority.studioWorktree)) throw new Error("Studio worktree ownership mismatch");
    const studio = inspectRepository(authority.studioWorktree);
    if (studio.commit !== record.sourceStudioCommit) throw new Error("Studio worktree commit ownership mismatch");
    if (dirtyCount(studio.dirty)) throw new Error("Studio worktree is dirty; removal refused");
  }
  if (existsSync(gameWorktree)) {
    if (!registeredWorktree(record.sourceGameRoot, gameWorktree)) throw new Error("game worktree ownership mismatch");
    const game = inspectRepository(gameWorktree, { requireAttached: true });
    if (game.ref !== `refs/heads/${record.gameBranch}`) throw new Error("game branch ownership mismatch");
    if (!gitSucceeds(gameWorktree, ["merge-base", "--is-ancestor", record.sourceGameCommit, "HEAD"])) {
      throw new Error("game worktree no longer descends from its recorded source commit");
    }
    if (dirtyCount(game.dirty)) throw new Error("game worktree is dirty; removal refused");
  }
  if (existsSync(join(engineRoot, ".git"))) {
    const engine = inspectRepository(engineRoot);
    if (engine.commit !== record.ownership.engineGitlink) throw new Error("engine gitlink ownership mismatch");
    if (dirtyCount(engine.dirty)) throw new Error("engine worktree is dirty; removal refused");
  }
  return { gameWorktree, engineRoot };
}

function assertManifestMatchesRecord(authority, record) {
  if (!existsSync(authority.manifestPath)) return;
  const manifest = readJson(authority.manifestPath, `workspace manifest ${authority.name}`);
  if (manifest.schema !== FEATURE_WORKSPACE_SCHEMA) throw new Error("workspace manifest schema is invalid");
  for (const key of [
    "operationId", "name", "taskId", "gameId", "sourceStudioRoot", "sourceStudioCommit",
    "sourceGameRoot", "sourceGameCommit", "studioWorktree", "gameWorktree", "gameBranch",
  ]) {
    const actual = key.endsWith("Root") || key.endsWith("Worktree")
      ? comparablePath(manifest[key])
      : String(manifest[key]);
    const expected = key.endsWith("Root") || key.endsWith("Worktree")
      ? comparablePath(record[key])
      : String(record[key]);
    if (actual !== expected) throw new Error(`workspace manifest and active record disagree on ${key}`);
  }
}

export async function listFeatureWorkspaces({ base }) {
  const canonicalBase = canonicalPhysicalPath(base, "workspace base");
  return Promise.all(liveRecords(canonicalBase).map(async (record) => {
    const row = {
      name: record.name,
      state: record.state,
      taskId: record.taskId,
      gameId: record.gameId,
      gameBranch: record.gameBranch,
      sourceCommits: { studio: record.sourceStudioCommit, game: record.sourceGameCommit },
      ports: record.ports,
      studioWorktree: record.studioWorktree,
      gameWorktree: record.gameWorktree,
    };
    try {
      const checked = await checkFeatureWorkspace({ base: canonicalBase, name: record.name });
      return {
        ...row,
        ok: checked.ok,
        dirty: { studio: checked.studio?.dirty, game: checked.game?.dirty },
        divergence: checked.game?.divergence,
        ignored: checked.ignored,
        registrations: { studio: checked.studio?.registered, game: checked.game?.registered },
        registryLock: checked.registryLock,
      };
    } catch (error) {
      return { ...row, ok: false, error: error.message };
    }
  }));
}

export async function checkFeatureWorkspace({ base, name }) {
  const authority = loadWorkspaceAuthority(base, name);
  if (authority.removed) return { name: authority.name, state: "removed", removed: true, tombstone: authority.record };
  const gameWorktree = assertOwnedPaths(authority);
  const record = authority.record;
  const problems = [];
  if (!existsSync(authority.manifestPath)) problems.push("workspace manifest is missing");
  else {
    const manifest = readJson(authority.manifestPath, `workspace manifest ${authority.name}`);
    if (manifest.schema !== FEATURE_WORKSPACE_SCHEMA) problems.push("workspace manifest schema is invalid");
    if (manifest.operationId !== record.operationId) problems.push("manifest operation id differs from active record");
    if (JSON.stringify(manifest.ports) !== JSON.stringify(record.ports)) problems.push("manifest ports differ from the active lease");
  }
  const studioRegistered = registeredWorktree(record.sourceStudioRoot, authority.studioWorktree);
  const gameRegistered = registeredWorktree(record.sourceGameRoot, gameWorktree);
  if (!studioRegistered) problems.push("Studio worktree registration is missing");
  if (!gameRegistered) problems.push("game worktree registration is missing");
  const studio = inspectRepository(authority.studioWorktree);
  const game = inspectRepository(gameWorktree, { requireAttached: true });
  if (studio.commit !== record.sourceStudioCommit) problems.push("Studio commit differs from the recorded source commit");
  if (game.ref !== `refs/heads/${record.gameBranch}`) problems.push("game branch ownership differs from the active record");
  const engineRoot = join(authority.studioWorktree, "external/neotolis-engine");
  assertPhysicalDirectory(engineRoot, "engine worktree");
  const engine = inspectRepository(engineRoot);
  if (engine.commit !== record.ownership.engineGitlink) problems.push("engine commit differs from the recorded gitlink");
  try {
    validateCreatedWorkspace(authority.studioWorktree, record.gameId, record.taskId, { requireEligible: false });
  } catch (error) {
    problems.push(error.message);
  }
  let integrationTip = null;
  let divergence = null;
  try {
    integrationTip = git(record.sourceGameRoot, ["rev-parse", "--verify", record.integrationRef]);
    const [behind, ahead] = git(gameWorktree, ["rev-list", "--left-right", "--count", `${record.integrationRef}...HEAD`])
      .split(/\s+/)
      .map(Number);
    divergence = { integrationTip, behindIntegration: behind, aheadOfIntegration: ahead };
  } catch (error) {
    problems.push(`integration ref diagnostic failed: ${error.message}`);
  }
  return {
    name: authority.name,
    state: record.state,
    removed: false,
    ok: problems.length === 0,
    problems,
    ports: record.ports,
    portOccupied: {
      devapi: !(await portAvailable(record.ports.devapi)),
      web: !(await portAvailable(record.ports.web)),
    },
    studio: { commit: studio.commit, dirty: studio.dirty, registered: studioRegistered },
    game: {
      commit: game.commit,
      branch: record.gameBranch,
      dirty: game.dirty,
      registered: gameRegistered,
      aheadOfSource: Number(git(gameWorktree, ["rev-list", "--count", `${record.sourceGameCommit}..HEAD`])),
      divergence,
    },
    engine: { commit: engine.commit, dirty: engine.dirty },
    ignored: {
      studio: ignoredPaths(authority.studioWorktree).length,
      game: ignoredPaths(gameWorktree).length,
      engine: ignoredPaths(engineRoot).length,
    },
    registryLock: lockDiagnostics(authority.base),
  };
}

export async function reallocateWorkspacePorts({ base, name, devapiPort, webPort, failAt }) {
  const authority = loadWorkspaceAuthority(base, name);
  if (authority.removed) throw new Error(`workspace '${authority.name}' was removed`);
  assertOwnedPaths(authority);
  const releaseLock = await acquireRegistryLock(authority.base);
  try {
    const record = readJson(authority.paths.active, `active workspace ${authority.name}`);
    assertManifestMatchesRecord(authority, record);
    if (record.state !== "ready") throw new Error("ports can be changed only for a ready workspace");
    const excluded = new Set([record.ports.devapi, record.ports.web]);
    let nextDevapi = await allocatePort(authority.base, devapiPort, [17900, 17999], "devapi");
    if (excluded.has(nextDevapi) && devapiPort === undefined) {
      nextDevapi = await allocatePort(authority.base, nextDevapi + 1, [17900, 17999], "devapi");
    }
    let nextWeb = await allocatePort(authority.base, webPort, [5190, 5290], "web");
    if (excluded.has(nextWeb) && webPort === undefined) {
      nextWeb = await allocatePort(authority.base, nextWeb + 1, [5190, 5290], "web");
    }
    record.ports = { devapi: nextDevapi, web: nextWeb };
    if (record.ports.devapi === record.ports.web) throw new Error("DevAPI and web ports must be different");
    record.portsRevision = randomUUID();
    const manifest = readJson(authority.manifestPath, `workspace manifest ${authority.name}`);
    manifest.ports = record.ports;
    atomicWriteJson(authority.paths.active, record);
    if (failAt === "after-active-port-lease") throw new Error("injected failure after active port lease");
    manifest.portsRevision = record.portsRevision;
    atomicWriteJson(authority.manifestPath, manifest);
    return { name: authority.name, state: record.state, ports: record.ports };
  } finally {
    releaseLock();
  }
}

function deinitializeEngine(studioWorktree) {
  const enginePath = join(studioWorktree, "external/neotolis-engine");
  if (!existsSync(enginePath)) return;
  if (!existsSync(join(enginePath, ".git"))) {
    rmdirSync(enginePath);
    return;
  }
  const worktreeGitDir = resolve(studioWorktree, git(studioWorktree, ["rev-parse", "--git-dir"]));
  const commonGitDir = resolve(studioWorktree, git(studioWorktree, ["rev-parse", "--git-common-dir"]));
  const adminRelative = relative(commonGitDir, worktreeGitDir);
  if (!adminRelative.startsWith(`worktrees${sep}`)) {
    throw new Error("Studio linked-worktree Git administration path is not owned by this workspace");
  }
  git(studioWorktree, ["submodule", "deinit", "-f", "--", "external/neotolis-engine"]);
  if (existsSync(enginePath)) rmdirSync(enginePath);
  const modulesDir = join(worktreeGitDir, "modules");
  if (existsSync(modulesDir)) rmSync(modulesDir, { recursive: true, force: true });
}

async function continueRemoval(authority, record, { failAt } = {}) {
  const safety = assertRemovalSafety(authority, record);
  const gameWorktree = join(authority.studioWorktree, "games", "private", record.gameId);
  cleanOwnedIgnored(authority, record, safety);
  if (existsSync(gameWorktree)) {
    git(record.sourceGameRoot, ["worktree", "remove", gameWorktree]);
    record.completedSteps = [...new Set([...(record.completedSteps || []), "game-worktree-removed"])];
    atomicWriteJson(authority.paths.active, record);
  }
  if (existsSync(authority.studioWorktree)) {
    deinitializeEngine(authority.studioWorktree);
    record.completedSteps = [...new Set([...(record.completedSteps || []), "engine-deinitialized"])];
    atomicWriteJson(authority.paths.active, record);
    git(record.sourceStudioRoot, ["worktree", "remove", "--force", authority.studioWorktree]);
    record.completedSteps = [...new Set([...(record.completedSteps || []), "studio-worktree-removed"])];
    atomicWriteJson(authority.paths.active, record);
  }
  if (existsSync(authority.manifestPath)) rmSync(authority.manifestPath);
  if (existsSync(authority.root)) rmdirSync(authority.root);
  record.portsReleased = true;
  record.state = "removed";
  record.removedAt = new Date().toISOString();
  atomicWriteJson(authority.paths.active, record);
  if (failAt === "before-tombstone-rename") throw new Error("injected failure before tombstone rename");
  renameSync(authority.paths.active, authority.paths.tombstone);
  record.schema = "ai_studio.feature_workspace_tombstone.v1";
  atomicWriteJson(authority.paths.tombstone, record);
  return record;
}

export async function removeFeatureWorkspace({ base, name, failAt }) {
  const authority = loadWorkspaceAuthority(base, name);
  if (authority.removed) return authority.record;
  const gameWorktree = assertOwnedPaths(authority);
  const releaseLock = await acquireRegistryLock(authority.base);
  try {
    const record = readJson(authority.paths.active, `active workspace ${authority.name}`);
    assertManifestMatchesRecord(authority, record);
    if (record.state === "removing") return continueRemoval(authority, record, { failAt });
    if (record.state !== "ready") throw new Error(`workspace is ${record.state}; run recover`);
    const game = inspectRepository(gameWorktree, { requireAttached: true });
    assertRemovalSafety(authority, record);
    if (!(await portAvailable(record.ports.devapi)) || !(await portAvailable(record.ports.web))) {
      throw new Error("workspace ports are occupied; stop its processes before removal");
    }
    if (game.ref !== `refs/heads/${record.gameBranch}`) throw new Error("game branch ownership mismatch");
    record.unmergedBranch = !gitSucceeds(record.sourceGameRoot, ["merge-base", "--is-ancestor", record.gameBranch, record.integrationRef]);
    record.state = "removing";
    record.transactionMode = "remove";
    record.removalStartedAt = new Date().toISOString();
    atomicWriteJson(authority.paths.active, record);
    if (existsSync(authority.manifestPath)) {
      const manifest = readJson(authority.manifestPath, `workspace manifest ${authority.name}`);
      manifest.state = "removing";
      manifest.transactionMode = "remove";
      atomicWriteJson(authority.manifestPath, manifest);
    }
    return await continueRemoval(authority, record, { failAt });
  } catch (error) {
    if (existsSync(authority.paths.active)) {
      const record = readJson(authority.paths.active, `active workspace ${authority.name}`);
      if (record.state === "removing") {
        record.state = "recovery-required";
        record.transactionMode = "remove";
        record.recoveryErrors = [error.message];
        atomicWriteJson(authority.paths.active, record);
      }
    }
    throw error;
  } finally {
    releaseLock();
  }
}

export async function recoverFeatureWorkspace({ base, name }) {
  const authority = loadWorkspaceAuthority(base, name);
  if (authority.removed) return authority.record;
  const releaseLock = await acquireRegistryLock(authority.base, { allowStale: true });
  try {
    const record = readJson(authority.paths.active, `active workspace ${authority.name}`);
    assertOwnedPaths({ ...authority, record }, { allowMissing: true });
    assertSourceIdentity(record);
    assertManifestMatchesRecord(authority, record);
    const mode = record.transactionMode || (record.state === "creating" ? "create" : null);
    if (mode === "remove") return await continueRemoval(authority, record);
    if (mode === "create") {
      const safety = assertRemovalSafety({ ...authority, record }, record);
      cleanOwnedIgnored(authority, record, safety);
      const manifest = { ...record, schema: FEATURE_WORKSPACE_SCHEMA };
      const errors = rollbackCreate(manifest, authority.manifestPath, authority.paths.active);
      if (errors.length) throw new Error(`creation recovery incomplete: ${errors.join("; ")}`);
      return { name: authority.name, state: "rolled-back" };
    }
    throw new Error(`workspace '${authority.name}' does not need recovery`);
  } finally {
    releaseLock();
  }
}

function printHuman(command, result) {
  if (command === "list") {
    if (!result.length) {
      console.log("No active feature workspaces.");
      return;
    }
    for (const row of result) console.log(`${row.name}\t${row.state}\t${row.gameId}:${row.taskId}\t${row.gameBranch}`);
    return;
  }
  if (command === "check") {
    if (result.removed) console.log(`${result.name}: removed`);
    else console.log(`${result.name}: ${result.ok ? "OK" : `FAILED (${result.problems.join("; ")})`}`);
    return;
  }
  if (command === "new") {
    console.log(`Workspace ready: ${result.studioWorktree}`);
    console.log(`Task: ${result.gameId}:${result.taskId}`);
    console.log(`Game branch: ${result.gameBranch}`);
    console.log(`Studio commit: ${result.sourceStudioCommit}`);
    console.log(`Game commit: ${result.sourceGameCommit}`);
    console.log(`DevAPI: ${result.ports.devapi}; web: ${result.ports.web}`);
    for (const [repo, dirty] of Object.entries(result.sourceDirty)) {
      if (dirtyCount(dirty)) console.log(`Warning: omitted dirty ${repo} state: ${JSON.stringify(dirty)}`);
    }
    console.log(`Run the agent from: ${result.gameWorktree}`);
    console.log(`Game launch: <game.exe> --devapi ${result.ports.devapi} --fresh-state`);
    console.log(`Web launch: node tools/serve_web.mjs --port ${result.ports.web} --dir <dir>`);
    return;
  }
  if (command === "reallocate-ports") {
    console.log(`${result.name}: DevAPI ${result.ports.devapi}; web ${result.ports.web}`);
    return;
  }
  console.log(`${result.name}: ${result.state}`);
  if (result.unmergedBranch) console.log(`Warning: branch ${result.gameBranch} is not merged and was preserved.`);
}

export async function main(argv = process.argv.slice(2)) {
  const parsed = parseCommandLine(argv);
  const root = resolve(parsed.options.root || process.cwd());
  const base = resolve(parsed.options.base || defaultWorkspaceBase(root));
  let result;
  if (parsed.command === "new") result = await createFeatureWorkspace({ ...parsed.options, root, base });
  else if (parsed.command === "list") result = await listFeatureWorkspaces({ base });
  else if (parsed.command === "check") result = await checkFeatureWorkspace({ base, name: parsed.options.name });
  else if (parsed.command === "recover") result = await recoverFeatureWorkspace({ base, name: parsed.options.name });
  else if (parsed.command === "reallocate-ports") result = await reallocateWorkspacePorts({ ...parsed.options, base });
  else result = await removeFeatureWorkspace({ base, name: parsed.options.name });
  if (parsed.options.json) console.log(JSON.stringify(result, null, 2));
  else printHuman(parsed.command, result);
  if (parsed.command === "check" && result.ok === false) process.exitCode = 1;
  return result;
}

if (import.meta.url === pathToFileURL(process.argv[1] || "").href) {
  try {
    await main();
  } catch (error) {
    if (process.argv.includes("--json")) console.error(JSON.stringify({ ok: false, error: error.message }));
    else console.error(`error: ${error.message}`);
    process.exitCode = 1;
  }
}
