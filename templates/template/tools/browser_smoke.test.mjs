import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import test from "node:test";
import { deflateSync } from "node:zlib";

import {
  analyzePngFrame,
  assertBrowserSmokeResult,
  canvasScreenshotClip,
  chromeLaunchArgs,
  findChrome,
  normalizeBrowserEvent,
  readDevToolsPort,
  startZipServer,
} from "./browser_smoke.mjs";
import { crc32, createStoreZip } from "./lib/zip_store.mjs";

function pngChunk(type, data = Buffer.alloc(0)) {
  const name = Buffer.from(type, "ascii");
  const chunk = Buffer.alloc(12 + data.length);
  chunk.writeUInt32BE(data.length, 0);
  name.copy(chunk, 4);
  data.copy(chunk, 8);
  chunk.writeUInt32BE(crc32(Buffer.concat([name, data])), 8 + data.length);
  return chunk;
}

function rgbaPng(width, height, pixels) {
  const header = Buffer.alloc(13);
  header.writeUInt32BE(width, 0);
  header.writeUInt32BE(height, 4);
  header[8] = 8;
  header[9] = 6;
  const rows = [];
  for (let y = 0; y < height; y += 1) {
    rows.push(Buffer.from([0]), Buffer.from(pixels.slice(y * width * 4, (y + 1) * width * 4)));
  }
  return Buffer.concat([
    Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]),
    pngChunk("IHDR", header),
    pngChunk("IDAT", deflateSync(Buffer.concat(rows))),
    pngChunk("IEND"),
  ]);
}

test("explicit supported-browser path resolves without platform probing", () => {
  assert.equal(findChrome(process.execPath, "win32", {}), resolve(process.execPath));
  assert.throws(() => findChrome(join(tmpdir(), "missing-chrome"), "linux", {}), /does not exist/);
});

test("headless launch delegates the debug port to Chrome and enables ANGLE SwiftShader", () => {
  const profile = "C:\\temp\\isolated-profile";
  const args = chromeLaunchArgs(profile);
  assert.ok(args.includes("--remote-debugging-port=0"));
  assert.ok(args.includes(`--user-data-dir=${profile}`));
  assert.ok(args.includes("--use-gl=angle"));
  assert.ok(args.includes("--use-angle=swiftshader"));
  assert.ok(args.includes("--enable-unsafe-swiftshader"));
  assert.equal(args.includes("--disable-gpu"), false);
});

test("DevToolsActivePort parser fails closed until Chrome publishes a valid owned port", (t) => {
  const profile = mkdtempSync(join(tmpdir(), "browser-smoke-profile-"));
  t.after(() => rmSync(profile, { recursive: true, force: true }));
  assert.equal(readDevToolsPort(profile), 0);
  writeFileSync(join(profile, "DevToolsActivePort"), "not-a-port\n/devtools/browser/id\n");
  assert.equal(readDevToolsPort(profile), 0);
  writeFileSync(join(profile, "DevToolsActivePort"), "49231\n/devtools/browser/id\n");
  assert.equal(readDevToolsPort(profile), 49231);
});

test("ZIP server exposes only reopened package entries", async (t) => {
  const root = mkdtempSync(join(tmpdir(), "browser-smoke-zip-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const zipPath = join(root, "fixture.zip");
  writeFileSync(join(root, "outside.txt"), "must not be served");
  writeFileSync(zipPath, createStoreZip([
    { path: "index.html", bytes: Buffer.from("<!doctype html><script src='game.js'></script>") },
    { path: "game.js", bytes: Buffer.from("window.fixture = true;") },
  ]));

  const server = await startZipServer(zipPath);
  t.after(() => server.close());
  assert.equal((await fetch(server.url)).status, 200);
  assert.equal(await (await fetch(new URL("game.js", server.url))).text(), "window.fixture = true;");
  assert.equal((await fetch(new URL("outside.txt", server.url))).status, 404);
  assert.equal((await fetch(new URL("../outside.txt", server.url))).status, 404);
});

test("PNG frame analysis distinguishes rendered pixels from blank and black frames", () => {
  const black = analyzePngFrame(rgbaPng(2, 1, [0, 0, 0, 255, 0, 0, 0, 255]));
  assert.equal(black.blank, true);
  assert.equal(black.darkRatio, 1);

  const rendered = analyzePngFrame(rgbaPng(2, 1, [0, 0, 0, 255, 255, 80, 20, 255]));
  assert.equal(rendered.blank, false);
  assert.ok(rendered.lumaRange > 20);
});

test("screenshot clip selects only the rendered canvas", () => {
  assert.deepEqual(canvasScreenshotClip({ canvasX: 11, canvasY: 49, displayWidth: 938, displayHeight: 442 }), {
    x: 11, y: 49, width: 938, height: 442, scale: 1,
  });
  assert.throws(() => canvasScreenshotClip({ canvasX: 0, canvasY: 0, displayWidth: 0, displayHeight: 442 }), /canvas clip/i);
});

test("browser smoke accepts a ready rendered frame", () => {
  const result = assertBrowserSmokeResult({
    issues: [],
    readiness: { ready: true, canvasWidth: 960, canvasHeight: 540 },
    frame: { blank: false, darkRatio: 0.4, lumaRange: 120 },
  });
  assert.match(result, /960x540/);
});

test("browser smoke reports page, console, and resource failure classes compactly", () => {
  for (const [event, expected] of [
    [{ method: "Runtime.exceptionThrown", params: { exceptionDetails: { exception: { description: "Uncaught Error: boot" } } } }, /page: Uncaught Error: boot/],
    [{ method: "Runtime.consoleAPICalled", params: { type: "error", args: [{ value: "asset contract failed" }] } }, /console: asset contract failed/],
    [{ method: "Network.responseReceived", params: { response: { status: 404, url: "http://127.0.0.1/game.wasm" } } }, /resource: 404 http:\/\/127\.0\.0\.1\/game\.wasm/],
  ]) {
    const issue = normalizeBrowserEvent(event);
    assert.ok(issue);
    assert.throws(() => assertBrowserSmokeResult({
      issues: [issue],
      readiness: { ready: true, canvasWidth: 960, canvasHeight: 540 },
      frame: { blank: false, darkRatio: 0.4, lumaRange: 120 },
    }), expected);
  }
  assert.equal(normalizeBrowserEvent({ method: "Runtime.consoleAPICalled", params: { type: "log" } }), null);
});

test("browser smoke fails closed on missing readiness and a blank first frame", () => {
  assert.throws(() => assertBrowserSmokeResult({
    issues: [],
    readiness: { ready: false, canvasWidth: 0, canvasHeight: 0 },
    frame: null,
  }), /runtime readiness/i);
  assert.throws(() => assertBrowserSmokeResult({
    issues: [],
    readiness: { ready: true, canvasWidth: 960, canvasHeight: 540 },
    frame: { blank: true, darkRatio: 1, lumaRange: 0 },
  }), /blank\/black first frame.*dark=100\.0%.*range=0/i);
});
