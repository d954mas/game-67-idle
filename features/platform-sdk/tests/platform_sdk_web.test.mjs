import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { createServer } from "node:http";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import test from "node:test";

import { findSupportedBrowser, launchHeadlessBrowser } from "../scripts/lib/headless_browser.mjs";
import { resolveEmscriptenRoot } from "../scripts/generate_release_bundles.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..", "..");
const OUTPUT = join(ROOT, "tmp", "platform-sdk-web-contract");
const FIXTURE = join(HERE, "platform_sdk_web_fixture.c");
const TIMEOUT_MS = 90000;

function missingTool(message) {
  const error = new Error(message);
  error.code = "MISSING_WEB_TEST_TOOL";
  return error;
}

function run(command, args) {
  const result = spawnSync(command, args, {
    cwd: ROOT,
    encoding: "utf8",
    windowsHide: true,
    timeout: TIMEOUT_MS,
  });
  if (result.error) throw result.error;
  if (result.status !== 0) {
    throw new Error(`${command} failed (${result.status}):\n${result.stdout}\n${result.stderr}`);
  }
}

async function compiler() {
  let emscripten;
  try {
    emscripten = resolveEmscriptenRoot();
  } catch (error) {
    throw missingTool(error.message);
  }
  const emcc = join(emscripten, "emcc.py");
  if (!existsSync(emcc)) throw missingTool(`Emscripten compiler is missing: ${emcc}`);
  return { emcc, python: process.env.PYTHON || (process.platform === "win32" ? "py" : "python3") };
}

async function buildFixture() {
  const { emcc, python } = await compiler();
  mkdirSync(OUTPUT, { recursive: true });
  run(python, [
    ...(python === "py" ? ["-3"] : []), emcc,
    FIXTURE,
    join(ROOT, "features", "platform-sdk", "src", "platform_sdk.c"),
    join(ROOT, "features", "platform-sdk", "src", "platform_sdk_web.c"),
    join(ROOT, "features", "platform-sdk", "src", "platform_sdk_storage.c"),
    join(ROOT, "features", "platform-sdk", "src", "platform_sdk_cloud.c"),
    "-I", join(ROOT, "features", "platform-sdk", "include"),
    "-DFEATURE_GAME_EVENTS=0",
    "-DPLATFORM_SDK_STORAGE_SUPPORTED=1",
    "--no-entry",
    "-sMODULARIZE=1",
    "-sEXPORT_NAME=createPlatformSdkWebFixture",
    "-sENVIRONMENT=web",
    "-sALLOW_MEMORY_GROWTH=1",
    "-sEXPORTED_FUNCTIONS=[\"_fixture_boot\",\"_fixture_status\",\"_fixture_portal_paused\",\"_fixture_audio_enabled\",\"_fixture_break_active\",\"_fixture_pause_count\",\"_fixture_resume_count\",\"_fixture_show_interstitial\",\"_fixture_active_interstitial_id\",\"_fixture_complete_interstitial\",\"_fixture_interstitial_completions\",\"_fixture_last_interstitial_reason\",\"_fixture_cloud_load\",\"_fixture_cloud_load_status\",\"_fixture_cloud_take_is\",\"_fixture_cloud_store\",\"_fixture_cloud_write_status\",\"_malloc\",\"_free\"]",
    "-o", join(OUTPUT, "fixture.js"),
  ]);
}

function writePage() {
  writeFileSync(join(OUTPUT, "index.html"), `<!doctype html>
<meta charset="utf-8">
<script>
  globalThis.__platformSdkLifecycleState = { paused: true, audioEnabled: false };
  globalThis.__fixture = { ads: [], loads: [], stores: [] };
  globalThis.__platformSdkInternalBackend = {
    ready: () => Promise.resolve(true),
    getLocale: () => "en-US",
    getPlayer: () => Promise.resolve({}),
    showInterstitial(placement, requestId) {
      return new Promise((resolve, reject) => globalThis.__fixture.ads.push({ placement, requestId, resolve, reject }));
    },
    loadData(key) {
      return new Promise((resolve, reject) => globalThis.__fixture.loads.push({ key, resolve, reject }));
    },
    saveData(key, text) {
      return new Promise((resolve, reject) => globalThis.__fixture.stores.push({ key, text, resolve, reject }));
    },
  };
</script>
<script src="fixture.js"></script>
<script>
  createPlatformSdkWebFixture().then((module) => { globalThis.fixtureModule = module; });
</script>
`, "utf8");
}

function startServer() {
  const server = createServer((request, response) => {
    const path = request.url === "/" ? "index.html" : String(request.url || "").replace(/^\//, "");
    const file = resolve(OUTPUT, path);
    if (relative(OUTPUT, file).startsWith("..") || !existsSync(file)) {
      response.writeHead(404).end();
      return;
    }
    const contentType = path.endsWith(".wasm") ? "application/wasm"
      : path.endsWith(".html") ? "text/html; charset=utf-8"
      : "text/javascript; charset=utf-8";
    response.writeHead(200, { "Content-Type": contentType });
    response.end(readFileSync(file));
  });
  return new Promise((done) => server.listen(0, "127.0.0.1", () => done(server)));
}

async function evaluate(client, expression, deadline) {
  const reply = await client.call("Runtime.evaluate", {
    expression,
    awaitPromise: true,
    returnByValue: true,
  }, deadline);
  if (reply.exceptionDetails) throw new Error(reply.exceptionDetails.text || "page evaluation failed");
  return reply.result?.value;
}

async function waitFor(client, expression, deadline) {
  while (Date.now() < deadline) {
    if (await evaluate(client, expression, deadline)) return;
    await new Promise((done) => setTimeout(done, 20));
  }
  throw new Error(`timed out waiting for ${expression}`);
}

test("web bridge replays lifecycle and rejects stale ads and storage completions", { timeout: TIMEOUT_MS + 10000 }, async (context) => {
  let browser;
  let server;
  const deadline = Date.now() + TIMEOUT_MS;
  try {
    await buildFixture();
    writePage();
    server = await startServer();
    try {
      findSupportedBrowser();
    } catch (error) {
      context.skip(error.message);
      return;
    }
    browser = await launchHeadlessBrowser({ deadline });
    const { client } = browser;
    const port = server.address().port;
    await client.call("Page.navigate", { url: `http://127.0.0.1:${port}/` }, deadline);
    await waitFor(client, "Boolean(globalThis.fixtureModule)", deadline);

    const replay = await evaluate(client, `(() => {
      const m = globalThis.fixtureModule;
      m._fixture_boot();
      return [m._fixture_portal_paused(), m._fixture_audio_enabled(), m._fixture_pause_count(), m._fixture_break_active()];
    })()`, deadline);
    assert.deepEqual(replay, [1, 0, 0, 1]);
    await waitFor(client, "fixtureModule._fixture_status() === 2", deadline);

    const first = await evaluate(client, `(() => {
      const m = globalThis.fixtureModule;
      const started = m._fixture_show_interstitial();
      return [started, m._fixture_active_interstitial_id(), globalThis.__fixture.ads.length];
    })()`, deadline);
    assert.equal(first[0], 0);
    assert.ok(first[1] > 0);
    assert.equal(first[2], 1);
    const firstId = first[1];

    await evaluate(client, `(() => {
      const m = globalThis.fixtureModule;
      globalThis.__platformSdkAdVisible(${firstId}, true);
      m._fixture_complete_interstitial(${firstId}, 1, 7);
      globalThis.__platformSdkAdVisible(${firstId}, false);
    })()`, deadline);
    assert.equal(await evaluate(client, "fixtureModule._fixture_interstitial_completions()", deadline), 1);

    const second = await evaluate(client, `(() => {
      const m = globalThis.fixtureModule;
      m._fixture_show_interstitial();
      return [m._fixture_active_interstitial_id(), globalThis.__fixture.ads.length];
    })()`, deadline);
    assert.ok(second[0] > firstId);
    assert.equal(second[1], 2);
    await evaluate(client, `globalThis.__fixture.ads[0].resolve({ supported: true, shown: true, reason: "completed" })`, deadline);
    await new Promise((done) => setTimeout(done, 30));
    assert.equal(await evaluate(client, "fixtureModule._fixture_interstitial_completions()", deadline), 1);
    await evaluate(client, `globalThis.__fixture.ads[1].resolve({ supported: true, shown: true, reason: "completed" })`, deadline);
    await waitFor(client, "fixtureModule._fixture_interstitial_completions() === 2", deadline);
    assert.equal(await evaluate(client, "fixtureModule._fixture_last_interstitial_reason()", deadline), 7);

    await evaluate(client, "fixtureModule._fixture_cloud_load()", deadline);
    await waitFor(client, "globalThis.__fixture.loads.length === 1", deadline);
    await evaluate(client, "fixtureModule._fixture_cloud_load()", deadline);
    await waitFor(client, "globalThis.__fixture.loads.length === 2", deadline);
    await evaluate(client, `globalThis.__fixture.loads[0].resolve({ status: "found", value: "stale" })`, deadline);
    await new Promise((done) => setTimeout(done, 30));
    assert.equal(await evaluate(client, "fixtureModule._fixture_cloud_load_status()", deadline), 1);
    await evaluate(client, `globalThis.__fixture.loads[1].resolve({ status: "found", value: "current" })`, deadline);
    await waitFor(client, "fixtureModule._fixture_cloud_load_status() === 2", deadline);
    assert.equal(await evaluate(client, "fixtureModule._fixture_cloud_take_is()", deadline), 1);

    await evaluate(client, "fixtureModule._fixture_cloud_store()", deadline);
    await waitFor(client, "globalThis.__fixture.stores.length === 1", deadline);
    await evaluate(client, `globalThis.__fixture.stores[0].resolve({ status: "acknowledged" })`, deadline);
    await waitFor(client, "fixtureModule._fixture_cloud_write_status() === 2", deadline);
    await evaluate(client, "fixtureModule._fixture_cloud_store()", deadline);
    await waitFor(client, "globalThis.__fixture.stores.length === 2", deadline);
    await evaluate(client, `globalThis.__fixture.stores[1].reject(new Error("quota"))`, deadline);
    await waitFor(client, "fixtureModule._fixture_cloud_write_status() === 4", deadline);
  } catch (error) {
    if (error?.code === "MISSING_WEB_TEST_TOOL") context.skip(error.message);
    else throw error;
  } finally {
    if (browser) await Promise.resolve(browser.close());
    if (server) await new Promise((done) => server.close(done));
  }
});
