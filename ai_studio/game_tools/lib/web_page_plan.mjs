// Drives a packaged web game from outside the runtime, without DevAPI: unpack a
// store ZIP, open it in headless Chrome and run a plan of input, wait and capture
// steps. Shared by web_shot.mjs and the GL inventory of web_memory_census.mjs.
//
// Plan steps, each a JSON array:
//   ["wait",ms] ["shot",name] ["burst",name,count,gapMs] ["key"|"hold",k,ms]
//   ["walkburst",k,name,count,gapMs] (burst while k is held) ["keys",[k1,k2],ms]
//   ["click",x,y] (a tap on a touch viewport) ["move",x,y] (mouse hover)
//   ["perf",ms,name] (frame times) ["blur"] (the canvas loses keyboard focus)
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";

import { readStoreZip } from "../../core_harness/tool_lib/zip_store.mjs";

export const VIEWPORTS = Object.freeze({
  desktop: { width: 1280, height: 720, deviceScaleFactor: 1, mobile: false },
  phone: { width: 390, height: 844, deviceScaleFactor: 2, mobile: true },
  "phone-land": { width: 844, height: 390, deviceScaleFactor: 2, mobile: true },
});

const NAMED_KEYS = { " ": [32, "Space"], Escape: [27, "Escape"] };
const STEP_OPS = ["wait", "shot", "burst", "walkburst", "key", "hold", "keys", "click", "move", "perf", "blur"];
// What a packaged page needs before its first input; there is no DevAPI to ask it.
const LOAD_MS = 9000;
const AFTER_FIRST_TAP_MS = 1500;
const CHROME_ARGS = ["--use-gl=angle", "--enable-gpu", "--ignore-gpu-blocklist", "--enable-precise-memory-info",
  "--disable-renderer-backgrounding", "--disable-background-timer-throttling", "--autoplay-policy=no-user-gesture-required"];
const FRAME_RECORDER = `(() => { const f = window.__webPlanFrames = []; const raf = window.requestAnimationFrame.bind(window);
  window.requestAnimationFrame = (cb) => raf((t) => { if (f.at(-1) !== t) f.push(t); return cb(t); }); })();`;

const defaultSleep = (ms) => new Promise((done) => setTimeout(done, ms));

export function keyEvent(key) {
  if (NAMED_KEYS[key]) return { key, code: NAMED_KEYS[key][1], windowsVirtualKeyCode: NAMED_KEYS[key][0], nativeVirtualKeyCode: NAMED_KEYS[key][0] };
  if (!/^[a-z]$/.test(key)) throw new Error(`unknown key ${JSON.stringify(key)}`);
  const virtual = key.toUpperCase().charCodeAt(0);
  return { key, code: `Key${key.toUpperCase()}`, windowsVirtualKeyCode: virtual, nativeVirtualKeyCode: virtual };
}

// --flag value pairs; a flag outside `known` is an error, so a typo never passes silently.
export function parseFlags(argv, known) {
  const flags = {};
  for (let index = 0; index < argv.length; index += 2) {
    const name = String(argv[index]).replace(/^--/, "");
    if (!String(argv[index]).startsWith("--") || !known.includes(name)) throw new Error(`unknown argument ${argv[index]}`);
    if (index + 1 >= argv.length) throw new Error(`${argv[index]} needs a value`);
    flags[name] = argv[index + 1];
  }
  return flags;
}

// Refused before Chrome starts, so a typo at the end of a plan costs no run.
export function checkPlan(plan) {
  if (!Array.isArray(plan)) throw new Error("a plan is a JSON array of steps");
  for (const step of plan) {
    if (!Array.isArray(step) || !STEP_OPS.includes(step[0])) throw new Error(`unknown plan step ${JSON.stringify(step)}`);
    if (["key", "hold", "walkburst"].includes(step[0])) keyEvent(step[1]);
    if (step[0] === "keys") step[1].forEach(keyEvent);
  }
  return plan;
}

// JSON text, or the path of a JSON file.
export function readJsonInput(text) {
  return JSON.parse(existsSync(text) ? readFileSync(text, "utf8") : text);
}

// Page scripts installing storage before the game reads it. The save goes in once per
// tab, so a reload in the plan sees what the game wrote; storage entries go in on every load.
export function storageScripts({ save = null, saveKey = null, storage = null } = {}) {
  if (save !== null && !saveKey) throw new Error("--save needs --save-key: the storage slot the game reads");
  const scripts = [];
  if (save !== null) {
    scripts.push(`try{if(!sessionStorage.getItem('__web_plan_seeded')){localStorage.setItem(${JSON.stringify(saveKey)},${JSON.stringify(save)});sessionStorage.setItem('__web_plan_seeded','1');}}catch(e){console.error('seed',e)}`);
  }
  if (storage) {
    scripts.push(`try{for(const [k,v] of Object.entries(${JSON.stringify(storage)}))localStorage.setItem(k,v);}catch(e){console.error('seed',e)}`);
  }
  return scripts;
}

// The storage options web_shot and the inventory share: --save, --save-key, --storage.
export function storageFromFlags(flags) {
  return storageScripts({ save: flags.save ? readFileSync(flags.save, "utf8") : null, saveKey: flags["save-key"],
    storage: flags.storage ? JSON.parse(flags.storage) : null });
}

export function viewportFor(mode) {
  const viewport = VIEWPORTS[mode];
  if (!viewport) throw new Error(`unknown mode ${mode}; one of ${Object.keys(VIEWPORTS).join(", ")}`);
  return viewport;
}

export function frameStats(times) {
  const gaps = times.slice(1).map((time, index) => time - times[index]).sort((a, b) => a - b);
  if (gaps.length === 0) return { n: 0, mean: null, p95: null };
  return { n: gaps.length, mean: +(gaps.reduce((sum, gap) => sum + gap, 0) / gaps.length).toFixed(2),
    p95: +gaps[Math.floor(gaps.length * 0.95)].toFixed(2) };
}

// call(method, params) is a CDP call; shot(name) captures one picture.
export function createPlanRunner({ call, viewport, shot, sleep = defaultSleep }) {
  const key = (type, name) => call("Input.dispatchKeyEvent", { type, ...keyEvent(name) });
  const tap = async (x, y) => {
    if (viewport.mobile) {
      await call("Input.dispatchTouchEvent", { type: "touchStart", touchPoints: [{ x, y }] });
      await sleep(80);
      await call("Input.dispatchTouchEvent", { type: "touchEnd", touchPoints: [] });
      return;
    }
    await call("Input.dispatchMouseEvent", { type: "mouseMoved", x, y });
    await call("Input.dispatchMouseEvent", { type: "mousePressed", x, y, button: "left", clickCount: 1 });
    await sleep(60);
    await call("Input.dispatchMouseEvent", { type: "mouseReleased", x, y, button: "left", clickCount: 1 });
  };
  const frames = async (expression) => (await call("Runtime.evaluate", { expression, returnByValue: true })).result.value;
  const run = async (plan) => {
    const perf = {};
    for (const [op, a, b, c, d] of plan) {
      if (op === "wait") await sleep(a);
      else if (op === "shot") await shot(a);
      else if (op === "burst") for (let i = 0; i < b; i += 1) { await shot(`${a}${i}`); await sleep(c); }
      else if (op === "walkburst") {
        await key("rawKeyDown", a);
        for (let i = 0; i < c; i += 1) { await shot(`${b}${i}`); await sleep(d); }
        await key("keyUp", a);
      } else if (op === "key" || op === "hold") { await key("rawKeyDown", a); await sleep(b); await key("keyUp", a); }
      else if (op === "keys") {
        for (const name of a) await key("rawKeyDown", name);
        await sleep(b);
        for (const name of a) await key("keyUp", name);
      } else if (op === "click") await tap(a, b);
      else if (op === "move") await call("Input.dispatchMouseEvent", { type: "mouseMoved", x: a, y: b });
      else if (op === "blur") await call("Runtime.evaluate", { expression: "document.activeElement && document.activeElement.blur()" });
      else if (op === "perf") {
        const start = await frames("window.__webPlanFrames.length");
        await sleep(a);
        perf[b || "perf"] = frameStats(await frames(`window.__webPlanFrames.slice(${start})`));
      } else throw new Error(`unknown plan step ${op}`);
    }
    return perf;
  };
  return { tap, run };
}

export function unpackStoreZip(zip, prefix = "web-plan-site-") {
  const site = mkdtempSync(join(tmpdir(), prefix));
  for (const [path, bytes] of readStoreZip(readFileSync(resolve(zip)))) {
    if (path.endsWith("/")) continue;
    mkdirSync(dirname(join(site, path)), { recursive: true });
    writeFileSync(join(site, path), bytes);
  }
  return site;
}

// Serves the site and opens it in Chrome with `scripts` installed before any page code;
// close() in the caller's finally stops Chrome, disposes its profile and stops the server.
export async function openGamePage({ site, viewport, scripts = [], lang = null, shot = async () => {}, profilePrefix, budgetMs = 600000 }) {
  const { launchHeadlessChrome, serveSites } = await import("../package_web_smoke.mjs");
  const deadline = Date.now() + budgetMs;
  const server = await serveSites([site]);
  let chrome;
  try {
    chrome = await launchHeadlessChrome({ deadline, profilePrefix, args: CHROME_ARGS });
  } catch (error) {
    await server.close();
    throw error;
  }
  const call = (method, params = {}) => chrome.client.call(method, params, deadline);
  const logs = [];
  chrome.client.on("Runtime.consoleAPICalled", ({ type, args }) => logs.push(`${type} ${args.map((value) => value.value ?? value.description).join(" ")}`));
  const close = async () => { await chrome.close(); await server.close(); };
  try {
    await call("Page.enable");
    await call("Runtime.enable");
    await call("Emulation.setDeviceMetricsOverride", viewport);
    if (viewport.mobile) await call("Emulation.setTouchEmulationEnabled", { enabled: true, maxTouchPoints: 5 });
    for (const source of [FRAME_RECORDER, ...scripts]) await call("Page.addScriptToEvaluateOnNewDocument", { source });
  } catch (error) {
    await close();
    throw error;
  }
  const runner = createPlanRunner({ call, viewport, shot: (name) => shot(name, call) });
  const url = `${server.origin}0/index.html${lang ? `?lang=${encodeURIComponent(lang)}` : ""}`;
  return {
    call, logs, close, run: runner.run,
    evaluate: async (expression) => (await call("Runtime.evaluate", { expression, returnByValue: true })).result.value,
    // The first tap gives the page its focus and the user gesture its audio waits for.
    load: async () => {
      await call("Page.navigate", { url });
      await defaultSleep(LOAD_MS);
      await runner.tap(viewport.width / 2, viewport.height / 2);
      await defaultSleep(AFTER_FIRST_TAP_MS);
    },
  };
}
