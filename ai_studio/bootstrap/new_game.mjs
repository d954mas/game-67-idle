#!/usr/bin/env node
// Start a new game by COPYING a templates/<template-id>/ folder into
// games/<game-id>/. The game then owns and customizes its copy.
//
//   node ai_studio/bootstrap/new_game.mjs --id mygame
//   node ai_studio/bootstrap/new_game.mjs --id mygame --template template
//   node ai_studio/bootstrap/new_game.mjs --id mygame --from templates/template --force
//   node ai_studio/bootstrap/new_game.mjs --root <repo> --id mygame
//
// Build/run the new game:
//   cmake -S games/mygame -B games/mygame/build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug
//   cmake --build games/mygame/build
//   ./games/mygame/build/bin/game.exe
import { existsSync, mkdirSync, readdirSync, copyFileSync, statSync } from "node:fs";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { gameRegistryPath, registerGameAssetSource } from "../assets/storage/sources/games.mjs";
import { listRegisteredTemplates } from "../assets/storage/sources/templates.mjs";

const defaultRepoRoot = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function parseArgs(argv) {
  const a = { id: "", template: "", from: "templates/template", root: "", force: false };
  for (let i = 0; i < argv.length; i += 1) {
    const k = argv[i];
    if (k === "--force") a.force = true;
    else if (k === "--id") a.id = argv[++i];
    else if (k === "--template") a.template = argv[++i];
    else if (k === "--from") a.from = argv[++i];
    else if (k === "--root") a.root = argv[++i];
  }
  return a;
}

// Per-game build output is not copied. Tracked source files under src/generated
// are part of the template contract and must copy with the game.
const SKIP = new Set(["build", "_cache", "node_modules", ".git"]);

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
  console.error("usage: node ai_studio/bootstrap/new_game.mjs [--root <repo>] --id <game-id> [--template <template-id>|--from <path>]  (lowercase, kebab)");
  process.exit(1);
}
const repoRoot = a.root ? resolve(a.root) : defaultRepoRoot;
let fromRel = a.from;
if (a.template) {
  const template = listRegisteredTemplates(repoRoot).find((item) => item.id === a.template && item.status !== "disabled");
  if (!template) {
    console.error(`error: template '${a.template}' is not registered or is disabled`);
    process.exit(1);
  }
  fromRel = template.folder;
}
const fromDir = join(repoRoot, fromRel);
const toDir = join(repoRoot, "games", a.id);
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
  folder: `games/${a.id}`,
  assets: `games/${a.id}/assets`,
  status: "active",
});
console.log(`new game '${a.id}' created from ${fromRel}/ -> games/${a.id}/`);
console.log(`registered assets: ${gameRegistryPath(repoRoot)} -> ${registered.assets}`);
console.log("\nbuild + run:");
console.log(`  cmake -S games/${a.id} -B games/${a.id}/build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug`);
console.log(`  cmake --build games/${a.id}/build`);
console.log(`  games/${a.id}/build/bin/game.exe`);
console.log("\nThen: set the title/pack/concept, pull library assets (skill nt-asset-workflow),");
console.log("copy feature packs from features/ as needed, and build your game on top.");
