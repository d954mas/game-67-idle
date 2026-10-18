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
import { validateRuntimeBuildRecord } from "./lib/runtime_build.mjs";

const GAME_DIR = resolve(fileURLToPath(new URL("..", import.meta.url)));
const USAGE = "usage: node tools/portal_evidence.mjs --manifest release/artifacts/<artifact>.manifest.json [--local-mock-observation .ai_studio/evidence/local-mock/<observation>.json]";
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

function releaseLevels(localMockObservation = null) {
  return [
    localMockObservation ? {
      id: "local-mock",
      status: "pass",
      reason: "A game-owned browser observation matched this release runtime build fingerprint.",
      evidence: `local mock observation SHA-256 ${localMockObservation.sha256}`,
    } : {
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

function exactKeys(value, keys, label) {
  if (!value || typeof value !== "object" || Array.isArray(value)
      || JSON.stringify(Object.keys(value).sort()) !== JSON.stringify([...keys].sort())) {
    throw new Error(`${label} has unexpected fields`);
  }
}

function localHttp(raw) {
  try {
    const value = new URL(raw);
    return value.protocol === "http:" && ["127.0.0.1", "localhost", "::1", "[::1]"].includes(value.hostname.toLowerCase())
      && !value.username && !value.password;
  } catch { return false; }
}

function validateLocalMockObservation(value) {
  exactKeys(value, ["schema", "result", "observation"], "local mock observation");
  if (value.schema !== "ai_studio.runtime.local_mock_web_observation.v1" || value.result !== "pass") {
    throw new Error("local mock observation schema/result is invalid");
  }
  const observation = value.observation;
  exactKeys(observation, [
    "url", "finalUrl", "readyState", "config", "runtimeBuild", "compiledRuntimeBuildFingerprint", "overlay", "canvas",
    "lifecycleTranscript", "issues",
  ], "local mock browser observation");
  exactKeys(observation.config, ["target", "platformSdk", "release", "runtimeBuildFingerprint"], "local mock config");
  exactKeys(observation.overlay, ["present", "className", "display", "opacity", "progressPercent"], "local mock overlay");
  exactKeys(observation.canvas, ["width", "height"], "local mock canvas");
  const runtimeBuild = validateRuntimeBuildRecord(observation.runtimeBuild);
  if (!localHttp(observation.url) || !localHttp(observation.finalUrl) || observation.readyState !== "complete"
      || observation.config.target !== "local" || observation.config.platformSdk !== "mock" || observation.config.release !== true
      || observation.config.runtimeBuildFingerprint !== runtimeBuild.fingerprint
      || observation.compiledRuntimeBuildFingerprint !== runtimeBuild.fingerprint
      || observation.overlay.present !== true || observation.overlay.display !== "none" || observation.overlay.progressPercent !== 100
      || !(observation.canvas.width > 0) || !(observation.canvas.height > 0)
      || !Array.isArray(observation.issues) || observation.issues.length !== 0
      || !Array.isArray(observation.lifecycleTranscript)) {
    throw new Error("local mock browser observation failed its recorded runtime contract");
  }
  let finalProgress = -1;
  let finished = -1;
  for (let index = 0; index < observation.lifecycleTranscript.length; index += 1) {
    const event = observation.lifecycleTranscript[index];
    exactKeys(event, ["kind", "value", "source"], "local mock lifecycle event");
    if (event.kind === "progress" && event.source === "c-bridge" && event.value === 1 && finalProgress < 0) finalProgress = index;
    if (event.kind === "finished" && event.source === "c-bridge" && finished < 0) finished = index;
  }
  if (finalProgress < 0 || finished < finalProgress) throw new Error("local mock C lifecycle transcript is invalid");
  return value;
}

function readLocalMockObservation(gameRoot, requestedPath) {
  const path = resolve(requestedPath);
  const evidenceRoot = join(gameRoot, ".ai_studio", "evidence", "local-mock");
  assertNoSymlinks(gameRoot, path, false, "local mock observation");
  if (dirname(path) !== evidenceRoot || !basename(path).endsWith(".json")
      || realpathSync(dirname(path)) !== evidenceRoot || realpathSync(path) !== path) {
    throw new Error("local mock observation must be a direct game-owned .ai_studio/evidence/local-mock JSON file");
  }
  const bytes = readFileSync(path);
  let value;
  try { value = JSON.parse(bytes.toString("utf8").replace(/^\uFEFF/, "")); }
  catch (error) { throw new Error(`local mock observation is not valid JSON: ${error.message}`); }
  validateLocalMockObservation(value);
  return { path, bytes, value };
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
  localMockObservationPath = "",
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
  const localMock = localMockObservationPath ? readLocalMockObservation(gameRoot, localMockObservationPath) : null;
  if (localMock && !manifest.runtimeBuild) {
    throw new Error("legacy v1 releases have no runtime build witness for local mock attachment");
  }
  if (localMock && JSON.stringify(localMock.value.observation.runtimeBuild) !== JSON.stringify(manifest.runtimeBuild)) {
    throw new Error("local mock runtime build fingerprint does not match this exact release");
  }
  if (localMock && !readFileSync(localMock.path).equals(localMock.bytes)) {
    throw new Error("local mock observation changed during portal evidence verification");
  }
  const localMockSummary = localMock ? {
    file: basename(localMock.path),
    size: localMock.bytes.length,
    sha256: sha256(localMock.bytes),
    runtimeBuildFingerprint: localMock.value.observation.runtimeBuild.fingerprint,
  } : null;
  const validateLocalMockInput = () => {
    if (!localMock) return;
    assertNoSymlinks(gameRoot, localMock.path, false, "local mock observation");
    if (realpathSync(localMock.path) !== localMock.path || !readFileSync(localMock.path).equals(localMock.bytes)) {
      throw new Error("local mock observation changed during portal evidence verification");
    }
  };
  const report = {
    schema: manifest.runtimeBuild ? "ai_studio.game.portal_evidence.v2" : "ai_studio.game.portal_evidence.v1",
    release: {
      game: manifest.game,
      target: manifest.target,
      platformAdapter: manifest.platformAdapter,
      zip: { file: basename(zipPath), size: zipBytes.length, sha256: zipHash },
      manifest: { file: basename(requestedManifest), size: manifestBytes.length, sha256: sha256(manifestBytes) },
      ...(manifest.runtimeBuild ? { runtimeBuildFingerprint: manifest.runtimeBuild.fingerprint } : {}),
    },
    ...(localMockSummary ? { localMockObservation: localMockSummary } : {}),
    levels: releaseLevels(localMockSummary),
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
  validateLocalMockInput();
  publishPortalEvidenceReport(reportPath, jsonBytes(report), {
    tempDir: gameRoot,
    beforeLink() {
      validateOutputPath();
      validateLocalMockInput();
    },
  });
  return { reportPath, report };
}

function parseArgs(argv) {
  const result = {};
  if (argv.length < 2 || argv.length > 4 || argv.length % 2 !== 0) throw new Error(USAGE);
  for (let index = 0; index < argv.length; index += 2) {
    const flag = argv[index];
    const value = argv[index + 1];
    if (!value || (flag !== "--manifest" && flag !== "--local-mock-observation") || Object.hasOwn(result, flag)) throw new Error(USAGE);
    result[flag] = resolve(value);
  }
  if (!result["--manifest"]) throw new Error(USAGE);
  return {
    manifestPath: result["--manifest"],
    ...(result["--local-mock-observation"] ? { localMockObservationPath: result["--local-mock-observation"] } : {}),
  };
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
