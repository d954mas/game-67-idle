#!/usr/bin/env node
// Transactionally copy a registered template into games/<game-id>.
import {
  appendFileSync, copyFileSync, existsSync, mkdirSync, readdirSync, readFileSync,
  renameSync, rmSync, statSync, writeFileSync,
} from "node:fs";
import { randomUUID } from "node:crypto";
import { dirname, isAbsolute, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { listRegisteredGames, listRegisteredTemplates } from "../ai_studio/assets/sources/ops.mjs";
import { writeVscodeProjectFiles } from "../ai_studio/dev_environment/vscode_projects.mjs";
import { ensureProject, updateDoc } from "../ai_studio/taskboard/lib.mjs";
import {
  localWorkspaceCatalogRelPath, runPrivateGamePreflight, upsertLocalWorkspaceGameMount,
} from "../ai_studio/workspace/games.mjs";
import { catalogRelPath, upsertWorkspaceMount } from "../ai_studio/workspace/catalog.mjs";
import { copyGitSourceTree } from "../ai_studio/workspace/copy_source_tree.mjs";

const defaultRepoRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
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

function sleepSync(milliseconds) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, milliseconds);
}

function pidIsAlive(pid) {
  if (!Number.isSafeInteger(pid) || pid <= 0) return true;
  try { process.kill(pid, 0); return true; }
  catch (error) { return error.code !== "ESRCH"; }
}

function cleanupAbandonedClaimCandidates(gamesDir, lockDir) {
  const exact = /^\.new-game\.claim-(\d+)-([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})\.candidate$/i;
  const released = /^\.new-game\.claim\.release-(\d+)-([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})$/i;
  const now = Date.now();
  for (const name of readdirSync(gamesDir)) {
    const path = join(gamesDir, name);
    if (released.test(name)) {
      try {
        if (now - statSync(path).mtimeMs >= 5 * 60 * 1000) {
          rmSync(path, { recursive: true, force: true, maxRetries: 6, retryDelay: 20 });
        }
      } catch { /* Quarantined release cleanup is best-effort. */ }
      continue;
    }
    const match = exact.exec(name);
    if (!match) continue;
    if (!statSync(path).isDirectory()) continue;
    let provenDead = false;
    let provenLive = false;
    try {
      const owner = readJsonStrict(join(path, "owner.json"), "new-game candidate owner");
      const expectedToken = `${match[1]}-${match[2]}`;
      if (owner.token === expectedToken && owner.pid === Number(match[1])) {
        provenLive = pidIsAlive(owner.pid);
        provenDead = !provenLive;
      }
    } catch { /* Fresh incomplete candidates are owned until age proves abandonment. */ }
    const oldUnpublished = !provenLive && !existsSync(lockDir) && now - statSync(path).mtimeMs >= 5 * 60 * 1000;
    if (provenDead || oldUnpublished) rmSync(path, { recursive: true, force: true });
  }
}

function acquireNewGameClaim(gamesDir) {
  mkdirSync(gamesDir, { recursive: true });
  const lockDir = join(gamesDir, ".new-game.claim");
  cleanupAbandonedClaimCandidates(gamesDir, lockDir);
  const ownerPath = join(lockDir, "owner.json");
  const token = `${process.pid}-${randomUUID()}`;
  const candidateDir = join(gamesDir, `.new-game.claim-${token}.candidate`);
  mkdirSync(candidateDir);
  writeJson(join(candidateDir, "owner.json"), { token, pid: process.pid });
  const deadline = Date.now() + 15000;
  while (Date.now() < deadline) {
    try {
      renameSync(candidateDir, lockDir);
      let released = false;
      const release = () => {
        if (released) return;
        released = true;
        process.removeListener("exit", release);
        try {
          const owner = readJsonStrict(ownerPath, "new-game claim owner");
          if (owner.token !== token || owner.pid !== process.pid) return;
          const releaseDir = join(gamesDir, `.new-game.claim.release-${token}`);
          renameSync(lockDir, releaseDir);
          try { rmSync(releaseDir, { recursive: true, force: true, maxRetries: 6, retryDelay: 20 }); }
          catch { /* The live claim is free; token-unique cleanup is best-effort. */ }
        } catch { /* Never remove a lock whose ownership cannot be proven. */ }
      };
      process.once("exit", release);
      return release;
    } catch (error) {
      if (!existsSync(lockDir)) {
        if (["EEXIST", "EPERM", "ENOTEMPTY"].includes(error.code)) {
          sleepSync(25);
          continue;
        }
        rmSync(candidateDir, { recursive: true, force: true });
        throw error;
      }
      try {
        const first = readJsonStrict(ownerPath, "new-game claim owner");
        if (!pidIsAlive(first.pid)) {
          const second = readJsonStrict(ownerPath, "new-game claim owner");
          if (first.token === second.token && first.pid === second.pid && !pidIsAlive(second.pid)) {
            rmSync(lockDir, { recursive: true, force: true });
            continue;
          }
        }
      } catch { /* Owner may still be publishing its token; bounded wait is safer than stealing. */ }
      sleepSync(25);
    }
  }
  rmSync(candidateDir, { recursive: true, force: true });
  throw new Error("timed out waiting for the global new-game claim");
}

function waitAtClaimBarrier() {
  const barrier = process.env.AI_STUDIO_NEW_GAME_TEST_LOCK_BARRIER || "";
  if (!barrier) return;
  if (process.env.NODE_ENV !== "test") throw new Error("new_game lock barrier is test-only (NODE_ENV=test required)");
  writeFileSync(`${barrier}.ready`, `${process.pid}\n`, "utf8");
  const deadline = Date.now() + 10000;
  while (!existsSync(`${barrier}.release`)) {
    if (Date.now() >= deadline) throw new Error("timed out waiting at the new-game test lock barrier");
    sleepSync(20);
  }
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
  for (const rel of ["taskboard/items", "canvas/projects", "evidence"]) mkdirSync(join(studioDir, rel), { recursive: true });
  writeJson(join(studioDir, "workspace.json"), {
    schema: "ai_studio.game.workspace.v1", gameId: identity.id, visibility,
    stores: { taskboard: ".ai_studio/taskboard/items", canvas: ".ai_studio/canvas/projects", evidence: ".ai_studio/evidence", assets: "assets" },
  });
  for (const [rel, text] of [
    ["taskboard/items/README.md", "Game-local taskboard items live here for private or game-owned work.\n"],
    ["canvas/projects/README.md", "Game-local canvas projects live here when they must stay with this game.\n"],
    ["evidence/README.md", "Game-local validation evidence, screenshots, and reports live here.\n"],
  ]) writeFileSync(join(studioDir, rel), text, "utf8");
}

function transformIdentityOwnedFiles(gameDir, template, identity) {
  // Exact producer-owned substitutions only. This is deliberately not a tree-wide name replacement.
  const table = [
    ["CMakeLists.txt", `GAME_STORAGE_APP_ID="${template.storageNamespace}"`, `GAME_STORAGE_APP_ID="${identity.storageNamespace}"`, 1],
    ["cmake/GamePlatform.cmake", `GAME_STORAGE_APP_ID="${template.storageNamespace}"`, `GAME_STORAGE_APP_ID="${identity.storageNamespace}"`, 1],
    ["cmake/GameTests.cmake", `GAME_STORAGE_APP_ID="${template.storageNamespace}_test"`, `GAME_STORAGE_APP_ID="${identity.storageNamespace}-test"`, 4],
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

function ensureNestedGit(gameDir, gameId) {
  const init = runGit(gameDir, ["init"]);
  if (init.status !== 0) throw new Error(`failed to initialize private game git repository: ${init.stderr}`);
  const proof = runGit(gameDir, ["rev-parse", "--is-inside-work-tree", "--show-prefix"]);
  if (proof.status !== 0 || proof.stdout.trim() !== "true") throw new Error(`games/${gameId} does not contain a valid nested git repository`);
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
  exactKeys(identity, ["schema", "id", "title", "storageNamespace"], "generated game identity");
  if (JSON.stringify(identity) !== JSON.stringify(expected)) throw new Error("generated game identity failed strict reread");
  const rereadDependencies = readJsonStrict(join(gameDir, "dependencies.json"), "generated dependencies");
  if (JSON.stringify(rereadDependencies) !== JSON.stringify(dependencies)) throw new Error("generated dependencies failed strict reread");
  const workspace = readJsonStrict(join(gameDir, ".ai_studio", "workspace.json"), "generated workspace scaffold");
  if (workspace.gameId !== identity.id || workspace.visibility !== visibility) throw new Error("generated workspace identity mismatch");
  if (existsSync(join(gameDir, "template.json")) || existsSync(join(gameDir, "game-dependencies.json"))) throw new Error("template identity seeds survived generation");
  if (existsSync(join(gameDir, "content", "items.lock.json"))) readJsonStrict(join(gameDir, "content", "items.lock.json"), "items lock");
  return identity;
}

function requireParentGitExcludePath(repoRoot) {
  const result = runGit(repoRoot, ["rev-parse", "--git-path", "info/exclude"]);
  if (result.status !== 0) throw new Error("private game creation requires the parent Studio checkout to be a Git repository");
  const text = result.stdout.trim();
  return isAbsolute(text) ? text : resolve(repoRoot, text);
}

function ensureParentExclude(excludePath, relRoot, directory = false) {
  mkdirSync(dirname(excludePath), { recursive: true });
  const normalized = relRoot.replace(/\\/g, "/").replace(/\/+$/, "");
  const line = directory ? `${normalized}/` : normalized;
  const existing = existsSync(excludePath) ? readFileSync(excludePath, "utf8") : "";
  if (!existing.split(/\r?\n/).includes(line)) appendFileSync(excludePath, `${existing && !existing.endsWith("\n") ? "\n" : ""}${line}\n`, "utf8");
}

function parentTracksPath(repoRoot, relRoot) {
  const result = runGit(repoRoot, ["ls-files", "--", relRoot.replace(/\\/g, "/")]);
  if (result.status !== 0) throw new Error(`failed to inspect parent git tracking: ${result.stderr}`);
  return Boolean(result.stdout.trim());
}

function readCatalogMounts(path) {
  if (!existsSync(path)) return [];
  const catalog = readJsonStrict(path, `workspace catalog ${path}`);
  return Array.isArray(catalog.mounts) ? catalog.mounts : [];
}

function registeredGameIdentities(repoRoot) {
  const mounts = [
    ...readCatalogMounts(join(repoRoot, catalogRelPath(false))),
    ...readCatalogMounts(join(repoRoot, localWorkspaceCatalogRelPath())),
  ];
  const identities = [];
  for (const mount of mounts) {
    if (mount.kind !== "game" || typeof mount.root !== "string") continue;
    const identityPath = join(repoRoot, mount.root, "game.json");
    if (!existsSync(identityPath)) continue;
    const identity = readJsonStrict(identityPath, `registered game identity ${mount.root}`);
    if (typeof identity.id === "string" && typeof identity.storageNamespace === "string") identities.push({ ...identity, root: mount.root });
  }
  return identities;
}

function snapshotFile(path) {
  return { path, existed: existsSync(path), bytes: existsSync(path) ? readFileSync(path) : null };
}

function restoreFile(snapshot) {
  if (snapshot.existed) {
    mkdirSync(dirname(snapshot.path), { recursive: true });
    writeFileSync(snapshot.path, snapshot.bytes);
  } else rmSync(snapshot.path, { force: true });
}

function snapshotExternal(repoRoot, excludePath) {
  return [
    snapshotFile(join(repoRoot, catalogRelPath(false))),
    snapshotFile(join(repoRoot, localWorkspaceCatalogRelPath())),
    snapshotFile(excludePath),
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

function restoreOwnedCatalogMount(write, ownedRoot) {
  const current = snapshotFile(write.after.path);
  if (!current.existed) return !sameSnapshot(current, write.before);
  const catalog = readJsonStrict(current.path, `workspace catalog ${current.path}`);
  const beforeCatalog = write.before.existed
    ? JSON.parse(write.before.bytes.toString("utf8").replace(/^\uFEFF/, "")) : { mounts: [] };
  const previous = (beforeCatalog.mounts || []).find((mount) => mount.root === ownedRoot);
  catalog.mounts = (catalog.mounts || []).filter((mount) => mount.root !== ownedRoot);
  if (previous) catalog.mounts.push(previous);
  const onlyOwnedCreatedCatalog = !write.before.existed
    && (catalog.mounts || []).length === 0
    && Object.keys(catalog).every((key) => key === "schema" || key === "mounts");
  if (onlyOwnedCreatedCatalog || JSON.stringify(catalog) === JSON.stringify(beforeCatalog)) {
    restoreFile(write.before);
    return false;
  }
  writeJson(current.path, catalog);
  return true;
}

function restoreOwnedExclude(write, ownedLines) {
  const current = snapshotFile(write.after.path);
  if (!current.existed) return !sameSnapshot(current, write.before);
  const beforeLines = new Set(write.before.existed ? write.before.bytes.toString("utf8").split(/\r?\n/) : []);
  const segments = current.bytes.toString("utf8").match(/[^\r\n]*(?:\r\n|\n|\r|$)/g) || [];
  const restored = segments.filter((segment) => {
    const line = segment.replace(/(?:\r\n|\n|\r)$/, "");
    return !ownedLines.includes(line) || beforeLines.has(line);
  }).join("");
  const before = write.before.existed ? write.before.bytes.toString("utf8") : "";
  if (restored === before) {
    restoreFile(write.before);
    return false;
  }
  writeFileSync(current.path, restored, "utf8");
  return true;
}

function rollbackExternalWrites(writes, { repoRoot, identity, isPrivate, excludePath }) {
  const byPath = new Map(writes.map((write) => [resolve(write.after.path), write]));
  const publicCatalog = byPath.get(resolve(join(repoRoot, catalogRelPath(false))));
  const localCatalog = byPath.get(resolve(join(repoRoot, localWorkspaceCatalogRelPath())));
  const exclude = byPath.get(resolve(excludePath));
  const concurrentCatalogWrite = [publicCatalog, localCatalog]
    .filter(Boolean)
    .map((write) => restoreOwnedCatalogMount(write, `games/${identity.id}`))
    .some(Boolean);
  if (exclude && isPrivate) restoreOwnedExclude(exclude, [localWorkspaceCatalogRelPath(), `games/${identity.id}/`]);
  const vscodeWrites = writes.filter((write) => /[\\/]\.vscode[\\/](tasks|launch)\.json$/.test(write.after.path));
  if (concurrentCatalogWrite || vscodeWrites.some((write) => !sameSnapshot(snapshotFile(write.after.path), write.after))) {
    writeVscodeProjectFiles(repoRoot);
  } else restoreUnchangedWrites(vscodeWrites);
}

function rollbackTaskboardMutation(mutation) {
  if (!mutation) return;
  if (process.env.AI_STUDIO_NEW_GAME_TEST_FAIL_ROLLBACK_AT === "taskboard") {
    if (process.env.NODE_ENV !== "test") throw new Error("new_game rollback injection is test-only (NODE_ENV=test required)");
    throw new Error(`injected Taskboard rollback failure for ${mutation.post.path}`);
  }
  const current = snapshotFile(mutation.post.path);
  if (!sameSnapshot(current, mutation.post)) return;
  if (mutation.created) rmSync(mutation.post.path, { force: true });
  else {
    if (resolve(mutation.post.path) !== resolve(mutation.before.path)) rmSync(mutation.post.path, { force: true });
    restoreFile(mutation.before);
  }
}

function injectConcurrentSentinel(repoRoot, sourceGameId, point = "after-post-capture") {
  const requestedPoint = process.env.AI_STUDIO_NEW_GAME_TEST_CONCURRENT_SENTINEL_AT
    || (process.env.AI_STUDIO_NEW_GAME_TEST_CONCURRENT_SENTINEL === "1" ? "after-post-capture" : "");
  if (requestedPoint !== point) return;
  if (process.env.NODE_ENV !== "test") throw new Error("new_game concurrency injection is test-only (NODE_ENV=test required)");
  const sentinelDir = join(repoRoot, "games", "concurrent-sentinel");
  mkdirSync(join(sentinelDir, "assets"), { recursive: true });
  writeJson(join(sentinelDir, "game.json"), {
    schema: "ai_studio.game.v1", id: "concurrent-sentinel", title: "Concurrent Sentinel", storageNamespace: "concurrent-sentinel",
  });
  copyFileSync(join(repoRoot, "games", sourceGameId, "dependencies.json"), join(sentinelDir, "dependencies.json"));
  writeFileSync(join(sentinelDir, "CMakeLists.txt"), "cmake_minimum_required(VERSION 3.20)\n", "utf8");
  upsertWorkspaceMount(repoRoot, {
    kind: "game", root: "games/concurrent-sentinel", visibility: "public", gitRoot: "",
    commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [],
  }, { local: false });
  writeVscodeProjectFiles(repoRoot);
  appendFileSync(requireParentGitExcludePath(repoRoot), "/concurrent-sentinel/\n", "utf8");
  const projectDir = join(repoRoot, "ai_studio", "taskboard", "items", "projects");
  mkdirSync(projectDir, { recursive: true });
  writeFileSync(join(projectDir, "P777-concurrent-sentinel.md"), "concurrent sentinel\n", "utf8");
  writeJson(join(repoRoot, "ai_studio", "taskboard", "items", ".counters.json"), { project: 777, epic: 55, task: 9999 });
}

function cleanupCommittedBackup(backupDir) {
  if (process.env.AI_STUDIO_NEW_GAME_TEST_FAIL_AT === "backup-cleanup-partial") {
    if (process.env.NODE_ENV !== "test") throw new Error("new_game backup cleanup injection is test-only (NODE_ENV=test required)");
    const first = readdirSync(backupDir).sort()[0];
    if (first) rmSync(join(backupDir, first), { recursive: true, force: true });
    throw new Error("injected partial backup cleanup failure");
  }
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
let releaseClaim;
try {
let args;
try { args = parseArgs(argv); }
catch (error) { fail(error.message, true); }
if (args.help) { io.log(usageText()); return 0; }

let requestedIdentity;
let visibility;
try {
  requestedIdentity = validateRequestedIdentity(args);
  visibility = resolveVisibility(args);
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
const gamesDir = join(repoRoot, "games");
const finalDir = join(gamesDir, args.id);
try {
  releaseClaim = acquireNewGameClaim(gamesDir);
  waitAtClaimBarrier();
} catch (error) { fail(error.message); }
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
  if (isPrivate) {
    const publicGame = listRegisteredGames(repoRoot).find((game) => game.id === args.id && game.status !== "fixture");
    if (publicGame) throw new Error(`private game id '${args.id}' is already registered as a public game`);
    if (parentTracksPath(repoRoot, `games/${args.id}`)) throw new Error(`games/${args.id} is already tracked by the parent repository; cannot create it as private`);
  } else {
    const localRoot = `games/${args.id}`;
    const localOwnsId = readCatalogMounts(join(repoRoot, localWorkspaceCatalogRelPath()))
      .some((mount) => mount.kind === "game" && mount.root === localRoot);
    if (localOwnsId || existsSync(join(finalDir, ".git"))) {
      throw new Error(`public game id '${args.id}' is already owned by a private game`);
    }
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

const excludePath = requireParentGitExcludePath(repoRoot);
const external = snapshotExternal(repoRoot, excludePath);
let externalWrites = [];
let taskboardMutation = null;
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
    ensureParentExclude(excludePath, localWorkspaceCatalogRelPath());
    ensureParentExclude(excludePath, `games/${identity.id}`, true);
    const mount = upsertLocalWorkspaceGameMount(repoRoot, {
      gameId: identity.id, visibility: "private", commitPolicy: "nested-private", publicAlias: args.publicAlias,
    });
    const preflight = runPrivateGamePreflight(repoRoot, { mounts: [mount] });
    if (!preflight.ok) throw new Error(`private game preflight failed:\n${preflight.violations.map((item) => `- ${item.path}: ${item.reason}`).join("\n")}`);
    injectConcurrentSentinel(repoRoot, identity.id, "before-post-capture");
    externalWrites = capturePostWrites(external);
    maybeFail("private-preflight");
  } else {
    const mount = upsertWorkspaceMount(repoRoot, {
      kind: "game", root: `games/${identity.id}`, visibility: "public", gitRoot: "",
      commitPolicy: "parent-public", enabledStores: ["assets"], aliases: [],
    }, { local: false });
    const registered = { assets: mount.assetRoot };
    const vscode = writeVscodeProjectFiles(repoRoot);
    injectConcurrentSentinel(repoRoot, identity.id, "before-post-capture");
    externalWrites = capturePostWrites(external);
    injectConcurrentSentinel(repoRoot, identity.id);
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
    if ((process.env.AI_STUDIO_NEW_GAME_TEST_FAIL_AT || "") === "after-taskboard") injectConcurrentSentinel(repoRoot, identity.id);
    maybeFail("after-taskboard");
    registrationMessages.push(
      `registered assets: ${catalogRelPath(false)} -> ${registered.assets}`,
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
  rollbackPhase("published destination", () => {
    if (published && existsSync(finalDir)) rmSync(finalDir, { recursive: true, force: true });
  });
  rollbackPhase("backup restore", () => {
    if (backupMade && existsSync(backupDir)) renameSync(backupDir, finalDir);
  });
  rollbackPhase("external", () => {
    if (!externalWrites.length) externalWrites = capturePostWrites(external);
    rollbackExternalWrites(externalWrites, { repoRoot, identity, isPrivate, excludePath });
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
  io.log(`new private game '${identity.id}' created from ${fromRel}/ -> games/${identity.id}/`);
  io.log(`nested git repository: created`);
  io.log(`local workspace catalog: ${localWorkspaceCatalogRelPath()} -> games/${identity.id}`);
  io.log("public Studio registries, Taskboard, Canvas, and VS Code files were not updated");
} else {
  io.log(`new game '${identity.id}' created from ${fromRel}/ -> games/${identity.id}/`);
}
if (itemsLockReset) io.log(`reset items lock baseline: games/${identity.id}/content/items.lock.json`);
return 0;
} finally {
  releaseClaim?.();
}
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
