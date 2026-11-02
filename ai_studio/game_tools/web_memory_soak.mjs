#!/usr/bin/env node
// Memory soak of a web build: a seeded driver plays the transitions a session
// goes through for --minutes while the WebGL memory census
// (web_memory_census.mjs) samples the WebAssembly heap, its grows, the GPU
// ledger, programs and the JS heap every --sample seconds. After --warmup
// minutes nothing may keep growing: the peak of the last third of the samples
// may not exceed the peak of the first post-warm-up third by more than the
// series' tolerance. Rule: ai_studio/quality/rules/technical/checks/QTECH_003_memory_budget.md.
//
//   node ai_studio/game_tools/web_memory_soak.mjs --site <dir> | --url <url>
//     [--driver <module.mjs>] [--minutes 30] [--warmup 10] [--sample 10] [--seed 1]
//     [--viewport 390x844@2] [--out <dir>]
//
// Without --driver the seeded UI monkey taps, drags and presses keys anywhere on
// the page. A game driver module exports `createSoakDriver(options)` returning
// { openPage?, ready?, actions: [[name, async (ctx) => {}]], tick?, sample?,
// tolerance?, informational?, pauseMs?: [min, max], columns? }; see the
// "Memory soak" section of ai_studio/game_tools/README.md.
import { mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { launchHeadlessChrome, serveSites } from "./package_web_smoke.mjs";
import { DEFAULT_MEMORY_VIEWPORT, parseViewport } from "./web_memory_budget.mjs";
import { MEMORY_CENSUS_HOOK, readMemoryCensus } from "./web_memory_census.mjs";

const MB = 1048576;

// Allowed rise of a series' peak between the early and late post-warm-up thirds.
export const SOAK_TOLERANCE = Object.freeze({ wasm_bytes: 1 * MB, wasm_grows: 0, gpu_bytes: 2 * MB, gpu_objects: 64, programs: 2 });
// Reported, never judged: Chrome's heap figure moves with collection timing.
export const SOAK_INFORMATIONAL = Object.freeze(["js_heap_used_bytes"]);

export function seededRandom(seed) {
  let x = seed >>> 0 || 1;
  return () => { x ^= x << 13; x ^= x >>> 17; x ^= x << 5; return (x >>> 0) / 4294967296; };
}

// Peak growth between the first and last third of the post-warm-up samples, and the least-squares slope per hour.
export function judgeSoak(samples, warmupSeconds, { tolerance = SOAK_TOLERANCE, informational = SOAK_INFORMATIONAL } = {}) {
  const settled = samples.filter((s) => s.t >= warmupSeconds);
  const third = Math.floor(settled.length / 3);
  const verdict = { settled_samples: settled.length, series: {}, pass: third >= 2 };
  for (const key of [...Object.keys(tolerance), ...informational]) {
    const values = settled.filter((s) => Number.isFinite(s[key]));
    if (values.length < 6) {
      verdict.series[key] = null;
      // A judged series that stops reporting would otherwise pass as flat.
      if (key in tolerance) verdict.pass = false;
      continue;
    }
    const early = Math.max(...values.slice(0, third).map((s) => s[key]));
    const late = Math.max(...values.slice(-third).map((s) => s[key]));
    const n = values.length, mt = values.reduce((a, s) => a + s.t, 0) / n, mv = values.reduce((a, s) => a + s[key], 0) / n;
    const slope = values.reduce((a, s) => a + (s.t - mt) * (s[key] - mv), 0) / (values.reduce((a, s) => a + (s.t - mt) ** 2, 0) || 1);
    const ok = key in tolerance ? late - early <= tolerance[key] : true;
    verdict.series[key] = { early_peak: early, late_peak: late, growth: late - early, slope_per_hour: slope * 3600, ok };
    if (!ok) verdict.pass = false;
  }
  return verdict;
}

// A census sample: the fields every soak judges.
export function censusSample(census) {
  return {
    wasm_bytes: census.wasm_bytes, wasm_grows: census.wasm_grows, gpu_bytes: census.gpu_bytes,
    gpu_objects: Object.values(census.kinds || {}).reduce((a, k) => a + k.count, 0), programs: census.programs,
    js_heap_used_bytes: census.js_heap_used_bytes,
  };
}

// A fresh headless Chrome on `url` with the census installed before any page code.
export async function openCensusPage({ url, viewport = DEFAULT_MEMORY_VIEWPORT, profilePrefix = "ai-studio-memory-soak", chromeArgs = [] }) {
  const deadline = () => Date.now() + 120000;
  const chrome = await launchHeadlessChrome({ deadline: deadline(), profilePrefix,
    args: ["--disable-renderer-backgrounding", "--disable-background-timer-throttling", "--enable-precise-memory-info", ...chromeArgs] });
  const call = (method, params = {}) => chrome.client.call(method, params, deadline());
  const errors = [];
  const page = {
    errors, call, close: chrome.close, viewport: parseViewport(viewport),
    async evaluate(expression) {
      const r = await call("Runtime.evaluate", { expression, returnByValue: true, awaitPromise: true });
      if (r.exceptionDetails) throw new Error(`page: ${(r.exceptionDetails.exception?.description || r.exceptionDetails.text || "").slice(0, 300)}`);
      return r.result?.value;
    },
    census: (topN) => readMemoryCensus(page.evaluate, topN),
  };
  try {
    chrome.client.on("Runtime.exceptionThrown", (e) => errors.push((e.exceptionDetails?.exception?.description || e.exceptionDetails?.text || "").slice(0, 300)));
    await call("Runtime.enable");
    await call("Page.enable");
    const { width, height, deviceScaleFactor } = page.viewport;
    await call("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor, mobile: true });
    await call("Emulation.setTouchEmulationEnabled", { enabled: true, maxTouchPoints: 5 });
    await call("Page.addScriptToEvaluateOnNewDocument", { source: MEMORY_CENSUS_HOOK });
    await call("Page.navigate", { url });
    return page;
  } catch (error) {
    await chrome.close();
    throw error;
  }
}

// Ready once the page made a WebGL context and the census holds the module's memory.
export async function waitForCensus(page, timeoutMs = 180000) {
  const until = Date.now() + timeoutMs;
  const probe = "Boolean(window.__memoryCensus && window.__memoryCensus.contexts.length && window.__memoryCensus.memory)";
  while (!(await page.evaluate(probe).catch(() => false))) {
    if (Date.now() > until) throw new Error("the page made no WebGL context and no WebAssembly memory");
    await new Promise((done) => setTimeout(done, 250));
  }
}

const KEYS = [["Escape", 27], ["Enter", 13], [" ", 32], ["ArrowLeft", 37], ["ArrowRight", 39], ["ArrowUp", 38], ["ArrowDown", 40],
  ["w", 87], ["a", 65], ["s", 83], ["d", 68], ["Tab", 9]];

// The fallback driver for a game without its own: seeded taps, drags and keys
// over the whole viewport, which reach menus, windows and the scene alike.
export function uiMonkeyDriver() {
  const point = ({ page, rand }) => ({ x: Math.round(rand() * page.viewport.width), y: Math.round(rand() * page.viewport.height) });
  const touch = (page, type, points) => page.call("Input.dispatchTouchEvent", { type, touchPoints: points });
  return {
    actions: [
      ["tap", async (ctx) => {
        const p = point(ctx);
        await touch(ctx.page, "touchStart", [p]);
        await ctx.sleep(60);
        await touch(ctx.page, "touchEnd", []);
      }],
      ["drag", async (ctx) => {
        const from = point(ctx), to = point(ctx);
        await touch(ctx.page, "touchStart", [from]);
        for (let step = 1; step <= 8; step += 1) {
          await ctx.sleep(30);
          await touch(ctx.page, "touchMove", [{ x: from.x + ((to.x - from.x) * step) / 8, y: from.y + ((to.y - from.y) * step) / 8 }]);
        }
        await touch(ctx.page, "touchEnd", []);
      }],
      ["key", async (ctx) => {
        const [key, code] = ctx.pick(KEYS);
        await ctx.page.call("Input.dispatchKeyEvent", { type: "keyDown", key, windowsVirtualKeyCode: code });
        await ctx.sleep(80 + ctx.rand() * 600);
        await ctx.page.call("Input.dispatchKeyEvent", { type: "keyUp", key, windowsVirtualKeyCode: code });
      }],
    ],
    pauseMs: [300, 1500],
  };
}

function countBy(actions) {
  const counts = {};
  for (const a of actions) {
    const c = counts[a.name] ||= { done: 0, errors: 0 };
    if (a.error) c.errors += 1; else c.done += 1;
  }
  return counts;
}

const mb = (bytes) => (bytes == null ? "-" : (bytes / MB).toFixed(1));

export function soakMarkdown(report) {
  const lines = [`Soak (${report.driver}): ${report.minutes} min, warm-up ${report.warmup} min, seed ${report.seed}, viewport ${report.viewport}; `
    + `verdict ${report.verdict.pass ? "flat" : "GROWS"}.`, "",
  "| series | early peak | late peak | growth | slope /h | ok |", "|---|---|---|---|---|---|"];
  for (const [key, s] of Object.entries(report.verdict.series)) {
    if (!s) continue;
    const show = (v) => (/bytes/.test(key) ? `${(v / MB).toFixed(2)} MB` : v.toFixed(2));
    lines.push(`| ${key} | ${show(s.early_peak)} | ${show(s.late_peak)} | ${show(s.growth)} | ${show(s.slope_per_hour)} | ${s.ok ? "yes" : "NO"} |`);
  }
  const columns = report.columns;
  lines.push("", `| t min | ${columns.map((key) => key.replace(/_bytes$/, " MB")).join(" | ")} | actions |`, `|---|${columns.map(() => "---|").join("")}---|`);
  const every = Math.max(1, Math.round(report.samples.length / 24));
  report.samples.forEach((s, i) => {
    if (i % every !== 0 && i !== report.samples.length - 1) return;
    lines.push(`| ${(s.t / 60).toFixed(1)} | ${columns.map((key) => (/bytes$/.test(key) ? mb(s[key]) : s[key] ?? "-")).join(" | ")} | ${s.actions} |`);
  });
  lines.push("", "Actions: " + Object.entries(report.action_counts).map(([k, c]) => `${k} ${c.done}${c.errors ? ` (${c.errors} refused)` : ""}`).join(", ") + ".", "");
  return lines.join("\n");
}

// driver: see the header. url: a page to open, or site: a directory to serve.
export async function runMemorySoak({ url, site, driver = uiMonkeyDriver(), driverName = "ui-monkey", minutes = 30, warmup = 10, sample = 10,
  seed = 1, out, port = 0, viewport = DEFAULT_MEMORY_VIEWPORT, log = console.log }) {
  if (!url === !site) throw new Error("memory soak: give a url or a site");
  mkdirSync(out, { recursive: true });
  const server = site ? await serveSites([site], port) : null;
  const pageUrl = url || `${server.origin}0/`;
  const open = driver.openPage || openCensusPage;
  const page = await open({ url: pageUrl, viewport }).catch(async (error) => { await server?.close(); throw error; });
  const rand = seededRandom(seed);
  const sleep = (ms) => new Promise((done) => setTimeout(done, ms));
  const ctx = { page, rand, sleep, pick: (list) => list[Math.floor(rand() * list.length)] };
  const tolerance = { ...SOAK_TOLERANCE, ...driver.tolerance };
  const informational = [...SOAK_INFORMATIONAL, ...(driver.informational || [])];
  const [pauseMin, pauseMax] = driver.pauseMs || [2000, 8000];
  const samples = [], actions = [];
  let start = Date.now();
  const act = async (name, steps) => {
    let error = null;
    try { await steps(ctx); } catch (e) { error = e.message.slice(0, 200); }
    actions.push({ t: Math.round((Date.now() - start) / 1000), name, error });
    if (error && driver.fatal?.test(error)) throw new Error(error);
  };
  const take = async () => {
    const row = { t: Math.round((Date.now() - start) / 1000), ...censusSample(await page.census(4)),
      ...(driver.sample ? await driver.sample(page) : {}), actions: actions.length, action_errors: actions.filter((a) => a.error).length };
    samples.push(row);
    log(`t=${row.t}s wasm ${mb(row.wasm_bytes)} (${row.wasm_grows} grows) gpu ${mb(row.gpu_bytes)} objects ${row.gpu_objects} `
      + `programs ${row.programs} js ${mb(row.js_heap_used_bytes)} actions ${row.actions}`);
  };
  try {
    await (driver.ready ? driver.ready(page) : waitForCensus(page));
    start = Date.now();
    let nextSample = 0;
    while (Date.now() - start < minutes * 60000) {
      if (Date.now() - start >= nextSample) { await take(); nextSample += sample * 1000; }
      if (driver.tick) await act(driver.tick[0], driver.tick[1]);
      const [name, steps] = ctx.pick(driver.actions);
      await act(name, steps);
      await sleep(pauseMin + rand() * (pauseMax - pauseMin));
      if (page.errors.length) throw new Error(`page errors: ${page.errors.join(" | ")}`);
    }
    await take();
  } finally {
    await page.close();
    await server?.close();
  }
  const verdict = judgeSoak(samples, warmup * 60, { tolerance, informational });
  const columns = driver.columns || ["wasm_bytes", "wasm_grows", "gpu_bytes", "gpu_objects", "programs", "js_heap_used_bytes"];
  const report = { url: pageUrl, site: site ?? null, driver: driverName, minutes, warmup, sample, seed, viewport, verdict, columns,
    action_counts: countBy(actions), samples, actions };
  writeFileSync(join(out, "soak.json"), `${JSON.stringify(report, null, 2)}\n`);
  writeFileSync(join(out, "soak.md"), soakMarkdown(report));
  return report;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const opt = { minutes: "30", warmup: "10", sample: "10", seed: "1", port: "0", viewport: DEFAULT_MEMORY_VIEWPORT };
  const argv = process.argv.slice(2);
  for (let i = 0; i < argv.length; i += 2) {
    if (!argv[i].startsWith("--") || i + 1 >= argv.length) throw new Error(`bad argument ${argv[i]}`);
    opt[argv[i].slice(2)] = argv[i + 1];
  }
  if (!opt.site === !opt.url) {
    throw new Error("usage: node ai_studio/game_tools/web_memory_soak.mjs --site <dir>|--url <url> [--driver <module.mjs>] [--minutes 30] [--warmup 10] [--sample 10] [--seed 1]");
  }
  const options = { minutes: Number(opt.minutes), warmup: Number(opt.warmup), sample: Number(opt.sample), seed: Number(opt.seed),
    port: Number(opt.port), viewport: opt.viewport };
  const driver = opt.driver ? (await import(pathToFileURL(resolve(opt.driver)).href)).createSoakDriver(options) : uiMonkeyDriver();
  const out = resolve(opt.out ?? join("tmp", "memory_soak", new Date().toISOString().replace(/[:.]/g, "-")));
  const report = await runMemorySoak({ ...options, site: opt.site && resolve(opt.site), url: opt.url, driver,
    driverName: opt.driver ? opt.driver : "ui-monkey", out });
  console.log(`\n${soakMarkdown(report)}\n${join(out, "soak.json")}`);
  if (!report.verdict.pass) process.exitCode = 1;
}
