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
  browserSandboxArgs,
  CdpClient,
  findSupportedBrowser,
  PipeTransport,
  registerBrowserIssueCapture,
  smokeProbePlan,
  smokePackagedWebArtifact,
} from "./package_web_smoke.mjs";

const FINGERPRINT = "1".repeat(64);

function passingRecoveryCycle(variance) {
  return {
    extensionAvailable: true,
    lostEvent: true,
    contextLostDuringLoss: true,
    overlayVisibleDuringLoss: true,
    restoredEvent: true,
    overlayVisibleAtRestore: true,
    contextRestored: true,
    overlayClearedAfterReadyFrame: true,
    runtimePresentedFrameAck: true,
    failure: "",
    frame: { width: 1280, height: 720, minLuma: 3, maxLuma: 239, variance },
  };
}

function passingObservation() {
  const cycles = [passingRecoveryCycle(790.5), passingRecoveryCycle(801.5)];
  return {
    finalUrl: "http://127.0.0.1:8123/",
    ready: true,
    runtimeBuildFingerprint: FINGERPRINT,
    compiledRuntimeBuildFingerprint: FINGERPRINT,
    canvas: { width: 1280, height: 720 },
    frame: { width: 1280, height: 720, minLuma: 2, maxLuma: 241, variance: 812.5 },
    recovery: {
      ...cycles[1],
      completedCycles: 2,
      cycles,
    },
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

test("quick packaged browser observation skips PNG and WebGL recovery evidence", () => {
  const observation = passingObservation();
  observation.frame = null;
  observation.recovery = null;
  assert.deepEqual(assessPackagedWebObservation(observation, FINGERPRINT, { mode: "quick" }), []);
});

test("quick browser probe schedules no screenshot or recovery cycle", () => {
  assert.deepEqual(smokeProbePlan("quick"), { captureFrame: false, recoveryCycles: 0 });
  assert.deepEqual(smokeProbePlan("full"), { captureFrame: true, recoveryCycles: 2 });
  assert.throws(() => smokeProbePlan("unknown"), /unknown smoke mode/);
});

test("packaged browser observation requires WebGL loss restore and a presented recovered frame", () => {
  const observation = passingObservation();
  observation.recovery = null;
  assert.deepEqual(assessPackagedWebObservation(observation, FINGERPRINT), [
    "WebGL recovery evidence is missing",
  ]);
});

test("packaged browser observation requires two consecutive recovery cycles", () => {
  const observation = passingObservation();
  observation.recovery.completedCycles = 1;
  assert.deepEqual(assessPackagedWebObservation(observation, FINGERPRINT), [
    "two consecutive WebGL recovery cycles were not completed",
  ]);
});

test("packaged browser observation rejects a missing runtime-present ack and blank cycle frame", () => {
  const observation = passingObservation();
  observation.recovery.cycles[0].runtimePresentedFrameAck = false;
  observation.recovery.cycles[0].frame = {
    width: 1280, height: 720, minLuma: 247, maxLuma: 247, variance: 0,
  };
  assert.deepEqual(assessPackagedWebObservation(observation, FINGERPRINT), [
    "runtime did not acknowledge a presented frame after recovery cycle 1",
    "recovered frame after cycle 1 is blank",
  ]);
});

test("browser discovery accepts an explicit Windows Chrome path without WSL", (t) => {
  const root = mkdtempSync(join(tmpdir(), "packaged-web-browser-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const chromePath = join(root, "chrome.exe");
  writeFileSync(chromePath, "fixture");
  assert.equal(findSupportedBrowser({ env: { CHROME_PATH: chromePath }, platform: "win32" }), chromePath);
});

test("browser sandbox bypass requires an explicit constrained-environment opt-in", () => {
  assert.deepEqual(browserSandboxArgs({}), []);
  assert.deepEqual(
    browserSandboxArgs({ AI_STUDIO_CHROME_NO_SANDBOX: "1" }),
    ["--no-sandbox", "--disable-gpu-sandbox"],
  );
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

test("browser issue capture consumes only the authorized context-loss console errors", () => {
  const listeners = new Map();
  const client = { on(method, listener) { listeners.set(method, listener); } };
  const issues = [];
  let allowance = 2;
  let consumed = 0;
  registerBrowserIssueCapture(client, issues, [], [], {
    consumeConsoleError(text) {
      if (text !== "ERROR [gfx] WebGL context lost" || allowance <= 0) return false;
      allowance -= 1;
      consumed += 1;
      return true;
    },
  });

  for (let occurrence = 0; occurrence < 3; occurrence += 1) {
    listeners.get("Runtime.consoleAPICalled")({
      type: "error",
      args: [{ value: "ERROR [gfx] WebGL context lost" }],
    });
  }

  assert.equal(consumed, 2);
  assert.deepEqual(issues, [{
    kind: "console.error",
    text: "ERROR [gfx] WebGL context lost",
  }]);
});

test("browser issue capture ignores canceled aborts but keeps real loading failures", () => {
  const listeners = new Map();
  const client = { on(method, listener) { listeners.set(method, listener); } };
  const issues = [];
  registerBrowserIssueCapture(client, issues);

  listeners.get("Network.loadingFailed")({ errorText: "net::ERR_ABORTED", canceled: true });
  assert.deepEqual(issues, []);

  listeners.get("Network.loadingFailed")({ requestId: "missing-1", errorText: "net::ERR_ABORTED", canceled: false });
  listeners.get("Network.loadingFailed")({ requestId: "missing-2", errorText: "net::ERR_FAILED", canceled: true });
  listeners.get("Network.loadingFailed")({
    requestId: "missing-3",
    errorText: "net::ERR_ABORTED",
    blockedReason: "inspector",
    canceled: true,
  });
  assert.deepEqual(issues, [
    { kind: "resource.load", text: "Other unknown URL failed: net::ERR_ABORTED" },
    { kind: "resource.load", text: "Other unknown URL failed: net::ERR_FAILED; canceled=true" },
    { kind: "resource.load", text: "Other unknown URL failed: net::ERR_ABORTED; blocked=inspector; canceled=true" },
  ]);
});

test("browser issue capture diagnoses a blocked allowed Poki SDK request", () => {
  const listeners = new Map();
  const client = { on(method, listener) { listeners.set(method, listener); } };
  const issues = [];
  registerBrowserIssueCapture(client, issues, [/^https:\/\/game-cdn\.poki\.com\//]);

  listeners.get("Network.requestWillBeSent")({
    requestId: "poki-sdk",
    type: "Script",
    request: { url: "https://game-cdn.poki.com/scripts/v2/poki-sdk.js" },
    initiator: {
      type: "parser",
      url: "http://127.0.0.1:8123/index.html",
      lineNumber: 17,
      columnNumber: 3,
    },
  });
  listeners.get("Network.loadingFailed")({
    requestId: "poki-sdk",
    type: "Script",
    errorText: "net::ERR_BLOCKED_BY_CLIENT",
    blockedReason: "inspector",
    canceled: false,
  });

  assert.deepEqual(issues, [{
    kind: "resource.load",
    text: "Script https://game-cdn.poki.com/scripts/v2/poki-sdk.js failed: net::ERR_BLOCKED_BY_CLIENT; blocked=inspector; initiator=parser http://127.0.0.1:8123/index.html:18:4",
  }]);
});

test("browser issue capture keeps HTTP failures from an allowed Poki origin", () => {
  const listeners = new Map();
  const client = { on(method, listener) { listeners.set(method, listener); } };
  const issues = [];
  registerBrowserIssueCapture(
    client,
    issues,
    [/^https:\/\/game-cdn\.poki\.com\//],
    [/doubleclick\.net/],
  );

  listeners.get("Network.requestWillBeSent")({
    requestId: "poki-http",
    type: "Script",
    request: { url: "https://game-cdn.poki.com/scripts/v2/poki-sdk.js" },
    initiator: { type: "parser" },
  });
  listeners.get("Network.responseReceived")({
    requestId: "poki-http",
    response: {
      status: 503,
      url: "https://game-cdn.poki.com/scripts/v2/poki-sdk.js",
    },
  });

  listeners.get("Network.requestWillBeSent")({
    requestId: "tolerated-ad",
    type: "Script",
    request: { url: "https://securepubads.doubleclick.net/tag/js/gpt.js" },
    initiator: { type: "script" },
  });
  listeners.get("Network.responseReceived")({
    requestId: "tolerated-ad",
    response: {
      status: 503,
      url: "https://securepubads.doubleclick.net/tag/js/gpt.js",
    },
  });

  assert.deepEqual(issues, [{
    kind: "resource.http",
    text: "503 https://game-cdn.poki.com/scripts/v2/poki-sdk.js",
  }]);
});

test("browser issue capture tolerates ad plumbing served from an allowed origin", () => {
  const listeners = new Map();
  const client = { on(method, listener) { listeners.set(method, listener); } };
  const issues = [];
  registerBrowserIssueCapture(
    client,
    issues,
    [/^https:\/\/([a-z0-9-]+\.)?poki\.com\//],
    [/(^|[/.])ads\.poki\.com(\/|$|[:?#'" )])/],
  );

  listeners.get("Network.requestWillBeSent")({
    requestId: "poki-ads",
    type: "Fetch",
    request: { url: "https://ads.poki.com/ads/settings?loc=" },
    initiator: { type: "script" },
  });
  listeners.get("Network.loadingFailed")({
    requestId: "poki-ads",
    type: "Fetch",
    errorText: "net::ERR_BLOCKED_BY_CLIENT",
    blockedReason: "inspector",
    canceled: false,
  });
  listeners.get("Log.entryAdded")({
    entry: {
      level: "error",
      source: "network",
      text: "Failed to load resource: net::ERR_BLOCKED_BY_CLIENT",
      url: "https://ads.poki.com/ads/settings?loc=",
    },
  });

  assert.deepEqual(issues, []);
});

test("browser issue capture diagnoses a generic asset fetch failure with its script provenance", () => {
  const listeners = new Map();
  const client = { on(method, listener) { listeners.set(method, listener); } };
  const issues = [];
  registerBrowserIssueCapture(client, issues);

  listeners.get("Network.requestWillBeSent")({
    requestId: "slime-texture",
    type: "Image",
    request: { url: "http://127.0.0.1:8123/assets/slime.png" },
    initiator: {
      type: "script",
      stack: {
        callFrames: [
          {
            functionName: "loadTexture",
            url: "http://127.0.0.1:8123/loader.js",
            lineNumber: 7,
            columnNumber: 2,
          },
          {
            functionName: "boot",
            url: "http://127.0.0.1:8123/game.js",
            lineNumber: 20,
            columnNumber: 4,
          },
        ],
      },
    },
  });
  listeners.get("Network.loadingFailed")({
    requestId: "slime-texture",
    type: "Image",
    errorText: "net::ERR_FAILED",
    canceled: true,
  });

  assert.deepEqual(issues, [{
    kind: "resource.load",
    text: "Image http://127.0.0.1:8123/assets/slime.png failed: net::ERR_FAILED; canceled=true; initiator=script loadTexture@http://127.0.0.1:8123/loader.js:8:3 <- boot@http://127.0.0.1:8123/game.js:21:5",
  }]);
});

test("browser issue capture diagnoses runtime exceptions with source and compact stack frames", () => {
  const listeners = new Map();
  const client = { on(method, listener) { listeners.set(method, listener); } };
  const issues = [];
  registerBrowserIssueCapture(client, issues);

  listeners.get("Runtime.exceptionThrown")({
    exceptionDetails: {
      text: "Uncaught",
      url: "http://127.0.0.1:8123/game.js",
      lineNumber: 42,
      columnNumber: 9,
      exception: { description: "ReferenceError: slime is not defined\n    at update (game.js:43:10)" },
      stackTrace: {
        callFrames: [
          {
            functionName: "update",
            url: "http://127.0.0.1:8123/game.js",
            lineNumber: 42,
            columnNumber: 9,
          },
          {
            functionName: "tick",
            url: "http://127.0.0.1:8123/runtime.js",
            lineNumber: 7,
            columnNumber: 1,
          },
        ],
      },
    },
  });

  assert.deepEqual(issues, [{
    kind: "page.exception",
    text: "ReferenceError: slime is not defined at http://127.0.0.1:8123/game.js:43:10; stack=update@http://127.0.0.1:8123/game.js:43:10 <- tick@http://127.0.0.1:8123/runtime.js:8:2",
  }]);
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
  // Scoped profile root: enumerating the global tmpdir races concurrent smoke runs.
  const profileRoot = mkdtempSync(join(root, "smoke-profiles-"));
  const profileNames = () => readdirSync(profileRoot).filter((name) => name.startsWith("ai-studio-package-smoke-")).sort();
  const beforeProfiles = profileNames();
  await assert.rejects(
    smokePackagedWebArtifact({ zipPath, expectedTarget: "itch", chromePath: invalidBrowser, timeoutMs: 1000, profileRoot }),
    /spawn|EACCES|EFTYPE|UNKNOWN/i,
  );
  assert.deepEqual(profileNames(), beforeProfiles);
});
