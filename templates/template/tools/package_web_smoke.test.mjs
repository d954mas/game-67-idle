import assert from "node:assert/strict";
import { mkdtempSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { PassThrough } from "node:stream";
import test from "node:test";
import { deflateSync } from "node:zlib";

import { crc32, createStoreZip } from "./lib/zip_store.mjs";
import {
  analyzePngFrame,
  assessPackagedWebObservation,
  CdpClient,
  findSupportedBrowser,
  PipeTransport,
  registerBrowserIssueCapture,
  smokePackagedWebArtifact,
} from "./package_web_smoke.mjs";

const FINGERPRINT = "1".repeat(64);

function passingObservation() {
  return {
    finalUrl: "http://127.0.0.1:8123/",
    ready: true,
    runtimeBuildFingerprint: FINGERPRINT,
    compiledRuntimeBuildFingerprint: FINGERPRINT,
    canvas: { width: 1280, height: 720 },
    frame: { width: 1280, height: 720, minLuma: 2, maxLuma: 241, variance: 812.5 },
    issues: [],
  };
}

function pngChunk(type, data) {
  const name = Buffer.from(type, "ascii");
  const out = Buffer.alloc(12 + data.length);
  out.writeUInt32BE(data.length, 0);
  name.copy(out, 4);
  data.copy(out, 8);
  out.writeUInt32BE(crc32(Buffer.concat([name, data])), 8 + data.length);
  return out;
}

function rgbPng(width, height, pixels) {
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0);
  header.writeUInt32BE(height, 4);
  header[8] = 8;
  header[9] = 2;
  const rows = [];
  for (let row = 0; row < height; row += 1) {
    rows.push(Buffer.from([0]), Buffer.from(pixels.slice(row * width * 3, (row + 1) * width * 3)));
  }
  return Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    pngChunk("IHDR", header),
    pngChunk("IDAT", deflateSync(Buffer.concat(rows))),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

test("packaged browser observation accepts a ready rendered release frame", () => {
  assert.deepEqual(assessPackagedWebObservation(passingObservation(), FINGERPRINT), []);
});

test("browser discovery accepts an explicit Windows Chrome path without WSL", (t) => {
  const root = mkdtempSync(join(tmpdir(), "packaged-web-browser-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const chromePath = join(root, "chrome.exe");
  writeFileSync(chromePath, "fixture");
  assert.equal(findSupportedBrowser({ env: { CHROME_PATH: chromePath }, platform: "win32" }), chromePath);
});

test("CDP pipe routes flattened session commands events and deadlines", async () => {
  const input = new PassThrough();
  const output = new PassThrough();
  const client = new CdpClient(new PipeTransport(input, output));
  client.sessionId = "session-one";
  let command = null;
  let eventUrl = "";
  client.on("Network.requestWillBeSent", ({ request }) => { eventUrl = request.url; });
  input.on("data", (bytes) => {
    command = JSON.parse(bytes.toString("utf8").replace(/\0$/, ""));
    output.write(`${JSON.stringify({ id: command.id, result: { enabled: true }, sessionId: command.sessionId })}\0`);
    output.write(`${JSON.stringify({
      method: "Network.requestWillBeSent",
      params: { request: { url: "http://127.0.0.1/game.wasm" } },
      sessionId: command.sessionId,
    })}\0`);
  });
  assert.deepEqual(await client.call("Page.enable", {}, Date.now() + 1000), { enabled: true });
  assert.equal(command.sessionId, "session-one");
  assert.equal(eventUrl, "http://127.0.0.1/game.wasm");
  client.close();

  const stalledInput = new PassThrough();
  const stalledOutput = new PassThrough();
  const stalled = new CdpClient(new PipeTransport(stalledInput, stalledOutput));
  await assert.rejects(stalled.call("Page.enable", {}, Date.now() + 30), /Page\.enable timed out/);
  stalled.close();

  const brokenInput = new PassThrough();
  const brokenOutput = new PassThrough();
  const broken = new CdpClient(new PipeTransport(brokenInput, brokenOutput));
  const pending = broken.call("Runtime.enable", {}, Date.now() + 1000);
  brokenInput.destroy(new Error("broken pipe"));
  await assert.rejects(pending, /CDP connection closed/);
  broken.close();
});

test("browser issue capture rejects remote socket and direct transport side channels", () => {
  const listeners = new Map();
  const client = { on(method, listener) { listeners.set(method, listener); } };
  const issues = [];
  registerBrowserIssueCapture(client, issues);
  listeners.get("Network.webSocketCreated")({ url: "wss://example.com/socket" });
  listeners.get("Network.webTransportCreated")({ url: "https://example.com/transport" });
  listeners.get("Network.directTCPSocketCreated")({ remoteAddr: "203.0.113.1:443" });
  listeners.get("Network.directUDPSocketCreated")({ options: { remoteAddr: "203.0.113.2:443" } });
  listeners.get("Network.webSocketCreated")({ url: "ws://127.0.0.1/local" });
  assert.deepEqual(issues, [
    { kind: "resource.remote", text: "wss://example.com/socket" },
    { kind: "resource.remote", text: "https://example.com/transport" },
    { kind: "resource.direct", text: "direct TCP socket: 203.0.113.1:443" },
    { kind: "resource.direct", text: "direct UDP socket: 203.0.113.2:443" },
  ]);
});

test("PNG first-frame decoder measures rendered contrast and rejects malformed input", () => {
  assert.deepEqual(analyzePngFrame(rgbPng(2, 1, [0, 0, 0, 255, 255, 255])), {
    width: 2, height: 1, minLuma: 0, maxLuma: 255, variance: 16256.25,
  });
  assert.throws(() => analyzePngFrame(Buffer.from("not png")), /not PNG/);
});

test("packaged browser observation reports page console and resource failures", () => {
  for (const [kind, expected] of [
    ["page.exception", "page error"],
    ["console.error", "console error"],
    ["resource.http", "resource error"],
  ]) {
    const observation = passingObservation();
    observation.issues = [{ kind, text: `${kind} detail` }];
    assert.deepEqual(assessPackagedWebObservation(observation, FINGERPRINT), [`${expected}: ${kind} detail`]);
  }
});

test("packaged browser observation rejects missing runtime readiness", () => {
  const observation = passingObservation();
  observation.ready = false;
  observation.compiledRuntimeBuildFingerprint = "0".repeat(64);
  assert.deepEqual(assessPackagedWebObservation(observation, FINGERPRINT), [
    "runtime readiness was not reached",
    "executed WASM runtime build marker does not match the package",
  ]);
});

test("packaged browser observation stays bound to the loopback package fingerprint", () => {
  const observation = passingObservation();
  observation.finalUrl = "https://example.com/redirected";
  observation.runtimeBuildFingerprint = "0".repeat(64);
  assert.deepEqual(assessPackagedWebObservation(observation, FINGERPRINT), [
    "final page URL left loopback",
    "page runtime build fingerprint does not match the package",
  ]);
});

test("packaged browser observation rejects blank and black first frames", () => {
  const blank = passingObservation();
  blank.frame = { width: 1280, height: 720, minLuma: 247, maxLuma: 247, variance: 0 };
  assert.deepEqual(assessPackagedWebObservation(blank, FINGERPRINT), ["first frame is blank"]);

  const black = passingObservation();
  black.frame = { width: 1280, height: 720, minLuma: 0, maxLuma: 3, variance: 0.4 };
  assert.deepEqual(assessPackagedWebObservation(black, FINGERPRINT), ["first frame is black"]);
});

test("smoke reopens the ZIP and serves only its entries", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "packaged-web-smoke-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const zipPath = join(root, "fixture.zip");
  writeFileSync(zipPath, createStoreZip([
    { path: "index.html", bytes: Buffer.from("<!doctype html><script src='game.js'></script>") },
    { path: "game.js", bytes: Buffer.from("globalThis.fixture = true;") },
    { path: "release.json", bytes: Buffer.from(JSON.stringify({
      schema: "ai_studio.game.release.v2",
      target: "itch",
      entrypoint: "index.html",
      runtimeBuildFingerprint: FINGERPRINT,
    })) },
  ]));

  const observation = await smokePackagedWebArtifact({
    zipPath,
    expectedTarget: "itch",
    probe: async ({ url, expectedRuntimeBuildFingerprint }) => {
      assert.equal(expectedRuntimeBuildFingerprint, FINGERPRINT);
      assert.equal(await (await fetch(new URL("game.js", url))).text(), "globalThis.fixture = true;");
      assert.equal((await fetch(new URL("not-in-package.txt", url))).status, 404);
      return { ...passingObservation(), finalUrl: url };
    },
  });
  assert.deepEqual(assessPackagedWebObservation(observation, FINGERPRINT), []);

  await assert.rejects(
    smokePackagedWebArtifact({
      zipPath,
      expectedTarget: "itch",
      probe: async ({ url }) => ({
        ...passingObservation(),
        finalUrl: url,
        issues: [
          { kind: "console.error", text: "boot exploded" },
          { kind: "resource.http", text: "404 game.wasm" },
        ],
      }),
    }),
    /packaged web smoke failed: console error: boot exploded; resource error: 404 game\.wasm/,
  );

  const invalidBrowser = join(root, "not-a-browser.txt");
  writeFileSync(invalidBrowser, "not executable");
  const profileNames = () => readdirSync(tmpdir()).filter((name) => name.startsWith("ai-studio-package-smoke-")).sort();
  const beforeProfiles = profileNames();
  await assert.rejects(
    smokePackagedWebArtifact({ zipPath, expectedTarget: "itch", chromePath: invalidBrowser, timeoutMs: 1000 }),
    /spawn|EACCES|EFTYPE|UNKNOWN/i,
  );
  assert.deepEqual(profileNames(), beforeProfiles);
});
