#!/usr/bin/env node
// Start a new game by COPYING the template/ folder into a new game folder at the
// repo root (sibling to external/ and template/, so its CMake's ../external/...
// reference resolves the same way). The game then owns and customizes its copy.
//
//   node tools/bootstrap/new_game.mjs --id mygame
//   node tools/bootstrap/new_game.mjs --id mygame --from template --force
//   node tools/bootstrap/new_game.mjs --root <repo> --id mygame
//
// Build/run the new game:
//   cmake -S mygame -B mygame/build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug
//   cmake --build mygame/build
//   ./mygame/build/bin/game.exe
import { existsSync, mkdirSync, readdirSync, copyFileSync, statSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { gameRegistryPath, registerGameAssetSource } from "../../ai_studio/assets/storage/sources/games.mjs";

const defaultRepoRoot = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function parseArgs(argv) {
  const a = { id: "", from: "template", root: "", force: false };
  for (let i = 0; i < argv.length; i += 1) {
    const k = argv[i];
    if (k === "--force") a.force = true;
    else if (k === "--id") a.id = argv[++i];
    else if (k === "--from") a.from = argv[++i];
    else if (k === "--root") a.root = argv[++i];
  }
  return a;
}

// Per-game build output + generated headers are NOT copied (each game regenerates).
const SKIP = new Set(["build", "generated", "_cache", "node_modules", ".git"]);

function copyDir(src, dst) {
  mkdirSync(dst, { recursive: true });
  for (const name of readdirSync(src)) {
    if (SKIP.has(name)) continue;
    const s = join(src, name);
    const d = join(dst, name);
    if (statSync(s).isDirectory()) copyDir(s, d);
    else copyFileSync(s, d);
  }
}

const a = parseArgs(process.argv.slice(2));
if (!a.id || !/^[a-z][a-z0-9-]*$/.test(a.id)) {
  console.error("usage: node tools/bootstrap/new_game.mjs [--root <repo>] --id <game-id>  (lowercase, kebab)");
  process.exit(1);
}
const repoRoot = a.root ? resolve(a.root) : defaultRepoRoot;
const fromDir = join(repoRoot, a.from);
const toDir = join(repoRoot, a.id);
if (!existsSync(join(fromDir, "CMakeLists.txt"))) {
  console.error(`error: template not found at ${fromDir}`);
  process.exit(1);
}
if (existsSync(toDir) && !a.force) {
  console.error(`error: ${toDir} already exists (use --force)`);
  process.exit(1);
}

copyDir(fromDir, toDir);
const registered = registerGameAssetSource(repoRoot, {
  id: a.id,
  title: a.id,
  folder: a.id,
  assets: `${a.id}/assets`,
  status: "active",
});
console.log(`new game '${a.id}' created from ${a.from}/ -> ${a.id}/`);
console.log(`registered assets: ${gameRegistryPath(repoRoot)} -> ${registered.assets}`);
console.log("\nbuild + run:");
console.log(`  cmake -S ${a.id} -B ${a.id}/build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug`);
console.log(`  cmake --build ${a.id}/build`);
console.log(`  ${a.id}/build/bin/game.exe`);
console.log("\nThen: set the title/pack/concept, pull library assets (skill game-3d-models),");
console.log("pull systems from systems_showcase/ as needed, and build your game on top.");
