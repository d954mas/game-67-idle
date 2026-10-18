import {
  existsSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  realpathSync,
  renameSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { spawnSync } from "node:child_process";
import { basename, dirname, join, resolve } from "node:path";

export const GAME_IDENTITY_SCHEMA = "ai_studio.game.v1";
export const TEMPLATE_IDENTITY_SCHEMA = "ai_studio.template.v1";
export const GAME_DEPENDENCIES_SCHEMA = "ai_studio.game.dependencies.v3";

// "version" is the game's own release version and "webDownloadBudgetBytes" its budget
// for the bytes a player waits for before the first frame, not the whole archive;
// both are read by game packaging tools, the catalog tolerates them but does not
// interpret them.
const IDENTITY_KEYS = new Set(["schema", "id", "title", "storageNamespace", "aliases", "version", "webDownloadBudgetBytes"]);
const DEPENDENCY_KEYS = new Set(["schema", "engine", "features", "compatibility"]);
const ENGINE_KEYS = new Set(["source", "version", "revision", "compatibility"]);
const FEATURE_KEYS = new Set(["id", "source", "version", "compatibility"]);
const EXACT_SEMVER = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/;

function slash(value) {
  return String(value || "").replace(/\\/g, "/");
}

function comparable(value) {
  return slash(value).toLowerCase();
}

function assertObject(value, label) {
  if (!value || typeof value !== "object" || Array.isArray(value)) {
    throw new Error(`${label} must be an object`);
  }
}

function assertKnownKeys(value, allowed, label) {
  for (const key of Object.keys(value)) {
    if (!allowed.has(key)) throw new Error(`${label}: unknown field '${key}'`);
  }
}

function readJson(path, label) {
  let value;
  try {
    value = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
  } catch (error) {
    throw new Error(`${label}: ${error && error.message ? error.message : error}`);
  }
  assertObject(value, label);
  return value;
}

function normalizeId(value, label) {
  const text = String(value || "").trim();
  if (!/^[a-z][a-z0-9-]*$/.test(text)) throw new Error(`${label} must be lowercase kebab-case`);
  return text;
}

function normalizeAliases(value, label) {
  if (value === undefined) return [];
  if (!Array.isArray(value)) throw new Error(`${label} must be an array`);
  return value.map((item, index) => {
    const alias = String(item || "").trim();
    if (!alias || alias.length > 80 || !/^[a-z0-9][a-z0-9 ._-]*$/i.test(alias)) {
      throw new Error(`${label}[${index}] must be a safe display alias`);
    }
    return alias;
  });
}

function validateGameDependenciesValue(value, rel) {
  assertObject(value, rel);
  assertKnownKeys(value, DEPENDENCY_KEYS, rel);
  if (value.schema !== GAME_DEPENDENCIES_SCHEMA) throw new Error(`${rel}: expected schema ${GAME_DEPENDENCIES_SCHEMA}`);
  assertObject(value.engine, `${rel}.engine`);
  assertKnownKeys(value.engine, ENGINE_KEYS, `${rel}.engine`);
  for (const key of ["source", "version", "revision", "compatibility"]) {
    if (!String(value.engine[key] || "").trim()) throw new Error(`${rel}.engine.${key} must not be empty`);
  }
  const gitVersion = /^[0-9a-f]{8}$/.test(value.engine.version);
  if (!gitVersion && !EXACT_SEMVER.test(value.engine.version)) throw new Error(`${rel}.engine.version must be exact SemVer x.y.z or an eight-character Git version`);
  if (!/^[0-9a-f]{40,64}$/i.test(value.engine.revision)) throw new Error(`${rel}.engine.revision must be an exact Git revision`);
  if (gitVersion && !value.engine.revision.toLowerCase().startsWith(value.engine.version)) throw new Error(`${rel}.engine.version must match the pinned revision`);
  if (!Array.isArray(value.features)) throw new Error(`${rel}.features must be an array`);
  const featureIds = new Set();
  for (const [index, feature] of value.features.entries()) {
    assertObject(feature, `${rel}.features[${index}]`);
    assertKnownKeys(feature, FEATURE_KEYS, `${rel}.features[${index}]`);
    const id = normalizeId(feature.id, `${rel}.features[${index}].id`);
    if (featureIds.has(id)) throw new Error(`${rel}: duplicate feature '${id}'`);
    featureIds.add(id);
    for (const key of ["source", "compatibility"]) {
      if (!String(feature[key] || "").trim()) throw new Error(`${rel}.features[${index}].${key} must not be empty`);
    }
    if (!EXACT_SEMVER.test(String(feature.version || ""))) throw new Error(`${rel}.features[${index}].version must be exact SemVer x.y.z`);
  }
  if (!String(value.compatibility || "").trim()) throw new Error(`${rel}.compatibility must not be empty`);
  return value;
}

function readIdentity(root, relRoot, kind, { allowRootIdMismatch = false } = {}) {
  const rel = `${relRoot}/${kind}.json`;
  const identity = readJson(join(root, rel), rel);
  assertKnownKeys(identity, IDENTITY_KEYS, rel);
  const expectedSchema = kind === "game" ? GAME_IDENTITY_SCHEMA : TEMPLATE_IDENTITY_SCHEMA;
  if (identity.schema !== expectedSchema) throw new Error(`${rel}: expected schema ${expectedSchema}`);
  const id = normalizeId(identity.id, `${kind} id`);
  if (!allowRootIdMismatch && basename(relRoot) !== id) {
    throw new Error(`${relRoot}: root basename must match identity id '${id}'`);
  }
  const title = String(identity.title || "").trim();
  if (!title) throw new Error(`${rel}: title must not be empty`);
  return {
    id,
    title,
    storageNamespace: normalizeId(identity.storageNamespace, `${kind} storageNamespace`),
    aliases: normalizeAliases(identity.aliases, `${rel}.aliases`),
    identitySignature: JSON.stringify({
      schema: identity.schema,
      id,
      title,
      storageNamespace: normalizeId(identity.storageNamespace, `${kind} storageNamespace`),
      aliases: normalizeAliases(identity.aliases, `${rel}.aliases`),
    }),
  };
}

function enabledStores(root, relRoot, kind) {
  const candidates = [
    ["assets", "assets"],
    ["taskboard", ".ai_studio/taskboard/items"],
    ["canvas", ".ai_studio/canvas"],
    ["evidence", ".ai_studio/evidence"],
  ];
  return candidates
    .filter(([store]) => kind === "game" || store === "assets")
    .filter(([, rel]) => {
      try { return statSync(join(root, relRoot, rel)).isDirectory(); }
      catch { return false; }
    })
    .map(([store]) => store);
}

function resolveMount(root, relRoot, kind, visibility, identityOptions) {
  const absolute = join(root, relRoot);
  const linked = lstatSync(absolute).isSymbolicLink();
  if (visibility === "public" && linked) {
    throw new Error(`public ${kind} root must not be a symlink or junction: ${relRoot}`);
  }
  const identity = readIdentity(root, relRoot, kind, identityOptions);
  const { identitySignature, ...identityFields } = identity;
  if (kind === "game") {
    const rel = `${relRoot}/dependencies.json`;
    if (!existsSync(join(root, rel))) throw new Error(`${relRoot}: missing game dependencies.json`);
    validateGameDependenciesValue(readJson(join(root, rel), rel), rel);
  }
  const nestedGit = kind === "game" && visibility === "private" && existsSync(join(absolute, ".git"));
  const mount = {
    kind,
    root: relRoot,
    visibility,
    gitRoot: visibility === "public" ? "" : relRoot,
    commitPolicy: visibility === "public" ? "parent-public" : nestedGit ? "nested-private" : "local-only",
    enabledStores: enabledStores(root, relRoot, kind),
    aliases: identity.aliases,
    ...identityFields,
    storeId: `${kind}:${identity.storageNamespace}`,
    gameId: kind === "game" ? identity.id : undefined,
    templateId: kind === "template" ? identity.id : undefined,
    folder: relRoot,
    assetRoot: `${relRoot}/assets`,
    assets: `${relRoot}/assets`,
    publicAlias: identity.aliases[0] || "",
    source: "scan",
  };
  Object.defineProperties(mount, {
    auditRoots: { value: [relRoot], writable: true },
    identitySignature: { value: identitySignature },
  });
  return mount;
}

const GIT_CONTEXT_ENV = ["GIT_DIR", "GIT_INDEX_FILE", "GIT_WORK_TREE", "GIT_COMMON_DIR", "GIT_OBJECT_DIRECTORY", "GIT_PREFIX"];

function gitEnvironment() {
  const env = { ...process.env };
  for (const name of GIT_CONTEXT_ENV) delete env[name];
  return env;
}

function git(root, args, spawnGit = spawnSync) {
  const cwd = resolve(root);
  return spawnGit("git", ["-c", `safe.directory=${slash(cwd)}`, ...args], {
    cwd,
    encoding: "utf8",
    env: gitEnvironment(),
    shell: false,
  });
}

function gitError(result, fallback) {
  if (!result?.error && result?.status === 0) return "";
  const stderr = Buffer.isBuffer(result?.stderr) ? result.stderr.toString("utf8") : String(result?.stderr || "");
  return result?.error?.message || stderr.trim().split(/\r?\n/, 1)[0] || `${fallback} exited ${result?.status}`;
}

function physicalGitRoot(root, spawnGit) {
  let physical;
  try { physical = realpathSync(root); }
  catch { throw new Error("private worktree path is invalid"); }
  const result = git(physical, ["rev-parse", "--is-inside-work-tree", "--show-toplevel", "--git-common-dir"], spawnGit);
  const error = gitError(result, "git rev-parse");
  if (error) throw new Error(`private worktree Git validation failed: ${error}`);
  const lines = String(result.stdout || "").replace(/\r\n/g, "\n").trimEnd().split("\n");
  if (lines.length !== 3 || lines[0].trim() !== "true") throw new Error("private worktree is not a Git worktree root");
  const topLevel = resolve(physical, lines[1].trim());
  const commonDir = resolve(physical, lines[2].trim());
  if (slash(topLevel) !== slash(physical)) throw new Error("private worktree is not rooted at its game folder");
  if (!existsSync(commonDir)) throw new Error("private worktree Git common directory is missing");
  return { physical, commonDir };
}

function verifyPrivateWorktreeAlias(root, canonical, alias, options) {
  const aliasAbsolute = join(root, alias.root);
  const gitFile = join(aliasAbsolute, ".git");
  if (!existsSync(gitFile) || !lstatSync(gitFile).isFile()) {
    throw new Error(`${alias.root}: private identity alias must have linked Git worktree metadata`);
  }
  if (canonical.identitySignature !== alias.identitySignature) {
    throw new Error(`${alias.root}: private worktree identity differs from canonical game '${canonical.id}'`);
  }
  const canonicalGit = physicalGitRoot(join(root, canonical.root), options.spawnGit);
  const aliasGit = physicalGitRoot(aliasAbsolute, options.spawnGit);
  if (slash(canonicalGit.commonDir) !== slash(aliasGit.commonDir)) {
    throw new Error(`${alias.root}: private worktree Git common directory differs from canonical game`);
  }
  const result = git(canonicalGit.physical, ["worktree", "list", "--porcelain"], options.spawnGit);
  const error = gitError(result, "git worktree list");
  if (error) throw new Error(`${alias.root}: private worktree registration check failed: ${error}`);
  const aliasPath = slash(aliasGit.physical);
  let registered = false;
  for (const line of String(result.stdout || "").replace(/\r\n/g, "\n").split("\n")) {
    if (!line.startsWith("worktree ")) continue;
    const listed = resolve(canonicalGit.physical, line.slice("worktree ".length));
    if (slash(listed) !== aliasPath) continue;
    try { registered = slash(realpathSync(listed)) === aliasPath; }
    catch { registered = false; }
    if (registered) break;
  }
  if (!registered) throw new Error(`${alias.root}: private identity alias is not a registered Git worktree`);
}

function warn(options, message) {
  if (Array.isArray(options.warnings)) options.warnings.push(message);
  else console.warn(`warning: ${message}`);
}

function scanChildren(root, relParent, kind, visibility, options, unusable) {
  const parent = join(root, relParent);
  if (!existsSync(parent)) return [];
  const mounts = [];
  for (const entry of readdirSync(parent, { withFileTypes: true })) {
    if (entry.name.startsWith(".")) continue;
    if (relParent === "games" && entry.name === "private") continue;
    if (!entry.isDirectory() && !entry.isSymbolicLink()) continue;
    const relRoot = `${relParent}/${entry.name}`;
    if (visibility === "public" && lstatSync(join(root, relRoot)).isSymbolicLink()) {
      throw new Error(`public ${kind} root must not be a symlink or junction: ${relRoot}`);
    }
    const manifest = join(root, relRoot, `${kind}.json`);
    if (!existsSync(manifest)) {
      warn(options, `${relRoot}: missing ${kind}.json; skipping incomplete folder`);
      continue;
    }
    // One malformed folder must not take the workspace down with it: an
    // unrelated broken game would otherwise break every command that only
    // needed to enumerate the healthy ones. The failure is not lost -- it is
    // warned here and rethrown by the caller if it IS the requested target.
    try {
      mounts.push(resolveMount(root, relRoot, kind, visibility));
    } catch (error) {
      const message = error && error.message ? error.message : String(error);
      unusable.push({ root: relRoot, name: entry.name, kind, message });
      warn(options, `${message}; skipping unusable ${kind} folder`);
    }
  }
  return mounts;
}

function scanPrivateGames(root, options) {
  const relParent = "games/private";
  const parent = join(root, relParent);
  if (!existsSync(parent)) return [];
  const canonical = [];
  const aliases = [];
  for (const entry of readdirSync(parent, { withFileTypes: true })) {
    if (entry.name.startsWith(".")) continue;
    if (!entry.isDirectory() && !entry.isSymbolicLink()) continue;
    const relRoot = `${relParent}/${entry.name}`;
    if (lstatSync(join(root, relRoot)).isSymbolicLink()) {
      throw new Error(`private game root must not be a symlink or junction: ${relRoot}`);
    }
    const manifest = join(root, relRoot, "game.json");
    if (!existsSync(manifest)) {
      warn(options, `${relRoot}: missing game.json; skipping incomplete folder`);
      continue;
    }
    const mount = resolveMount(root, relRoot, "game", "private", { allowRootIdMismatch: true });
    if (entry.name !== mount.id && comparable(entry.name) === comparable(mount.id)) {
      throw new Error(`${relRoot}: private worktree alias case-collides canonical game '${mount.id}'`);
    }
    if (entry.name === mount.id) canonical.push(mount);
    else aliases.push(mount);
  }
  assertUniqueResolved(canonical);
  const canonicalById = new Map(canonical.map((mount) => [comparable(mount.id), mount]));
  for (const alias of aliases) {
    const canonicalMount = canonicalById.get(comparable(alias.id));
    if (!canonicalMount) {
      throw new Error(`${alias.root}: private identity alias has no canonical game '${alias.id}'`);
    }
    verifyPrivateWorktreeAlias(root, canonicalMount, alias, options);
    canonicalMount.auditRoots.push(alias.root);
  }
  return canonical;
}

function assertUniqueResolved(mounts) {
  const dimensions = [
    ["root", (mount) => mount.root],
    ["derived id", (mount) => mount.id],
    ["storage namespace", (mount) => mount.storageNamespace],
    ["store id", (mount) => mount.storeId],
  ];
  for (const [label, select] of dimensions) {
    const seen = new Set();
    for (const mount of mounts) {
      const value = comparable(select(mount));
      if (seen.has(value)) throw new Error(`duplicate ${label} '${select(mount)}' in workspace folders`);
      seen.add(value);
    }
  }
  const aliases = new Set(mounts.flatMap((mount) => [mount.id, mount.storageNamespace, mount.storeId]).map(comparable));
  for (const mount of mounts) {
    for (const alias of mount.aliases) {
      const key = comparable(alias);
      if (aliases.has(key)) throw new Error(`colliding alias '${alias}' in workspace folders`);
      aliases.add(key);
    }
  }
}

function privateMountSelected(mount, options) {
  return options.includePrivate === true
    || (options.activeGameId && comparable(options.activeGameId) === comparable(mount.id))
    || (options.activeStoreId && comparable(options.activeStoreId) === comparable(mount.storeId));
}

/* A folder the scan had to skip stays silent only while nobody asks for it.
   The moment a caller names that exact folder, its own error is the answer. */
function assertRequestedTargetUsable(unusable, options) {
  const wanted = comparable(options.activeGameId || "");
  if (!wanted) return;
  const broken = unusable.find((entry) => comparable(entry.name) === wanted);
  if (broken) throw new Error(broken.message);
}

export function listWorkspaceMounts(root, options = {}) {
  const unusable = Array.isArray(options.unusable) ? options.unusable : [];
  const mounts = [
    ...scanChildren(root, "games", "game", "public", options, unusable),
    ...scanChildren(root, "templates", "template", "public", options, unusable),
  ];
  if (options.includePrivate === true || options.activeGameId || options.activeStoreId) {
    mounts.push(...scanPrivateGames(root, options).filter((mount) => privateMountSelected(mount, options)));
  }
  assertRequestedTargetUsable(unusable, options);
  assertUniqueResolved(mounts);
  const kinds = options.kinds ? new Set(options.kinds) : null;
  return mounts
    .filter((mount) => !kinds || kinds.has(mount.kind))
    .sort((a, b) => a.kind.localeCompare(b.kind) || a.id.localeCompare(b.id));
}

function atomicWriteJson(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  const temp = `${path}.${process.pid}.tmp`;
  writeFileSync(temp, `${JSON.stringify(value, null, 2)}\n`, "utf8");
  renameSync(temp, path);
}

export function writeIdentityManifest(root, kind, { id, title, storageNamespace = id, aliases = [] }) {
  const normalizedId = normalizeId(id, `${kind} id`);
  const folder = kind === "game" ? "games" : kind === "template" ? "templates" : "";
  if (!folder) throw new Error("identity kind must be game or template");
  const value = {
    schema: kind === "game" ? GAME_IDENTITY_SCHEMA : TEMPLATE_IDENTITY_SCHEMA,
    id: normalizedId,
    title: String(title || "").trim(),
    storageNamespace: normalizeId(storageNamespace, `${kind} storageNamespace`),
    ...(aliases.length ? { aliases: normalizeAliases(aliases, `${kind} aliases`) } : {}),
  };
  if (!value.title) throw new Error(`${kind} title must not be empty`);
  atomicWriteJson(join(root, folder, normalizedId, `${kind}.json`), value);
  return value;
}

export function writeGameDependencies(root, gameId, value) {
  const id = normalizeId(gameId, "game id");
  const candidate = { schema: GAME_DEPENDENCIES_SCHEMA, ...value };
  const rel = `games/${id}/dependencies.json`;
  validateGameDependenciesValue(candidate, rel);
  atomicWriteJson(join(root, rel), candidate);
  return candidate;
}
