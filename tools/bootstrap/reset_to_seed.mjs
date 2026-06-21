// Reset the in-repo runtime + project artifacts back to the clean game seed so
// the next game iteration starts clean. The reusable base is preserved (tools/,
// .codex skills, engine submodule, state/, gamedesign/knowledge + sources, the
// task system, AGENTS.md/AI_PIPELINE.md).
//
//   node tools/bootstrap/reset_to_seed.mjs --game-id <id> [--seed-ref clean-seed] [--apply]
//
// Default is a DRY RUN (prints the plan, changes nothing). Pass --apply to run.
// ALWAYS tag/branch the current game first (e.g. `git tag <id>-snapshot-<date>`):
// this is destructive in the working tree (recoverable from git, but tag to be
// safe). After applying, review the game-named files it lists, then run
// `node tools/ai.mjs validate --full`.

import { existsSync, rmSync, readdirSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const args = process.argv.slice(2);
const apply = args.includes("--apply");
const argval = (name, def) => {
  const i = args.indexOf(name);
  return i !== -1 ? args[i + 1] : def;
};
const gameId = argval("--game-id", null);
const seedRef = argval("--seed-ref", "clean-seed");
if (!gameId) {
  console.error("usage: node tools/bootstrap/reset_to_seed.mjs --game-id <id> [--seed-ref clean-seed] [--apply]");
  process.exit(1);
}

const git = (a) => spawnSync("git", a, { cwd: root, encoding: "utf8" });
if (git(["rev-parse", "--verify", "--quiet", seedRef]).status !== 0) {
  console.error(`error: seed ref "${seedRef}" not found. Tag a known clean-seed commit first: git tag clean-seed <commit>`);
  process.exit(1);
}

// Files restored verbatim from the clean seed reference.
const RESTORE = ["src/clean_seed_main.c", "CMakeLists.txt", "tasks/STATUS.md"];
// Whole directories that are always game-specific. Game tooling lives under
// tools/<game-id>/ by convention, so it is removed wholesale here.
const REMOVE_DIRS = [`gamedesign/projects/${gameId}`, "assets/runtime", `tools/${gameId}`];
// Common game-specific files that historically leaked under generic names and
// therefore are not caught by the game-id loose-file scan.
const REMOVE_FILES = ["src/game_actions.c", "src/game_actions.h", "tools/devapi/smoke.py"];
// Game-named loose files discovered anywhere under src/, assets/, tools/, state/
// (id with - or _). The recursive tools/ scan is the fallback that catches game
// scripts which leaked into shared dirs instead of tools/<game-id>/ -- the exact
// bug that stranded backrooms_* under tools/assets and tools/perf.
const idUnder = gameId.replace(/-/g, "_");
const matchers = [new RegExp(`^${idUnder}`), new RegExp(gameId.replace(/[-_]/g, "[-_]"))];
const SKIP_DIRS = new Set(["node_modules", ".git", "__pycache__"]);
const looseFiles = [];
const scan = (relDir) => {
  const abs = join(root, relDir);
  if (!existsSync(abs)) return;
  for (const name of readdirSync(abs)) {
    if (SKIP_DIRS.has(name)) continue;
    const relPath = `${relDir}/${name}`;
    let isDir = false;
    try {
      isDir = statSync(join(root, relPath)).isDirectory();
    } catch {
      continue;
    }
    if (isDir) {
      if (relPath === `tools/${gameId}`) continue; // removed wholesale above
      scan(relPath);
    } else if (matchers.some((m) => m.test(name))) {
      looseFiles.push(relPath);
    }
  }
};
["src", "assets", "tools", "state"].forEach(scan);

console.log(`reset_to_seed: game-id=${gameId}  seed-ref=${seedRef}  mode=${apply ? "APPLY" : "dry-run"}\n`);
console.log("Restore from seed ref:");
RESTORE.forEach((f) => console.log(`  git checkout ${seedRef} -- ${f}`));
console.log("\nRemove game directories:");
REMOVE_DIRS.forEach((p) => console.log(`  rm -rf ${p}`));
console.log("\nRemove known game-specific loose files:");
REMOVE_FILES.forEach((p) => console.log(`  rm ${p}`));
console.log("\nRemove game-named loose files (auto-detected):");
console.log(looseFiles.length ? looseFiles.map((f) => `  rm ${f}`).join("\n") : "  (none)");
console.log("\nReview manually (CMake target blocks + task epics/archive are game-named):");
console.log(`  CMakeLists.txt: any add_executable/target referencing ${idUnder}_* (the seed CMakeLists restore should drop them)`);
console.log(`  tasks/active/*, tasks/archive/<epic>/, tasks/epics/<epic>.md, tools/${gameId}/`);

if (!apply) {
  console.log("\n(dry run — pass --apply to execute. TAG THE CURRENT GAME FIRST.)");
  process.exit(0);
}

for (const f of RESTORE) {
  const r = git(["checkout", seedRef, "--", f]);
  console.log(r.status === 0 ? `restored ${f}` : `WARN could not restore ${f}: ${(r.stderr || "").trim()}`);
}
for (const p of [...REMOVE_DIRS, ...REMOVE_FILES, ...looseFiles]) {
  const abs = join(root, p);
  if (existsSync(abs)) {
    rmSync(abs, { recursive: true, force: true });
    console.log(`removed ${p}`);
  }
}
console.log("\nDONE. Now: review the manual items above, strip any leftover game CMake targets, then run:");
console.log("  node tools/ai.mjs validate --full");
