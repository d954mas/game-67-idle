import { randomUUID, createHash } from "node:crypto";
import { lstatSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, isAbsolute, join, parse, resolve } from "node:path";

import { createStoreZip, readStoreZip } from "../../core_harness/tool_lib/zip_store.mjs";
import { publishArtifactSet, readWasmSymbols } from "./wasm_symbol_artifact.mjs";

const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");
const jsonBytes = (value) => Buffer.from(`${JSON.stringify(value, null, 2)}\n`, "utf8");
const DEFAULT_LAYOUT = Object.freeze({
  schema: "ai_studio.web_package_layout.v1", clientJs: "game.js", clientWasm: "game.wasm",
  primaryPack: "assets/game.ntpack", packManifestDir: null,
  symbolMap: "game.js.symbols", linkedWasm: "bin/game.wasm",
  symbolDigest: "game.js.symbols.wasm.sha256", requireSymbols: true,
});

function safeRelativePath(value, label) {
  if (typeof value !== "string" || !value || isAbsolute(value) || value.includes("\\")
      || value.split("/").some((part) => !/^[A-Za-z0-9._-]+$/.test(part) || part === "." || part === "..")) {
    throw new Error(`invalid web package ${label}`);
  }
  return value;
}

function rejectRedirectedDirectory(path) {
  let current = resolve(path);
  while (current !== parse(current).root) {
    const stat = lstatSync(current, { throwIfNoEntry: false });
    if (stat?.isSymbolicLink()) throw new Error(`web release output directory redirects through a link: ${current}`);
    current = dirname(current);
  }
}

export function loadWebPackageLayout(gameDir) {
  const path = join(gameDir, "release", "web-package.json");
  const stat = lstatSync(path, { throwIfNoEntry: false });
  if (!stat) return DEFAULT_LAYOUT;
  if (!stat.isFile() || stat.isSymbolicLink()) throw new Error(`web package layout must be a regular file: ${path}`);
  const layout = JSON.parse(readFileSync(path, "utf8"));
  const keys = Object.keys(DEFAULT_LAYOUT).sort();
  if (!layout || JSON.stringify(Object.keys(layout).sort()) !== JSON.stringify(keys)
      || layout.schema !== DEFAULT_LAYOUT.schema || typeof layout.requireSymbols !== "boolean") {
    throw new Error(`invalid web package layout: ${path}`);
  }
  for (const key of ["clientJs", "clientWasm", "primaryPack", "symbolMap", "linkedWasm"]) {
    safeRelativePath(layout[key], key);
  }
  for (const key of ["packManifestDir", "symbolDigest"]) {
    if (layout[key] !== null) safeRelativePath(layout[key], key);
  }
  for (const [key, extension] of [["clientJs", ".js"], ["clientWasm", ".wasm"],
    ["primaryPack", ".ntpack"], ["symbolMap", ".symbols"]]) {
    if (!layout[key].endsWith(extension)) throw new Error(`invalid web package ${key}`);
  }
  return layout;
}

export function publishWebRelease({ gameDir, identity, target, adapter, dependencies, runtimeBuild,
  releaseFiles, siteFiles, artifactDir, layout = loadWebPackageLayout(gameDir), outDir,
  verifyPackage, publisher, proof = "game", assetPacks = null }) {
  const compactDependencies = { sha256: sha256(jsonBytes(dependencies)), record: dependencies };
  const release = {
    schema: "ai_studio.game.release.v2",
    game: { id: identity.id, title: identity.title, storageNamespace: identity.storageNamespace },
    target, platformAdapter: adapter, entrypoint: "index.html",
    dependenciesSha256: compactDependencies.sha256, runtimeBuildFingerprint: runtimeBuild.fingerprint,
    build: { preset: "wasm-release", devapi: false, debug: false }, proof,
  };
  if (assetPacks) release.assetPacks = assetPacks;
  for (const path of [layout.clientJs, layout.clientWasm, layout.primaryPack]) {
    if (!siteFiles.some((file) => file.path === path && file.bytes?.length)) {
      throw new Error(`web package layout requires a nonempty site file: ${path}`);
    }
  }
  const entries = [...releaseFiles, { path: "release.json", bytes: jsonBytes(release) }];
  const zipBytes = createStoreZip(entries);
  const buildDir = dirname(resolve(artifactDir));
  const symbolMap = readWasmSymbols({
    symbolPath: join(buildDir, layout.symbolMap),
    linkedWasmPath: join(buildDir, layout.linkedWasm),
    packagedWasm: siteFiles.find((file) => file.path === layout.clientWasm)?.bytes,
    buildDir,
    ...(layout.symbolDigest ? { digestPath: join(buildDir, layout.symbolDigest) } : {}),
    required: layout.requireSymbols,
  });
  const outputDir = resolve(outDir || join(gameDir, "release", "artifacts"));
  rejectRedirectedDirectory(outputDir);
  mkdirSync(outputDir, { recursive: true });
  rejectRedirectedDirectory(outputDir);
  const baseName = `${identity.id}-${target}${identity.version ? `-v${identity.version}` : ""}-${sha256(zipBytes).slice(0, 16)}`;
  const zipName = `${baseName}.zip`;
  const manifestName = `${baseName}.manifest.json`;
  const symbolsName = `${baseName}.js.symbols`;
  const zipPath = join(outputDir, zipName);
  const manifestPath = join(outputDir, manifestName);
  const symbolsPath = symbolMap ? join(outputDir, symbolsName) : null;
  const tempDir = join(outputDir, `.package-${process.pid}-${randomUUID()}.tmp`);
  mkdirSync(tempDir);
  const tempZip = join(tempDir, zipName);
  const tempManifest = join(tempDir, manifestName);
  const tempSymbols = symbolMap ? join(tempDir, symbolsName) : null;
  try {
    writeFileSync(tempZip, zipBytes);
    if (symbolMap) writeFileSync(tempSymbols, symbolMap);
    const reopened = readStoreZip(readFileSync(tempZip));
    const manifest = {
      schema: "ai_studio.game.artifact_manifest.v2",
      game: release.game, target, platformAdapter: adapter,
      artifact: { file: basename(tempZip), size: zipBytes.length, sha256: sha256(zipBytes) },
      entrypoint: release.entrypoint, releaseMetadataSha256: sha256(reopened.get("release.json")),
      dependencies: compactDependencies, runtimeBuild,
      entries: [...reopened].map(([path, bytes]) => ({ path, size: bytes.length, sha256: sha256(bytes) })),
    };
    const manifestBytes = jsonBytes(manifest);
    writeFileSync(tempManifest, manifestBytes);
    verifyPackage({ zipPath: tempZip, manifestPath: tempManifest, expectedTarget: target });
    publishArtifactSet([
      { tempPath: tempManifest, finalPath: manifestPath, bytes: manifestBytes },
      ...(symbolMap ? [{ tempPath: tempSymbols, finalPath: symbolsPath, bytes: symbolMap }] : []),
      { tempPath: tempZip, finalPath: zipPath, bytes: zipBytes },
    ], publisher);
    return { zipPath, manifestPath, symbolsPath, manifest };
  } finally {
    rmSync(tempDir, { recursive: true, force: true });
  }
}
