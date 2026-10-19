#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";

import { fail, isMain } from "../core_harness/tool_lib/cli.mjs";
import { listGameMounts } from "./games.mjs";

const USAGE = `usage: node ai_studio/workspace/version_drift.mjs [--game <id>] [--include-private] [--json]

Games compile features/ and the engine from this Studio checkout, so a
dependencies.json version is only true when it matches what is checked out.
Lists every recorded version that does not; exits 1 when any game drifts.
Frozen games are skipped unless named with --game.`;

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8").replace(/^﻿/, ""));
}

export function studioEngineRevision(root, spawn = spawnSync) {
  const result = spawn("git", ["-C", join(root, "external", "neotolis-engine"), "rev-parse", "HEAD"], { encoding: "utf8" });
  return result.status === 0 ? result.stdout.trim() : "";
}

export function gameVersionDrift(root, mount, engineRevision) {
  const dependencies = readJson(join(root, mount.root, "dependencies.json"));
  const drift = [];
  if (engineRevision && dependencies.engine.revision.toLowerCase() !== engineRevision.toLowerCase()) {
    drift.push({ id: "engine", recorded: dependencies.engine.revision.slice(0, 8), current: engineRevision.slice(0, 8) });
  }
  for (const feature of dependencies.features) {
    const manifest = join(root, feature.source, "feature.json");
    if (!existsSync(manifest)) {
      drift.push({ id: feature.id, recorded: feature.version, current: "missing" });
      continue;
    }
    const current = String(readJson(manifest).version || "");
    if (current !== feature.version) drift.push({ id: feature.id, recorded: feature.version, current });
  }
  return drift;
}

export function versionDriftReport(root, options = {}) {
  const engineRevision = options.engineRevision ?? studioEngineRevision(root);
  const mounts = listGameMounts(root, {
    includePrivate: options.includePrivate === true,
    activeGameId: options.game || "",
  }).filter((mount) => options.game ? mount.gameId === options.game : mount.lifecycle?.state !== "frozen");
  return mounts.map((mount) => ({ game: mount.gameId, drift: gameVersionDrift(root, mount, engineRevision) }));
}

function parseArgs(argv) {
  const args = { game: "", includePrivate: false, json: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--game") args.game = argv[++index] || fail("--game needs an id");
    else if (arg === "--include-private") args.includePrivate = true;
    else if (arg === "--json") args.json = true;
    else if (arg === "--help" || arg === "-h") {
      console.log(USAGE);
      process.exit(0);
    } else fail(`unknown argument ${arg}\n${USAGE}`);
  }
  return args;
}

if (isMain(import.meta.url)) {
  const args = parseArgs(process.argv.slice(2));
  const report = versionDriftReport(resolve(import.meta.dirname, "..", ".."), args);
  const drifting = report.filter((row) => row.drift.length);
  if (args.json) {
    console.log(JSON.stringify({ ok: drifting.length === 0, games: report }, null, 2));
  } else {
    for (const row of report) {
      if (!row.drift.length) console.log(`${row.game}: current`);
      else console.log(`${row.game}: ${row.drift.map((d) => `${d.id} ${d.recorded} -> ${d.current}`).join(", ")}`);
    }
  }
  process.exit(drifting.length ? 1 : 0);
}
