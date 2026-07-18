import { createHash } from "node:crypto";
import {
  existsSync,
  lstatSync,
  readFileSync,
  readdirSync,
  realpathSync,
} from "node:fs";
import { isAbsolute, join, relative, resolve, sep } from "node:path";

const SCHEMA = "ai_studio.runtime_build.v1";
const SHA256 = /^[0-9a-f]{64}$/;
const GAME_IGNORED_ROOTS = new Set([".ai_studio", ".git", ".mypy_cache", ".pytest_cache", "__pycache__", "build", "design", "node_modules", "release", "tests"]);
const DEPENDENCY_IGNORED_ROOTS = new Set([".git", ".mypy_cache", ".pytest_cache", "__pycache__", "build", "node_modules", "out", "tests"]);
const IGNORED_ANYWHERE = new Set([".mypy_cache", ".pytest_cache", "__pycache__"]);
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");

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

function hashTree(root, ignoredRoots) {
  const realRoot = realpathSync(resolve(root));
  const rows = [];
  function visit(dir, prefix = "") {
    for (const name of readdirSync(dir).sort()) {
      if ((!prefix && ignoredRoots.has(name)) || IGNORED_ANYWHERE.has(name)) continue;
      const path = join(dir, name);
      const rel = prefix ? `${prefix}/${name}` : name;
      const info = lstatSync(path);
      if (info.isSymbolicLink()) throw new Error(`runtime build input must not contain a symbolic link: ${rel}`);
      const actual = realpathSync(path);
      const confined = relative(realRoot, actual);
      if (confined === ".." || confined.startsWith(`..${sep}`) || isAbsolute(confined)) {
        throw new Error(`runtime build input escapes its root: ${rel}`);
      }
      if (info.isDirectory()) visit(path, rel);
      else if (info.isFile()) {
        if (/\.md$/i.test(name) || /(?:\.test\.mjs|_test\.py|\.pyc)$/i.test(name)) continue;
        const bytes = readFileSync(path);
        rows.push({ path: rel, size: bytes.length, sha256: sha256(bytes) });
      } else throw new Error(`unsupported runtime build input: ${rel}`);
    }
  }
  visit(realRoot);
  const digest = createHash("sha256");
  for (const row of rows) digest.update(row.path).update("\0").update(String(row.size)).update("\0").update(row.sha256).update("\n");
  return { files: rows.length, sha256: digest.digest("hex") };
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

export function validateRuntimeBuildRecord(record) {
  exactKeys(record, ["schema", "fingerprint", "inputs"], "runtime build record");
  if (record.schema !== SCHEMA || !Array.isArray(record.inputs) || record.inputs.length < 2) {
    throw new Error("runtime build record schema/inputs are invalid");
  }
  const ids = new Set();
  for (const input of record.inputs) {
    exactKeys(input, ["id", "source", "files", "sha256"], "runtime build input");
    if (!/^(?:game|engine|feature:[a-z][a-z0-9-]*)$/.test(input.id || "") || ids.has(input.id)) {
      throw new Error("runtime build input id is invalid or duplicated");
    }
    ids.add(input.id);
    if (input.id === "game") {
      if (input.source !== ".") throw new Error("runtime build game source is invalid");
    } else {
      safeSource(input.source, `runtime build ${input.id}`);
      const expectedSource = input.id === "engine" ? "external/neotolis-engine" : `features/${input.id.slice("feature:".length)}`;
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
  const expected = sha256(Buffer.from(JSON.stringify(record.inputs), "utf8"));
  if (!SHA256.test(record.fingerprint || "") || record.fingerprint !== expected) {
    throw new Error("runtime build fingerprint does not match its inputs");
  }
  return record;
}

export function createRuntimeBuildRecord({ gameDir, studioRoot, dependencies = readDependencies(gameDir) }) {
  const gameRoot = realpathSync(resolve(gameDir));
  const root = realpathSync(resolve(studioRoot));
  const game = hashTree(gameRoot, GAME_IGNORED_ROOTS);
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
  const record = { schema: SCHEMA, fingerprint: sha256(Buffer.from(JSON.stringify(inputs), "utf8")), inputs };
  return validateRuntimeBuildRecord(record);
}
