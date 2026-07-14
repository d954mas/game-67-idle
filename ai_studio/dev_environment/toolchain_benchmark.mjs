#!/usr/bin/env node
import { mkdirSync, rmSync, statSync, utimesSync, writeFileSync } from "node:fs";
import { cpus, platform, release } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { performance } from "node:perf_hooks";
import { spawnSync } from "node:child_process";

import { studioPythonPath } from "./python.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const argv = process.argv.slice(2);
const value = (flag, fallback = "") => argv.includes(flag) ? argv[argv.indexOf(flag) + 1] : fallback;
const game = value("--game", "template");
const sampleCount = Number.parseInt(value("--samples", "3"), 10);
if (!Number.isInteger(sampleCount) || sampleCount < 1) throw new Error("--samples must be a positive integer");
const source = game === "template" ? "templates/template" : `games/${game}`;
const touchPath = join(root, source, "src", "game_format.c");
const build = join(root, "tmp", "benchmarks", `t0357-${game}`);

function run(command, args) {
  const result = spawnSync(command, args, { cwd: root, encoding: "utf8" });
  if (result.error || result.status !== 0) throw new Error(`${command} ${args.join(" ")} failed:\n${result.stderr || result.stdout}`);
  return String(result.stdout || result.stderr).trim();
}

function timed(command, args) {
  const started = performance.now();
  run(command, args);
  return Number((performance.now() - started).toFixed(1));
}

function summary(samples) {
  const sorted = [...samples].sort((a, b) => a - b);
  const middle = Math.floor(sorted.length / 2);
  const median = sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
  return { samplesMs: samples, minMs: sorted[0], medianMs: Number(median.toFixed(1)), maxMs: sorted.at(-1) };
}

const python = studioPythonPath(root);
const samples = { pythonStartup: [], coldConfigure: [], coldBuild: [], warmBuild: [], noopBuild: [] };
for (let index = 0; index < sampleCount; index += 1) {
  rmSync(build, { recursive: true, force: true });
  mkdirSync(build, { recursive: true });
  samples.pythonStartup.push(timed(python, ["-c", "pass"]));
  samples.coldConfigure.push(timed("cmake", ["-S", source, "-B", build, "-G", "Ninja", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_BUILD_TYPE=Debug"]));
  samples.coldBuild.push(timed("cmake", ["--build", build, "--target", "game", "-j", "4"]));
  const before = statSync(touchPath);
  try {
    utimesSync(touchPath, new Date(), new Date());
    samples.warmBuild.push(timed("cmake", ["--build", build, "--target", "game", "-j", "4"]));
  } finally {
    utimesSync(touchPath, before.atime, before.mtime);
  }
  samples.noopBuild.push(timed("cmake", ["--build", build, "--target", "game", "-j", "4"]));
}

const status = run("git", ["status", "--porcelain"]);
const result = {
  schema: "ai_studio.toolchain_benchmark.v2",
  measuredAt: new Date().toISOString(),
  game,
  source,
  sampleCount,
  workspaceNote: value("--workspace-note", ""),
  machine: { platform: platform(), release: release(), cpu: cpus()[0]?.model || "unknown", cores: cpus().length, node: process.version },
  toolchain: {
    python: run(python, ["--version"]),
    cmake: run("cmake", ["--version"]).split(/\r?\n/)[0],
    ninja: run("ninja", ["--version"]),
    clang: run("clang", ["--version"]).split(/\r?\n/)[0],
  },
  revision: { commit: run("git", ["rev-parse", "HEAD"]), dirtyEntries: status ? status.split(/\r?\n/).length : 0 },
  metrics: Object.fromEntries(Object.entries(samples).map(([name, values]) => [name, summary(values)])),
};
const out = value("--out");
if (out) {
  const path = resolve(root, out);
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(result, null, 2)}\n`, "utf8");
}
console.log(JSON.stringify(result, null, 2));
