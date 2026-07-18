import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import {
  cpSync,
  existsSync,
  lstatSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawn, spawnSync } from "node:child_process";
import test from "node:test";

import { packageWebArtifact } from "./package_web.mjs";
import { createPortalEvidence, publishPortalEvidenceReport } from "./portal_evidence.mjs";
import { findStudioRoot } from "./lib/studio_root.mjs";

const gameModuleRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const studioRoot = findStudioRoot(gameModuleRoot);
const reporterScript = resolve(fileURLToPath(new URL("portal_evidence.mjs", import.meta.url)));
const RELEASE_WASM = Buffer.from([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
  0x01, 0x04, 0x01, 0x60, 0x00, 0x00,
  0x03, 0x02, 0x01, 0x00,
  0x05, 0x03, 0x01, 0x00, 0x01,
  0x07, 0x10, 0x02,
  0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00,
  0x03, 0x72, 0x75, 0x6e, 0x00, 0x00,
  0x0a, 0x04, 0x01, 0x02, 0x00, 0x0b,
]);

function runtimeBuildRecord() {
  const inputs = [
    { id: "game", source: ".", files: 3, sha256: "1".repeat(64) },
    { id: "engine", source: "external/neotolis-engine", files: 5, sha256: "2".repeat(64) },
    { id: "feature:platform-sdk", source: "features/platform-sdk", files: 7, sha256: "3".repeat(64) },
  ];
  return {
    schema: "ai_studio.runtime_build.v1",
    fingerprint: createHash("sha256").update(JSON.stringify(inputs)).digest("hex"),
    inputs,
  };
}

function runtimeBoundWasm(record) {
  const name = Buffer.from("runtime_build", "ascii");
  const marker = Buffer.from(`ai_studio.runtime_build:${record.fingerprint}`, "ascii");
  const payloadSize = 1 + name.length + marker.length;
  assert.ok(payloadSize < 128);
  return Buffer.concat([RELEASE_WASM, Buffer.from([0, payloadSize, name.length]), name, marker]);
}

function write(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, value);
}

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "portal-evidence-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const gameDir = join(root, "games", "evidence-game");
  const artifactDir = join(gameDir, "build", "wasm-release-itch", "bin");
  const runtimeBuild = runtimeBuildRecord();
  write(join(artifactDir, "index.html"), `<!doctype html><script>window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true, runtimeBuildFingerprint: '${runtimeBuild.fingerprint}' });</script><script src='game.js'></script>\n`);
  write(join(artifactDir, "game.js"), [
    "var wasmBinaryFile;",
    "function findWasmBinary() { return locateFile('game.wasm'); }",
    "async function instantiateAsync(binaryFile, imports) {",
    "  const response = fetch(binaryFile);",
    "  return WebAssembly.instantiateStreaming(response, imports);",
    "}",
    "async function createWasm() {",
    "  wasmBinaryFile ??= findWasmBinary();",
    "  return instantiateAsync(wasmBinaryFile, {});",
    "}",
    "createWasm();",
    "",
  ].join("\n"));
  write(join(artifactDir, "game.wasm"), runtimeBoundWasm(runtimeBuild));
  write(join(artifactDir, "assets", "game.ntpack"), Buffer.from("pack"));
  write(join(artifactDir, "runtime-build.json"), `${JSON.stringify(runtimeBuild, null, 2)}\n`);
  for (const [from, to] of [
    ["platform-sdk.js", "platform-sdk.js"],
    ["platform-sdk-core.js", "platform-sdk-core.js"],
    ["adapters/mock.js", "platform-sdk-adapter.js"],
  ]) cpSync(join(studioRoot, "features", "platform-sdk", "web", from), join(artifactDir, to));
  const packaged = packageWebArtifact({
    gameDir,
    artifactDir,
    target: "itch",
    studioRoot,
    identity: { schema: "ai_studio.game.v1", id: "evidence-game", title: "Evidence Game", storageNamespace: "evidence-game" },
    dependencies: {
      schema: "ai_studio.game.dependencies.v2",
      engine: { source: "external/neotolis-engine", version: "0.1.0", revision: "1".repeat(40), compatibility: "tested" },
      features: [{ id: "platform-sdk", source: "features/platform-sdk", version: "1.1.0", revision: "2".repeat(40), compatibility: "tested" }],
      compatibility: "evidence fixture",
    },
    dependencyVerifier: () => {},
    runtimeBuildVerifier: () => runtimeBuild,
  });
  return { root, gameDir, runtimeBuild, ...packaged };
}

function writeLocalMockObservation(item, record = item.runtimeBuild) {
  const path = join(item.gameDir, ".ai_studio", "evidence", "local-mock", `${record.fingerprint}.json`);
  write(path, `${JSON.stringify({
    schema: "ai_studio.runtime.local_mock_web_observation.v1",
    result: "pass",
    observation: {
      url: "http://127.0.0.1:8092/",
      finalUrl: "http://127.0.0.1:8092/",
      readyState: "complete",
      config: {
        target: "local", platformSdk: "mock", release: true,
        runtimeBuildFingerprint: record.fingerprint,
      },
      runtimeBuild: record,
      compiledRuntimeBuildFingerprint: record.fingerprint,
      overlay: { present: true, className: "is-hidden", display: "none", opacity: "0", progressPercent: 100 },
      canvas: { width: 1280, height: 720 },
      lifecycleTranscript: [
        { kind: "progress", value: 1, source: "c-bridge" },
        { kind: "finished", value: null, source: "c-bridge" },
      ],
      issues: [],
    },
  }, null, 2)}\n`);
  return path;
}

function concurrentPublisher(reportPath, text) {
  const source = [
    `import { publishPortalEvidenceReport } from ${JSON.stringify(pathToFileURL(reporterScript).href)};`,
    `publishPortalEvidenceReport(${JSON.stringify(reportPath)}, Buffer.from(${JSON.stringify(text)}));`,
  ].join("\n");
  return new Promise((resolveResult) => {
    const child = spawn(process.execPath, ["--input-type=module", "--eval", source], { stdio: ["ignore", "pipe", "pipe"] });
    let stderr = "";
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    child.on("close", (status) => resolveResult({ status, stderr }));
  });
}

test("offline report binds the exact verified release and keeps five evidence levels distinct", (t) => {
  const item = fixture(t);
  const zipCount = readdirSync(dirname(item.zipPath)).filter((name) => name.endsWith(".zip")).length;
  const secret = "must-not-enter-portal-evidence";
  process.env.PORTAL_TEST_CREDENTIAL = secret;
  t.after(() => { delete process.env.PORTAL_TEST_CREDENTIAL; });

  const one = createPortalEvidence({ gameDir: item.gameDir, manifestPath: item.manifestPath, studioRoot });
  const bytesOne = readFileSync(one.reportPath);
  const two = createPortalEvidence({ gameDir: item.gameDir, manifestPath: item.manifestPath, studioRoot });
  const bytesTwo = readFileSync(two.reportPath);
  const report = JSON.parse(bytesOne);
  const zipBytes = readFileSync(item.zipPath);
  const manifestBytes = readFileSync(item.manifestPath);
  const zipSha256 = createHash("sha256").update(zipBytes).digest("hex");

  assert.equal(one.reportPath, join(item.gameDir, ".ai_studio", "evidence", "releases", zipSha256, "portal-evidence.json"));
  assert.deepEqual(bytesOne, bytesTwo);
  assert.equal(JSON.stringify(report).includes(secret), false);
  assert.equal(report.schema, "ai_studio.game.portal_evidence.v2");
  assert.deepEqual(report.release.game, { id: "evidence-game", title: "Evidence Game", storageNamespace: "evidence-game" });
  assert.equal(report.release.target, "itch");
  assert.equal(report.release.platformAdapter, "mock");
  assert.deepEqual(report.release.zip, {
    file: item.manifest.artifact.file,
    size: zipBytes.length,
    sha256: zipSha256,
  });
  assert.deepEqual(report.release.manifest, {
    file: item.manifestPath.split(/[\\/]/).at(-1),
    size: manifestBytes.length,
    sha256: createHash("sha256").update(manifestBytes).digest("hex"),
  });
  assert.deepEqual(report.levels.map((level) => level.id), [
    "local-mock",
    "local-sdk-contract",
    "public-inspector",
    "credentialed-portal-smoke",
    "production-certification",
  ]);
  assert.deepEqual(report.levels.map((level) => level.status), ["unverified", "pass", "unverified", "unverified", "unverified"]);
  for (const level of report.levels.filter((entry) => entry.status === "unverified")) {
    assert.ok(level.reason);
    assert.ok(level.requiredEvidence);
  }
  assert.equal(Object.hasOwn(report, "timestamp"), false);
  assert.equal(readdirSync(dirname(item.zipPath)).filter((name) => name.endsWith(".zip")).length, zipCount);
});

test("matching game-owned local mock observation upgrades only that exact release level", (t) => {
  const item = fixture(t);
  const observationPath = writeLocalMockObservation(item);
  const result = createPortalEvidence({
    gameDir: item.gameDir,
    manifestPath: item.manifestPath,
    studioRoot,
    localMockObservationPath: observationPath,
  });
  const report = JSON.parse(readFileSync(result.reportPath, "utf8"));
  assert.deepEqual(report.levels.map((level) => level.status), ["pass", "pass", "unverified", "unverified", "unverified"]);
  assert.equal(report.release.runtimeBuildFingerprint, item.runtimeBuild.fingerprint);
  assert.equal(report.localMockObservation.sha256, createHash("sha256").update(readFileSync(observationPath)).digest("hex"));
  assert.equal(report.localMockObservation.runtimeBuildFingerprint, item.runtimeBuild.fingerprint);

  const mismatch = fixture(t);
  const other = runtimeBuildRecord();
  other.inputs[0] = { ...other.inputs[0], sha256: "9".repeat(64) };
  other.fingerprint = createHash("sha256").update(JSON.stringify(other.inputs)).digest("hex");
  const mismatchPath = writeLocalMockObservation(mismatch, other);
  assert.throws(() => createPortalEvidence({
    gameDir: mismatch.gameDir,
    manifestPath: mismatch.manifestPath,
    studioRoot,
    localMockObservationPath: mismatchPath,
  }), /runtime build fingerprint.*exact release/i);
});

test("local mock observation cannot change between validation and report publication", (t) => {
  const item = fixture(t);
  const observationPath = writeLocalMockObservation(item);
  assert.throws(() => createPortalEvidence({
    gameDir: item.gameDir,
    manifestPath: item.manifestPath,
    studioRoot,
    localMockObservationPath: observationPath,
    beforePublish() { writeFileSync(observationPath, "{}\n"); },
  }), /local mock observation changed/i);
});

test("copied game-owned reporter records the exact package from a standalone games/<id> layout", (t) => {
  const item = fixture(t);
  const toolsDir = join(item.gameDir, "tools");
  mkdirSync(join(toolsDir, "lib"), { recursive: true });
  for (const rel of ["portal_evidence.mjs", "package_web.mjs", "lib/studio_root.mjs", "lib/zip_store.mjs", "lib/runtime_build.mjs"]) {
    cpSync(join(dirname(reporterScript), ...rel.split("/")), join(toolsDir, ...rel.split("/")));
  }
  cpSync(join(studioRoot, "features", "platform-sdk"), join(item.root, "features", "platform-sdk"), { recursive: true });
  mkdirSync(join(item.root, "external", "neotolis-engine"), { recursive: true });
  const result = spawnSync(process.execPath, ["tools/portal_evidence.mjs", "--manifest", `release/artifacts/${item.manifestPath.split(/[\\/]/).at(-1)}`], {
    cwd: item.gameDir,
    encoding: "utf8",
  });

  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /portal evidence recorded:/i);
  const reportPath = join(item.gameDir, ".ai_studio", "evidence", "releases", item.manifest.artifact.sha256, "portal-evidence.json");
  const report = JSON.parse(readFileSync(reportPath, "utf8"));
  assert.deepEqual(report.levels.map((level) => level.status), ["unverified", "pass", "unverified", "unverified", "unverified"]);
  assert.equal(readdirSync(dirname(item.zipPath)).filter((name) => name.endsWith(".zip")).length, 1);
});

test("corrupt, mismatched, invalid, and out-of-root inputs fail before writing evidence", (t) => {
  const cases = [
    {
      name: "corrupt ZIP",
      mutate(item) {
        const bytes = Buffer.from(readFileSync(item.zipPath));
        bytes[bytes.length - 1] ^= 1;
        writeFileSync(item.zipPath, bytes);
        return item.manifestPath;
      },
      pattern: /CRC|canonical|hash|ZIP/i,
    },
    {
      name: "sidecar mismatch",
      mutate(item) {
        const manifest = JSON.parse(readFileSync(item.manifestPath, "utf8"));
        manifest.artifact.sha256 = "0".repeat(64);
        writeFileSync(item.manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);
        return item.manifestPath;
      },
      pattern: /hash|mismatch/i,
    },
    {
      name: "invalid manifest",
      mutate(item) {
        writeFileSync(item.manifestPath, "not json\n");
        return item.manifestPath;
      },
      pattern: /JSON/i,
    },
    {
      name: "out-of-root manifest",
      mutate(item) {
        const outside = join(item.root, "outside.manifest.json");
        cpSync(item.manifestPath, outside);
        return outside;
      },
      pattern: /release\/artifacts|game root|outside/i,
    },
  ];
  for (const itemCase of cases) {
    const item = fixture(t);
    const manifestPath = itemCase.mutate(item);
    assert.throws(
      () => createPortalEvidence({ gameDir: item.gameDir, manifestPath, studioRoot }),
      itemCase.pattern,
      itemCase.name,
    );
    assert.equal(existsSync(join(item.gameDir, ".ai_studio", "evidence")), false, itemCase.name);
  }
});

test("symlinked release input is rejected and cannot redirect the game-owned report", (t) => {
  const item = fixture(t);
  const aliasGame = join(item.root, "games", "alias-game");
  mkdirSync(aliasGame, { recursive: true });
  const link = join(aliasGame, "release");
  try {
    symlinkSync(join(item.gameDir, "release"), link, process.platform === "win32" ? "junction" : "dir");
  } catch (error) {
    if (["EPERM", "EACCES", "ENOTSUP"].includes(error.code)) return t.skip(`symlink unavailable: ${error.code}`);
    throw error;
  }
  assert.throws(
    () => createPortalEvidence({ gameDir: aliasGame, manifestPath: join(link, "artifacts", item.manifestPath.split(/[\\/]/).at(-1)), studioRoot }),
    /symbolic link|symlink|game root|release\/artifacts/i,
  );
  assert.equal(existsSync(join(aliasGame, ".ai_studio")), false);
});

test("symlinked evidence output cannot redirect a report outside its game root", (t) => {
  const item = fixture(t);
  const outside = join(item.root, "outside-evidence");
  mkdirSync(outside);
  const link = join(item.gameDir, ".ai_studio");
  try {
    symlinkSync(outside, link, process.platform === "win32" ? "junction" : "dir");
  } catch (error) {
    if (["EPERM", "EACCES", "ENOTSUP"].includes(error.code)) return t.skip(`symlink unavailable: ${error.code}`);
    throw error;
  }
  assert.throws(
    () => createPortalEvidence({ gameDir: item.gameDir, manifestPath: item.manifestPath, studioRoot }),
    /symbolic link|symlink|escaped/i,
  );
  assert.deepEqual(readdirSync(outside), []);
});

test("a link introduced before the final publication check fails without an outside write", (t) => {
  const item = fixture(t);
  const outside = join(item.root, "race-outside");
  mkdirSync(outside);
  writeFileSync(join(outside, "sentinel.txt"), "owned outside state\n");
  const outsideMtime = lstatSync(outside, { bigint: true }).mtimeNs;
  let linkUnavailable = null;
  let thrown = null;
  try {
    createPortalEvidence({
      gameDir: item.gameDir,
      manifestPath: item.manifestPath,
      studioRoot,
      beforePublish({ reportPath }) {
        const parent = dirname(reportPath);
        rmSync(parent, { recursive: true, force: true });
        try {
          symlinkSync(outside, parent, process.platform === "win32" ? "junction" : "dir");
        } catch (error) {
          if (["EPERM", "EACCES", "ENOTSUP"].includes(error.code)) {
            linkUnavailable = error;
            throw new Error(`symlink unavailable: ${error.code}`);
          }
          throw error;
        }
      },
    });
  } catch (error) {
    thrown = error;
  }
  if (linkUnavailable) return t.skip(`symlink unavailable: ${linkUnavailable.code}`);
  assert.match(thrown?.message || "", /symbolic link|symlink|escaped/i);
  assert.deepEqual(readdirSync(outside), ["sentinel.txt"]);
  assert.equal(lstatSync(outside, { bigint: true }).mtimeNs, outsideMtime);
});

test("concurrent conflicting publishers create once and never overwrite the winner", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "portal-publish-race-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const reportPath = join(root, "portal-evidence.json");
  const contenders = ["first report\n", "second report\n"];

  const results = await Promise.all(contenders.map((text) => concurrentPublisher(reportPath, text)));
  assert.deepEqual(results.map((result) => result.status).toSorted(), [0, 1]);
  assert.match(results.find((result) => result.status === 1).stderr, /different portal evidence/i);
  const winner = readFileSync(reportPath, "utf8");
  assert.ok(contenders.includes(winner));
  assert.throws(() => publishPortalEvidenceReport(reportPath, Buffer.from("third report\n")), /different portal evidence/i);
  assert.equal(readFileSync(reportPath, "utf8"), winner);
  assert.deepEqual(readdirSync(root), ["portal-evidence.json"]);
});

test("reporter CLI rejects missing arguments with usage exit 2", () => {
  const result = spawnSync(process.execPath, [reporterScript], { encoding: "utf8" });
  assert.equal(result.status, 2);
  assert.match(result.stderr, /^usage: node tools\/portal_evidence\.mjs/m);
});
