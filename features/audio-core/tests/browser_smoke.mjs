import assert from "node:assert/strict";
import { createReadStream, existsSync, statSync } from "node:fs";
import { createServer } from "node:http";
import { extname, join, normalize, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const artifact = resolve(process.argv[2] || "templates/template/build/wasm-debug/bin");
assert.ok(existsSync(join(artifact, "index.html")), `missing web artifact: ${artifact}`);

async function loadPlaywright() {
  try {
    return await import("playwright");
  } catch {
    const configured = process.env.PLAYWRIGHT_MODULE;
    if (!configured || !existsSync(configured)) {
      throw new Error("playwright is unavailable; install it or set PLAYWRIGHT_MODULE to playwright/index.mjs");
    }
    return import(pathToFileURL(configured).href);
  }
}

const mime = new Map([
  [".html", "text/html; charset=utf-8"], [".js", "text/javascript; charset=utf-8"],
  [".wasm", "application/wasm"], [".ntpack", "application/octet-stream"],
]);
const server = createServer((request, response) => {
  const pathname = decodeURIComponent(new URL(request.url, "http://127.0.0.1").pathname);
  const relative = pathname === "/" ? "index.html" : pathname.slice(1);
  const file = resolve(artifact, normalize(relative));
  if (file !== artifact && !file.startsWith(`${artifact}\\`) && !file.startsWith(`${artifact}/`)) {
    response.writeHead(403).end();
    return;
  }
  if (!existsSync(file) || !statSync(file).isFile()) {
    response.writeHead(404).end();
    return;
  }
  response.writeHead(200, { "content-type": mime.get(extname(file)) || "application/octet-stream" });
  createReadStream(file).pipe(response);
});
await new Promise((resolveListen) => server.listen(0, "127.0.0.1", resolveListen));

const { chromium } = await loadPlaywright();
const browser = await chromium.launch({
  headless: process.env.AUDIO_SMOKE_HEADED !== "1",
  channel: process.env.AUDIO_SMOKE_BROWSER_CHANNEL || undefined,
  args: ["--autoplay-policy=user-gesture-required"],
});
const page = await browser.newPage();
const errors = [];
page.on("console", (message) => { if (message.type() === "error") errors.push(`console: ${message.text()}`); });
page.on("pageerror", (error) => errors.push(`page: ${error.message}`));
page.on("requestfailed", (request) => errors.push(`request: ${request.url()} ${request.failure()?.errorText || "failed"}`));

await page.addInitScript(() => {
  globalThis.__audioSmoke = { contexts: [], decodeBytes: [], decoded: 0, decodeErrors: 0, starts: [], stops: 0, resumes: 0, suspends: 0 };
  const Original = window.AudioContext || window.webkitAudioContext;
  const Wrapped = new Proxy(Original, {
    construct(target, args) {
      const context = Reflect.construct(target, args);
      __audioSmoke.contexts.push(context);
      const decode = context.decodeAudioData.bind(context);
      context.decodeAudioData = (bytes, ...rest) => {
        __audioSmoke.decodeBytes.push(bytes.byteLength);
        const result = decode(bytes, ...rest);
        if (result && typeof result.then === "function") {
          result.then(() => { __audioSmoke.decoded += 1; }, () => { __audioSmoke.decodeErrors += 1; });
        }
        return result;
      };
      const createSource = context.createBufferSource.bind(context);
      context.createBufferSource = () => {
        const source = createSource();
        const start = source.start.bind(source);
        source.start = (...startArgs) => {
          __audioSmoke.starts.push({ loop: source.loop, duration: source.buffer?.duration || 0 });
          return start(...startArgs);
        };
        const stop = source.stop.bind(source);
        source.stop = (...stopArgs) => { __audioSmoke.stops += 1; return stop(...stopArgs); };
        return source;
      };
      const resume = context.resume.bind(context);
      context.resume = (...resumeArgs) => { __audioSmoke.resumes += 1; return resume(...resumeArgs); };
      const suspend = context.suspend.bind(context);
      context.suspend = (...suspendArgs) => { __audioSmoke.suspends += 1; return suspend(...suspendArgs); };
      // Chrome may create a running context even under the strict autoplay
      // flag on some desktop profiles. Force the deterministic pre-gesture
      // state; only the runtime's real pointer listener may resume it.
      void context.suspend();
      return context;
    },
  });
  window.AudioContext = Wrapped;
  window.webkitAudioContext = Wrapped;
});

try {
  const address = server.address();
  await page.goto(`http://127.0.0.1:${address.port}/`, { waitUntil: "load" });
  await page.waitForFunction(() => globalThis.Module?._game_audio_play_cue, null, { timeout: 30_000 });
  await page.waitForFunction(() => __audioSmoke.decoded >= 2, null, { timeout: 15_000 });
  const beforeClick = await page.evaluate(() => ({
    state: __audioSmoke.contexts.at(-1)?.state,
    resumes: __audioSmoke.resumes,
    starts: __audioSmoke.starts.length,
  }));
  assert.deepEqual(beforeClick, { state: "suspended", resumes: 0, starts: 0 });
  const canvas = page.locator("canvas");
  await canvas.click({ position: { x: 32, y: 32 } });
  await page.waitForFunction(() => __audioSmoke.contexts.at(-1)?.state === "running");
  await page.waitForFunction(() => Module._game_audio_play_cue(0) === 1, null, { timeout: 15_000 });
  assert.equal(await page.evaluate(() => Module._game_audio_play_music(0, 1)), 1, "music did not start");
  await page.evaluate(() => Module._game_audio_set_paused(1));
  await page.waitForFunction(() => __audioSmoke.contexts.at(-1)?.state === "suspended");
  await page.evaluate(() => Module._game_audio_set_paused(0));
  await page.waitForFunction(() => __audioSmoke.contexts.at(-1)?.state === "running");
  await page.evaluate(() => Module._game_audio_stop_music());
  const proof = await page.evaluate(() => ({
    decodeBytes: __audioSmoke.decodeBytes,
    decoded: __audioSmoke.decoded,
    decodeErrors: __audioSmoke.decodeErrors,
    starts: __audioSmoke.starts,
    stops: __audioSmoke.stops,
    resumes: __audioSmoke.resumes,
    suspends: __audioSmoke.suspends,
    contextState: __audioSmoke.contexts.at(-1)?.state,
  }));
  assert.deepEqual([...proof.decodeBytes].sort((a, b) => a - b), [8098, 28883]);
  assert.equal(proof.decodeErrors, 0);
  assert.ok(proof.starts.length >= 2 && proof.starts.some((source) => source.loop));
  assert.ok(proof.stops >= 1 && proof.resumes >= 2 && proof.suspends >= 1);
  assert.equal(proof.contextState, "running");
  assert.deepEqual(errors, []);
  console.log(JSON.stringify({ ok: true, artifact, ...proof }));
} catch (error) {
  const diagnostics = await page.evaluate(() => ({
    audio: globalThis.__audioSmoke,
    moduleReady: Boolean(globalThis.Module?._game_audio_play_cue),
  })).catch(() => ({}));
  console.error(JSON.stringify({ errors, diagnostics }));
  throw error;
} finally {
  await browser.close();
  await new Promise((resolveClose) => server.close(resolveClose));
}
