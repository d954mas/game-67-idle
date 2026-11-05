import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { addServedSizes, assessSizes, firstDownload, measureSite, PORTALS, servedBytes } from "../web_budget.mjs";

const TOOL = resolve(dirname(fileURLToPath(import.meta.url)), "..", "web_budget.mjs");
const FIRST = ["index.html", "a.wasm", "packs/boot.ntpack"];
const sized = (path, raw, gzip, brotli = 1, zstd = 1) => ({ path, raw, gzip, brotli, zstd });

function fixture(run) {
  const site = mkdtempSync(join(tmpdir(), "web-budget-"));
  try {
    mkdirSync(join(site, "packs"));
    writeFileSync(join(site, "index.html"), "<html>".repeat(100));
    writeFileSync(join(site, "packs", "boot.ntpack"), Buffer.alloc(4096));
    writeFileSync(join(site, "packs", "music.ntpack"), Buffer.alloc(8192));
    return run(site);
  } finally {
    rmSync(site, { recursive: true, force: true });
  }
}

test("the first download adds only the page, the runtime and the named files", () => {
  const sizes = [sized("index.html", 10, 5, 4, 6), sized("a.wasm", 100, 40, 30, 35), sized("packs/boot.ntpack", 1000, 300, 200, 250),
    { path: "packs/music.ntpack", raw: 5000, gzip: 4900 }];
  assert.deepEqual(firstDownload(sizes, ["packs/boot.ntpack"]), FIRST);
  const { totals, exceeded } = assessSizes(sizes, { limits: { first_download_brotli: 234, wasm_raw: 100 } }, FIRST);
  assert.equal(totals.first_download_raw, 1110);
  assert.equal(totals.first_download_gzip, 345);
  assert.equal(totals.first_download_brotli, 234);
  assert.equal(totals.first_download_zstd, 291);
  assert.equal(totals.all_gzip, 5245);
  assert.deepEqual(exceeded, []);
});

test("each portal counts the encoding it sends: itch.io leaves packs raw", () => {
  const sizes = [sized("index.html", 10, 5, 4, 6), sized("packs/boot.ntpack", 1000, 300, 200, 250)];
  const { totals } = assessSizes(sizes, { limits: {} }, ["index.html", "packs/boot.ntpack"]);
  assert.deepEqual(totals.served, { poki: 204, self_hosted: 256, itch_io: 1005 });
  assert.equal(totals.first_download_served, 1005);
  assert.equal(servedBytes(sizes[1]), PORTALS.itch_io(sizes[1]));
});

test("a limit passed is named with its measure", () => {
  const sizes = [sized("index.html", 1, 1), sized("a.wasm", 101, 1), sized("packs/boot.ntpack", 1, 1)];
  const { exceeded } = assessSizes(sizes, { limits: { wasm_raw: 100, first_download_served: 1000 } }, FIRST);
  assert.deepEqual(exceeded, [{ name: "wasm_raw", measured: 101, limit: 100 }]);
});

test("a first-download file the site lacks, or an unknown limit, fails", () => {
  const sizes = [sized("index.html", 1, 1)];
  assert.throws(() => assessSizes(sizes, { limits: {} }, FIRST), /first download needs a\.wasm/);
  const all = [...sizes, sized("a.wasm", 1, 1), sized("packs/boot.ntpack", 1, 1)];
  assert.throws(() => assessSizes(all, { limits: { bogus: 1 } }, FIRST), /limit bogus measures nothing/);
});

test("the site is measured raw and gzipped, reports left out; only named files pay for the served encodings", () => fixture((site) => {
  writeFileSync(join(site, "size-report.json"), "{}");
  const measured = measureSite(site);
  assert.deepEqual(measured.map((entry) => entry.path), ["index.html", "packs/boot.ntpack", "packs/music.ntpack"]);
  assert.ok(measured.every((entry) => entry.gzip > 0 && entry.gzip < entry.raw));
  const sizes = addServedSizes(site, measured, ["packs/boot.ntpack"]);
  assert.ok(sizes[1].brotli > 0 && sizes[1].brotli < sizes[1].raw && sizes[1].zstd > 0);
  assert.equal(sizes[0].brotli, undefined);
}));

test("the CLI enforces the limits only when asked, and a memory section does not disturb it", () => fixture((site) => {
  const budget = join(site, "budget.json");
  writeFileSync(budget, JSON.stringify({ limits: { first_download_raw: 10 }, memory: { limits: { wasm_bytes: 1 } } }));
  const run = (...extra) => spawnSync(process.execPath, [TOOL, "--site", site, "--budget", budget, "--first", "packs/boot.ntpack", ...extra], { encoding: "utf8" });
  const reported = run();
  assert.equal(reported.status, 0);
  assert.match(reported.stderr, /over budget \(not enforced\): first_download_raw/);
  const enforced = run("--enforce");
  assert.equal(enforced.status, 1);
  assert.match(enforced.stderr, /over budget: first_download_raw/);
  const report = JSON.parse(readFileSync(join(site, "size-report.json"), "utf8"));
  assert.deepEqual(report.first_download, ["index.html", "packs/boot.ntpack"]);
  assert.equal(report.enforced, true);
}));

test("--assert-text lifts only the wasm limit", () => fixture((site) => {
  const budget = join(site, "budget.json");
  writeFileSync(join(site, "a.wasm"), Buffer.alloc(64));
  writeFileSync(budget, JSON.stringify({ limits: { wasm_raw: 1 } }));
  const run = (...extra) => spawnSync(process.execPath, [TOOL, "--site", site, "--budget", budget, "--enforce", ...extra], { encoding: "utf8" });
  assert.equal(run().status, 1);
  assert.equal(run("--assert-text").status, 0);
}));
