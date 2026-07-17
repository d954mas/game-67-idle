#!/usr/bin/env node

import { createHash, randomUUID } from "node:crypto";
import {
  existsSync,
  linkSync,
  lstatSync,
  mkdirSync,
  readFileSync,
  realpathSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { basename, dirname, isAbsolute, join, relative, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { verifyWebPackage } from "./package_web.mjs";
import { findStudioRoot } from "./lib/studio_root.mjs";

const GAME_DIR = resolve(fileURLToPath(new URL("..", import.meta.url)));
const USAGE = "usage: node tools/portal_evidence.mjs --manifest release/artifacts/<artifact>.manifest.json";
const jsonBytes = (value) => Buffer.from(`${JSON.stringify(value, null, 2)}\n`, "utf8");
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");

function inside(root, path) {
  const rel = relative(root, path);
  return rel === "" || (!isAbsolute(rel) && rel !== ".." && !rel.startsWith(`..${process.platform === "win32" ? "\\" : "/"}`));
}

function assertNoSymlinks(root, path, allowMissing, label) {
  if (!inside(root, path)) throw new Error(`${label} is outside the game root`);
  let cursor = root;
  const rel = relative(root, path);
  for (const part of rel.split(/[\\/]/).filter(Boolean)) {
    cursor = join(cursor, part);
    if (!existsSync(cursor)) {
      if (allowMissing) return;
      throw new Error(`${label} is missing: ${cursor}`);
    }
    if (lstatSync(cursor).isSymbolicLink()) throw new Error(`${label} must not traverse a symbolic link: ${cursor}`);
  }
}

function readManifest(path) {
  try {
    const value = JSON.parse(readFileSync(path, "utf8").replace(/^\uFEFF/, ""));
    if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("must be an object");
    return value;
  } catch (error) {
    throw new Error(`artifact manifest is not valid JSON: ${error.message}`);
  }
}

function releaseLevels() {
  return [
    {
      id: "local-mock",
      status: "unverified",
      reason: "Package verification does not run or record a local mock simulator session.",
      requiredEvidence: "Record a separate game-owned local mock simulator run for this exact release when that proof is required.",
    },
    {
      id: "local-sdk-contract",
      status: "pass",
      reason: "The exact ZIP and sidecar passed the game-owned reopened package and platform SDK contract verifier.",
      evidence: "tools/package_web.mjs#verifyWebPackage",
    },
    {
      id: "public-inspector",
      status: "unverified",
      reason: "The offline reporter does not submit the artifact to a public portal inspector.",
      requiredEvidence: "Attach a public inspector result that identifies this exact ZIP SHA-256.",
    },
    {
      id: "credentialed-portal-smoke",
      status: "unverified",
      reason: "No portal credentials or authenticated portal access are read by the offline reporter.",
      requiredEvidence: "Attach a credentialed portal smoke result that identifies this exact ZIP SHA-256.",
    },
    {
      id: "production-certification",
      status: "unverified",
      reason: "Local package verification is not production portal certification.",
      requiredEvidence: "Attach the portal's production certification record for this exact ZIP SHA-256.",
    },
  ];
}

export function publishPortalEvidenceReport(reportPath, bytes, options = {}) {
  const parent = dirname(reportPath);
  const tempPath = join(resolve(options.tempDir || parent), `.portal-evidence-${process.pid}-${randomUUID()}.tmp`);
  try {
    writeFileSync(tempPath, bytes, { flag: "wx" });
    try {
      options.beforeLink?.();
      linkSync(tempPath, reportPath);
    } catch (error) {
      if (error?.code === "EEXIST" && existsSync(reportPath)
          && !lstatSync(reportPath).isSymbolicLink() && readFileSync(reportPath).equals(bytes)) return;
      if (error?.code === "EEXIST") throw new Error(`refusing to replace different portal evidence: ${reportPath}`);
      throw error;
    }
  } finally {
    rmSync(tempPath, { force: true });
  }
}

export function createPortalEvidence({
  gameDir = GAME_DIR,
  manifestPath,
  studioRoot = findStudioRoot(gameDir),
  beforePublish = () => {},
}) {
  // The game owner must run this local tool with exclusive ownership of the
  // game directory. We reject pre-existing links, traversal, and conflicting
  // publishers; hostile same-user replacement of owned ancestors mid-call is
  // outside the portable Node/Windows threat model.
  const requestedGameRoot = resolve(gameDir);
  if (!existsSync(requestedGameRoot) || !lstatSync(requestedGameRoot).isDirectory()) throw new Error("game root is missing");
  if (lstatSync(requestedGameRoot).isSymbolicLink()) throw new Error("game root must not be a symbolic link");
  const gameRoot = realpathSync(requestedGameRoot);
  const artifactsRoot = join(gameRoot, "release", "artifacts");
  const requestedManifest = resolve(manifestPath || "");
  assertNoSymlinks(gameRoot, requestedManifest, false, "artifact manifest");
  if (dirname(requestedManifest) !== artifactsRoot) throw new Error("artifact manifest must be a direct child of game-owned release/artifacts");
  if (realpathSync(dirname(requestedManifest)) !== artifactsRoot || realpathSync(requestedManifest) !== requestedManifest) {
    throw new Error("artifact manifest must remain inside the game root without symbolic links");
  }

  const preliminary = readManifest(requestedManifest);
  const zipName = preliminary.artifact?.file;
  if (typeof zipName !== "string" || basename(zipName) !== zipName || !zipName.endsWith(".zip")) {
    throw new Error("artifact manifest ZIP filename must be a safe sibling .zip name");
  }
  const zipPath = join(artifactsRoot, zipName);
  assertNoSymlinks(gameRoot, zipPath, false, "artifact ZIP");
  if (realpathSync(zipPath) !== zipPath) throw new Error("artifact ZIP must remain inside the game root without symbolic links");

  const zipBytesBeforeVerification = readFileSync(zipPath);
  const manifestBytesBeforeVerification = readFileSync(requestedManifest);
  const manifest = verifyWebPackage({
    zipPath,
    manifestPath: requestedManifest,
    expectedTarget: preliminary.target,
    studioRoot: resolve(studioRoot),
  });
  const zipBytes = readFileSync(zipPath);
  const manifestBytes = readFileSync(requestedManifest);
  if (!zipBytes.equals(zipBytesBeforeVerification) || !manifestBytes.equals(manifestBytesBeforeVerification)) {
    throw new Error("release ZIP or sidecar changed during portal evidence verification");
  }
  const zipHash = sha256(zipBytes);
  const report = {
    schema: "ai_studio.game.portal_evidence.v1",
    release: {
      game: manifest.game,
      target: manifest.target,
      platformAdapter: manifest.platformAdapter,
      zip: { file: basename(zipPath), size: zipBytes.length, sha256: zipHash },
      manifest: { file: basename(requestedManifest), size: manifestBytes.length, sha256: sha256(manifestBytes) },
    },
    levels: releaseLevels(),
  };

  const reportPath = join(gameRoot, ".ai_studio", "evidence", "releases", zipHash, "portal-evidence.json");
  const validateOutputPath = () => {
    assertNoSymlinks(gameRoot, reportPath, true, "portal evidence output");
    if (realpathSync(dirname(reportPath)) !== dirname(reportPath)) {
      throw new Error("portal evidence output escaped the game root through a symbolic link");
    }
  };
  assertNoSymlinks(gameRoot, dirname(reportPath), true, "portal evidence output");
  mkdirSync(dirname(reportPath), { recursive: true });
  beforePublish({ reportPath });
  validateOutputPath();
  publishPortalEvidenceReport(reportPath, jsonBytes(report), {
    tempDir: gameRoot,
    beforeLink: validateOutputPath,
  });
  return { reportPath, report };
}

function parseArgs(argv) {
  if (argv.length !== 2 || argv[0] !== "--manifest" || !argv[1]) throw new Error(USAGE);
  return { manifestPath: resolve(argv[1]) };
}

export function main(argv = process.argv.slice(2)) {
  try {
    const result = createPortalEvidence(parseArgs(argv));
    console.log(`portal evidence recorded: ${result.reportPath}`);
    return 0;
  } catch (error) {
    console.error(error?.message || String(error));
    return String(error?.message || "").startsWith("usage:") ? 2 : 1;
  }
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  process.exitCode = main();
}
