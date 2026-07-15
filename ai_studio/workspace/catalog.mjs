import {
  existsSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  renameSync,
  statSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, join } from "node:path";

export const GAME_IDENTITY_SCHEMA = "ai_studio.game.v1";
export const TEMPLATE_IDENTITY_SCHEMA = "ai_studio.template.v1";
export const GAME_DEPENDENCIES_SCHEMA = "ai_studio.game.dependencies.v2";

const IDENTITY_KEYS = new Set(["schema", "id", "title", "storageNamespace", "aliases"]);
const DEPENDENCY_KEYS = new Set(["schema", "engine", "features", "compatibility"]);
const ENGINE_KEYS = new Set(["source", "version", "revision", "compatibility"]);
const FEATURE_KEYS = new Set(["id", "source", "version", "revision", "compatibility"]);
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

function readIdentity(root, relRoot, kind) {
  const rel = `${relRoot}/${kind}.json`;
  const identity = readJson(join(root, rel), rel);
  assertKnownKeys(identity, IDENTITY_KEYS, rel);
  const expectedSchema = kind === "game" ? GAME_IDENTITY_SCHEMA : TEMPLATE_IDENTITY_SCHEMA;
  if (identity.schema !== expectedSchema) throw new Error(`${rel}: expected schema ${expectedSchema}`);
  const id = normalizeId(identity.id, `${kind} id`);
  if (basename(relRoot) !== id) throw new Error(`${relRoot}: root basename must match identity id '${id}'`);
  const title = String(identity.title || "").trim();
  if (!title) throw new Error(`${rel}: title must not be empty`);
  return {
    id,
    title,
    storageNamespace: normalizeId(identity.storageNamespace, `${kind} storageNamespace`),
    aliases: normalizeAliases(identity.aliases, `${rel}.aliases`),
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

function resolveMount(root, relRoot, kind, visibility) {
  const absolute = join(root, relRoot);
  const linked = lstatSync(absolute).isSymbolicLink();
  if (visibility === "public" && linked) {
    throw new Error(`public ${kind} root must not be a symlink or junction: ${relRoot}`);
  }
  const identity = readIdentity(root, relRoot, kind);
  if (kind === "game") {
    const rel = `${relRoot}/dependencies.json`;
    if (!existsSync(join(root, rel))) throw new Error(`${relRoot}: missing game dependencies.json`);
    validateGameDependenciesValue(readJson(join(root, rel), rel), rel);
  }
  const nestedGit = kind === "game" && visibility === "private" && existsSync(join(absolute, ".git"));
  return {
    kind,
    root: relRoot,
    visibility,
    gitRoot: visibility === "public" ? "" : relRoot,
    commitPolicy: visibility === "public" ? "parent-public" : nestedGit ? "nested-private" : "local-only",
    enabledStores: enabledStores(root, relRoot, kind),
    aliases: identity.aliases,
    ...identity,
    storeId: `${kind}:${identity.storageNamespace}`,
    gameId: kind === "game" ? identity.id : undefined,
    templateId: kind === "template" ? identity.id : undefined,
    folder: relRoot,
    assetRoot: `${relRoot}/assets`,
    assets: `${relRoot}/assets`,
    publicAlias: identity.aliases[0] || "",
    source: "scan",
  };
}

function warn(options, message) {
  if (Array.isArray(options.warnings)) options.warnings.push(message);
  else console.warn(`warning: ${message}`);
}

function scanChildren(root, relParent, kind, visibility, options) {
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
    mounts.push(resolveMount(root, relRoot, kind, visibility));
  }
  return mounts;
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

export function listWorkspaceMounts(root, options = {}) {
  const mounts = [
    ...scanChildren(root, "games", "game", "public", options),
    ...scanChildren(root, "templates", "template", "public", options),
  ];
  if (options.includePrivate === true || options.activeGameId || options.activeStoreId) {
    mounts.push(...scanChildren(root, "games/private", "game", "private", options).filter((mount) => privateMountSelected(mount, options)));
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
