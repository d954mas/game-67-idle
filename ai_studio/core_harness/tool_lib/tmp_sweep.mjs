#!/usr/bin/env node
// Housekeeping for the ignored tmp/ scratch folders.
//
//   node ai_studio/core_harness/tool_lib/tmp_sweep.mjs [--list] [--older-than <days>] [--include-games]
//                                                       [--apply | --all-scratch [--dry-run]] [--root <dir>]
//
// tmp/ is gitignored disposable scratch; durable evidence lives under
// games/<id>/design/. Nothing is deleted without --apply or --all-scratch.

import { existsSync, lstatSync, readdirSync, rmSync } from "node:fs";
import { join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const USAGE = "usage: node ai_studio/core_harness/tool_lib/tmp_sweep.mjs [--list] [--older-than <days>] [--include-games] [--apply | --all-scratch [--dry-run]] [--root <dir>]";
const DAY_MS = 24 * 60 * 60 * 1000;

export function parseSweepArgs(argv) {
  const args = { root: "", olderThanDays: null, includeGames: false, apply: false, allScratch: false, dryRun: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--root") args.root = argv[++index] || "";
    else if (arg === "--older-than") {
      const days = Number(argv[++index]);
      if (!Number.isFinite(days) || days < 0) throw new Error(`--older-than needs a number of days\n${USAGE}`);
      args.olderThanDays = days;
    } else if (arg === "--include-games") args.includeGames = true;
    else if (arg === "--apply") args.apply = true;
    else if (arg === "--all-scratch") args.allScratch = true;
    else if (arg === "--dry-run") args.dryRun = true;
    else if (arg !== "--list") throw new Error(`unknown arg: ${arg}\n${USAGE}`);
  }
  if (args.apply && args.olderThanDays === null) throw new Error(`--apply needs --older-than; use --all-scratch to clear everything\n${USAGE}`);
  return args;
}

export function scratchRoots(root, includeGames) {
  const roots = [join(root, "tmp")];
  if (includeGames) {
    for (const parent of [join(root, "games"), join(root, "games", "private")]) {
      if (!existsSync(parent)) continue;
      for (const entry of readdirSync(parent, { withFileTypes: true })) {
        if (entry.isDirectory() && entry.name !== "private") roots.push(join(parent, entry.name, "tmp"));
      }
    }
  }
  return roots.filter((path) => existsSync(path));
}

// Size and the newest modification inside an entry: a folder's own mtime only
// moves when its direct children change, so it would call active work old.
export function measureEntry(path) {
  let bytes = 0;
  let newest = 0;
  const stack = [path];
  while (stack.length) {
    const current = stack.pop();
    let info;
    try { info = lstatSync(current); } catch { continue; }
    newest = Math.max(newest, info.mtimeMs);
    if (info.isDirectory()) {
      for (const name of readdirSync(current)) stack.push(join(current, name));
    } else {
      bytes += info.size;
    }
  }
  return { bytes, newest };
}

export function planSweep(root, { includeGames = false, olderThanDays = null, now = Date.now() } = {}) {
  const cutoff = olderThanDays === null ? Infinity : now - olderThanDays * DAY_MS;
  const rows = [];
  for (const scratch of scratchRoots(root, includeGames)) {
    for (const name of readdirSync(scratch).sort()) {
      const path = join(scratch, name);
      const { bytes, newest } = measureEntry(path);
      rows.push({ path, rel: relative(root, path).replaceAll("\\", "/"), bytes, newest, selected: newest < cutoff });
    }
  }
  return rows;
}

function human(bytes) {
  if (bytes >= 1024 * 1024 * 1024) return `${(bytes / 1024 / 1024 / 1024).toFixed(1)}G`;
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)}M`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)}K`;
  return `${bytes}B`;
}

const total = (rows) => rows.reduce((sum, row) => sum + row.bytes, 0);
const plural = (count) => `${count} entr${count === 1 ? "y" : "ies"}`;

export function main(argv = process.argv.slice(2), log = console.log) {
  let args;
  try { args = parseSweepArgs(argv); } catch (error) { console.error(error.message); return 2; }
  if (argv.includes("--help") || argv.includes("-h")) { log(USAGE); return 0; }
  const root = resolve(args.root || fileURLToPath(new URL("../../..", import.meta.url)));
  const rows = planSweep(root, { includeGames: args.includeGames, olderThanDays: args.allScratch ? null : args.olderThanDays });
  if (rows.length === 0) { log(`no tmp/ scratch under ${root}; nothing to sweep`); return 0; }
  const selected = rows.filter((row) => row.selected);
  const deleting = (args.apply || args.allScratch) && !args.dryRun;

  if (!args.apply && !args.allScratch) {
    const age = args.olderThanDays === null ? "" : ` untouched for ${args.olderThanDays}+ days`;
    log(`tmp/ scratch report${age}; nothing deleted:`);
    for (const row of [...selected].sort((a, b) => b.bytes - a.bytes)) log(`  ${human(row.bytes).padStart(7)}  ${row.rel}`);
    log(`reclaimable: ${human(total(selected))} across ${plural(selected.length)}; kept: ${human(total(rows) - total(selected))}`);
    return 0;
  }
  for (const row of selected) {
    if (deleting) rmSync(row.path, { recursive: true, force: true });
    log(`${deleting ? "removed" : "would remove"} ${human(row.bytes).padStart(7)}  ${row.rel}`);
  }
  log(`${deleting ? "freed" : "would free"} ${human(total(selected))} across ${plural(selected.length)}`);
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exitCode = main();
}
