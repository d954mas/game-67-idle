import { createHash } from "node:crypto";
import { createRequire } from "node:module";
import {
  existsSync,
  lstatSync,
  readFileSync,
  readdirSync,
  realpathSync,
} from "node:fs";
import { isAbsolute, join, relative, resolve, sep } from "node:path";

import { hasCopiedGameTooling } from "./copied_tooling.mjs";
import { gameAssetPackDeclaration, readGameAssetPacks, validateGameAssetPacks } from "./game_asset_packs.mjs";

const { hashRuntimeBuildTree } = createRequire(import.meta.url)("./runtime_hash_tree.cjs");

const SCHEMA_V1 = "ai_studio.runtime_build.v1";
const SCHEMA_V2 = "ai_studio.runtime_build.v2";
const SHA256 = /^[0-9a-f]{64}$/;
const GAME_IGNORED_ROOTS = new Set([".ai_studio", ".git", ".mypy_cache", ".pytest_cache", "__pycache__", "build", "capture", "design", "node_modules", "release", "tests", "tmp"]);
const DEPENDENCY_IGNORED_ROOTS = new Set([".git", ".mypy_cache", ".pytest_cache", "__pycache__", "build", "node_modules", "out", "tests"]);
const IGNORED_ANYWHERE = new Set([".mypy_cache", ".pytest_cache", "__pycache__"]);
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
// The Studio files that change the bytes of a web artifact. A game with copied
// tooling hashes its own copies as part of the game tree instead.
const TOOLING_SOURCE = "ai_studio/game_tools";
const TOOLING_FILES = ["build_web.mjs", "inject_asset_bytes.mjs", "minify_web_release.mjs", "lib/game_asset_packs.mjs"];

function exactKeys(value, keys, label) {
  if (!value || typeof value !== "object" || Array.isArray(value)
      || JSON.stringify(Object.keys(value).sort()) !== JSON.stringify([...keys].sort())) {
    throw new Error(`${label} has unexpected fields`);
  }
}

function safeSource(source, label) {
  if (typeof source !== "string" || !source || source.includes("\\") || isAbsolute(source)
      || source.split("/").some((part) => !part || part === "." || part === "..")) {
    throw new Error(`${label} source is invalid`);
  }
  return source;
}

function confinedRoot(studioRoot, source, label) {
  safeSource(source, label);
  const root = realpathSync(resolve(studioRoot));
  const requested = resolve(root, ...source.split("/"));
  if (!existsSync(requested)) throw new Error(`${label} source is missing: ${source}`);
  if (lstatSync(requested).isSymbolicLink()) throw new Error(`${label} source must not be a symbolic link`);
  const actual = realpathSync(requested);
  const rel = relative(root, actual);
  if (!rel || rel === ".." || rel.startsWith(`..${sep}`) || isAbsolute(rel)) {
    throw new Error(`${label} source escapes the Studio root`);
  }
  return actual;
}

// Generated code is derived from hashed sources; a leftover copy from an older
// in-source codegen must not shift the fingerprint.
const GAME_IGNORED_PATHS = new Set(["src/generated"]);

// One confined tree walk for every runtime record (games, capture, package).
function hashTree(root, ignoredRoots, extraRoots = [], ignoredPaths = new Set()) {
  return hashRuntimeBuildTree({
    root, ignoredRoots, ignoredAnywhere: IGNORED_ANYWHERE, extraRoots,
    shouldIgnoreEntry: ({ prefix, name }) => ignoredPaths.has(prefix ? `${prefix}/${name}` : name),
  });
}

function hashFiles(root, names) {
  const digest = createHash("sha256");
  for (const name of names) {
    const bytes = readFileSync(join(root, name));
    digest.update(name).update("\0").update(String(bytes.length)).update("\0").update(sha256(bytes)).update("\n");
  }
  return { files: names.length, sha256: digest.digest("hex") };
}

function readDependencies(gameDir) {
  const path = ["dependencies.json", "game-dependencies.json"]
    .map((name) => join(gameDir, name))
    .find((candidate) => existsSync(candidate));
  if (!path) throw new Error("runtime build requires dependencies.json or game-dependencies.json");
  let value;
  try { value = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, "")); }
  catch (error) { throw new Error(`runtime build dependencies are invalid: ${error.message}`); }
  return value;
}

// The record covers what ships. Test registration reaches into features that
// the game never links -- a golden bank, a feature's own suite -- so a mention
// there is not a runtime dependency and must not be declared as one.
const TEST_ONLY_CMAKE = new Set(["GameTests.cmake"]);

function referencedFeatureSources(gameDir) {
  const cmakeFiles = [];
  function visit(dir, topLevel = false) {
    for (const name of readdirSync(dir).sort()) {
      if (topLevel && GAME_IGNORED_ROOTS.has(name)) continue;
      const path = join(dir, name);
      const info = lstatSync(path);
      if (info.isDirectory()) visit(path);
      else if (info.isFile() && (name === "CMakeLists.txt" || name.endsWith(".cmake"))
        && !TEST_ONLY_CMAKE.has(name)) cmakeFiles.push(path);
    }
  }
  visit(gameDir, true);
  const sources = new Set();
  const pattern = /\$\{(?:GAME_REPO_ROOT|STUDIO_ROOT)\}\/features\/([a-z][a-z0-9-]*)/g;
  for (const path of cmakeFiles) {
    const text = readFileSync(path, "utf8").replace(/#.*$/gm, "");
    if (/\$\{(?:GAME_REPO_ROOT|STUDIO_ROOT)\}\/features\/\s*\$\{/.test(text)) {
      throw new Error("runtime feature paths must use a literal feature id in CMake");
    }
    for (const match of text.matchAll(pattern)) sources.add(`features/${match[1]}`);
  }
  return sources;
}

function validateCompileProfile(profile) {
  exactKeys(profile, ["target", "adapter", "preset", "debugUi", "devapi", "analytics", "eventsLogMirror"], "runtime build profile");
  if (!/^[a-z][a-z0-9-]*$/.test(profile.target || "")
      || !/^[a-z][a-z0-9-]*$/.test(profile.adapter || "")
      || !["wasm-release", "wasm-debug", "wasm-devapi-debug", "wasm-devapi-perf"].includes(profile.preset)
      || typeof profile.debugUi !== "boolean" || typeof profile.devapi !== "boolean"
      || typeof profile.analytics !== "boolean" || typeof profile.eventsLogMirror !== "boolean") {
    throw new Error("runtime build profile is invalid");
  }
  return profile;
}

export function runtimeBuildWitness(record) {
  const validated = validateRuntimeBuildRecord(record);
  if (validated.schema !== SCHEMA_V2) throw new Error("runtime build witness requires a target-bound profile");
  return `ai_studio.runtime_build:${validated.fingerprint};t:${validated.profile.target};a:${validated.profile.adapter};p:${validated.profile.preset};du:${Number(validated.profile.debugUi)};d:${Number(validated.profile.devapi)};a:${Number(validated.profile.analytics)};l:${Number(validated.profile.eventsLogMirror)}`;
}

export function validateRuntimeBuildRecord(record) {
  const v2 = record?.schema === SCHEMA_V2;
  const keys = v2 ? ["schema", "fingerprint", "inputs", "profile"] : ["schema", "fingerprint", "inputs"];
  const hasAssetPacks = record && Object.hasOwn(record, "assetPacks");
  if (hasAssetPacks) keys.push("assetPacks");
  exactKeys(record, keys, "runtime build record");
  if (hasAssetPacks && !validateGameAssetPacks(record.assetPacks).length) {
    throw new Error("runtime build asset pack declaration must not be empty");
  }
  if (!([SCHEMA_V1, SCHEMA_V2].includes(record.schema)) || !Array.isArray(record.inputs) || record.inputs.length < 2) {
    throw new Error("runtime build record schema/inputs are invalid");
  }
  const ids = new Set();
  for (const input of record.inputs) {
    exactKeys(input, ["id", "source", "files", "sha256"], "runtime build input");
    if (!/^(?:game|engine|tooling|feature:[a-z][a-z0-9-]*)$/.test(input.id || "") || ids.has(input.id)) {
      throw new Error("runtime build input id is invalid or duplicated");
    }
    ids.add(input.id);
    if (input.id === "game") {
      if (input.source !== ".") throw new Error("runtime build game source is invalid");
    } else {
      safeSource(input.source, `runtime build ${input.id}`);
      const expectedSource = input.id === "engine" ? "external/neotolis-engine"
        : input.id === "tooling" ? TOOLING_SOURCE : `features/${input.id.slice("feature:".length)}`;
      if (input.source !== expectedSource) throw new Error(`runtime build ${input.id} source must be exactly ${expectedSource}`);
    }
    if (!Number.isSafeInteger(input.files) || input.files < 1 || !SHA256.test(input.sha256 || "")) {
      throw new Error("runtime build input file count/hash is invalid");
    }
  }
  if (record.inputs[0]?.id !== "game" || record.inputs[1]?.id !== "engine"
      || record.inputs.slice(2).some((input, index, rows) => index > 0 && rows[index - 1].id >= input.id)) {
    throw new Error("runtime build inputs are not canonically ordered");
  }
  const profile = v2 ? validateCompileProfile(record.profile) : null;
  const boundInputs = v2 ? { inputs: record.inputs, profile } : hasAssetPacks ? { inputs: record.inputs } : record.inputs;
  if (hasAssetPacks) boundInputs.assetPacks = record.assetPacks;
  const expected = sha256(Buffer.from(JSON.stringify(boundInputs), "utf8"));
  if (!SHA256.test(record.fingerprint || "") || record.fingerprint !== expected) {
    throw new Error("runtime build fingerprint does not match its inputs");
  }
  return record;
}

export function createRuntimeBuildRecord({ gameDir, studioRoot, dependencies = readDependencies(gameDir), compileProfile = null }) {
  const gameRoot = realpathSync(resolve(gameDir));
  const root = realpathSync(resolve(studioRoot));
  const game = hashTree(gameRoot, GAME_IGNORED_ROOTS, ["design/items"], GAME_IGNORED_PATHS);
  const engineSource = safeSource(dependencies?.engine?.source, "engine dependency");
  if (engineSource !== "external/neotolis-engine") {
    throw new Error("engine dependency source must be exactly external/neotolis-engine");
  }
  const inputs = [{ id: "game", source: ".", ...game }];
  inputs.push({
    id: "engine",
    source: engineSource,
    ...hashTree(confinedRoot(root, engineSource, "engine dependency"), DEPENDENCY_IGNORED_ROOTS),
  });
  if (!Array.isArray(dependencies?.features)) throw new Error("runtime build feature dependencies are invalid");
  const declaredFeatureSources = new Set(
    dependencies.features.map((feature) => feature?.source),
  );
  const compiledFeatureSources = referencedFeatureSources(gameRoot);
  for (const source of compiledFeatureSources) {
    if (!declaredFeatureSources.has(source)) {
      throw new Error(`runtime feature ${source} is compiled but not declared`);
    }
  }
  for (const source of declaredFeatureSources) {
    if (!compiledFeatureSources.has(source)) {
      throw new Error(`runtime feature ${source} is declared but not compiled through the canonical CMake boundary`);
    }
  }
  const features = dependencies.features.map((feature) => {
    if (!/^[a-z][a-z0-9-]*$/.test(feature?.id || "")) throw new Error("runtime build feature id is invalid");
    const source = safeSource(feature.source, `feature ${feature.id}`);
    if (source !== `features/${feature.id}`) throw new Error(`feature ${feature.id} source must be exactly features/${feature.id}`);
    return {
      id: `feature:${feature.id}`,
      source,
      ...hashTree(confinedRoot(root, source, `feature ${feature.id}`), DEPENDENCY_IGNORED_ROOTS),
    };
  }).sort((left, right) => left.id.localeCompare(right.id));
  inputs.push(...features);
  if (!hasCopiedGameTooling(gameRoot)) {
    inputs.push({ id: "tooling", source: TOOLING_SOURCE, ...hashFiles(confinedRoot(root, TOOLING_SOURCE, "game tooling"), TOOLING_FILES) });
  }
  const profile = compileProfile ? validateCompileProfile(compileProfile) : null;
  const packs = readGameAssetPacks(gameRoot);
  const assetPacks = packs.length ? gameAssetPackDeclaration(packs) : null;
  const boundInputs = profile ? { inputs, profile } : assetPacks ? { inputs } : inputs;
  if (assetPacks) boundInputs.assetPacks = assetPacks;
  const record = {
    schema: profile ? SCHEMA_V2 : SCHEMA_V1,
    fingerprint: sha256(Buffer.from(JSON.stringify(boundInputs), "utf8")),
    inputs,
    ...(profile ? { profile } : {}),
    ...(assetPacks ? { assetPacks } : {}),
  };
  return validateRuntimeBuildRecord(record);
}
