import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import {
  cpSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  readdirSync,
  renameSync,
  rmSync,
  symlinkSync,
  writeFileSync,
} from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

import {
  packageWebArtifact,
  validateWebArtifact,
  verifyDependencySources,
  verifyWebPackage,
} from "./package_web.mjs";
import { createStoreZip, readStoreZip } from "./lib/zip_store.mjs";
import { findStudioRoot } from "./lib/studio_root.mjs";

const gameModuleRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const studioRoot = findStudioRoot(gameModuleRoot);
const sha256 = (bytes) => createHash("sha256").update(bytes).digest("hex");

function runtimeBuildRecord() {
  const inputs = [
    { id: "game", source: ".", files: 3, sha256: "1".repeat(64) },
    { id: "engine", source: "external/neotolis-engine", files: 5, sha256: "2".repeat(64) },
    { id: "feature:platform-sdk", source: "features/platform-sdk", files: 7, sha256: "3".repeat(64) },
  ];
  return {
    schema: "ai_studio.runtime_build.v1",
    fingerprint: sha256(Buffer.from(JSON.stringify(inputs))),
    inputs,
  };
}

function runtimeBoundWasm(record, base = RELEASE_WASM) {
  const name = Buffer.from("runtime_build", "ascii");
  const marker = Buffer.from(`ai_studio.runtime_build:${record.fingerprint}`, "ascii");
  const payloadSize = 1 + name.length + marker.length;
  assert.ok(payloadSize < 128);
  return Buffer.concat([base, Buffer.from([0, payloadSize, name.length]), name, marker]);
}

test("standalone template ZIP helper matches the canonical Studio helper", () => {
  const canonical = readFileSync(join(studioRoot, "ai_studio", "core_harness", "tool_lib", "zip_store.mjs"));
  const distributionCopy = readFileSync(join(studioRoot, "templates", "template", "tools", "lib", "zip_store.mjs"));
  assert.equal(distributionCopy.equals(canonical), true);
});
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
const MEMORY_ONLY_WASM = Buffer.from([
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
  0x05, 0x03, 0x01, 0x00, 0x01,
  0x07, 0x0a, 0x01, 0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 0x00,
]);

function write(path, value) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, value);
}

function wasmCustomSection(name, payload = "") {
  const nameBytes = Buffer.from(name);
  const payloadBytes = Buffer.from(payload);
  const sectionSize = 1 + nameBytes.length + payloadBytes.length;
  assert.ok(nameBytes.length < 128 && sectionSize < 128);
  return Buffer.concat([
    Buffer.from([0, 97, 115, 109, 1, 0, 0, 0]),
    Buffer.from([0, sectionSize, nameBytes.length]),
    nameBytes,
    payloadBytes,
  ]);
}

function fixture(t, target = "itch") {
  const root = mkdtempSync(join(tmpdir(), "game-package-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const gameDir = join(root, "games", "test-game");
  const artifactDir = join(gameDir, "build", target === "local" ? "wasm-release" : `wasm-release-${target}`, "bin");
  const adapter = target === "local" || target === "itch" ? "mock" : target;
  const runtimeBuild = runtimeBuildRecord();
  write(join(gameDir, "game.json"), `${JSON.stringify({
    schema: "ai_studio.game.v1", id: "test-game", title: "Test Game", storageNamespace: "test-game",
  }, null, 2)}\n`);
  write(join(gameDir, "dependencies.json"), `${JSON.stringify({
    schema: "ai_studio.game.dependencies.v2",
    engine: {
      source: "external/neotolis-engine", version: "0.1.0",
      revision: "1".repeat(40), compatibility: "tested",
    },
    features: [{
      id: "platform-sdk", source: "features/platform-sdk", version: "1.1.0",
      revision: "2".repeat(40), compatibility: "tested",
    }],
    compatibility: "tested game checkout",
  }, null, 2)}\n`);
  write(join(artifactDir, "index.html"), [
    "<!doctype html><script>",
    `window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: '${target}', platformSdk: '${adapter}', release: true, runtimeBuildFingerprint: '${runtimeBuild.fingerprint}' });`,
    "</script><script type=\"module\">import './platform-sdk.js';</script><script src=\"game.js\"></script>",
  ].join("\n"));
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
    [`adapters/${adapter}.js`, "platform-sdk-adapter.js"],
  ]) {
    cpSync(join(studioRoot, "features", "platform-sdk", "web", from), join(artifactDir, to));
  }
  return {
    root, gameDir, artifactDir, target, adapter, runtimeBuild,
    dependencyVerifier: () => {},
    runtimeBuildVerifier: () => runtimeBuild,
  };
}

test("final package is deterministic and binds the reopened ZIP to exact dependency revisions", (t) => {
  const item = fixture(t);
  const one = packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "one") });
  const two = packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "two") });

  assert.deepEqual(readFileSync(one.zipPath), readFileSync(two.zipPath));
  const manifestOne = JSON.parse(readFileSync(one.manifestPath, "utf8"));
  const manifestTwo = JSON.parse(readFileSync(two.manifestPath, "utf8"));
  assert.deepEqual(manifestOne, manifestTwo);
  assert.equal(manifestOne.schema, "ai_studio.game.artifact_manifest.v2");
  assert.equal(manifestOne.target, "itch");
  assert.equal(manifestOne.platformAdapter, "mock");
  assert.equal(manifestOne.dependencies.record.engine.revision, "1".repeat(40));
  assert.equal(manifestOne.dependencies.record.features[0].revision, "2".repeat(40));
  assert.deepEqual(manifestOne.runtimeBuild, item.runtimeBuild);
  assert.match(manifestOne.artifact.sha256, /^[0-9a-f]{64}$/);
  assert.doesNotMatch(JSON.stringify(manifestOne), /game-package-|[A-Z]:\\|\/tmp\//i);
  assert.equal(Object.hasOwn(manifestOne, "timestamp"), false);
  assert.deepEqual(verifyWebPackage({ zipPath: one.zipPath, manifestPath: one.manifestPath, expectedTarget: "itch" }), manifestOne);

  const zip = readStoreZip(readFileSync(one.zipPath));
  assert.deepEqual([...zip.keys()], [...zip.keys()].toSorted());
  assert.ok(zip.has("release.json"));
  assert.deepEqual(JSON.parse(zip.get("runtime-build.json").toString("utf8")), item.runtimeBuild);
  assert.equal(JSON.parse(zip.get("release.json").toString("utf8")).dependenciesSha256.length, 64);
  assert.equal(JSON.parse(zip.get("release.json").toString("utf8")).runtimeBuildFingerprint, item.runtimeBuild.fingerprint);
});

test("upgraded verifier retains read compatibility with pre-fingerprint v1 packages", (t) => {
  const item = fixture(t);
  const current = packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "current") });
  const entries = readStoreZip(readFileSync(current.zipPath));
  entries.delete("runtime-build.json");
  entries.set("index.html", Buffer.from(entries.get("index.html").toString("utf8")
    .replace(/, runtimeBuildFingerprint: '[0-9a-f]{64}'/, "")));
  const release = JSON.parse(entries.get("release.json").toString("utf8"));
  release.schema = "ai_studio.game.release.v1";
  delete release.runtimeBuildFingerprint;
  entries.set("release.json", Buffer.from(`${JSON.stringify(release, null, 2)}\n`));
  const zipBytes = createStoreZip([...entries].map(([path, bytes]) => ({ path, bytes })));
  const zipPath = join(item.root, "legacy.zip");
  const manifestPath = join(item.root, "legacy.manifest.json");
  writeFileSync(zipPath, zipBytes);
  const manifest = JSON.parse(readFileSync(current.manifestPath, "utf8"));
  manifest.schema = "ai_studio.game.artifact_manifest.v1";
  delete manifest.runtimeBuild;
  manifest.artifact = { file: "legacy.zip", size: zipBytes.length, sha256: sha256(zipBytes) };
  manifest.releaseMetadataSha256 = sha256(entries.get("release.json"));
  manifest.entries = [...readStoreZip(zipBytes)].map(([path, bytes]) => ({ path, size: bytes.length, sha256: sha256(bytes) }));
  writeFileSync(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);

  assert.deepEqual(verifyWebPackage({ zipPath, manifestPath, expectedTarget: "itch", studioRoot }), manifest);
});

test("artifact and reopened ZIP fail closed when runtime build bindings drift", (t) => {
  const htmlDrift = fixture(t);
  const htmlPath = join(htmlDrift.artifactDir, "index.html");
  write(htmlPath, readFileSync(htmlPath, "utf8").replace(htmlDrift.runtimeBuild.fingerprint, "0".repeat(64)));
  assert.throws(
    () => packageWebArtifact({ ...htmlDrift, studioRoot, outDir: join(htmlDrift.root, "html-drift") }),
    /runtime build fingerprint/i,
  );

  const recordDrift = fixture(t);
  write(join(recordDrift.artifactDir, "runtime-build.json"), `${JSON.stringify({
    ...recordDrift.runtimeBuild,
    fingerprint: "0".repeat(64),
  }, null, 2)}\n`);
  assert.throws(
    () => packageWebArtifact({ ...recordDrift, studioRoot, outDir: join(recordDrift.root, "record-drift") }),
    /runtime build fingerprint/i,
  );

  const staleWasm = fixture(t);
  write(join(staleWasm.artifactDir, "game.wasm"), RELEASE_WASM);
  assert.throws(
    () => packageWebArtifact({ ...staleWasm, studioRoot, outDir: join(staleWasm.root, "stale-wasm") }),
    /compiled runtime build fingerprint witness/i,
  );
});

test("reopened ZIP rejects CRC corruption and sidecar target mismatch", (t) => {
  const item = fixture(t);
  const result = packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "release") });
  const corrupted = Buffer.from(readFileSync(result.zipPath));
  const marker = corrupted.indexOf(Buffer.from("instantiateStreaming"));
  assert.notEqual(marker, -1);
  corrupted[marker] ^= 1;
  writeFileSync(result.zipPath, corrupted);
  assert.throws(
    () => verifyWebPackage({ zipPath: result.zipPath, manifestPath: result.manifestPath, expectedTarget: "itch" }),
    /CRC|hash/i,
  );

  const clean = packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "clean") });
  assert.throws(
    () => verifyWebPackage({ zipPath: clean.zipPath, manifestPath: clean.manifestPath, expectedTarget: "poki" }),
    /target mismatch/i,
  );
});

test("reopened verification re-derives the portal allowlist and dependency record instead of trusting the sidecar", (t) => {
  const item = fixture(t);
  const result = packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "release") });
  const manifest = JSON.parse(readFileSync(result.manifestPath, "utf8"));
  const entries = readStoreZip(readFileSync(result.zipPath));
  entries.set("unexpected.txt", Buffer.from("hidden"));
  const repacked = createStoreZip([...entries].map(([path, bytes]) => ({ path, bytes })));
  writeFileSync(result.zipPath, repacked);
  manifest.artifact.size = repacked.length;
  manifest.artifact.sha256 = sha256(repacked);
  manifest.entries = [...readStoreZip(repacked)].map(([path, bytes]) => ({ path, size: bytes.length, sha256: sha256(bytes) }));
  writeFileSync(result.manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);
  assert.throws(
    () => verifyWebPackage({ zipPath: result.zipPath, manifestPath: result.manifestPath, expectedTarget: "itch", studioRoot }),
    /allowlist/i,
  );

  const clean = packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "clean") });
  const badDependencies = JSON.parse(readFileSync(clean.manifestPath, "utf8"));
  badDependencies.dependencies.record.engine.revision = "3".repeat(40);
  writeFileSync(clean.manifestPath, `${JSON.stringify(badDependencies, null, 2)}\n`);
  assert.throws(
    () => verifyWebPackage({ zipPath: clean.zipPath, manifestPath: clean.manifestPath, expectedTarget: "itch", studioRoot }),
    /dependency record hash mismatch/i,
  );
});

test("artifact validation fails closed on missing, case-drifted, unexpected, source, debug, and DevAPI payloads", (t) => {
  const cases = [
    ["missing asset", (item) => rmSync(join(item.artifactDir, "assets", "game.ntpack"), { force: true }), /missing required file.*assets\/game\.ntpack/i],
    ["case drift", (item) => {
      const from = join(item.artifactDir, "game.wasm");
      const bytes = readFileSync(from);
      rmSync(from);
      write(join(item.artifactDir, "Game.wasm"), bytes);
    }, /case mismatch.*game\.wasm/i],
    ["unexpected", (item) => write(join(item.artifactDir, "extra.txt"), "extra"), /unexpected file.*extra\.txt/i],
    ["source", (item) => write(join(item.artifactDir, "secret.c"), "source"), /source-only|unexpected file/i],
    ["debug", (item) => write(join(item.artifactDir, "platform-sdk-debug-ui.js"), "debug_test"), /debug|unexpected file/i],
    ["devapi", (item) => {
      const path = join(item.artifactDir, "game.js");
      write(path, `${readFileSync(path, "utf8")}\nwindow.__devapi = {};\n`);
    }, /DevAPI/i],
  ];
  for (const [name, mutate, expected] of cases) {
    const item = fixture(t);
    mutate(item);
    assert.throws(() => validateWebArtifact({ ...item, studioRoot }), expected, name);
  }
});

test("target and adapter must match both release metadata and selected platform source", (t) => {
  const item = fixture(t, "poki");
  write(join(item.artifactDir, "index.html"), readFileSync(join(item.artifactDir, "index.html"), "utf8").replace("target: 'poki'", "target: 'yandex'"));
  assert.throws(() => validateWebArtifact({ ...item, studioRoot }), /target mismatch/i);

  const adapterMismatch = fixture(t, "poki");
  cpSync(
    join(studioRoot, "features", "platform-sdk", "web", "adapters", "mock.js"),
    join(adapterMismatch.artifactDir, "platform-sdk-adapter.js"),
  );
  assert.throws(() => validateWebArtifact({ ...adapterMismatch, studioRoot }), /adapter mismatch/i);
});

test("platform wrapper and core bytes must match the exact verified feature source", (t) => {
  for (const file of ["platform-sdk.js", "platform-sdk-core.js"]) {
    const item = fixture(t);
    write(join(item.artifactDir, file), "// substituted release wrapper\n");
    assert.throws(() => validateWebArtifact({ ...item, studioRoot }), /platform.*mismatch/i, file);
  }
});

test("release config requires one precise Object.freeze assignment and ignores no comment decoys", (t) => {
  const duplicate = fixture(t);
  const duplicatePath = join(duplicate.artifactDir, "index.html");
  const duplicateHtml = readFileSync(duplicatePath, "utf8");
  write(duplicatePath, `${duplicateHtml}\n<script>window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true, runtimeBuildFingerprint: '${duplicate.runtimeBuild.fingerprint}' });</script>`);
  assert.throws(() => validateWebArtifact({ ...duplicate, studioRoot }), /exactly one.*PLATFORM_SDK_CONFIG/i);

  const decoy = fixture(t);
  const decoyPath = join(decoy.artifactDir, "index.html");
  const wrongActual = readFileSync(decoyPath, "utf8").replace("target: 'itch'", "target: 'poki'");
  write(decoyPath, `<!-- window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true }); -->\n${wrongActual}`);
  assert.throws(() => validateWebArtifact({ ...decoy, studioRoot }), /exactly one.*PLATFORM_SDK_CONFIG|target mismatch/i);

  const commentOnly = fixture(t);
  const commentPath = join(commentOnly.artifactDir, "index.html");
  write(commentPath, "<!-- window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true }); --><script src='game.js'></script>");
  assert.throws(() => validateWebArtifact({ ...commentOnly, studioRoot }), /executable inline.*PLATFORM_SDK_CONFIG/i);

  for (const [label, html] of [
    ["JavaScript line comment", "<script>// window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true });\n</script><script src='game.js'></script>"],
    ["JavaScript string", "<script>const decoy = \"window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true });\";</script><script src='game.js'></script>"],
    ["external script body", "<script src='bootstrap.js'>window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true });</script><script src='game.js'></script>"],
    ["commented entrypoint", "<script>window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true });</script><script>// gameScript.src = 'game.js';</script>"],
  ]) {
    const item = fixture(t);
    write(join(item.artifactDir, "index.html"), html);
    assert.throws(() => validateWebArtifact({ ...item, studioRoot }), /executable|config|entrypoint|game\.js/i, label);
  }
});

test("release bootstrap ignores regex and non-executable script decoys and requires an attached dynamic entrypoint", (t) => {
  const config = `window.__PLATFORM_SDK_CONFIG__ = Object.freeze({ target: 'itch', platformSdk: 'mock', release: true, runtimeBuildFingerprint: '${runtimeBuildRecord().fingerprint}' });`;
  for (const [label, html] of [
    ["regex config", `<script>const decoy = /window.__PLATFORM_SDK_CONFIG__=Object.freeze({target:'itch',platformSdk:'mock',release:true});/;</script><script src="game.js"></script>`],
    ["regex entrypoint", `<script>${config}</script><script>const decoy = /gameScript.src='game.js'/;</script>`],
    ["non-executable external script", `<script>${config}</script><script type="application/json" src="game.js"></script>`],
    ["non-executable inline config", `<script type="application/json">${config}</script><script src="game.js"></script>`],
    ["dead release config", `<script>if (false) { ${config} }</script><script src="game.js"></script>`],
    ["unbraced dead release config", `<script>if (false) ${config}</script><script src="game.js"></script>`],
    ["unattached dynamic script", `<script>${config}</script><script>const gameScript = document.createElement('script'); gameScript.src = 'game.js';</script>`],
    ["dead dynamic script", `<script>${config}</script><script>if (false) { const gameScript = document.createElement('script'); gameScript.src = 'game.js'; document.body.appendChild(gameScript); }</script>`],
  ]) {
    const item = fixture(t);
    write(join(item.artifactDir, "index.html"), html);
    assert.throws(() => validateWebArtifact({ ...item, studioRoot }), /executable|config|entrypoint|game\.js/i, label);
  }

  const attached = fixture(t);
  write(join(attached.artifactDir, "index.html"), [
    `<script>${config}</script>`,
    "<script>const gameScript = document.createElement('script'); gameScript.src = 'game.js'; document.body.appendChild(gameScript);</script>",
  ].join("\n"));
  assert.doesNotThrow(() => validateWebArtifact({ ...attached, studioRoot }));
});

test("game.js must be an executable Emscripten-like loader before packaging and after ZIP reopen", (t) => {
  const placeholder = fixture(t);
  write(join(placeholder.artifactDir, "game.js"), "console.log('placeholder');\n");
  assert.throws(
    () => validateWebArtifact({ ...placeholder, studioRoot }),
    /game\.wasm|WebAssembly|Emscripten|loader/i,
  );

  const reopened = fixture(t);
  const result = packageWebArtifact({ ...reopened, studioRoot, outDir: join(reopened.root, "reopened-loader") });
  const entries = readStoreZip(readFileSync(result.zipPath));
  entries.set("game.js", Buffer.from("console.log('placeholder');\n"));
  const zipBytes = createStoreZip([...entries].map(([path, bytes]) => ({ path, bytes })));
  writeFileSync(result.zipPath, zipBytes);
  const manifest = JSON.parse(readFileSync(result.manifestPath, "utf8"));
  manifest.artifact.size = zipBytes.length;
  manifest.artifact.sha256 = sha256(zipBytes);
  manifest.entries = [...readStoreZip(zipBytes)].map(([path, bytes]) => ({ path, size: bytes.length, sha256: sha256(bytes) }));
  writeFileSync(result.manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);
  assert.throws(
    () => verifyWebPackage({ zipPath: result.zipPath, manifestPath: result.manifestPath, expectedTarget: "itch", studioRoot }),
    /game\.wasm|WebAssembly|Emscripten|loader/i,
  );

  const deadLoader = [
    "const wasmBinaryFile = 'game.wasm';",
    "function createWasm() {}",
    "if (false) WebAssembly.instantiate(new Uint8Array(), {});",
    "",
  ].join("\n");
  const deadBeforePackage = fixture(t);
  write(join(deadBeforePackage.artifactDir, "game.js"), deadLoader);
  assert.throws(
    () => validateWebArtifact({ ...deadBeforePackage, studioRoot }),
    /game\.wasm|WebAssembly|Emscripten|loader/i,
  );

  const deadReopened = fixture(t);
  const deadResult = packageWebArtifact({ ...deadReopened, studioRoot, outDir: join(deadReopened.root, "dead-reopened-loader") });
  const deadEntries = readStoreZip(readFileSync(deadResult.zipPath));
  deadEntries.set("game.js", Buffer.from(deadLoader));
  const deadZipBytes = createStoreZip([...deadEntries].map(([path, bytes]) => ({ path, bytes })));
  writeFileSync(deadResult.zipPath, deadZipBytes);
  const deadManifest = JSON.parse(readFileSync(deadResult.manifestPath, "utf8"));
  deadManifest.artifact.size = deadZipBytes.length;
  deadManifest.artifact.sha256 = sha256(deadZipBytes);
  deadManifest.entries = [...readStoreZip(deadZipBytes)].map(([path, bytes]) => ({ path, size: bytes.length, sha256: sha256(bytes) }));
  writeFileSync(deadResult.manifestPath, `${JSON.stringify(deadManifest, null, 2)}\n`);
  assert.throws(
    () => verifyWebPackage({ zipPath: deadResult.zipPath, manifestPath: deadResult.manifestPath, expectedTarget: "itch", studioRoot }),
    /game\.wasm|WebAssembly|Emscripten|loader/i,
  );

  for (const [label, instantiation] of [
    ["short-circuited instantiate", "false && WebAssembly.instantiateStreaming(response, imports);"],
    ["unbraced dead returned instantiate", "if (false) return WebAssembly.instantiateStreaming(response, imports);"],
  ]) {
    const item = fixture(t);
    write(join(item.artifactDir, "game.js"), [
      "var wasmBinaryFile;",
      "function findWasmBinary() { return locateFile('game.wasm'); }",
      "async function instantiateAsync(binaryFile, imports) {",
      "  const response = fetch(binaryFile);",
      `  ${instantiation}`,
      "}",
      "async function createWasm() {",
      "  wasmBinaryFile ??= findWasmBinary();",
      "  return instantiateAsync(wasmBinaryFile, {});",
      "}",
      "createWasm();",
      "",
    ].join("\n"));
    assert.throws(
      () => validateWebArtifact({ ...item, studioRoot }),
      /game\.wasm|WebAssembly|Emscripten|loader/i,
      label,
    );
  }

  for (const [label, finderBody, initializer] of [
    ["dead game.wasm finder", "if (false) return locateFile('game.wasm'); return locateFile('other.wasm');", "wasmBinaryFile ??= findWasmBinary();"],
    ["dead path initializer", "return locateFile('game.wasm');", "if (false) wasmBinaryFile ??= findWasmBinary();"],
  ]) {
    const item = fixture(t);
    write(join(item.artifactDir, "game.js"), [
      "var wasmBinaryFile;",
      `function findWasmBinary() { ${finderBody} }`,
      "async function instantiateAsync(binaryFile, imports) {",
      "  const response = fetch(binaryFile);",
      "  return WebAssembly.instantiateStreaming(response, imports);",
      "}",
      "async function createWasm() {",
      `  ${initializer}`,
      "  return instantiateAsync(wasmBinaryFile, {});",
      "}",
      "createWasm();",
      "",
    ].join("\n"));
    assert.throws(
      () => validateWebArtifact({ ...item, studioRoot }),
      /game\.wasm|WebAssembly|Emscripten|loader/i,
      label,
    );
  }
});

test("release WASM rejects invalid structure, debug custom sections, and DevAPI symbols", (t) => {
  for (const [label, bytes, expected] of [
    ["empty module", Buffer.from([0, 97, 115, 109, 1, 0, 0, 0]), /empty|module shape|sections/i],
    ["memory-only module", MEMORY_ONLY_WASM, /game module shape|function export/i],
    ["invalid magic", Buffer.from("not-wasm"), /WebAssembly magic|WASM/i],
    ["invalid section structure", Buffer.from([0, 97, 115, 109, 1, 0, 0, 0, 1, 1, 0xff]), /invalid WebAssembly structure|WASM/i],
    ["debug name", wasmCustomSection("name", "symbols"), /debug|custom section|name/i],
    ["DevAPI symbol", wasmCustomSection("metadata", "nt_devapi_command"), /DevAPI|debug marker/i],
  ]) {
    const item = fixture(t);
    write(join(item.artifactDir, "game.wasm"), bytes);
    assert.throws(() => validateWebArtifact({ ...item, studioRoot }), expected, label);
  }
});

test("web packaging requires exactly one canonical platform-sdk dependency", (t) => {
  for (const mutate of [
    (value) => { value.features = []; },
    (value) => { value.features.push({ ...value.features[0], id: "platform-sdk-copy" }); },
  ]) {
    const item = fixture(t);
    const path = join(item.gameDir, "dependencies.json");
    const value = JSON.parse(readFileSync(path, "utf8"));
    mutate(value);
    write(path, JSON.stringify(value));
    assert.throws(() => packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "out") }), /exactly one canonical platform-sdk dependency/i);
  }

  const direct = fixture(t);
  const directDependencies = JSON.parse(readFileSync(join(direct.gameDir, "dependencies.json"), "utf8"));
  directDependencies.features = [];
  assert.throws(
    () => verifyDependencySources({ studioRoot, dependencies: directDependencies }),
    /exactly one canonical platform-sdk dependency/i,
  );

  const reopened = fixture(t);
  const result = packageWebArtifact({ ...reopened, studioRoot, outDir: join(reopened.root, "reopened") });
  const manifest = JSON.parse(readFileSync(result.manifestPath, "utf8"));
  manifest.dependencies.record.features = [];
  const dependencyBytes = Buffer.from(`${JSON.stringify(manifest.dependencies.record, null, 2)}\n`, "utf8");
  manifest.dependencies.sha256 = sha256(dependencyBytes);
  const entries = readStoreZip(readFileSync(result.zipPath));
  const release = JSON.parse(entries.get("release.json").toString("utf8"));
  release.dependenciesSha256 = manifest.dependencies.sha256;
  entries.set("release.json", Buffer.from(`${JSON.stringify(release, null, 2)}\n`, "utf8"));
  const zipBytes = createStoreZip([...entries].map(([path, bytes]) => ({ path, bytes })));
  writeFileSync(result.zipPath, zipBytes);
  manifest.artifact.size = zipBytes.length;
  manifest.artifact.sha256 = sha256(zipBytes);
  manifest.releaseMetadataSha256 = sha256(entries.get("release.json"));
  manifest.entries = [...readStoreZip(zipBytes)].map(([path, bytes]) => ({ path, size: bytes.length, sha256: sha256(bytes) }));
  writeFileSync(result.manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);
  assert.throws(
    () => verifyWebPackage({ zipPath: result.zipPath, manifestPath: result.manifestPath, expectedTarget: "itch", studioRoot }),
    /exactly one canonical platform-sdk dependency/i,
  );
});

test("placeholder Playgama configuration and dependency placeholders are rejected before ZIP creation", (t) => {
  const playgama = fixture(t, "playgama");
  cpSync(
    join(studioRoot, "features", "platform-sdk", "web", "portal", "playgama-bridge-config.json"),
    join(playgama.artifactDir, "playgama-bridge-config.json"),
  );
  assert.throws(() => packageWebArtifact({ ...playgama, studioRoot, outDir: join(playgama.root, "out") }), /placeholder/i);

  const deps = fixture(t);
  const dependencyPath = join(deps.gameDir, "dependencies.json");
  const value = JSON.parse(readFileSync(dependencyPath, "utf8"));
  value.engine.revision = "HEAD";
  write(dependencyPath, JSON.stringify(value));
  assert.throws(() => packageWebArtifact({ ...deps, studioRoot, outDir: join(deps.root, "out") }), /revision/i);
});

test("artifact inventory rejects symlinks before reading payloads", (t) => {
  const item = fixture(t);
  const outside = join(item.root, "outside");
  write(join(outside, "payload.txt"), "outside");
  const link = join(item.artifactDir, "linked");
  symlinkSync(outside, link, process.platform === "win32" ? "junction" : "dir");
  assert.throws(() => validateWebArtifact({ ...item, studioRoot }), /symlink is forbidden/i);
});

test("STORE ZIP parser rejects unsafe names, case collisions, descriptors, hidden gaps, and trailing bytes", () => {
  for (const path of ["/absolute", "C:/drive", "../escape", "a/../escape", "a\\b", "a\0b"]) {
    assert.throws(() => createStoreZip([{ path, bytes: Buffer.from("x") }]), /invalid ZIP entry path/i, path);
  }
  assert.throws(() => createStoreZip([
    { path: "A.txt", bytes: Buffer.from("a") },
    { path: "a.txt", bytes: Buffer.from("b") },
  ]), /case-colliding/i);

  const clean = createStoreZip([{ path: "a.txt", bytes: Buffer.from("a") }]);
  const descriptor = Buffer.from(clean);
  descriptor.writeUInt16LE(0x0808, 6);
  assert.throws(() => readStoreZip(descriptor), /deterministic UTF-8 STORE|metadata mismatch/i);
  const hidden = Buffer.concat([Buffer.from([0]), clean]);
  assert.throws(() => readStoreZip(hidden), /bounds|missing|hidden|signature/i);
  assert.throws(() => readStoreZip(Buffer.concat([clean, Buffer.from([0])])), /end record/i);
});

test("package verification requires byte-canonical ZIP headers after reopen", (t) => {
  const item = fixture(t);
  const result = packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "release") });
  const bytes = Buffer.from(readFileSync(result.zipPath));
  const central = bytes.indexOf(Buffer.from([0x50, 0x4b, 0x01, 0x02]));
  assert.notEqual(central, -1);
  bytes.writeUInt16LE(21, central + 4);
  writeFileSync(result.zipPath, bytes);
  const manifest = JSON.parse(readFileSync(result.manifestPath, "utf8"));
  manifest.artifact.size = bytes.length;
  manifest.artifact.sha256 = sha256(bytes);
  writeFileSync(result.manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);
  assert.throws(
    () => verifyWebPackage({ zipPath: result.zipPath, manifestPath: result.manifestPath, expectedTarget: "itch", studioRoot }),
    /canonical ZIP/i,
  );
});

test("a conflicting sidecar is detected before publish and cannot orphan the ZIP", (t) => {
  const item = fixture(t);
  const seed = packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "seed") });
  const outDir = join(item.root, "conflict");
  mkdirSync(outDir, { recursive: true });
  write(join(outDir, seed.manifestPath.split(/[\\/]/).at(-1)), "conflict\n");

  assert.throws(() => packageWebArtifact({ ...item, studioRoot, outDir }), /different deterministic artifact/i);
  assert.equal(existsSync(join(outDir, seed.zipPath.split(/[\\/]/).at(-1))), false);

  const failedOut = join(item.root, "second-publish-failure");
  let publishes = 0;
  assert.throws(() => packageWebArtifact({
    ...item,
    studioRoot,
    outDir: failedOut,
    publisher: (temp, final) => {
      publishes += 1;
      if (publishes === 2) throw new Error("injected ZIP publish failure");
      writeFileSync(final, readFileSync(temp));
      rmSync(temp, { force: true });
      return true;
    },
  }), /injected ZIP publish failure/);
  assert.equal(readdirSync(failedOut).some((name) => name.endsWith(".zip")), false);
  assert.equal(readdirSync(failedOut).some((name) => name.endsWith(".manifest.json")), true);
  const retried = packageWebArtifact({ ...item, studioRoot, outDir: failedOut });
  assert.equal(existsSync(retried.zipPath), true);
  assert.equal(existsSync(retried.manifestPath), true);
  assert.doesNotThrow(() => verifyWebPackage({
    zipPath: retried.zipPath,
    manifestPath: retried.manifestPath,
    expectedTarget: "itch",
    studioRoot,
  }));

  const interleavedOut = join(item.root, "interleaved");
  let nested;
  let outerPublishes = 0;
  assert.throws(() => packageWebArtifact({
    ...item,
    studioRoot,
    outDir: interleavedOut,
    publisher: (temp, final) => {
      outerPublishes += 1;
      if (outerPublishes === 1) {
        renameSync(temp, final);
        nested = packageWebArtifact({ ...item, studioRoot, outDir: interleavedOut });
        return true;
      }
      throw new Error("outer ZIP publication failed after concurrent writer");
    },
  }), /outer ZIP publication failed/);
  assert.ok(nested);
  assert.equal(existsSync(nested.zipPath), true);
  assert.equal(existsSync(nested.manifestPath), true);
  assert.doesNotThrow(() => verifyWebPackage({
    zipPath: nested.zipPath,
    manifestPath: nested.manifestPath,
    expectedTarget: "itch",
    studioRoot,
  }));
});

test("packaging runs default dependency-source verification for exact source revisions", (t) => {
  const item = fixture(t);
  delete item.dependencyVerifier;
  const path = join(item.gameDir, "dependencies.json");
  const value = JSON.parse(readFileSync(path, "utf8"));
  value.engine.revision = "3".repeat(40);
  write(path, JSON.stringify(value));
  assert.throws(() => packageWebArtifact({ ...item, studioRoot, outDir: join(item.root, "out") }), /revision|version|dependency source/i);
});

test("dependency proof confines exact owners and checks metadata revisions and relevant cleanliness", (t) => {
  const root = mkdtempSync(join(tmpdir(), "dependency-proof-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  write(join(root, "external", "neotolis-engine", "engine", "core", "nt_core.h"), [
    "#define NT_VERSION_MAJOR 0", "#define NT_VERSION_MINOR 1", "#define NT_VERSION_PATCH 0", "",
  ].join("\n"));
  write(join(root, "features", "platform-sdk", "feature.json"), JSON.stringify({ id: "platform-sdk", version: "1.1.0" }));
  const engineRevision = "1".repeat(40);
  const studioRevision = "2".repeat(40);
  const dependencies = {
    schema: "ai_studio.game.dependencies.v2",
    engine: { source: "external/neotolis-engine", version: "0.1.0", revision: engineRevision, compatibility: "tested" },
    features: [{ id: "platform-sdk", source: "features/platform-sdk", version: "1.1.0", revision: studioRevision, compatibility: "tested" }],
    compatibility: "tested",
  };
  let dirty = "";
  const git = (cwd, args) => {
    const key = args.join(" ");
    if (key === "rev-parse HEAD") return { status: 0, stdout: cwd.includes("neotolis-engine") ? engineRevision : studioRevision };
    if (key.startsWith("ls-tree HEAD")) return { status: 0, stdout: `160000 commit ${engineRevision}\texternal/neotolis-engine` };
    if (key.startsWith("status ")) return { status: 0, stdout: key.endsWith("features/platform-sdk") ? dirty : "" };
    return { status: 1, stderr: `unexpected git call: ${key}` };
  };

  assert.deepEqual(verifyDependencySources({ studioRoot: root, dependencies, git }), { studioRevision, engineRevision });
  const wrongVersion = structuredClone(dependencies);
  wrongVersion.features[0].version = "1.2.0";
  assert.throws(() => verifyDependencySources({ studioRoot: root, dependencies: wrongVersion, git }), /version mismatch/i);
  dirty = " M features/platform-sdk/feature.json";
  assert.throws(() => verifyDependencySources({ studioRoot: root, dependencies, git }), /source is dirty/i);
  const escaped = structuredClone(dependencies);
  escaped.engine.source = "../neotolis-engine";
  assert.throws(() => verifyDependencySources({ studioRoot: root, dependencies: escaped, git }), /source must be exactly/i);
});
