import { createHash } from "node:crypto";
import { existsSync, lstatSync, readFileSync, readdirSync, realpathSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join, relative, sep } from "node:path";

const SCHEMA = "ai_studio.game_pack_input_receipt.v1";
const SHA256 = /^[a-f0-9]{64}$/;

function releaseContract(gameDir) {
  const file = join(gameDir, "assets", "release_inputs.json");
  return existsSync(file) ? JSON.parse(readFileSync(file, "utf8").replace(/^\uFEFF/, "")) : null;
}

export function usesPackInputReceipt(gameDir) {
  return releaseContract(gameDir)?.schema === "ai_studio.game_release_assets.v2";
}

function assetPath(path) {
  if (!/^assets\/[A-Za-z0-9_./-]+\.(?:glb|jpe?g|mp3|png|ttf|wav|webp)$/i.test(path)
      || path.split("/").some((part) => !part || part === "." || part === "..")) {
    throw new Error(`unsafe traced pack input: ${path}`);
  }
  return path;
}

function assertSelfContainedGlb(bytes, path) {
  if (bytes.length < 20 || bytes.toString("ascii", 0, 4) !== "glTF" || bytes.readUInt32LE(4) !== 2
      || bytes.readUInt32LE(8) !== bytes.length || bytes.toString("ascii", 16, 20) !== "JSON") {
    throw new Error(`traced GLB is invalid: ${path}`);
  }
  const jsonEnd = 20 + bytes.readUInt32LE(12);
  if (jsonEnd > bytes.length) throw new Error(`traced GLB has a truncated JSON chunk: ${path}`);
  const { buffers = [], images = [] } = JSON.parse(bytes.toString("utf8", 20, jsonEnd));
  for (const { uri } of [...buffers, ...images]) {
    if (uri !== undefined && (typeof uri !== "string" || !uri.startsWith("data:"))) {
      throw new Error(`traced GLB requires an external asset; embed its bytes before packing: ${path}`);
    }
  }
}

function fileHash(root, path, verifySource = false) {
  const file = join(root, ...path.split("/"));
  if (!lstatSync(file).isFile()) throw new Error(`pack receipt requires a regular file: ${path}`);
  const within = relative(realpathSync(root), realpathSync(file));
  if (!within || within === ".." || within.startsWith(`..${sep}`) || isAbsolute(within)) {
    throw new Error(`pack receipt path escapes its root: ${path}`);
  }
  const bytes = readFileSync(file);
  if (verifySource && path.toLowerCase().endsWith(".glb")) assertSelfContainedGlb(bytes, path);
  return createHash("sha256").update(bytes).digest("hex");
}

function packHashes(artifactDir) {
  const packs = readdirSync(join(artifactDir, "assets"))
    .filter((name) => name.endsWith(".ntpack")).sort();
  if (!packs.includes("game.ntpack")) throw new Error("pack receipt is missing the boot pack");
  return packs.map((name) => {
    if (!/^[a-z][a-z0-9_]*\.ntpack$/.test(name)) throw new Error(`unsafe staged pack name: ${name}`);
    const path = `assets/${name}`;
    return { path, sha256: fileHash(artifactDir, path) };
  });
}

function standaloneHashes(gameDir, artifactDir, contract) {
  return (contract?.standalone_inputs || []).map((path) => {
    const stagedPath = /^web\/[A-Za-z0-9_-]+\.webp$/.test(path) ? path.slice(4) : assetPath(path);
    const sha256 = fileHash(gameDir, path);
    if (fileHash(artifactDir, stagedPath) !== sha256) throw new Error(`staged standalone asset differs from its source: ${path}`);
    return { path, stagedPath, sha256 };
  });
}

function studioFontHashes(gameDir, studioRoot, contract) {
  const file = join(gameDir, "cmake", "GameAssets.cmake");
  const source = existsSync(file) ? readFileSync(file, "utf8").replace(/#.*$/gm, "") : "";
  const definitions = [...source.matchAll(/\bset\s*\(\s*GAME_UI_FONT\s+([^)]*)\)/g)];
  const fonts = contract?.studio_fonts || [];
  if (!Array.isArray(fonts) || fonts.length !== definitions.length || fonts.length > 1) {
    throw new Error("release contract must enumerate the explicitly configured Studio UI font");
  }
  return fonts.map(({ path, license, license_file: noticePath, sha256 }) => {
    const configured = definitions[0][1].trim();
    if (!/^ai_studio\/assets\/canvas\/site\/fonts\/[A-Za-z0-9_-]+\/[A-Za-z0-9_-]+\.ttf$/.test(path)
        || configured !== `"\${GAME_REPO_ROOT}/${path}"` || license !== "OFL-1.1"
        || noticePath !== path.replace(/[^/]+$/, "OFL.txt")) {
      throw new Error("Studio UI font path or OFL notice does not match the pack configuration");
    }
    if (fileHash(studioRoot, path) !== sha256) throw new Error(`Studio UI font source changed: ${path}`);
    return { path, sha256, noticePath, noticeSha256: fileHash(studioRoot, noticePath) };
  });
}

const receiptPath = (artifactDir) => join(dirname(artifactDir), "pack-input-receipt.json");

export function stagePackInputReceipt({ gameDir, studioRoot, artifactDir, trace, fingerprint }) {
  if (!SHA256.test(fingerprint)) throw new Error("pack receipt needs a release runtime fingerprint");
  const paths = [...new Set(readFileSync(trace, "utf8").split(/\r?\n/).filter(Boolean)
    .map((path) => assetPath(path.replaceAll("\\", "/"))))].sort();
  if (!paths.length) throw new Error("pack input trace is empty");
  const inputs = paths.map((path) => ({ path, sha256: fileHash(gameDir, path, true) }));
  const contract = releaseContract(gameDir);
  const receipt = {
    schema: SCHEMA, fingerprint, inputs, packs: packHashes(artifactDir),
    standalone: standaloneHashes(gameDir, artifactDir, contract), studioFonts: studioFontHashes(gameDir, studioRoot, contract),
  };
  writeFileSync(receiptPath(artifactDir), `${JSON.stringify(receipt, null, 2)}\n`);
}

export function readPackInputReceipt({ gameDir, studioRoot, artifactDir, fingerprint }) {
  const file = receiptPath(artifactDir);
  if (!existsSync(file)) throw new Error("pack input receipt is missing; rebuild the web artifact before packaging");
  const receipt = JSON.parse(readFileSync(file, "utf8"));
  if (receipt?.schema !== SCHEMA || !SHA256.test(fingerprint) || receipt.fingerprint !== fingerprint) {
    throw new Error("pack input receipt fingerprint does not match the release");
  }
  if (!Array.isArray(receipt.inputs) || !receipt.inputs.length) throw new Error("pack input receipt has no inputs");
  const paths = receipt.inputs.map(({ path, sha256 }) => {
    assetPath(path);
    if (fileHash(gameDir, path) !== sha256) throw new Error(`pack input source changed: ${path}`);
    return path;
  });
  if (paths.some((path, index) => index > 0 && path <= paths[index - 1])) {
    throw new Error("pack input receipt paths must be sorted and unique");
  }
  if (JSON.stringify(receipt.packs) !== JSON.stringify(packHashes(artifactDir))) {
    throw new Error("staged pack bytes changed after the input receipt was built");
  }
  const contract = releaseContract(gameDir);
  if (JSON.stringify(receipt.standalone) !== JSON.stringify(standaloneHashes(gameDir, artifactDir, contract))
      || JSON.stringify(receipt.studioFonts) !== JSON.stringify(studioFontHashes(gameDir, studioRoot, contract))) {
    throw new Error("standalone assets or Studio font evidence changed after the pack input receipt was built");
  }
  return paths;
}
