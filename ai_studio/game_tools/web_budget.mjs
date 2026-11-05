#!/usr/bin/env node
// A game's web download budget: the "limits" object of content/web_budget.json,
// beside the "memory" object web_memory_budget.mjs reads. Measures every file of a
// staged site raw and gzipped and adds up the first download, what play waits for:
// the page and runtime at the site's root plus each --first file; the rest loads
// while the game already plays. The first download is also counted as each portal
// serves it (PORTALS). Writes size-report.json beside the site and, with --enforce
// (release builds; a debug runtime carries symbols), fails on a limit passed.
//
//   node ai_studio/game_tools/web_budget.mjs --site <dir> --budget <web_budget.json> [--first <path in site>]... [--enforce] [--assert-text]
//
// A limit names any field of the report's "measured": first_download_raw, _gzip,
// _brotli, _zstd, _served, all_raw, all_gzip, wasm_raw, wasm_gzip. --assert-text
// drops wasm_raw: assertion strings are diagnostic payload.
import { existsSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { brotliCompressSync, constants, gzipSync, zstdCompressSync } from "node:zlib";

const MB = 1024 * 1024;
const RUNTIME = /\.(html|js|wasm)$/;

function fail(message) {
  throw new Error(`web_budget: ${message}`);
}

function files(root, dir = root) {
  const found = [];
  for (const name of readdirSync(dir).sort()) {
    const path = join(dir, name);
    if (statSync(path).isDirectory()) found.push(...files(root, path));
    else found.push(relative(root, path).replaceAll("\\", "/"));
  }
  return found;
}

export function readBudget(path) {
  const budget = JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, ""));
  if (!budget.limits || typeof budget.limits !== "object") fail(`${path} has no "limits"`);
  return budget;
}

// The page and runtime at the site's root, and the files the game names.
export function firstDownload(sizes, firstFiles) {
  const runtime = sizes.map((entry) => entry.path).filter((path) => !path.includes("/") && RUNTIME.test(path));
  return [...runtime, ...firstFiles];
}

// The bytes each host sends for a first-download file, by the encoding observed on
// it. Poki's CDN sends brotli for every file, packs (application/octet-stream)
// included, to a browser that accepts it. A self-hosted Caddy edge compresses every
// first-download file with zstd at its default level. itch.io sends gzip for the
// page and runtime; its packs count raw, since no itch.io response has shown an
// octet-stream file compressed.
export const PORTALS = Object.freeze({
  poki: (entry) => entry.brotli,
  self_hosted: (entry) => entry.zstd,
  itch_io: (entry) => (RUNTIME.test(entry.path) ? entry.gzip : entry.raw),
});

// The first_download_served limit: the least compressing host of PORTALS.
export function servedBytes(entry) {
  return PORTALS.itch_io(entry);
}

// sizes: [{path, raw, gzip, and brotli, zstd for the first download}]; returns the report with every limit it passed.
export function assessSizes(sizes, budget, firstPaths) {
  const byPath = new Map(sizes.map((entry) => [entry.path, entry]));
  const first = firstPaths.map((path) => byPath.get(path) ?? fail(`the first download needs ${path}, which the site lacks`));
  const sum = (entries, field) => entries.reduce((total, entry) => total + entry[field], 0);
  const totals = {
    first_download_raw: sum(first, "raw"),
    first_download_gzip: sum(first, "gzip"),
    first_download_brotli: sum(first, "brotli"),
    first_download_zstd: sum(first, "zstd"),
    first_download_served: first.reduce((total, entry) => total + servedBytes(entry), 0),
    served: Object.fromEntries(Object.entries(PORTALS).map(([portal, served]) => [portal, first.reduce((total, entry) => {
      const bytes = served(entry);
      if (!Number.isInteger(bytes)) fail(`${entry.path} has no ${portal} size`);
      return total + bytes;
    }, 0)])),
    all_raw: sum(sizes, "raw"),
    all_gzip: sum(sizes, "gzip"),
  };
  const { served: _perPortal, ...measured } = totals;
  for (const entry of sizes) {
    if (entry.path.endsWith(".wasm")) {
      measured.wasm_raw = entry.raw;
      measured.wasm_gzip = entry.gzip;
    }
  }
  const exceeded = [];
  for (const [name, limit] of Object.entries(budget.limits)) {
    if (!(name in measured)) fail(`limit ${name} measures nothing; known: ${Object.keys(measured).join(", ")}`);
    if (measured[name] > limit) exceeded.push({ name, measured: measured[name], limit });
  }
  return { files: sizes, first_download: firstPaths, totals, measured, limits: budget.limits, exceeded };
}

export function measureSite(site) {
  if (!existsSync(join(site, "index.html"))) fail(`${site} holds no index.html`);
  return files(site).filter((path) => !path.endsWith(".json")).map((path) => {
    const bytes = readFileSync(join(site, path));
    return { path, raw: bytes.length, gzip: gzipSync(bytes, { level: 9 }).length };
  });
}

// Brotli at quality 11 (what a CDN precompresses static files at) and zstd level 3
// (Caddy's default). Quality 11 takes seconds a megabyte: only the named files pay for it.
export function addServedSizes(site, sizes, paths) {
  const wanted = new Set(paths);
  return sizes.map((entry) => {
    if (!wanted.has(entry.path)) return entry;
    const bytes = readFileSync(join(site, entry.path));
    return { ...entry,
      brotli: brotliCompressSync(bytes, { params: { [constants.BROTLI_PARAM_QUALITY]: 11, [constants.BROTLI_PARAM_SIZE_HINT]: entry.raw } }).length,
      zstd: zstdCompressSync(bytes, { params: { [constants.ZSTD_c_compressionLevel]: 3 } }).length };
  });
}

function table(report) {
  const mb = (value) => (value / MB).toFixed(2).padStart(7);
  const lines = report.files.map((entry) => `${mb(entry.raw)} MB ${mb(entry.gzip)} MB gz  ${entry.path}`
    + (report.first_download.includes(entry.path) ? "  (first download)" : ""));
  lines.push(`${mb(report.totals.first_download_raw)} MB ${mb(report.totals.first_download_gzip)} MB gz  first download`);
  for (const [portal, bytes] of Object.entries(report.totals.served)) lines.push(`${mb(bytes)} MB served  first download, ${portal}`);
  lines.push(`${mb(report.totals.all_raw)} MB ${mb(report.totals.all_gzip)} MB gz  everything`);
  return lines.join("\n");
}

export function main(argv = process.argv.slice(2)) {
  const args = { first: [], enforce: false };
  for (let index = 0; index < argv.length; index += 1) {
    const key = argv[index];
    if (key === "--site") args.site = argv[++index];
    else if (key === "--budget") args.budget = argv[++index];
    else if (key === "--first") args.first.push(argv[++index]);
    else if (key === "--enforce") args.enforce = true;
    else if (key === "--assert-text") args.assertText = true;
    else fail(`unknown argument ${key}`);
  }
  if (!args.site || !args.budget) fail("--site and --budget are required");
  const site = resolve(args.site);
  const measured = measureSite(site);
  const first = firstDownload(measured, args.first);
  const sizes = addServedSizes(site, measured, first);
  const budget = readBudget(args.budget);
  if (args.assertText) delete budget.limits.wasm_raw;
  const report = { ...assessSizes(sizes, budget, first), enforced: args.enforce,
    ...(budget.profile ? { profile: budget.profile } : {}) };
  writeFileSync(join(site, "size-report.json"), `${JSON.stringify(report, null, 2)}\n`);
  console.log(table(report));
  for (const item of report.exceeded) {
    console.error(`${args.enforce ? "over budget" : "over budget (not enforced)"}: ${item.name} ${item.measured} > ${item.limit}`);
  }
  return args.enforce && report.exceeded.length > 0 ? 1 : 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    process.exitCode = main();
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
