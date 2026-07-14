#!/usr/bin/env node
// Create a new reusable template by copying another templates/<template-id>/
// folder, then register it and refresh VS Code build/run entries.
//
//   node templates/new_template.mjs --id mobile-template
//   node templates/new_template.mjs --id mobile-template --from templates/template --force
//   node templates/new_template.mjs --root <repo> --id mobile-template
import { copyFileSync, existsSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { registerTemplateAssetSource, templateRegistryPath } from "../ai_studio/assets/sources/ops.mjs";
import { writeVscodeProjectFiles } from "../ai_studio/dev_environment/vscode_projects.mjs";
import { ensureProject } from "../ai_studio/taskboard/lib.mjs";

const defaultRepoRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));
const SKIP = new Set(["build", "_cache", "node_modules", ".git"]);

function parseArgs(argv) {
  const args = { id: "", from: "templates/template", root: "", force: false };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--force") args.force = true;
    else if (arg === "--id") args.id = argv[++i];
    else if (arg === "--from") args.from = argv[++i];
    else if (arg === "--root") args.root = argv[++i];
    else if (arg === "--help" || arg === "-h") args.help = true;
    else throw new Error(`unknown argument: ${arg}`);
  }
  return args;
}

function usageText() {
  return "usage: node templates/new_template.mjs [--root <repo>] --id <template-id> [--from <path>] [--force]";
}

function printUsage() {
  console.log(usageText());
}

function copyDir(src, dst) {
  mkdirSync(dst, { recursive: true });
  for (const name of readdirSync(src)) {
    if (SKIP.has(name)) continue;
    const from = join(src, name);
    const to = join(dst, name);
    if (statSync(from).isDirectory()) copyDir(from, to);
    else copyFileSync(from, to);
  }
}

let args;
try {
  args = parseArgs(process.argv.slice(2));
} catch (error) {
  console.error(error && error.message ? error.message : String(error));
  console.error(usageText());
  process.exit(1);
}
if (args.help) {
  printUsage();
  process.exit(0);
}
if (!args.id || !/^[a-z][a-z0-9-]*$/.test(args.id)) {
  console.error(`${usageText()}  (lowercase, kebab)`);
  process.exit(1);
}

const repoRoot = args.root ? resolve(args.root) : defaultRepoRoot;
const fromDir = join(repoRoot, args.from);
const toRel = `templates/${args.id}`;
const toDir = join(repoRoot, toRel);
if (!existsSync(join(fromDir, "CMakeLists.txt"))) {
  console.error(`error: template source not found at ${fromDir}`);
  process.exit(1);
}
if (existsSync(toDir) && !args.force) {
  console.error(`error: ${toDir} already exists (use --force)`);
  process.exit(1);
}
copyDir(fromDir, toDir);
const registered = registerTemplateAssetSource(repoRoot, {
  id: args.id,
  title: args.id,
  folder: toRel,
  assets: `${toRel}/assets`,
  status: "active",
});
const taskboard = ensureProject(repoRoot, {
  title: args.id,
  kind: "template",
  target: toRel,
  tags: ["template"],
  body: `## Goal

Track reusable template work for \`${toRel}\`.

## In scope

- Template setup, reusable systems, seed assets, validation, and documentation.

## Out of scope

- Game-specific lore, balance, roadmap, and release tasks.

## Log
`,
});
const vscode = writeVscodeProjectFiles(repoRoot);

console.log(`new template '${args.id}' created from ${args.from}/ -> ${toRel}/`);
console.log(`registered template: ${templateRegistryPath(repoRoot)} -> ${registered.assets}`);
console.log(`${taskboard.created ? "created" : "existing"} taskboard project: ${taskboard.project.fields.id}`);
console.log(`updated VS Code tasks/launch for ${vscode.projects.length} playable project(s)`);
console.log("\nbuild + run:");
console.log(`  cmake -S ${toRel} -B ${toRel}/build/native-debug -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug`);
console.log(`  cmake --build ${toRel}/build/native-debug --target game`);
console.log(`  ${toRel}/build/native-debug/bin/game.exe`);
console.log(`  or use VS Code: Debug Template ${args.id} (native debug)`);
