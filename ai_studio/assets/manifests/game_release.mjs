import { createHash } from "node:crypto";
import { existsSync, lstatSync, readFileSync, readdirSync, realpathSync } from "node:fs";
import { isAbsolute, join, relative, resolve } from "node:path";
import { spawnSync } from "node:child_process";

import { validateLicenseRecord } from "../licenses/ops.mjs";

const PACKED_EXTENSIONS = /\.(?:glb|mp3|png|wav)$/i;
const SAFE_ASSET_PATH = /^assets\/[A-Za-z0-9_./-]+$/;
const ORIGINS = new Set(["mine", "ai", "sourced"]);

const slash = (value) => String(value || "").replaceAll("\\", "/");
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
const isSafeAssetPath = (path) => SAFE_ASSET_PATH.test(path)
  && path.split("/").every((segment) => segment && segment !== "." && segment !== "..");

export function packedAssetPaths(gameDir) {
  const contractPath = join(resolve(gameDir), "assets", "release_inputs.json");
  if (!existsSync(contractPath)) throw new Error("game release asset input contract is missing");
  const contract = JSON.parse(readFileSync(contractPath, "utf8").replace(/^\uFEFF/, ""));
  if (contract.schema !== "ai_studio.game_release_assets.v1" || !Array.isArray(contract.inputs)) {
    throw new Error("game release asset input contract is invalid");
  }
  const inputs = contract.inputs.map(slash);
  if (new Set(inputs).size !== inputs.length || inputs.some((path) => (
    !isSafeAssetPath(path) || !PACKED_EXTENSIONS.test(path)
  ))) {
    throw new Error("game release asset inputs must be unique safe binary asset paths");
  }
  return [...inputs].sort();
}

export function standaloneAssetPaths(gameDir) {
  const contractPath = join(resolve(gameDir), "assets", "release_inputs.json");
  if (!existsSync(contractPath)) throw new Error("game release asset input contract is missing");
  const contract = JSON.parse(readFileSync(contractPath, "utf8").replace(/^\uFEFF/, ""));
  if (contract.schema !== "ai_studio.game_release_assets.v1" || !Array.isArray(contract.inputs)) {
    throw new Error("game release asset input contract is invalid");
  }
  if (contract.standalone_inputs === undefined) return [];
  if (!Array.isArray(contract.standalone_inputs)) {
    throw new Error("game release standalone inputs must be an array");
  }
  const inputs = contract.standalone_inputs.map(slash);
  if (new Set(inputs).size !== inputs.length || inputs.some((path) => (
    !isSafeAssetPath(path) || !PACKED_EXTENSIONS.test(path)
  ))) {
    throw new Error("game release standalone inputs must be unique safe binary asset paths");
  }
  const packed = new Set(contract.inputs.map(slash));
  if (inputs.some((path) => packed.has(path))) {
    throw new Error("game release standalone inputs must not overlap packed inputs");
  }
  return [...inputs].sort();
}

function stagedAssetPath(token, variables) {
  const variable = String(token).trim().match(/^\$\{([A-Za-z_][A-Za-z0-9_]*)\}$/);
  const value = slash(variable ? variables.get(variable[1]) || "" : token);
  const match = value.match(/(?:^|\/)(assets\/[A-Za-z0-9_./-]+\.(?:glb|mp3|png|wav))$/i);
  return match && isSafeAssetPath(match[1]) ? match[1] : "";
}

export function webStagedAssetPaths(gameDir) {
  const sourcePath = join(resolve(gameDir), "cmake", "GamePlatform.cmake");
  if (!existsSync(sourcePath)) throw new Error("game web staging source is missing");
  const source = readFileSync(sourcePath, "utf8").replace(/#.*$/gm, "");
  const variables = new Map();
  for (const match of source.matchAll(/^\s*set\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+"([^"]+)"\s*\)\s*$/gm)) {
    const path = stagedAssetPath(match[2], variables);
    if (path) variables.set(match[1], path);
  }
  const staged = new Set();
  const calls = [
    /configure_file\s*\(\s*"([^"]+)"\s+"[^"]+"\s+COPYONLY\s*\)/gi,
    /COMMAND\s+\$\{CMAKE_COMMAND\}\s+-E\s+copy_if_different\s+"([^"]+)"\s+"[^"]+"/gi,
  ];
  for (const expression of calls) {
    for (const match of source.matchAll(expression)) {
      const path = stagedAssetPath(match[1], variables);
      if (path) staged.add(path);
    }
  }
  return [...staged].sort();
}

export function builderAssetPaths(gameDir) {
  const root = resolve(gameDir);
  const sourcePath = join(root, "src", "build_packs.c");
  if (!existsSync(sourcePath)) throw new Error("game pack builder source is missing");
  const pending = [sourcePath];
  const visited = new Set();
  const sources = [];
  while (pending.length) {
    const current = pending.pop();
    if (visited.has(current)) continue;
    visited.add(current);
    const source = readFileSync(current, "utf8");
    sources.push(source);
    for (const match of source.matchAll(/^\s*#include\s+"([^"]+)"/gm)) {
      const candidates = [join(resolve(current, ".."), match[1]), join(root, "src", match[1])];
      const included = candidates.find((candidate) => existsSync(candidate));
      if (included && !relative(root, resolve(included)).split(/[\\/]/).includes("..")) {
        pending.push(resolve(included));
      }
    }
  }
  const paths = [...sources.join("\n").matchAll(/"(assets\/[^"\r\n]+\.(?:glb|mp3|png|wav))"/gi)]
    .map((match) => slash(match[1]));
  if (paths.some((path) => path.includes("%"))) {
    throw new Error("game pack builder binary asset paths must be explicit string literals");
  }
  if (paths.some((path) => !isSafeAssetPath(path) || !PACKED_EXTENSIONS.test(path))) {
    throw new Error("game pack builder contains an unsafe binary asset path");
  }
  return [...new Set(paths)].sort();
}

function addRecord(records, duplicates, path, record, metadataPath) {
  const normalized = slash(path);
  if (!isSafeAssetPath(normalized) || !PACKED_EXTENSIONS.test(normalized)) return;
  const next = { ...record, path: normalized, metadataPath };
  const current = records.get(normalized);
  if (current) duplicates.add(normalized);
  records.set(normalized, { ...current, ...next });
}

function loadPackRecords(gameDir, records, duplicates) {
  const packsRoot = join(gameDir, "assets", "packs");
  if (!existsSync(packsRoot)) return;
  for (const pack of readdirSync(packsRoot).sort()) {
    const packPath = join(packsRoot, pack, "pack.json");
    const defaults = existsSync(packPath)
      ? JSON.parse(readFileSync(packPath, "utf8").replace(/^\uFEFF/, ""))
      : {};
    const path = join(packsRoot, pack, "assets.jsonl");
    if (!existsSync(path)) continue;
    for (const [index, line] of readFileSync(path, "utf8").split(/\r?\n/).entries()) {
      if (!line.trim()) continue;
      let row;
      try { row = JSON.parse(line); }
      catch (error) { throw new Error(`${slash(relative(gameDir, path))}:${index + 1}: ${error.message}`); }
      const record = {
        ...defaults,
        ...row,
        defaultsPath: existsSync(packPath) ? slash(relative(gameDir, packPath)) : "",
      };
      const rowPaths = new Set();
      for (const key of ["source_resource", "source_model", "source_preview"]) {
        if (typeof row[key] === "string") {
          const sourcePath = slash(row[key]);
          if (sourcePath.startsWith("assets/") || !isSafeAssetPath(`assets/${sourcePath}`)) {
            throw new Error(`${slash(relative(gameDir, path))}:${index + 1}: ${key} must be a safe path relative to assets/`);
          }
          rowPaths.add(`assets/${sourcePath}`);
        }
      }
      for (const assetPath of rowPaths) {
        addRecord(records, duplicates, assetPath, record, slash(relative(gameDir, path)));
      }
    }
  }
}

function trackedPaths(gameDir) {
  const safe = slash(resolve(gameDir));
  const result = spawnSync("git", ["-c", `safe.directory=${safe}`, "ls-files", "-z"], {
    cwd: gameDir, encoding: "utf8", shell: false,
  });
  if (result.error || result.status !== 0) {
    throw new Error(`game asset audit cannot read Git index: ${result.error?.message || result.stderr || result.status}`);
  }
  return new Set(result.stdout.split("\0").map(slash).filter(Boolean));
}

export function auditGameReleaseAssets(gameDir, options = {}) {
  const root = resolve(gameDir);
  const realRoot = realpathSync(root);
  const packed = options.packedPaths || packedAssetPaths(root);
  const standalone = options.standalonePaths || standaloneAssetPaths(root);
  const staged = standalone.length === 0
    ? []
    : (options.stagedPaths || webStagedAssetPaths(root));
  const builderInputs = options.builderPaths
    ? [...new Set(options.builderPaths.map(slash))].sort()
    : builderAssetPaths(root);
  const tracked = options.trackedPaths ? new Set(options.trackedPaths.map(slash)) : trackedPaths(root);
  const records = new Map();
  const duplicates = new Set();
  loadPackRecords(root, records, duplicates);
  const issues = [];
  if (!tracked.has("assets/release_inputs.json")) {
    issues.push("assets/release_inputs.json: release input contract is not tracked");
  }
  const packedSet = new Set(packed);
  const releaseSet = new Set([...packed, ...standalone]);
  const builderSet = new Set(builderInputs);
  const stagedSet = new Set(staged);
  for (const path of builderInputs) {
    if (!packedSet.has(path)) issues.push(`${path}: pack builder input is missing from release input contract`);
  }
  for (const path of packed) {
    if (!builderSet.has(path)) issues.push(`${path}: release input is not consumed by the pack builder`);
  }
  for (const path of standalone) {
    if (!stagedSet.has(path)) issues.push(`${path}: standalone input is not staged by the local web build`);
  }
  for (const path of duplicates) issues.push(`${path}: duplicate asset metadata`);
  for (const path of releaseSet) {
    const record = records.get(path);
    const absolute = join(root, ...path.split("/"));
    // Restricted inputs are gitignored by design (paid/non-redistributable
    // sources never enter git); their integrity contract is the committed
    // reconstruction record (source sha + transform) checked below instead
    // of git tracking. On a fresh clone (CI) the file itself cannot exist, so
    // its presence and content checks apply only where it does; the record
    // and license checks always apply.
    const restricted = path.startsWith("assets/restricted/");
    const present = existsSync(absolute);
    if (!present && !restricted) {
      issues.push(`${path}: packed input is missing`);
      continue;
    }
    if (present) {
      const stat = lstatSync(absolute);
      if (stat.isSymbolicLink() || !stat.isFile()) {
        issues.push(`${path}: packed input must be a regular non-symlink file`);
        continue;
      }
      const resolvedInput = realpathSync(absolute);
      const confined = relative(realRoot, resolvedInput);
      if (isAbsolute(confined) || confined === ".." || confined.startsWith(`..\\`) || confined.startsWith("../")) {
        issues.push(`${path}: packed input resolves outside the game root`);
        continue;
      }
    }
    if (!restricted && !tracked.has(path)) issues.push(`${path}: packed input is not tracked`);
    if (restricted && !(record && String(record.transform || "").trim() && String(record.source_file_sha256 || "").trim())) {
      issues.push(`${path}: restricted input requires a reconstruction record with transform and source_file_sha256`);
    }
    if (!record) {
      issues.push(`${path}: missing asset metadata`);
      continue;
    }
    if (!tracked.has(record.metadataPath)) issues.push(`${path}: Pack Manifest record is not tracked`);
    if (record.defaultsPath && !tracked.has(record.defaultsPath)) {
      issues.push(`${path}: Pack Manifest defaults are not tracked`);
    }
    if (!String(record.asset_id || "").trim()) issues.push(`${path}: missing asset_id`);
    if (!ORIGINS.has(String(record.origin || "").trim())) {
      issues.push(`${path}: origin must be mine, ai, or sourced`);
    }
    if (!String(record.provenance || record.source_first || record.generator || "").trim()) {
      issues.push(`${path}: missing provenance`);
    }
    const license = validateLicenseRecord(record, { forDistribution: true, forRelease: true });
    if (!license.ok) issues.push(`${path}: ${license.issues.join("; ")}`);
    const recordedSha256 = String(record.sha256 || "").toLowerCase();
    if (!/^[0-9a-f]{64}$/.test(recordedSha256)) issues.push(`${path}: missing or malformed sha256`);
    if (!Number.isSafeInteger(Number(record.bytes))) issues.push(`${path}: missing or malformed byte size`);
    if (present) {
      const bytes = readFileSync(absolute);
      if (/^[0-9a-f]{64}$/.test(recordedSha256) && sha256(bytes) !== recordedSha256) {
        issues.push(`${path}: sha256 mismatch`);
      }
      if (Number.isSafeInteger(Number(record.bytes)) && Number(record.bytes) !== bytes.length) {
        issues.push(`${path}: byte size mismatch`);
      }
    }
  }
  for (const path of records.keys()) {
    if (!releaseSet.has(path)) issues.push(`${path}: stale asset metadata is not a release input`);
  }
  return { ok: issues.length === 0, packed: packed.length, issues };
}

export function assertGameReleaseAssets(gameDir, options = {}) {
  const result = auditGameReleaseAssets(gameDir, options);
  if (!result.ok) throw new Error(`game release asset audit failed:\n${result.issues.map((item) => `- ${item}`).join("\n")}`);
  return result;
}
