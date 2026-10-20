#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { join, resolve } from "node:path";

import { fail, isMain } from "../core_harness/tool_lib/cli.mjs";
import { listGameMounts } from "../workspace/games.mjs";
import { gameVersionDrift, studioEngineRevision } from "../workspace/version_drift.mjs";

const USAGE = `usage: node ai_studio/game_tools/live_games.mjs [--game <id>]... [--tier core|slow|taste] [--json]

Runs every live game's own test command against this Studio checkout, after
checking that its dependency record names what is checked out. Run it before
merging an engine move or a shared feature change. Frozen games are skipped.`;

const STUDIO_ROOT = resolve(import.meta.dirname, "..", "..");

function parseArgs(argv) {
  const args = { games: [], tier: "", json: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--game") args.games.push(argv[++index] || fail("--game needs an id"));
    else if (arg === "--tier") args.tier = argv[++index] || fail("--tier needs a name");
    else if (arg === "--json") args.json = true;
    else if (arg === "--help" || arg === "-h") {
      console.log(USAGE);
      process.exit(0);
    } else fail(`unknown argument ${arg}\n${USAGE}`);
  }
  return args;
}

function lastLine(text) {
  return String(text || "").trimEnd().split(/\r?\n/).at(-1) || "";
}

// A game listing the metaserver library builds its own main against it; a
// library API change breaks the game here, before it merges. Empty = passed.
function metaserverCheck(gameDir, spawn) {
  const dependencies = JSON.parse(readFileSync(join(gameDir, "dependencies.json"), "utf8").replace(/^\uFEFF/, ""));
  if (!dependencies.features?.some((feature) => feature.id === "metaserver")) return null;
  for (const step of ["vet", "build"]) {
    const result = spawn("go", [step, "./..."], { cwd: join(gameDir, "metaserver"), encoding: "utf8", stdio: ["ignore", "pipe", "pipe"] });
    if (result.error || result.status !== 0) return `go ${step}: ${result.error?.message || lastLine(`${result.stdout || ""}${result.stderr || ""}`)}`;
  }
  return "";
}

export function checkLiveGames(root, { games = [], tier = "", spawn = spawnSync, engineRevision } = {}) {
  const revision = engineRevision ?? studioEngineRevision(root);
  const mounts = listGameMounts(root, { includePrivate: true })
    .filter((mount) => mount.lifecycle?.state !== "frozen")
    .filter((mount) => games.length === 0 || games.includes(mount.gameId));
  return mounts.map((mount) => {
    const gameDir = join(root, mount.root);
    const drift = gameVersionDrift(root, mount, revision);
    const started = Date.now();
    const result = spawn(process.execPath, [join("tools", "game.mjs"), "test", ...(tier ? ["--tier", tier] : [])], {
      cwd: gameDir, encoding: "utf8", maxBuffer: 512 * 1024 * 1024, stdio: ["ignore", "pipe", "pipe"],
    });
    const output = `${result.stdout || ""}${result.stderr || ""}`;
    return {
      game: mount.gameId,
      drift,
      metaserver: metaserverCheck(gameDir, spawn),
      testsPassed: !result.error && result.status === 0,
      seconds: Number(((Date.now() - started) / 1000).toFixed(1)),
      summary: result.error ? String(result.error.message) : lastLine(output),
    };
  });
}

if (isMain(import.meta.url)) {
  const args = parseArgs(process.argv.slice(2));
  const rows = checkLiveGames(STUDIO_ROOT, args);
  const ok = rows.every((row) => row.testsPassed && row.drift.length === 0 && !row.metaserver);
  if (args.json) {
    console.log(JSON.stringify({ ok, games: rows }, null, 2));
  } else {
    for (const row of rows) {
      const drift = row.drift.length ? `drift ${row.drift.map((d) => `${d.id} ${d.recorded}->${d.current}`).join(", ")}` : "record current";
      const passed = row.testsPassed && !row.metaserver;
      const meta = row.metaserver === null ? "" : `; metaserver ${row.metaserver || "builds"}`;
      console.log(`${passed ? "PASS" : "FAIL"}  ${row.game}  (${row.seconds}s; ${drift}${meta})\n      ${row.summary}`);
    }
    console.log(ok ? `all ${rows.length} live games pass` : "live games need attention before this change merges");
  }
  process.exit(ok ? 0 : 1);
}
