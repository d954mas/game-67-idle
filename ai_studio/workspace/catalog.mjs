import {
  existsSync,
  mkdirSync,
  readFileSync,
  realpathSync,
  renameSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { dirname, isAbsolute, join, relative, resolve } from "node:path";

export const CATALOG_SCHEMA = "ai_studio.workspace.catalog.v1";
export const GAME_IDENTITY_SCHEMA = "ai_studio.game.v1";
export const TEMPLATE_IDENTITY_SCHEMA = "ai_studio.template.v1";
export const GAME_DEPENDENCIES_SCHEMA = "ai_studio.game.dependencies.v2";

const CATALOG_KEYS = new Set(["schema", "mounts"]);
const MOUNT_KEYS = new Set([
  "kind", "root", "visibility", "gitRoot", "commitPolicy", "enabledStores", "aliases",
]);
const IDENTITY_KEYS = new Set(["schema", "id", "title", "storageNamespace"]);
const DEPENDENCY_KEYS = new Set(["schema", "engine", "features", "compatibility"]);
const ENGINE_KEYS = new Set(["source", "version", "revision", "compatibility"]);
const FEATURE_KEYS = new Set(["id", "source", "version", "revision", "compatibility"]);
const ALLOWED_STORES = new Set(["assets", "taskboard", "canvas", "evidence"]);
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

function normalizeRoot(value, label) {
  const text = slash(value).trim().replace(/^\.\//, "").replace(/\/+$/, "");
  if (!text || isAbsolute(text) || /^[a-z]:\//i.test(text) || text.startsWith("//") || text.split("/").includes("..")) {
    throw new Error(`${label} must be repo-relative and stay inside the repository`);
  }
  return text;
}

function ensureRealRootInsideRepository(repoRoot, relRoot, label) {
  const abs = join(repoRoot, relRoot);
  if (!existsSync(abs) || !statSync(abs).isDirectory()) throw new Error(`${label} does not exist: ${relRoot}`);
  const realRepo = realpathSync(repoRoot);
  const realRoot = realpathSync(abs);
  const rel = relative(realRepo, realRoot);
  if (rel.startsWith("..") || isAbsolute(rel)) throw new Error(`${label} resolves outside the repository: ${relRoot}`);
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

function normalizeStores(value, label) {
  if (!Array.isArray(value) || value.length === 0) throw new Error(`${label} must be a non-empty array`);
  const stores = value.map((item) => String(item || "").trim());
  for (const store of stores) {
    if (!ALLOWED_STORES.has(store)) throw new Error(`${label}: unknown store '${store}'`);
  }
  if (new Set(stores).size !== stores.length) throw new Error(`${label} contains duplicates`);
  return stores;
}

export function catalogRelPath(local = false) {
  return `ai_studio/workspace/catalog${local ? ".local" : ""}.json`;
}

function catalogPath(root, local = false) {
  return join(root, ...catalogRelPath(local).split("/"));
}

function normalizeMount(raw, index, local) {
  const label = `${catalogRelPath(local)}: mounts[${index}]`;
  assertObject(raw, label);
  assertKnownKeys(raw, MOUNT_KEYS, label);
  const kind = String(raw.kind || "").trim();
  if (kind !== "game" && kind !== "template") throw new Error(`${label}.kind must be game or template`);
  const root = normalizeRoot(raw.root, `${label}.root`);
  const expectedPrefix = kind === "game" ? "games/" : "templates/";
  if (!root.startsWith(expectedPrefix) || root.slice(expectedPrefix.length).includes("/")) {
    throw new Error(`${label}.root must be a direct child of ${expectedPrefix.slice(0, -1)}`);
  }
  const visibility = String(raw.visibility || "").trim();
  const allowedVisibility = local ? ["private", "local"] : ["public"];
  if (!allowedVisibility.includes(visibility)) {
    throw new Error(`${label}.visibility must be ${allowedVisibility.join(" or ")}`);
  }
  const commitPolicy = String(raw.commitPolicy || "").trim();
  if (visibility === "public" && commitPolicy !== "parent-public") {
    throw new Error(`${label}.commitPolicy must be parent-public`);
  }
  if (visibility !== "public" && !["nested-private", "local-only"].includes(commitPolicy)) {
    throw new Error(`${label}.commitPolicy must be nested-private or local-only`);
  }
  const gitRoot = raw.gitRoot === "" ? "" : normalizeRoot(raw.gitRoot, `${label}.gitRoot`);
  if (visibility === "public" && gitRoot !== "") throw new Error(`${label}.gitRoot must be empty for public mounts`);
  if (visibility !== "public" && gitRoot !== root) throw new Error(`${label}.gitRoot must match root`);
  return {
    kind,
    root,
    visibility,
    gitRoot,
    commitPolicy,
    enabledStores: normalizeStores(raw.enabledStores, `${label}.enabledStores`),
    aliases: normalizeAliases(raw.aliases, `${label}.aliases`),
  };
}

export function readWorkspaceCatalog(root, { local = false } = {}) {
  const path = catalogPath(root, local);
  if (!existsSync(path)) {
    if (local) return { schema: CATALOG_SCHEMA, mounts: [] };
    throw new Error(`${catalogRelPath(false)}: catalog is required`);
  }
  const document = readJson(path, catalogRelPath(local));
  assertKnownKeys(document, CATALOG_KEYS, catalogRelPath(local));
  if (document.schema !== CATALOG_SCHEMA) {
    throw new Error(`${catalogRelPath(local)}: expected schema ${CATALOG_SCHEMA}`);
  }
  if (!Array.isArray(document.mounts)) throw new Error(`${catalogRelPath(local)}: mounts must be an array`);
  return {
    schema: CATALOG_SCHEMA,
    mounts: document.mounts.map((mount, index) => normalizeMount(mount, index, local)),
  };
}

function readIdentity(root, mount) {
  ensureRealRootInsideRepository(root, mount.root, `${mount.kind} root`);
  const path = join(root, mount.root, `${mount.kind}.json`);
  if (!existsSync(path)) throw new Error(`${mount.root}: missing identity manifest ${mount.kind}.json`);
  const identity = readJson(path, `${mount.root}/${mount.kind}.json`);
  assertKnownKeys(identity, IDENTITY_KEYS, `${mount.root}/${mount.kind}.json`);
  const expectedSchema = mount.kind === "game" ? GAME_IDENTITY_SCHEMA : TEMPLATE_IDENTITY_SCHEMA;
  if (identity.schema !== expectedSchema) throw new Error(`${mount.root}/${mount.kind}.json: expected schema ${expectedSchema}`);
  const id = normalizeId(identity.id, `${mount.kind} id`);
  if (mount.root.split("/").at(-1) !== id) throw new Error(`${mount.root}: root basename must match identity id '${id}'`);
  const title = String(identity.title || "").trim();
  if (!title) throw new Error(`${mount.root}/${mount.kind}.json: title must not be empty`);
  const storageNamespace = normalizeId(identity.storageNamespace, `${mount.kind} storageNamespace`);
  return { id, title, storageNamespace };
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
  if (!EXACT_SEMVER.test(value.engine.version)) throw new Error(`${rel}.engine.version must be exact SemVer x.y.z`);
  if (!/^[0-9a-f]{40,64}$/i.test(value.engine.revision)) throw new Error(`${rel}.engine.revision must be an exact Git revision`);
  if (!Array.isArray(value.features)) throw new Error(`${rel}.features must be an array`);
  const featureIds = new Set();
  for (const [index, feature] of value.features.entries()) {
    assertObject(feature, `${rel}.features[${index}]`);
    assertKnownKeys(feature, FEATURE_KEYS, `${rel}.features[${index}]`);
    const id = normalizeId(feature.id, `${rel}.features[${index}].id`);
    if (featureIds.has(id)) throw new Error(`${rel}: duplicate feature '${id}'`);
    featureIds.add(id);
    for (const key of ["source", "revision", "compatibility"]) {
      if (!String(feature[key] || "").trim()) throw new Error(`${rel}.features[${index}].${key} must not be empty`);
    }
    if (!EXACT_SEMVER.test(String(feature.version || ""))) throw new Error(`${rel}.features[${index}].version must be exact SemVer x.y.z`);
    if (!/^[0-9a-f]{40,64}$/i.test(feature.revision)) throw new Error(`${rel}.features[${index}].revision must be an exact Git revision`);
  }
  if (!String(value.compatibility || "").trim()) throw new Error(`${rel}.compatibility must not be empty`);
  return value;
}

function validateGameDependencies(root, mount) {
  const rel = `${mount.root}/dependencies.json`;
  const path = join(root, rel);
  if (!existsSync(path)) throw new Error(`${mount.root}: missing game dependencies.json`);
  return validateGameDependenciesValue(readJson(path, rel), rel);
}

function resolveMount(root, mount, source) {
  const identity = readIdentity(root, mount);
  if (mount.kind === "game") validateGameDependencies(root, mount);
  return {
    ...mount,
    ...identity,
    storeId: `${mount.kind}:${identity.storageNamespace}`,
    gameId: mount.kind === "game" ? identity.id : undefined,
    templateId: mount.kind === "template" ? identity.id : undefined,
    folder: mount.root,
    assetRoot: `${mount.root}/assets`,
    assets: `${mount.root}/assets`,
    publicAlias: mount.aliases[0] || "",
    source,
  };
}

function assertUniqueResolved(mounts) {
  const dimensions = [
    ["root", (mount) => mount.root],
    ["derived id", (mount) => mount.id],
    ["storage namespace", (mount) => mount.storageNamespace],
    ["store id", (mount) => mount.storeId],
  ];
  for (const [label, select] of dimensions) {
    const seen = new Map();
    for (const mount of mounts) {
      const value = comparable(select(mount));
      if (seen.has(value)) throw new Error(`duplicate ${label} '${select(mount)}' in workspace catalogs`);
      seen.set(value, mount.root);
    }
  }
  const aliases = new Map();
  for (const mount of mounts) {
    for (const value of [mount.id, mount.storageNamespace, mount.storeId]) aliases.set(comparable(value), mount.root);
  }
  for (const mount of mounts) {
    for (const alias of mount.aliases) {
      const key = comparable(alias);
      if (aliases.has(key)) throw new Error(`colliding alias '${alias}' in workspace catalogs`);
      aliases.set(key, mount.root);
    }
  }
}

function localMountSelected(mount, options) {
  return options.includePrivate === true
    || (options.activeGameId && mount.kind === "game" && comparable(options.activeGameId) === comparable(mount.id))
    || (options.activeStoreId && comparable(options.activeStoreId) === comparable(mount.storeId));
}

export function listWorkspaceMounts(root, options = {}) {
  const publicMounts = readWorkspaceCatalog(root).mounts.map((mount) => resolveMount(root, mount, "public"));
  let mounts = publicMounts;
  if (options.includePrivate === true || options.activeGameId || options.activeStoreId) {
    const local = readWorkspaceCatalog(root, { local: true }).mounts
      .map((mount) => resolveMount(root, mount, "local"))
      .filter((mount) => localMountSelected(mount, options));
    mounts = [...publicMounts, ...local];
  }
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

export function upsertWorkspaceMount(root, mount, { local = mount.visibility !== "public" } = {}) {
  const normalized = normalizeMount(mount, 0, local);
  const catalog = !local && !existsSync(catalogPath(root, false))
    ? { schema: CATALOG_SCHEMA, mounts: [] }
    : readWorkspaceCatalog(root, { local });
  const existing = catalog.mounts.findIndex((entry) => comparable(entry.root) === comparable(normalized.root));
  if (existing >= 0) catalog.mounts[existing] = normalized;
  else catalog.mounts.push(normalized);
  catalog.mounts.sort((a, b) => a.kind.localeCompare(b.kind) || a.root.localeCompare(b.root));
  const publicMounts = (local ? readWorkspaceCatalog(root).mounts : catalog.mounts)
    .map((entry) => resolveMount(root, entry, "public"));
  const localMounts = (local ? catalog.mounts : readWorkspaceCatalog(root, { local: true }).mounts)
    .map((entry) => resolveMount(root, entry, "local"));
  const resolved = [...publicMounts, ...localMounts];
  assertUniqueResolved(resolved);
  atomicWriteJson(catalogPath(root, local), catalog);
  return resolved.find((entry) => comparable(entry.root) === comparable(normalized.root));
}

export function writeIdentityManifest(root, kind, { id, title, storageNamespace = id }) {
  const normalizedId = normalizeId(id, `${kind} id`);
  const folder = kind === "game" ? "games" : kind === "template" ? "templates" : "";
  if (!folder) throw new Error("identity kind must be game or template");
  const value = {
    schema: kind === "game" ? GAME_IDENTITY_SCHEMA : TEMPLATE_IDENTITY_SCHEMA,
    id: normalizedId,
    title: String(title || "").trim(),
    storageNamespace: normalizeId(storageNamespace, `${kind} storageNamespace`),
  };
  if (!value.title) throw new Error(`${kind} title must not be empty`);
  atomicWriteJson(join(root, folder, normalizedId, `${kind}.json`), value);
  return value;
}

export function writeGameDependencies(root, gameId, value) {
  const id = normalizeId(gameId, "game id");
  const candidate = {
    schema: GAME_DEPENDENCIES_SCHEMA,
    ...value,
  };
  const rel = `games/${id}/dependencies.json`;
  validateGameDependenciesValue(candidate, rel);
  atomicWriteJson(join(root, rel), candidate);
  return candidate;
}
