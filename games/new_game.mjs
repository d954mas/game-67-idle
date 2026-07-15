#!/usr/bin/env node
// Transactionally copy a registered template into games/<game-id>.
import {
  chmodSync, existsSync, mkdirSync, readFileSync, renameSync, rmSync, statSync, writeFileSync,
} from "node:fs";
import { randomUUID } from "node:crypto";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { listRegisteredTemplates } from "../ai_studio/assets/sources/ops.mjs";
import { writeVscodeProjectFiles } from "../ai_studio/dev_environment/vscode_projects.mjs";
import { ensureProject, updateDoc } from "../ai_studio/taskboard/lib.mjs";
import { listGameMounts, runPrivateGamePreflight } from "../ai_studio/workspace/games.mjs";
import { copyGitSourceTree } from "../ai_studio/workspace/copy_source_tree.mjs";

const defaultRepoRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const preflightScript = fileURLToPath(new URL("../ai_studio/workspace/games.mjs", import.meta.url));
const EXACT_SEMVER = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/;

function takeValue(argv, index, flag) {
  const value = argv[index + 1];
  if (!value || value.startsWith("--")) {
    if (flag === "--visibility") throw new Error("--visibility requires public or private");
    throw new Error(`${flag} requires a value`);
  }
  return value;
}

export function parseArgs(argv) {
  const out = {
    id: "", title: "", storageNamespace: "", template: "", from: "templates/template",
    root: "", replace: false, private: false, visibility: "", requireVisibility: false,
    publicAlias: "", help: false,
  };
  for (let i = 0; i < argv.length; i += 1) {
    const flag = argv[i];
    if (flag === "--replace") out.replace = true;
    else if (flag === "--force") throw new Error("--force was retired; use explicit --replace");
    else if (flag === "--private") out.private = true;
    else if (["--id", "--title", "--storage-namespace", "--template", "--from", "--root", "--visibility", "--public-alias"].includes(flag)) {
      const value = takeValue(argv, i, flag);
      i += 1;
      if (flag === "--id") out.id = value;
      else if (flag === "--title") out.title = value;
      else if (flag === "--storage-namespace") out.storageNamespace = value;
      else if (flag === "--template") out.template = value;
      else if (flag === "--from") out.from = value;
      else if (flag === "--root") out.root = value;
      else if (flag === "--visibility") out.visibility = value;
      else out.publicAlias = value;
    } else if (flag === "--require-visibility") out.requireVisibility = true;
    else if (flag === "--help" || flag === "-h") out.help = true;
    else throw new Error(`unknown argument: ${flag}`);
  }
  return out;
}

export function usageText() {
  return [
    "usage: node games/new_game.mjs [--root <repo>] --id <game-id> [--title <title>] [--storage-namespace <id>] [--visibility public|private] [--template <template-id>|--from <path>] [--private] [--public-alias <safe-name>] [--require-visibility] [--replace]  (lowercase, kebab)",
    "",
    "visibility: public registers the game in the parent Studio; private creates a nested private repo.",
    "compatibility: omitting --visibility still creates a public/tracked game; human/Studio flows may require the choice.",
    "--private is a compatibility alias for --visibility private.",
    "--replace atomically replaces an existing destination and rolls it back on failure.",
  ].join("\n");
}

export function resolveVisibility(args) {
  if (args.visibility && !["public", "private"].includes(args.visibility)) {
    throw new Error(`invalid --visibility '${args.visibility}' (use public or private)`);
  }
  if (args.private && args.visibility === "public") throw new Error("--private conflicts with --visibility public");
  const value = args.private ? "private" : args.visibility;
  if (args.publicAlias && value !== "private") throw new Error("--public-alias is only valid with private visibility");
  if (value) return value;
  if (args.requireVisibility) throw new Error("missing visibility choice: pass --visibility public or --visibility private");
  return "public";
}

export function validateRequestedIdentity(args) {
  if (!/^[a-z][a-z0-9-]*$/.test(args.id)) throw new Error("game id must be lowercase kebab-case");
  const title = args.title || args.id;
  const storageNamespace = args.storageNamespace || args.id;
  if (title !== title.trim() || title.length < 1 || title.length > 80 || /[\x00-\x1f"\\]/.test(title)) {
    throw new Error("game title must be 1-80 trimmed characters without controls, quotes, or backslashes");
  }
  if (!/^[a-z][a-z0-9-]{0,57}$/.test(storageNamespace)) {
    throw new Error("storage namespace must be lowercase kebab-case and at most 58 characters");
  }
  return { schema: "ai_studio.game.v1", id: args.id, title, storageNamespace };
}

function writeJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function readJsonStrict(path, label) {
  let value;
  try { value = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, "")); }
  catch (error) { throw new Error(`${label} is not valid JSON: ${error.message}`); }
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value;
}

function exactKeys(value, keys, label) {
  const actual = Object.keys(value).sort();
  const expected = [...keys].sort();
  if (JSON.stringify(actual) !== JSON.stringify(expected)) throw new Error(`${label} has unexpected fields`);
}

function readTemplateIdentity(templateDir) {
  const value = readJsonStrict(join(templateDir, "template.json"), "template identity");
  exactKeys(value, ["schema", "id", "title", "storageNamespace"], "template identity");
  if (value.schema !== "ai_studio.template.v1" || !/^[a-z][a-z0-9-]*$/.test(value.id)
      || !String(value.title || "").trim() || !/^[a-z][a-z0-9-]*$/.test(value.storageNamespace)) {
    throw new Error("template identity is invalid");
  }
  return value;
}

function readTemplateDependencySeed(templateDir) {
  const value = readJsonStrict(join(templateDir, "game-dependencies.json"), "template dependency seed");
  exactKeys(value, ["schema", "engine", "features", "compatibility"], "template dependency seed");
  if (value.schema !== "ai_studio.game.dependencies.seed.v2") throw new Error("template dependency seed has an unsupported schema");
  exactKeys(value.engine, ["source", "version", "compatibility"], "template dependency seed engine");
  if (value.engine.source !== "external/neotolis-engine" || !EXACT_SEMVER.test(String(value.engine.version || ""))
      || !String(value.engine.compatibility || "").trim()) {
    throw new Error("template dependency seed engine is invalid");
  }
  if (!Array.isArray(value.features) || !String(value.compatibility || "").trim()) throw new Error("template dependency seed is invalid");
  const ids = new Set();
  for (const feature of value.features) {
    exactKeys(feature, ["id", "source", "version", "compatibility"], "template dependency seed feature");
    if (!/^[a-z][a-z0-9-]*$/.test(feature.id) || feature.source !== `features/${feature.id}`
        || !EXACT_SEMVER.test(String(feature.version || ""))
        || !String(feature.compatibility || "").trim() || ids.has(feature.id)) {
      throw new Error("template dependency seed feature is invalid or duplicated");
    }
    ids.add(feature.id);
  }
  return value;
}

function runGit(root, args) {
  return spawnSync("git", ["-C", root, ...args], { encoding: "utf8", shell: false });
}

function requireGitRevision(root, args, label) {
  const result = runGit(root, args);
  const revision = result.status === 0 ? result.stdout.trim() : "";
  if (!/^[0-9a-f]{40,64}$/i.test(revision)) throw new Error(`${label} requires an exact Git revision`);
  return revision;
}

function tryGitRevisions(root, revisions) {
  const result = runGit(root, ["rev-parse", ...revisions]);
  const values = result.status === 0 ? result.stdout.trim().split(/\r?\n/) : [];
  return values.length === revisions.length && values.every((value) => /^[0-9a-f]{40,64}$/i.test(value))
    ? values
    : null;
}

function requireClean(root, path, label) {
  const result = runGit(root, ["status", "--porcelain", ...(path ? ["--", path] : [])]);
  if (result.status !== 0 || result.stdout.trim()) throw new Error(`${label} must be clean before recording exact dependency revisions`);
}

function pathsAreClean(root, paths) {
  const result = runGit(root, ["status", "--porcelain", "--", ...paths]);
  return result.status === 0 && !result.stdout.trim();
}

function readEngineSemVer(repoRoot) {
  const rel = "external/neotolis-engine/engine/core/nt_core.h";
  const header = readFileSync(join(repoRoot, rel), "utf8");
  const component = (name) => {
    const match = new RegExp(`^#define\\s+NT_VERSION_${name}\\s+(0|[1-9]\\d*)\\s*$`, "m").exec(header);
    if (!match) throw new Error(`engine version metadata is missing NT_VERSION_${name}`);
    return match[1];
  };
  return `${component("MAJOR")}.${component("MINOR")}.${component("PATCH")}`;
}

function ensureGameStudioScaffold(gameDir, identity, visibility) {
  const studioDir = join(gameDir, ".ai_studio");
  if (visibility === "private") {
    for (const rel of ["taskboard/items", "canvas/projects", "evidence"]) mkdirSync(join(studioDir, rel), { recursive: true });
  } else {
    rmSync(join(studioDir, "taskboard"), { recursive: true, force: true });
    rmSync(join(studioDir, "canvas"), { recursive: true, force: true });
    mkdirSync(join(studioDir, "evidence"), { recursive: true });
  }
  const stores = visibility === "private"
    ? { taskboard: ".ai_studio/taskboard/items", canvas: ".ai_studio/canvas/projects", evidence: ".ai_studio/evidence", assets: "assets" }
    : { evidence: ".ai_studio/evidence", assets: "assets" };
  writeJson(join(studioDir, "workspace.json"), {
    schema: "ai_studio.game.workspace.v1", gameId: identity.id, visibility,
    stores,
  });
  const docs = [
    ["taskboard/items/README.md", "Game-local taskboard items live here for private or game-owned work.\n"],
    ["canvas/projects/README.md", "Game-local canvas projects live here when they must stay with this game.\n"],
    ["evidence/README.md", "Game-local validation evidence, screenshots, and reports live here.\n"],
  ];
  for (const [rel, text] of docs) {
    if (visibility === "private" || rel.startsWith("evidence/")) writeFileSync(join(studioDir, rel), text, "utf8");
  }
}

function transformIdentityOwnedFiles(gameDir, template, identity) {
  // Exact producer-owned substitutions only. This is deliberately not a tree-wide name replacement.
  const table = [
    ["cmake/GamePlatform.cmake", `GAME_STORAGE_APP_ID="${template.storageNamespace}"`, `GAME_STORAGE_APP_ID="${identity.storageNamespace}"`, 1],
    ...["storage", "save", "analytics", "composition"].map((suffix) => [
      "cmake/GameTests.cmake",
      `GAME_STORAGE_APP_ID="${template.storageNamespace}_${suffix}_test"`,
      `GAME_STORAGE_APP_ID="${identity.storageNamespace}-${suffix}-test"`,
      1,
    ]),
    ["src/game_save.c", `GAME_STORAGE_APP_ID "${template.storageNamespace}"`, `GAME_STORAGE_APP_ID "${identity.storageNamespace}"`, 1],
    ["tests/web_persistence_check.py", `STORAGE_KEY = "${template.storageNamespace}/save/autosave"`, `STORAGE_KEY = "${identity.storageNamespace}/save/autosave"`, 1],
    ["cmake/GameOptions.cmake", `set(GAME_TITLE "${template.title}" CACHE STRING "Game window title base")`, `set(GAME_TITLE "${identity.title}" CACHE STRING "Game window title base")`, 1],
    ["src/main.c", `config.app_name = "${template.title}"`, `config.app_name = "${identity.title}"`, 1],
    ["src/main.c", `GAME_WINDOW_TITLE "${template.title}"`, `GAME_WINDOW_TITLE "${identity.title}"`, 1],
  ];
  for (const [rel, before, after, required] of table) {
    const path = join(gameDir, rel);
    if (!existsSync(path)) throw new Error(`identity-owned template producer is missing: ${rel}`);
    const source = readFileSync(path, "utf8");
    const count = source.split(before).length - 1;
    if (count !== required) throw new Error(`identity-owned template token drifted in ${rel}: expected ${required}, found ${count}`);
    const transformed = source.split(before).join(after);
    if (before !== after && transformed.includes(before)) throw new Error(`template identity survived transformation in ${rel}`);
    writeFileSync(path, transformed, "utf8");
  }
}

function shellPath(path) {
  return String(path).replace(/\\/g, "/").replace(/"/g, '\\"');
}

const PARENT_HOOK_MARKER = "ai-studio-private-preflight-v1";

function parentPrecommitPaths(repoRoot) {
  const result = runGit(repoRoot, ["rev-parse", "--git-path", "hooks/pre-commit"]);
  if (result.status !== 0 || !result.stdout.trim()) throw new Error("failed to locate parent Git pre-commit hook");
  const hook = resolve(repoRoot, result.stdout.trim());
  return { hook, backup: `${hook}.ai-studio-original` };
}

function installParentPrecommit(repoRoot) {
  const paths = parentPrecommitPaths(repoRoot);
  const snapshots = { hook: snapshotFile(paths.hook), backup: snapshotFile(paths.backup) };
  try {
    if (snapshots.hook.existed && !readFileSync(paths.hook, "utf8").includes(PARENT_HOOK_MARKER)) {
      if (snapshots.backup.existed) throw new Error(`cannot preserve parent pre-commit: backup already exists at ${paths.backup}`);
      renameSync(paths.hook, paths.backup);
    }
    mkdirSync(dirname(paths.hook), { recursive: true });
    const original = existsSync(paths.backup)
      ? 'if [ -x "$0.ai-studio-original" ]; then "$0.ai-studio-original" "$@" || exit $?; fi\n'
      : "";
    const node = shellPath(process.execPath);
    const preflight = shellPath(preflightScript);
    const root = shellPath(repoRoot);
    writeFileSync(paths.hook, `#!/bin/sh\n# ${PARENT_HOOK_MARKER}\n${original}exec "${node}" "${preflight}" preflight --root "${root}"\n`, "utf8");
    chmodSync(paths.hook, 0o755);
    return snapshots;
  } catch (error) {
    restoreFile({ ...snapshots.hook, path: paths.hook });
    restoreFile({ ...snapshots.backup, path: paths.backup });
    throw error;
  }
}

function rollbackParentPrecommit(repoRoot, snapshots) {
  if (!snapshots) return;
  const paths = parentPrecommitPaths(repoRoot);
  restoreFile({ ...snapshots.hook, path: paths.hook });
  restoreFile({ ...snapshots.backup, path: paths.backup });
}

function ensureNestedGit(gameDir, gameId) {
  const init = runGit(gameDir, ["init"]);
  if (init.status !== 0) throw new Error(`failed to initialize private game git repository: ${init.stderr}`);
  const proof = runGit(gameDir, ["rev-parse", "--is-inside-work-tree", "--show-prefix"]);
  if (proof.status !== 0 || proof.stdout.trim() !== "true") throw new Error(`games/private/${gameId} does not contain a valid nested git repository`);
}

function resetItemsLock(gameDir) {
  const path = join(gameDir, "content", "items.lock.json");
  if (!existsSync(path)) return false;
  const templateReceipt = readJsonStrict(path, "template items lock");
  if (templateReceipt.schema_version !== 4 || templateReceipt.receipt?.schema !== "items.release_receipt.v2") {
    throw new Error("template items lock must be an items.release_receipt.v2 schema_version 4 document");
  }
  writeJson(path, {
    ...templateReceipt,
    comment: "Baseline of def_id shipped to THIS game's players -- starts empty (copy-then-own reset by games/new_game.mjs).",
    def_ids: {}, removed: {},
  });
  return true;
}

function validatePreparedGame(gameDir, expected, dependencies, visibility) {
  if (!existsSync(join(gameDir, "CMakeLists.txt"))) throw new Error("prepared game has no CMakeLists.txt");
  const identity = readJsonStrict(join(gameDir, "game.json"), "generated game identity");
  exactKeys(identity, Object.keys(expected), "generated game identity");
  if (JSON.stringify(identity) !== JSON.stringify(expected)) throw new Error("generated game identity failed strict reread");
  const rereadDependencies = readJsonStrict(join(gameDir, "dependencies.json"), "generated dependencies");
  if (JSON.stringify(rereadDependencies) !== JSON.stringify(dependencies)) throw new Error("generated dependencies failed strict reread");
  const workspace = readJsonStrict(join(gameDir, ".ai_studio", "workspace.json"), "generated workspace scaffold");
  if (workspace.gameId !== identity.id || workspace.visibility !== visibility) throw new Error("generated workspace identity mismatch");
  if (existsSync(join(gameDir, "template.json")) || existsSync(join(gameDir, "game-dependencies.json"))) throw new Error("template identity seeds survived generation");
  if (existsSync(join(gameDir, "content", "items.lock.json"))) readJsonStrict(join(gameDir, "content", "items.lock.json"), "items lock");
  return identity;
}

function registeredGameIdentities(repoRoot) {
  return listGameMounts(repoRoot, { includePrivate: true, skipPreflight: true, warnings: [] });
}

function snapshotFile(path) {
  return {
    path,
    existed: existsSync(path),
    bytes: existsSync(path) ? readFileSync(path) : null,
    mode: existsSync(path) ? statSync(path).mode : null,
  };
}

function restoreFile(snapshot) {
  if (snapshot.existed) {
    mkdirSync(dirname(snapshot.path), { recursive: true });
    writeFileSync(snapshot.path, snapshot.bytes);
    if (snapshot.mode !== null) chmodSync(snapshot.path, snapshot.mode);
  } else rmSync(snapshot.path, { force: true });
}

function snapshotExternal(repoRoot) {
  return [
    snapshotFile(join(repoRoot, ".vscode", "tasks.json")),
    snapshotFile(join(repoRoot, ".vscode", "launch.json")),
  ];
}

function capturePostWrites(snapshots) {
  return snapshots.map((before) => ({ before, after: snapshotFile(before.path) }));
}

function sameSnapshot(current, expected) {
  return current.existed === expected.existed
    && (!current.existed || Buffer.compare(current.bytes, expected.bytes) === 0);
}

function restoreUnchangedWrites(writes) {
  for (const write of writes) {
    if (sameSnapshot(snapshotFile(write.after.path), write.after)) restoreFile(write.before);
  }
}

function rollbackExternalWrites(writes) {
  restoreUnchangedWrites(writes);
}

function rollbackTaskboardMutation(mutation) {
  if (!mutation) return;
  const current = snapshotFile(mutation.post.path);
  if (!sameSnapshot(current, mutation.post)) return;
  if (mutation.created) rmSync(mutation.post.path, { force: true });
  else {
    if (resolve(mutation.post.path) !== resolve(mutation.before.path)) rmSync(mutation.post.path, { force: true });
    restoreFile(mutation.before);
  }
}

function cleanupCommittedBackup(backupDir) {
  rmSync(backupDir, { recursive: true, force: true });
}

function maybeFail(point) {
  const requested = process.env.AI_STUDIO_NEW_GAME_TEST_FAIL_AT || "";
  if (!requested) return;
  if (process.env.NODE_ENV !== "test") throw new Error("new_game failure injection is test-only (NODE_ENV=test required)");
  if (requested === point) throw new Error(`injected test failure at ${point}`);
}

function fail(message, showUsage = false) {
  const error = new Error(message);
  error.showUsage = showUsage;
  throw error;
}

function executeMain(argv, io) {
let args;
try { args = parseArgs(argv); }
catch (error) { fail(error.message, true); }
if (args.help) { io.log(usageText()); return 0; }

let requestedIdentity;
let visibility;
try {
  requestedIdentity = validateRequestedIdentity(args);
  visibility = resolveVisibility(args);
  if (visibility === "private" && args.publicAlias) requestedIdentity.aliases = [args.publicAlias];
} catch (error) { fail(error.message, true); }

const repoRoot = args.root ? resolve(args.root) : defaultRepoRoot;
const isPrivate = visibility === "private";
let fromRel = args.from;
try {
  if (args.template) {
    const template = listRegisteredTemplates(repoRoot).find((item) => item.id === args.template && item.status !== "disabled");
    if (!template) throw new Error(`template '${args.template}' is not registered or is disabled`);
    fromRel = template.folder;
  }
} catch (error) { fail(error.message); }

const fromDir = join(repoRoot, fromRel);
const gameRel = visibility === "private" ? `games/private/${args.id}` : `games/${args.id}`;
const gamesDir = join(repoRoot, visibility === "private" ? "games/private" : "games");
const finalDir = join(repoRoot, gameRel);
mkdirSync(gamesDir, { recursive: true });
let templateIdentity;
let dependencySeed;
let repoRevision;
let engineRevision;
try {
  if (!existsSync(join(fromDir, "CMakeLists.txt"))) throw new Error(`template not found at ${fromDir}`);
  templateIdentity = readTemplateIdentity(fromDir);
  dependencySeed = readTemplateDependencySeed(fromDir);
  const rootRevisions = tryGitRevisions(repoRoot, ["HEAD", "HEAD:external/neotolis-engine"]);
  repoRevision = rootRevisions?.[0]
    || requireGitRevision(repoRoot, ["rev-parse", "HEAD"], "game dependency record");
  const engineGitlink = rootRevisions?.[1]
    || requireGitRevision(repoRoot, ["rev-parse", "HEAD:external/neotolis-engine"], "parent engine gitlink");
  engineRevision = requireGitRevision(join(repoRoot, "external", "neotolis-engine"), ["rev-parse", "HEAD"], "engine dependency record");
  if (engineRevision !== engineGitlink) throw new Error("engine checkout HEAD must match the parent engine gitlink");
  const engineVersion = readEngineSemVer(repoRoot);
  if (dependencySeed.engine.version !== engineVersion) {
    throw new Error(`template engine version ${dependencySeed.engine.version} does not match nt_core.h ${engineVersion}`);
  }
  const rootPathsClean = pathsAreClean(repoRoot, [fromRel, ...dependencySeed.features.map((feature) => feature.source)]);
  if (!rootPathsClean) requireClean(repoRoot, fromRel, `template ${fromRel}`);
  requireClean(join(repoRoot, "external", "neotolis-engine"), "", "external/neotolis-engine");
  for (const feature of dependencySeed.features) {
    if (!existsSync(join(repoRoot, feature.source))) throw new Error(`template references missing shared feature ${feature.source}`);
    const manifest = readJsonStrict(join(repoRoot, feature.source, "feature.json"), `${feature.id} feature metadata`);
    if (manifest.schema !== "ai_studio.feature.v1" || manifest.id !== feature.id
        || !EXACT_SEMVER.test(String(manifest.version || ""))) {
      throw new Error(`${feature.id} feature metadata is invalid`);
    }
    if (manifest.version !== feature.version) {
      throw new Error(`${feature.id} dependency seed version ${feature.version} does not match feature.json ${manifest.version}`);
    }
    if (!rootPathsClean) requireClean(repoRoot, feature.source, `feature ${feature.source}`);
  }
  if (existsSync(finalDir) && !args.replace) throw new Error(`${finalDir} already exists (use --replace)`);
  const identities = registeredGameIdentities(repoRoot);
  const namespaceOwner = identities.find((game) => game.id !== args.id && game.storageNamespace === requestedIdentity.storageNamespace);
  if (namespaceOwner) throw new Error(`storage namespace '${requestedIdentity.storageNamespace}' is already owned by game '${namespaceOwner.id}'`);
  const conflictingRoot = identities.find((game) => game.id === args.id && game.root !== gameRel);
  if (conflictingRoot) {
    throw new Error(`${visibility} game id '${args.id}' is already owned by ${conflictingRoot.root}`);
  }
} catch (error) { fail(error.message); }

const nonce = `${process.pid}-${randomUUID()}`;
const stagingDir = join(gamesDir, `.${args.id}.new-${nonce}`);
const backupDir = join(gamesDir, `.${args.id}.backup-${nonce}`);
const dependencies = {
  schema: "ai_studio.game.dependencies.v2",
  engine: {
    source: dependencySeed.engine.source, version: dependencySeed.engine.version,
    revision: engineRevision, compatibility: dependencySeed.engine.compatibility,
  },
  features: dependencySeed.features.map((feature) => ({
    id: feature.id, source: feature.source, version: feature.version,
    revision: repoRevision, compatibility: feature.compatibility,
  })),
  compatibility: `${dependencySeed.compatibility}; created from ${fromRel} at exact Studio revision ${repoRevision}`,
};
let itemsLockReset = false;
try {
  copyGitSourceTree(repoRoot, fromDir, stagingDir);
  rmSync(join(stagingDir, "template.json"), { force: true });
  rmSync(join(stagingDir, "game-dependencies.json"), { force: true });
  writeJson(join(stagingDir, "game.json"), requestedIdentity);
  writeJson(join(stagingDir, "dependencies.json"), dependencies);
  ensureGameStudioScaffold(stagingDir, requestedIdentity, visibility);
  itemsLockReset = resetItemsLock(stagingDir);
  transformIdentityOwnedFiles(stagingDir, templateIdentity, requestedIdentity);
  if (isPrivate) ensureNestedGit(stagingDir, args.id);
} catch (error) {
  rmSync(stagingDir, { recursive: true, force: true });
  fail(error.message);
}

let identity;
try { identity = validatePreparedGame(stagingDir, requestedIdentity, dependencies, visibility); }
catch (error) {
  rmSync(stagingDir, { recursive: true, force: true });
  fail(error.message);
}

const external = snapshotExternal(repoRoot);
let externalWrites = [];
let taskboardMutation = null;
let parentHookMutation = null;
let backupMade = false;
let published = false;
const registrationMessages = [];
try {
  if (existsSync(finalDir)) {
    renameSync(finalDir, backupDir);
    backupMade = true;
  }
  renameSync(stagingDir, finalDir);
  published = true;

  if (isPrivate) {
    const mount = listGameMounts(repoRoot, { activeGameId: identity.id, skipPreflight: true, warnings: [] })
      .find((entry) => entry.root === gameRel);
    if (!mount) throw new Error(`private game scanner did not discover ${gameRel}`);
    const preflight = runPrivateGamePreflight(repoRoot, { mounts: [mount] });
    if (!preflight.ok) throw new Error(`private game preflight failed:\n${preflight.violations.map((item) => `- ${item.path}: ${item.reason}`).join("\n")}`);
    parentHookMutation = installParentPrecommit(repoRoot);
    externalWrites = capturePostWrites(external);
    maybeFail("private-preflight");
  } else {
    const mount = listGameMounts(repoRoot, { warnings: [] }).find((entry) => entry.root === gameRel);
    if (!mount) throw new Error(`public game scanner did not discover ${gameRel}`);
    const registered = { assets: mount.assetRoot };
    const vscode = writeVscodeProjectFiles(repoRoot);
    externalWrites = capturePostWrites(external);
    maybeFail("public-registration");
    const taskboard = ensureProject(repoRoot, {
      title: identity.title, kind: "game", target: `games/${identity.id}`, tags: ["game"],
      body: `## Goal\n\nTrack playable game work for \`games/${identity.id}\`.\n\n## In scope\n\n- Game setup, first playable work, assets, validation, and release tasks.\n\n## Out of scope\n\n- Reusable template work and unrelated AI Studio infrastructure.\n\n## Log\n`,
    });
    if (taskboard.created) {
      taskboardMutation = { created: true, post: snapshotFile(taskboard.project.file) };
    } else if (taskboard.project.fields.title !== identity.title) {
      const projectBefore = snapshotFile(taskboard.project.file);
      taskboard.project = updateDoc(repoRoot, taskboard.project.fields.id, {
        rev: taskboard.project.rev,
        fields: { title: identity.title },
      });
      taskboardMutation = { created: false, before: projectBefore, post: snapshotFile(taskboard.project.file) };
    }
    maybeFail("after-taskboard");
    registrationMessages.push(
      `discovered assets: ${registered.assets}`,
      `${taskboard.created ? "created" : "existing"} taskboard project: ${taskboard.project.fields.id}`,
      `updated VS Code tasks/launch for ${vscode.projects.length} playable project(s)`,
    );
  }
} catch (error) {
  const residuals = [];
  const rollbackPhase = (label, operation) => {
    try { operation(); }
    catch (rollbackError) { residuals.push(`${label}: ${rollbackError.message}`); }
  };
  rollbackPhase("taskboard", () => rollbackTaskboardMutation(taskboardMutation));
  rollbackPhase("parent pre-commit", () => rollbackParentPrecommit(repoRoot, parentHookMutation));
  rollbackPhase("published destination", () => {
    if (published && existsSync(finalDir)) rmSync(finalDir, { recursive: true, force: true });
  });
  rollbackPhase("backup restore", () => {
    if (backupMade && existsSync(backupDir)) renameSync(backupDir, finalDir);
  });
  rollbackPhase("external", () => {
    if (!externalWrites.length) externalWrites = capturePostWrites(external);
    rollbackExternalWrites(externalWrites);
  });
  rollbackPhase("staging", () => {
    if (existsSync(stagingDir)) rmSync(stagingDir, { recursive: true, force: true });
  });
  for (const residual of residuals) io.error(`error: rollback residual ${residual}`);
  fail(error.message);
}

// Registration and Taskboard writes are committed. Backup cleanup must never cross back into rollback.
if (backupMade && existsSync(backupDir)) {
  try { cleanupCommittedBackup(backupDir); }
  catch (cleanupError) {
    io.error(`warning: new game committed but backup cleanup pending at ${backupDir}: ${cleanupError.message}`);
  }
}
for (const message of registrationMessages) io.log(message);

if (isPrivate) {
  io.log(`new private game '${identity.id}' created from ${fromRel}/ -> ${gameRel}/`);
  io.log(`nested git repository: created`);
  io.log(`parent pre-commit privacy preflight: installed`);
  io.log("public Studio registries, Taskboard, Canvas, and VS Code files were not updated");
} else {
  io.log(`new game '${identity.id}' created from ${fromRel}/ -> ${gameRel}/`);
}
if (itemsLockReset) io.log(`reset items lock baseline: ${gameRel}/content/items.lock.json`);
return 0;
}

export function main(argv, io = console) {
  try { return executeMain(argv, io); }
  catch (error) {
    io.error(`error: ${error.message}`);
    if (error.showUsage) io.error(usageText());
    return 1;
  }
}

const isDirectInvocation = Boolean(process.argv[1])
  && resolve(process.argv[1]) === fileURLToPath(import.meta.url);

if (isDirectInvocation) process.exitCode = main(process.argv.slice(2));
