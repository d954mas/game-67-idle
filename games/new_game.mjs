#!/usr/bin/env node
// Start a new game by COPYING a templates/<template-id>/ folder into
// games/<game-id>/. The game then owns and customizes its copy.
//
//   node games/new_game.mjs --id mygame --visibility public
//   node games/new_game.mjs --id mygame --template template --visibility public
//   node games/new_game.mjs --id mygame --from templates/template --visibility public --force
//   node games/new_game.mjs --id mygame --visibility private [--public-alias "Safe Alias"]
//   node games/new_game.mjs --id mygame --private [--public-alias "Safe Alias"]
//   node games/new_game.mjs --root <repo> --id mygame --visibility public
//
// Build/run the new game:
//   cmake -S games/mygame -B games/mygame/build/native-debug -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug
//   cmake --build games/mygame/build/native-debug --target game
//   ./games/mygame/build/native-debug/bin/game.exe
import { appendFileSync, existsSync, mkdirSync, readdirSync, copyFileSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, isAbsolute, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { gameRegistryPath, listRegisteredGames, registerGameAssetSource } from "../ai_studio/assets/backlog/storage/sources/games.mjs";
import { listRegisteredTemplates } from "../ai_studio/assets/backlog/storage/sources/templates.mjs";
import { writeVscodeProjectFiles } from "../ai_studio/dev_environment/vscode_projects.mjs";
import { ensureProject } from "../ai_studio/taskboard/lib.mjs";
import { localGameRegistryRelPath, runPrivateGamePreflight, upsertLocalGameMount } from "../ai_studio/workspace/games.mjs";

const defaultRepoRoot = resolve(fileURLToPath(new URL("..", import.meta.url)));

function parseArgs(argv) {
  const a = {
    id: "",
    template: "",
    from: "templates/template",
    root: "",
    force: false,
    private: false,
    visibility: "",
    requireVisibility: false,
    publicAlias: "",
    help: false,
  };
  for (let i = 0; i < argv.length; i += 1) {
    const k = argv[i];
    if (k === "--force") a.force = true;
    else if (k === "--private") a.private = true;
    else if (k === "--visibility") {
      const value = argv[i + 1];
      if (!value || value.startsWith("--")) throw new Error("--visibility requires public or private");
      a.visibility = value;
      i += 1;
    }
    else if (k === "--require-visibility") a.requireVisibility = true;
    else if (k === "--public-alias") a.publicAlias = argv[++i];
    else if (k === "--id") a.id = argv[++i];
    else if (k === "--template") a.template = argv[++i];
    else if (k === "--from") a.from = argv[++i];
    else if (k === "--root") a.root = argv[++i];
    else if (k === "--help" || k === "-h") a.help = true;
    else throw new Error(`unknown argument: ${k}`);
  }
  return a;
}

function usageText() {
  return [
    "usage: node games/new_game.mjs [--root <repo>] --id <game-id> [--visibility public|private] [--template <template-id>|--from <path>] [--private] [--public-alias <safe-name>] [--require-visibility] [--force]  (lowercase, kebab)",
    "",
    "visibility: pass --visibility public for tracked parent Studio registration or --visibility private for a nested private repo.",
    "compatibility: omitting --visibility still creates a public/tracked game; human/Studio flows should pass --require-visibility so missing choices fail.",
    "--private is a compatibility alias for --visibility private.",
    "--public-alias is only valid with private visibility.",
  ].join("\n");
}

function resolveVisibility(a) {
  if (a.visibility && a.visibility !== "public" && a.visibility !== "private") {
    throw new Error(`invalid --visibility '${a.visibility}' (use public or private)`);
  }
  if (a.private && a.visibility === "public") {
    throw new Error("--private conflicts with --visibility public");
  }
  const visibility = a.private ? "private" : a.visibility;
  if (a.publicAlias && visibility !== "private") {
    throw new Error("--public-alias is only valid with private visibility");
  }
  if (visibility) return visibility;
  if (a.requireVisibility) {
    throw new Error("missing visibility choice: pass --visibility public or --visibility private");
  }
  return "public";
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

function writeJson(path, value) {
  writeFileSync(path, `${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function ensureGameStudioScaffold(gameDir, gameId, visibility) {
  const studioDir = join(gameDir, ".ai_studio");
  mkdirSync(join(studioDir, "taskboard", "items"), { recursive: true });
  mkdirSync(join(studioDir, "canvas", "projects"), { recursive: true });
  mkdirSync(join(studioDir, "evidence"), { recursive: true });
  writeJson(join(studioDir, "workspace.json"), {
    schema: "ai_studio.game.workspace.v1",
    gameId,
    visibility,
    stores: {
      taskboard: ".ai_studio/taskboard/items",
      canvas: ".ai_studio/canvas/projects",
      evidence: ".ai_studio/evidence",
      assets: "assets",
    },
  });
  for (const [rel, text] of [
    ["taskboard/items/README.md", "Game-local taskboard items live here for private or game-owned work.\n"],
    ["canvas/projects/README.md", "Game-local canvas projects live here when they must stay with this game.\n"],
    ["evidence/README.md", "Game-local validation evidence, screenshots, and reports live here.\n"],
  ]) {
    const path = join(studioDir, rel);
    if (!existsSync(path)) writeFileSync(path, text, "utf8");
  }
}

function runGit(root, args) {
  return spawnSync("git", ["-C", root, ...args], { encoding: "utf8", shell: false });
}

function requireParentGitExcludePath(repoRoot) {
  const result = runGit(repoRoot, ["rev-parse", "--git-path", "info/exclude"]);
  if (result.error || result.status !== 0) {
    throw new Error("private game creation requires the parent Studio checkout to be a Git repository");
  }
  const text = result.stdout.trim();
  const excludePath = isAbsolute(text) ? text : resolve(repoRoot, text);
  mkdirSync(dirname(excludePath), { recursive: true });
  return excludePath;
}

function ensureParentExclude(repoRoot, relRoot, options = {}) {
  const excludePath = requireParentGitExcludePath(repoRoot);
  const normalized = relRoot.replace(/\\/g, "/").replace(/\/+$/, "");
  const line = options.directory ? `${normalized}/` : normalized;
  const existing = existsSync(excludePath) ? readFileSync(excludePath, "utf8") : "";
  if (!existing.split(/\r?\n/).includes(line)) {
    appendFileSync(excludePath, `${existing.endsWith("\n") || existing.length === 0 ? "" : "\n"}${line}\n`, "utf8");
  }
}

function parentTracksPath(repoRoot, relRoot) {
  const result = runGit(repoRoot, ["ls-files", "--", relRoot.replace(/\\/g, "/").replace(/\/+$/, "")]);
  if (result.error || result.status !== 0) {
    throw new Error(`failed to inspect parent git tracking: ${result.error ? result.error.message : result.stderr}`);
  }
  return result.stdout.trim().length > 0;
}

function ensureNestedGit(gameDir, gameId) {
  const nested = join(gameDir, ".git");
  const created = !existsSync(nested);
  if (created) {
    const init = runGit(gameDir, ["init"]);
    if (init.error || init.status !== 0) {
      throw new Error(`failed to initialize private game git repository: ${init.error ? init.error.message : init.stderr}`);
    }
  }
  const top = runGit(gameDir, ["rev-parse", "--show-toplevel"]);
  const actual = top.stdout.trim().replace(/\\/g, "/").toLowerCase();
  const expected = resolve(gameDir).replace(/\\/g, "/").toLowerCase();
  if (top.error || top.status !== 0 || actual !== expected) {
    throw new Error(`games/${gameId} does not contain a valid nested git repository`);
  }
  return created ? "created" : "existing";
}

function verifyExistingNestedGitBeforeCopy(gameDir, gameId) {
  if (!existsSync(join(gameDir, ".git"))) return;
  ensureNestedGit(gameDir, gameId);
}

let a;
try {
  a = parseArgs(process.argv.slice(2));
} catch (error) {
  console.error(error && error.message ? error.message : String(error));
  console.error(usageText());
  process.exit(1);
}
if (a.help) {
  console.log(usageText());
  process.exit(0);
}
if (!a.id || !/^[a-z][a-z0-9-]*$/.test(a.id)) {
  console.error(usageText());
  process.exit(1);
}
let visibility;
try {
  visibility = resolveVisibility(a);
} catch (error) {
  console.error(error && error.message ? error.message : String(error));
  console.error(usageText());
  process.exit(1);
}
const isPrivate = visibility === "private";
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

if (isPrivate) {
  const publicGame = listRegisteredGames(repoRoot).find((game) => game.id === a.id && game.status !== "fixture");
  if (publicGame) {
    console.error(`error: private game id '${a.id}' is already registered as a public game`);
    process.exit(1);
  }
  try {
    requireParentGitExcludePath(repoRoot);
    if (parentTracksPath(repoRoot, `games/${a.id}`)) {
      throw new Error(`games/${a.id} is already tracked by the parent repository; cannot create it as private`);
    }
    verifyExistingNestedGitBeforeCopy(toDir, a.id);
    ensureParentExclude(repoRoot, localGameRegistryRelPath());
    ensureParentExclude(repoRoot, `games/${a.id}`, { directory: true });
  } catch (error) {
    console.error(error && error.message ? error.message : String(error));
    process.exit(1);
  }
}

copyDir(fromDir, toDir);
ensureGameStudioScaffold(toDir, a.id, visibility);

// F2 (T0327 И2c deep-review, lead-ratified 2026-07-07): copy-then-own for the
// items destructive-change lock. The template's own content/items.lock.json
// legitimately lists ITS shipped demo defs (tmpl.gold etc.) -- a fresh game
// has shipped NOTHING to ITS OWN players yet, so its copy must start from an
// empty baseline instead of inheriting the template's shipping history.
// Games copied from a template without the items feature (no content/ dir,
// or no lock file in it) are left alone -- existsSync on a missing nested
// path just returns false, no error.
const itemsLockPath = join(toDir, "content", "items.lock.json");
const itemsLockReset = existsSync(itemsLockPath);
if (itemsLockReset) {
  writeFileSync(
    itemsLockPath,
    `${JSON.stringify(
      {
        schema: "game_seed.items_lock",
        schema_version: 2,
        comment:
          "Baseline of def_id shipped to THIS game's players -- starts empty (copy-then-own reset by " +
          "games/new_game.mjs; see src/features/items/README.md 'Lock workflow' for the destructive-change guard).",
        def_ids: [],
        removed: {},
      },
      null,
      2,
    )}\n`,
  );
}

if (isPrivate) {
  const nestedGit = ensureNestedGit(toDir, a.id);
  const mount = upsertLocalGameMount(repoRoot, {
    gameId: a.id,
    visibility: "private",
    commitPolicy: "nested-private",
    publicAlias: a.publicAlias,
  });
  const preflight = runPrivateGamePreflight(repoRoot, { mounts: [mount] });
  if (!preflight.ok) {
    console.error(`error: private game preflight failed:\n${preflight.violations.map((item) => `- ${item.path}: ${item.reason}`).join("\n")}`);
    process.exit(1);
  }
  console.log(`new private game '${a.id}' created from ${fromRel}/ -> games/${a.id}/`);
  console.log(`nested git repository: ${nestedGit}`);
  console.log(`local private registry: ${localGameRegistryRelPath()} -> ${mount.root}`);
  console.log("public Studio registries, Taskboard, Canvas, and VS Code files were not updated");
  if (itemsLockReset) {
    console.log(`reset items lock baseline (fresh game -- no defs shipped yet): games/${a.id}/content/items.lock.json`);
  }
  process.exit(0);
}

const registered = registerGameAssetSource(repoRoot, {
  id: a.id,
  title: a.id,
  folder: `games/${a.id}`,
  assets: `games/${a.id}/assets`,
  status: "active",
});
const taskboard = ensureProject(repoRoot, {
  title: a.id,
  kind: "game",
  target: `games/${a.id}`,
  tags: ["game"],
  body: `## Goal

Track playable game work for \`games/${a.id}\`.

## In scope

- Game setup, first playable work, assets, validation, and release tasks.

## Out of scope

- Reusable template work and unrelated AI Studio infrastructure.

## Log
`,
});
const vscode = writeVscodeProjectFiles(repoRoot);
console.log(`new game '${a.id}' created from ${fromRel}/ -> games/${a.id}/`);
console.log(`registered assets: ${gameRegistryPath(repoRoot)} -> ${registered.assets}`);
console.log(`${taskboard.created ? "created" : "existing"} taskboard project: ${taskboard.project.fields.id}`);
console.log(`updated VS Code tasks/launch for ${vscode.projects.length} playable project(s)`);
if (itemsLockReset) {
  console.log(`reset items lock baseline (fresh game -- no defs shipped yet): games/${a.id}/content/items.lock.json`);
}
console.log("\nbuild + run:");
console.log(`  cmake -S games/${a.id} -B games/${a.id}/build/native-debug -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug`);
console.log(`  cmake --build games/${a.id}/build/native-debug --target game`);
console.log(`  games/${a.id}/build/native-debug/bin/game.exe`);
console.log(`  or use VS Code: Debug Game ${a.id} (native debug)`);
console.log("\nThen: set the title/pack/concept, pull library assets (skill nt-asset-workflow),");
console.log("copy feature packs from features/ as needed, and build your game on top.");
