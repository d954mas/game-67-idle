import { copyFileSync, lstatSync, mkdirSync, readFileSync, realpathSync, rmSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join, relative, resolve, sep } from "node:path";

const SCHEMA = "ai_studio.game.asset_packs.v1";
const DECLARATION = "assets/runtime-packs.json";
const STAGING_RECORD = "game-asset-packs.json";

export function validateGameAssetPacks(value) {
  if (!value || typeof value !== "object" || Array.isArray(value)
      || JSON.stringify(Object.keys(value).sort()) !== JSON.stringify(["packs", "schema"])
      || value.schema !== SCHEMA || !Array.isArray(value.packs) || value.packs.length > 64) {
    throw new Error("game asset pack declaration is invalid");
  }
  const seen = new Set();
  for (const path of value.packs) {
    if (typeof path !== "string" || !/^assets\/[a-z][a-z0-9_-]{0,63}\.ntpack$/.test(path)
        || path === "assets/game.ntpack" || seen.has(path)) {
      throw new Error("game asset pack path is invalid or duplicated");
    }
    seen.add(path);
  }
  return [...value.packs];
}

export function gameAssetPackDeclaration(packs) {
  const value = { schema: SCHEMA, packs };
  validateGameAssetPacks(value);
  return value;
}

function confinedPath(root, path, required = false) {
  const base = resolve(root);
  let current = base;
  const parts = path.split("/");
  for (let index = -1; index < parts.length; index += 1) {
    if (index >= 0) current = join(current, parts[index]);
    let info;
    try { info = lstatSync(current); }
    catch (error) {
      if (error.code !== "ENOENT") throw error;
      if (required) throw new Error(`game asset pack file is missing: ${path}`);
      return join(base, ...parts);
    }
    if (info.isSymbolicLink()) throw new Error(`game asset pack path must not contain a symbolic link: ${path}`);
    const rel = relative(realpathSync(base), realpathSync(current));
    if (isAbsolute(rel) || rel === ".." || rel.startsWith(`..${sep}`)) throw new Error("game asset pack path escapes its root");
    if (index + 1 === parts.length ? !info.isFile() : !info.isDirectory()) {
      throw new Error(`game asset pack path has the wrong type: ${path}`);
    }
  }
  return current;
}

function readDeclaration(root, path) {
  const file = confinedPath(root, path);
  let bytes;
  try {
    if (lstatSync(file).size > 16384) throw new Error("game asset pack declaration is too large");
    bytes = readFileSync(file);
  }
  catch (error) { if (error.code === "ENOENT") return []; throw error; }
  if (bytes.length > 16384) throw new Error("game asset pack declaration is too large");
  try { return validateGameAssetPacks(JSON.parse(bytes.toString("utf8"))); }
  catch (error) { throw new Error(`game asset pack declaration is invalid: ${error.message}`); }
}

export function readGameAssetPacks(gameDir) {
  return gameDir ? readDeclaration(gameDir, DECLARATION) : [];
}

export function validateGameAssetPackFiles(root, packs) {
  gameAssetPackDeclaration(packs);
  for (const path of packs) {
    const file = confinedPath(root, path, true);
    if (lstatSync(file).size === 0) throw new Error(`game asset pack payload is empty: ${path}`);
  }
}

export function stageGameAssetPacks({ sourceDir, artifactDir, packs }) {
  const declaration = gameAssetPackDeclaration(packs);
  const recordRoot = dirname(resolve(artifactDir));
  const previous = readDeclaration(recordRoot, STAGING_RECORD);
  validateGameAssetPackFiles(sourceDir, packs);
  // Validate the whole mutation set before copying or pruning any old output.
  for (const path of new Set([...previous, ...packs])) confinedPath(artifactDir, path);
  for (const path of packs) {
    const destination = confinedPath(artifactDir, path);
    mkdirSync(dirname(destination), { recursive: true });
    copyFileSync(confinedPath(sourceDir, path, true), destination);
  }
  for (const path of previous) {
    if (!packs.includes(path)) rmSync(confinedPath(artifactDir, path), { force: true });
  }
  if (packs.length || previous.length) {
    const record = confinedPath(recordRoot, STAGING_RECORD);
    mkdirSync(dirname(record), { recursive: true });
    writeFileSync(record, `${JSON.stringify(declaration, null, 2)}\n`);
  }
}
