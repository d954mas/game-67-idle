#!/usr/bin/env node
// A game's web memory budget: the "memory" object of content/web_budget.json,
// beside the download "limits". Every limit names a field of the WebGL memory
// census (web_memory_census.mjs), measured on a phone viewport at startup and,
// with "after_seconds", again once the streamed content is in; the verify
// browser smoke reads it and fails on overflow at either point. Rule and method:
// ai_studio/quality/rules/technical/checks/QTECH_003_memory_budget.md.
//
//   "memory": { "viewport": "390x844@2", "after_seconds": 60,
//               "limits": { "wasm_bytes": 100663296, "gpu_bytes": 83886080, "wasm_grows": 0 } }
//
// Measuring a build without a budget (a site directory or a packaged zip):
//
//   node ai_studio/game_tools/web_memory_budget.mjs --site <dir>|--zip <package.zip> [--viewport 390x844@2] [--play 60] [--json]
//
// --play keeps the page running that many seconds without input and reads the
// census again; a game's own driver is needed for a real minute of play.
import { existsSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { unpackStoreZip } from "./lib/web_page_plan.mjs";
import { MEMORY_CENSUS_HOOK, readMemoryCensus } from "./web_memory_census.mjs";

const MB = 1024 * 1024;

// Limits a budget may set; bytes unless the name says otherwise.
export const MEMORY_LIMIT_FIELDS = Object.freeze([
  "wasm_bytes", "wasm_peak_bytes", "wasm_grows", "gpu_bytes", "gpu_peak_bytes", "js_heap_used_bytes", "programs",
]);
export const DEFAULT_MEMORY_VIEWPORT = "390x844@2";
export const MAX_AFTER_SECONDS = 300;

export function parseViewport(text) {
  const match = /^(\d+)x(\d+)(?:@([\d.]+))?$/.exec(String(text || ""));
  if (!match) throw new Error(`memory budget: bad viewport "${text}" (want WxH or WxH@DPR)`);
  return { width: Number(match[1]), height: Number(match[2]), deviceScaleFactor: Number(match[3] ?? 1) };
}

// The memory section of a parsed web_budget.json, or null when it has none.
export function memoryBudgetFrom(budget, label = "web_budget.json") {
  const memory = budget?.memory;
  if (memory === undefined) return null;
  const fail = (message) => { throw new Error(`${label}: memory ${message}`); };
  if (!memory || typeof memory !== "object" || Array.isArray(memory)) fail("must be an object");
  const limits = memory.limits;
  if (!limits || typeof limits !== "object" || Array.isArray(limits) || Object.keys(limits).length === 0) fail("needs a non-empty \"limits\" object");
  for (const [key, value] of Object.entries(limits)) {
    if (!MEMORY_LIMIT_FIELDS.includes(key)) fail(`limit "${key}" is not one of ${MEMORY_LIMIT_FIELDS.join(", ")}`);
    if (!Number.isSafeInteger(value) || value < 0) fail(`limit "${key}" must be a non-negative integer`);
  }
  const viewport = memory.viewport ?? DEFAULT_MEMORY_VIEWPORT;
  parseViewport(viewport);
  const after = memory.after_seconds ?? 0;
  if (!Number.isSafeInteger(after) || after < 0 || after > MAX_AFTER_SECONDS) fail(`after_seconds must be an integer 0..${MAX_AFTER_SECONDS}`);
  return { viewport, afterSeconds: after, limits: { ...limits } };
}

export function readMemoryBudget(gameDir, path = join(gameDir, "content", "web_budget.json")) {
  if (!existsSync(path)) return null;
  return memoryBudgetFrom(JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, "")), path);
}

function show(key, value) {
  return /bytes$/.test(key) ? `${(value / MB).toFixed(1)} MiB` : String(value);
}

const asPoints = (memory) => (Array.isArray(memory) ? memory : memory ? [memory] : []);

// memory: one census or the measured points ({ point, settled, ...census });
// one failure per limit passed or not measured, and per point that never settled.
export function assessMemoryBudget(memory, budget) {
  if (!budget) return [];
  const points = asPoints(memory);
  if (points.length === 0) return ["memory census missing: the page was never measured"];
  const failures = [];
  for (const census of points) {
    const at = census.point ? ` at ${census.point}` : "";
    if (census.settled === false) {
      failures.push(`memory census${at} did not settle in ${Math.round((census.waited_ms ?? 0) / 1000)} s: heap, GPU or programs still changing`);
      continue;
    }
    for (const [key, limit] of Object.entries(budget.limits)) {
      const value = census[key];
      if (!Number.isFinite(value)) failures.push(`memory ${key}${at} was not measured`);
      else if (value > limit) failures.push(`memory ${key}${at} ${show(key, value)} is over its limit ${show(key, limit)}`);
    }
  }
  return failures;
}

export function memorySummary(memory) {
  return asPoints(memory).map((census) => {
    const grows = census.wasm_grows == null ? "" : ` (${census.wasm_grows} grow${census.wasm_grows === 1 ? "" : "s"})`;
    return `${census.point ? `${census.point}: ` : ""}wasm ${show("wasm_bytes", census.wasm_bytes ?? 0)}${grows}, `
      + `gpu ${show("gpu_bytes", census.gpu_bytes ?? 0)}, programs ${census.programs}`;
  }).join("; ");
}

// Startup is over when the heap, the GPU ledger and the program count stop
// moving for settleMs; streamed packs keep them moving until they land.
export async function settleMemoryCensus(evaluate, { settleMs = 4000, capMs = 20000, pollMs = 500, now = Date.now, sleep } = {}) {
  const wait = sleep || ((ms) => new Promise((done) => setTimeout(done, ms)));
  const started = now();
  let census = await readMemoryCensus(evaluate);
  let key = "";
  let since = started;
  while (now() - started < capMs) {
    const next = `${census.wasm_bytes}/${census.gpu_bytes}/${census.programs}`;
    if (next !== key) { key = next; since = now(); }
    else if (now() - since >= settleMs) return { ...census, settled: true, waited_ms: now() - started };
    await wait(pollMs);
    census = await readMemoryCensus(evaluate);
  }
  return { ...census, settled: false, waited_ms: now() - started };
}

// The points the verify smoke measures once the game is ready: startup, and
// with afterSeconds a second point once the streamed content had time to land.
// call(method, params) is a CDP call; evaluate(expression) returns a value.
export async function measureMemoryPoints({ call, evaluate, viewport = DEFAULT_MEMORY_VIEWPORT, afterSeconds = 0, settle = {}, sleep }) {
  const wait = sleep || ((ms) => new Promise((done) => setTimeout(done, ms)));
  const { width, height, deviceScaleFactor } = parseViewport(viewport);
  await call("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor, mobile: true });
  await evaluate("new Promise((resolve) => requestAnimationFrame(() => requestAnimationFrame(() => requestAnimationFrame(resolve))))");
  const points = [{ point: "startup", viewport, ...(await settleMemoryCensus(evaluate, settle)) }];
  if (afterSeconds > 0) {
    await wait(afterSeconds * 1000);
    points.push({ point: `after ${afterSeconds} s`, viewport, ...(await settleMemoryCensus(evaluate, settle)) });
  }
  return points;
}

async function measureCli(argv) {
  const opt = { viewport: DEFAULT_MEMORY_VIEWPORT, play: "0", json: false };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--json") opt.json = true;
    else if (["--site", "--zip", "--viewport", "--play"].includes(arg) && i + 1 < argv.length) opt[arg.slice(2)] = argv[++i];
    else throw new Error(`unknown argument ${arg}`);
  }
  if (!opt.site === !opt.zip) throw new Error("usage: web_memory_budget.mjs --site <dir>|--zip <package.zip> [--viewport 390x844@2] [--play 60] [--json]");
  const { launchHeadlessChrome, serveSites } = await import("./package_web_smoke.mjs");
  let site = opt.site ? resolve(opt.site) : "";
  let unpacked = "";
  if (opt.zip) unpacked = site = unpackStoreZip(opt.zip, "memory-budget-");
  const server = await serveSites([site]);
  const deadline = () => Date.now() + 120000;
  const chrome = await launchHeadlessChrome({ deadline: deadline(), profilePrefix: "ai-studio-memory-budget",
    args: ["--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--disable-renderer-backgrounding", "--disable-background-timer-throttling"] });
  try {
    const call = (method, params = {}) => chrome.client.call(method, params, deadline());
    const evaluate = async (expression) => {
      const r = await call("Runtime.evaluate", { expression, returnByValue: true, awaitPromise: true });
      if (r.exceptionDetails) throw new Error(`page: ${r.exceptionDetails.text}`);
      return r.result?.value;
    };
    await call("Page.enable");
    await call("Runtime.enable");
    await call("Page.addScriptToEvaluateOnNewDocument", { source: MEMORY_CENSUS_HOOK });
    const { width, height, deviceScaleFactor } = parseViewport(opt.viewport);
    await call("Emulation.setDeviceMetricsOverride", { width, height, deviceScaleFactor, mobile: true });
    await call("Page.navigate", { url: `${server.origin}0/` });
    const until = Date.now() + 120000;
    while (!(await evaluate("Boolean(window.__memoryCensus && window.__memoryCensus.contexts.length && window.__memoryCensus.memory)").catch(() => false))) {
      if (Date.now() > until) throw new Error("the page created no WebGL context and no WebAssembly memory in 120 s");
      await new Promise((done) => setTimeout(done, 250));
    }
    const points = await measureMemoryPoints({ call, evaluate, viewport: opt.viewport, afterSeconds: Number(opt.play) });
    if (opt.json) console.log(JSON.stringify(points, null, 2));
    else for (const census of points) {
      console.log(`${memorySummary(census)}; peak gpu ${show("gpu_peak_bytes", census.gpu_peak_bytes)}; `
        + `initial wasm ${show("wasm_bytes", census.wasm_initial_bytes ?? 0)}${census.settled === false ? "; NOT settled" : ""}`);
    }
  } finally {
    await chrome.close();
    await server.close();
    if (unpacked) rmSync(unpacked, { recursive: true, force: true });
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  measureCli(process.argv.slice(2)).catch((error) => {
    console.error(`web_memory_budget: ${error.message}`);
    process.exitCode = 1;
  });
}
