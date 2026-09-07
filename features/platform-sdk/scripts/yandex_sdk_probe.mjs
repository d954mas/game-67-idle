#!/usr/bin/env node
/* Drives a packaged Yandex build inside the portal's own dev proxy and reports
 * what the SDK actually saw. Moderation runs for a month, so the lifecycle
 * calls it checks (requirement 1.20) and the load-time locale read (2.14) are
 * proven here, before the draft is sent.
 *
 *   node features/platform-sdk/scripts/yandex_sdk_probe.mjs  *     --artifact games/<id>/build/wasm-release-yandex/bin
 *
 * The proxy is started by this script (`--dev-mode=true`, no draft id needed);
 * pass --url to drive an already running one instead.
 */

import { spawn, spawnSync } from "node:child_process";
import { connect } from "node:net";
import { existsSync, writeFileSync } from "node:fs";
import { request } from "node:https";
import { dirname, join, resolve } from "node:path";

import { launchHeadlessBrowser } from "./lib/headless_browser.mjs";

const USAGE = "usage: node features/platform-sdk/scripts/yandex_sdk_probe.mjs --artifact <dir> [--port 8099] [--url https://localhost:8099] [--json <file>]";

/* Recorded inside the page before any game script runs. Everything the verdict
   needs is a fact the SDK or the game observed, never an inference from the
   game's own logging. */
const RECORDER = `(() => {
  const probe = {
    seq: 0,
    calls: [],
    listeners: [],
    localeReads: [],
    lang: null,
    errors: [],
  };
  globalThis.__yandexProbe = probe;

  /* Who called matters as much as what: sdk.js drives some of these methods
     and events itself, and a verdict that cannot tell the portal's own frames
     from the game's would pass a game that handles nothing. */
  const origin = () => {
    const frames = String(new Error().stack || "").split("\\n").slice(1);
    const frame = frames.find((line) => !line.includes("<anonymous>")) || "";
    return frame.trim().slice(0, 160) || "recorder";
  };

  const record = (name, detail) => {
    probe.calls.push({ seq: ++probe.seq, name, detail: detail === undefined ? null : detail, from: origin() });
  };

  const patchMethod = (owner, key, label) => {
    if (!owner || typeof owner[key] !== "function" || owner[key].__probeWrapped) return;
    const original = owner[key].bind(owner);
    const wrapped = (...args) => {
      record(label, args.length && typeof args[0] === "string" ? args[0] : null);
      return original(...args);
    };
    wrapped.__probeWrapped = true;
    try { owner[key] = wrapped; } catch (error) { probe.errors.push(String(error)); }
  };

  const patchLocale = (ysdk) => {
    const environmentDescriptor = Object.getOwnPropertyDescriptor(ysdk, "environment");
    probe.environmentKind = environmentDescriptor
      ? (environmentDescriptor.get ? "accessor" : "data")
      : "inherited";
    const environment = ysdk.environment;
    const i18n = environment && environment.i18n;
    if (!i18n || typeof i18n !== "object") {
      probe.errors.push("environment.i18n is missing");
      return;
    }
    probe.sameEnvironmentObject = ysdk.environment === environment;
    const value = i18n.lang;
    probe.lang = value === undefined ? null : value;
    try {
      Object.defineProperty(i18n, "lang", {
        configurable: true,
        get() {
          probe.localeReads.push({ seq: ++probe.seq, from: origin() });
          return value;
        },
      });
      probe.localePatched = true;
    } catch (error) { probe.errors.push(String(error)); }

    /* A defineProperty that a proxy quietly swallows would report "the game
       never read the locale" about a game that reads it on every launch, so the
       instrument proves itself before it is trusted. */
    const selfTest = () => {
      const before = probe.localeReads.length;
      void ysdk.environment.i18n.lang;
      const observed = probe.localeReads.length > before;
      if (observed) probe.localeReads.pop();
      return observed;
    };
    probe.localeInstrumentWorks = selfTest();
    if (!probe.localeInstrumentWorks) {
      try {
        ysdk.environment = {
          ...environment,
          i18n: new Proxy(i18n, {
            get(target, key, receiver) {
              if (key === "lang") probe.localeReads.push({ seq: ++probe.seq, from: origin() });
              return Reflect.get(target, key, receiver);
            },
          }),
        };
        probe.localeInstrumentWorks = selfTest();
      } catch (error) { probe.errors.push(String(error)); }
    }
  };

  const patchSdk = (ysdk) => {
    if (!ysdk || typeof ysdk !== "object") return ysdk;
    const features = ysdk.features || {};
    patchMethod(features.LoadingAPI, "ready", "LoadingAPI.ready");
    patchMethod(features.GameplayAPI, "start", "GameplayAPI.start");
    patchMethod(features.GameplayAPI, "stop", "GameplayAPI.stop");
    patchMethod(ysdk.adv, "showFullscreenAdv", "adv.showFullscreenAdv");
    patchMethod(ysdk.adv, "showRewardedVideo", "adv.showRewardedVideo");
    patchMethod(ysdk.adv, "showBannerAdv", "adv.showBannerAdv");
    patchMethod(ysdk, "getPlayer", "getPlayer");
    patchLocale(ysdk);
    return ysdk;
  };

  const patchYaGames = (value) => {
    if (!value || typeof value.init !== "function" || value.init.__probeWrapped) return value;
    const original = value.init.bind(value);
    const wrapped = (...args) => {
      record("YaGames.init");
      return Promise.resolve(original(...args)).then((ysdk) => {
        record("YaGames.init.resolved");
        return patchSdk(ysdk);
      });
    };
    wrapped.__probeWrapped = true;
    value.init = wrapped;
    return value;
  };

  let held = globalThis.YaGames;
  if (held) patchYaGames(held);
  try {
    Object.defineProperty(globalThis, "YaGames", {
      configurable: true,
      get() { return held; },
      set(value) { held = patchYaGames(value); },
    });
  } catch (error) { probe.errors.push(String(error)); }

  const watched = new Set([
    "game_api_pause",
    "game_api_resume",
    "visibilitychange",
    "blur",
    "focus",
    "contextmenu",
  ]);
  const originalAdd = EventTarget.prototype.addEventListener;
  EventTarget.prototype.addEventListener = function addEventListener(type, listener, options) {
    if (watched.has(type)) {
      const owner = this === globalThis ? "window" : (this === globalThis.document ? "document" : "other");
      probe.listeners.push({ seq: ++probe.seq, type, owner, from: origin() });
    }
    return originalAdd.call(this, type, listener, options);
  };
})();`;

function parseArgs(argv) {
  const args = { artifact: "", port: 8099, url: "", json: "", timeoutMs: 180000 };
  for (let index = 2; index < argv.length; ++index) {
    const arg = argv[index];
    if (arg === "--artifact" && argv[index + 1]) args.artifact = argv[++index];
    else if (arg === "--port" && argv[index + 1]) args.port = Number(argv[++index]);
    else if (arg === "--url" && argv[index + 1]) args.url = argv[++index];
    else if (arg === "--json" && argv[index + 1]) args.json = argv[++index];
    else if (arg === "--timeout" && argv[index + 1]) args.timeoutMs = Number(argv[++index]) * 1000;
    else throw new Error(`${USAGE}\nunknown argument: ${arg}`);
  }
  if (!Number.isInteger(args.port) || args.port <= 0) throw new Error("--port must be a positive integer");
  return args;
}

function probeUrl(url) {
  return new Promise((done) => {
    const req = request(url, { method: "GET", rejectUnauthorized: false, timeout: 2000 }, (res) => {
      res.resume();
      done(res.statusCode ? res.statusCode < 500 : false);
    });
    req.on("error", () => done(false));
    req.on("timeout", () => { req.destroy(); done(false); });
    req.end();
  });
}

/* A proxy left behind by an earlier run answers on the same port and serves an
   older artifact, and the probe would report a verdict about a build nobody
   asked about. Refusing beats measuring the wrong thing. */
function portIsFree(port) {
  return new Promise((done) => {
    const socket = connect({ host: "127.0.0.1", port });
    const settle = (free) => { socket.destroy(); done(free); };
    socket.setTimeout(1000);
    socket.once("connect", () => settle(false));
    socket.once("timeout", () => settle(true));
    socket.once("error", () => settle(true));
  });
}

/* npx is a parent of the server it starts, so killing the child alone leaves
   the port held. */
function killTree(child) {
  if (!child || child.exitCode !== null) return;
  if (process.platform === "win32") {
    spawnSync("taskkill", ["/pid", String(child.pid), "/T", "/F"], { stdio: "ignore", windowsHide: true });
    return;
  }
  try { process.kill(-child.pid, "SIGKILL"); } catch { child.kill("SIGKILL"); }
}

async function waitForProxy(url, deadline) {
  for (;;) {
    if (await probeUrl(url)) return;
    if (Date.now() > deadline) throw new Error(`the Yandex dev proxy never answered on ${url}`);
    await new Promise((done) => setTimeout(done, 400));
  }
}

/* Spawned through npm's own npx script rather than the `npx` shim: a Windows
   `.cmd` shim is not a valid spawn target without a shell, and a shell would
   have to quote the artifact path. */
function npxCommand() {
  const bundled = join(dirname(process.execPath), "node_modules", "npm", "bin", "npx-cli.js");
  if (existsSync(bundled)) return [process.execPath, [bundled]];
  return [process.platform === "win32" ? "npx.cmd" : "npx", []];
}

function startProxy(artifactDir, port) {
  const [command, prefix] = npxCommand();
  const child = spawn(command, [
    ...prefix,
    "--yes",
    "@yandex-games/sdk-dev-proxy",
    "-p", artifactDir,
    "--dev-mode=true",
    "--log=false",
    "--port", String(port),
  ], {
    stdio: ["ignore", "ignore", "pipe"],
    windowsHide: true,
    detached: process.platform !== "win32",
    env: { ...process.env, NODE_OPTIONS: [process.env.NODE_OPTIONS, "--use-system-ca"].filter(Boolean).join(" ") },
  });
  let stderr = "";
  child.stderr.on("data", (bytes) => { stderr += bytes.toString("utf8"); });
  return { child, stderr: () => stderr };
}

async function readProbe(client, deadline) {
  const result = await client.call("Runtime.evaluate", {
    expression: "JSON.stringify(globalThis.__yandexProbe || null)",
    returnByValue: true,
    awaitPromise: false,
  }, deadline);
  const raw = result?.result?.value;
  return raw ? JSON.parse(raw) : null;
}

async function waitFor(client, predicate, deadline, label) {
  for (;;) {
    const probe = await readProbe(client, deadline);
    if (probe && predicate(probe)) return probe;
    if (Date.now() > deadline) throw new Error(`timed out waiting for ${label}`);
    await new Promise((done) => setTimeout(done, 500));
  }
}

async function pressCanvas(client, deadline) {
  const point = { x: 640, y: 400, button: "left", clickCount: 1 };
  await client.call("Input.dispatchMouseEvent", { type: "mouseMoved", ...point }, deadline);
  await client.call("Input.dispatchMouseEvent", { type: "mousePressed", ...point }, deadline);
  await new Promise((done) => setTimeout(done, 120));
  await client.call("Input.dispatchMouseEvent", { type: "mouseReleased", ...point }, deadline);
}

async function dispatchPortalEvent(client, name, deadline) {
  await client.call("Runtime.evaluate", {
    expression: `window.dispatchEvent(new Event(${JSON.stringify(name)}))`,
    returnByValue: true,
  }, deadline);
}

/* The portal's own sdk.js registers some of these events and calls some of
   these methods for its own reasons. Only a frame from the shipped bundle
   proves the game handles anything. */
function fromGame(frame) {
  const text = String(frame || "");
  if (!text || text === "recorder") return false;
  if (text.includes("/sdk.js") || text.includes("yandex.ru") || text.includes("yandex.net")) return false;
  return text.includes("game.js") || text.includes("platform-sdk");
}

function called(probe, name) {
  return probe.calls.filter((call) => call.name === name);
}

function verdict(probe, issues) {
  const rows = [];
  const readySeq = called(probe, "LoadingAPI.ready")[0]?.seq ?? null;
  const gameLocaleReads = probe.localeReads.filter((read) => fromGame(read.from));
  const localeFirst = gameLocaleReads[0]?.seq ?? null;
  const gameListeners = probe.listeners.filter((entry) => fromGame(entry.from));
  const listenerTypes = new Set(gameListeners.map((entry) => entry.type));

  rows.push({
    id: "1.20 LoadingAPI.ready",
    ok: readySeq !== null,
    detail: readySeq === null ? "never called" : `called (seq ${readySeq})`,
  });
  rows.push({
    id: "1.20 GameplayAPI.start",
    ok: called(probe, "GameplayAPI.start").length > 0,
    detail: `${called(probe, "GameplayAPI.start").length} call(s)`,
  });
  rows.push({
    id: "1.20 GameplayAPI.stop",
    ok: called(probe, "GameplayAPI.stop").length > 0,
    detail: `${called(probe, "GameplayAPI.stop").length} call(s)`,
  });
  rows.push({
    id: "1.20 game_api_pause / game_api_resume",
    ok: listenerTypes.has("game_api_pause") && listenerTypes.has("game_api_resume"),
    detail: `listeners: ${[...listenerTypes].join(", ") || "none"}`,
  });
  rows.push({
    id: "2.14 environment.i18n.lang read while loading",
    ok: localeFirst !== null && (readySeq === null || localeFirst <= readySeq),
    detail: localeFirst === null
      ? "never read"
      : `first read at seq ${localeFirst}, lang=${probe.lang ?? "null"}, ready at ${readySeq ?? "n/a"}`,
  });
  rows.push({
    id: "1.3 focus and visibility handled",
    ok: listenerTypes.has("visibilitychange") && (listenerTypes.has("blur") || listenerTypes.has("focus")),
    detail: `listeners: ${[...listenerTypes].join(", ") || "none"}`,
  });
  rows.push({
    id: "no page errors",
    ok: issues.length === 0,
    detail: issues.length === 0 ? "clean" : issues.slice(0, 5).join(" | "),
  });
  return rows;
}

async function main() {
  const args = parseArgs(process.argv);
  const artifactDir = args.artifact ? resolve(args.artifact) : "";
  const deadline = Date.now() + args.timeoutMs;
  const url = args.url || `https://localhost:${args.port}`;
  if (!args.url) {
    if (!artifactDir) throw new Error(`${USAGE}
--artifact is required unless --url points at a running proxy`);
    if (!existsSync(join(artifactDir, "index.html"))) {
      throw new Error(`no packaged Yandex artifact at ${artifactDir}; build it with: node tools/game.mjs package --target yandex`);
    }
  }

  if (!args.url && !(await portIsFree(args.port))) {
    throw new Error(`port ${args.port} is already in use; stop the process holding it or pass --port`);
  }

  const proxy = args.url ? null : startProxy(artifactDir, args.port);
  let browser = null;
  const issues = [];
  try {
    await waitForProxy(url, Math.min(deadline, Date.now() + 90000));
    browser = await launchHeadlessBrowser({
      deadline,
      extraArgs: ["--ignore-certificate-errors", "--allow-insecure-localhost"],
    });
    const { client } = browser;
    client.on("Runtime.exceptionThrown", ({ exceptionDetails }) => {
      issues.push(exceptionDetails?.exception?.description || exceptionDetails?.text || "page exception");
    });
    client.on("Runtime.consoleAPICalled", ({ type, args: values }) => {
      if (type !== "error") return;
      issues.push(values.map((value) => value.value ?? value.description ?? "").join(" ").slice(0, 200));
    });
    await client.call("Page.addScriptToEvaluateOnNewDocument", { source: RECORDER }, deadline);
    await client.call("Page.navigate", { url }, deadline);

    await waitFor(client, (probe) => called(probe, "LoadingAPI.ready").length > 0, deadline, "LoadingAPI.ready");
    await pressCanvas(client, deadline);
    await waitFor(client, (probe) => called(probe, "GameplayAPI.start").length > 0, deadline, "GameplayAPI.start");

    await dispatchPortalEvent(client, "game_api_pause", deadline);
    await new Promise((done) => setTimeout(done, 1000));
    await dispatchPortalEvent(client, "game_api_resume", deadline);
    await new Promise((done) => setTimeout(done, 1000));

    const probe = await readProbe(client, deadline);
    const rows = verdict(probe, issues);
    const report = {
      schema: "ai_studio.platform_sdk.yandex_probe.v1",
      url,
      artifact: artifactDir,
      lang: probe.lang ?? null,
      calls: probe.calls,
      listeners: probe.listeners,
      localeReads: probe.localeReads,
      localePatched: probe.localePatched === true,
      localeInstrumentWorks: probe.localeInstrumentWorks === true,
      environmentKind: probe.environmentKind || null,
      recorderErrors: probe.errors,
      issues,
      rows,
      ok: rows.every((row) => row.ok),
    };
    if (args.json) writeFileSync(resolve(args.json), `${JSON.stringify(report, null, 2)}\n`, "utf8");
    for (const row of rows) console.log(`${row.ok ? "PASS" : "FAIL"}  ${row.id}: ${row.detail}`);
    console.log(report.ok ? "yandex sdk probe passed" : "yandex sdk probe failed");
    if (!report.ok) process.exitCode = 1;
  } finally {
    if (browser) browser.close();
    if (proxy) killTree(proxy.child);
  }
}

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
