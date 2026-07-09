#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { listRegisteredGames } from "../assets/backlog/storage/sources/games.mjs";
import { listRegisteredTemplates } from "../assets/backlog/storage/sources/templates.mjs";
import { listGameMounts } from "../workspace/games.mjs";

const defaultRepoRoot = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function parseArgs(argv) {
  const args = { root: "", game: "" };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--root") args.root = argv[++i];
    else if (arg === "--game") args.game = argv[++i];
    else if (arg === "--help" || arg === "-h") args.help = true;
    else throw new Error(`unknown argument: ${arg}`);
  }
  return args;
}

function activeProject(kind, item) {
  if (item.status === "disabled") return null;
  return {
    kind,
    id: item.id,
    title: item.title || item.id,
    folder: item.folder,
  };
}

function projectExists(root, project) {
  return existsSync(join(root, project.folder, "CMakeLists.txt"));
}

function collectPlayableProjects(root) {
  return [
    ...listRegisteredTemplates(root).map((item) => activeProject("template", item)),
    ...listRegisteredGames(root).map((item) => activeProject("game", item)),
  ]
    .filter(Boolean)
    .filter((project) => projectExists(root, project))
    .sort((a, b) => `${a.kind}:${a.id}`.localeCompare(`${b.kind}:${b.id}`));
}

function titleKind(kind) {
  return kind === "template" ? "Template" : "Game";
}

function taskPrefix(project) {
  return `${titleKind(project.kind)}: ${project.id}`;
}

function buildDir(project, config) {
  const prefix = project.folder === "." ? "" : `${project.folder}/`;
  return `\${workspaceFolder}/${prefix}build/native-${config}`;
}

function sourceDir(project) {
  return project.folder === "." ? "${workspaceFolder}" : `\${workspaceFolder}/${project.folder}`;
}

function exePath(project, config) {
  return `${buildDir(project, config)}/bin/game.exe`;
}

function configureTask(project, config) {
  return {
    label: `${taskPrefix(project)}: configure native ${config}`,
    type: "shell",
    command: "cmake",
    args: [
      "-S",
      sourceDir(project),
      "-B",
      buildDir(project, config),
      "-G",
      "Ninja",
      "-DCMAKE_C_COMPILER=clang",
      `-DCMAKE_BUILD_TYPE=${config === "debug" ? "Debug" : "Release"}`,
    ],
    problemMatcher: [],
  };
}

function buildTask(project, target, config, labelPart, group = "build") {
  const task = {
    label: `${taskPrefix(project)}: ${labelPart} native ${config}`,
    type: "shell",
    command: "cmake",
    args: [
      "--build",
      buildDir(project, config),
      "--target",
      target,
    ],
    dependsOn: `${taskPrefix(project)}: configure native ${config}`,
    problemMatcher: ["$gcc"],
  };
  if (group) task.group = group;
  return task;
}

function runTask(project, config) {
  return {
    label: `${taskPrefix(project)}: run native ${config}`,
    type: "shell",
    command: exePath(project, config),
    dependsOn: `${taskPrefix(project)}: build native ${config}`,
    options: {
      cwd: sourceDir(project),
    },
    presentation: {
      panel: "dedicated",
      reveal: "always",
    },
    problemMatcher: [],
  };
}

function captureTask(project) {
  return {
    label: `${taskPrefix(project)}: capture settings native debug`,
    type: "shell",
    command: exePath(project, "debug"),
    args: [
      "--settings",
      "--capture",
      `\${workspaceFolder}/tmp/vscode-${project.kind}-${project.id}-settings.ppm`,
    ],
    dependsOn: `${taskPrefix(project)}: build native debug`,
    options: {
      cwd: sourceDir(project),
    },
    problemMatcher: [],
  };
}

function renderTasks(projects) {
  const tasks = [];
  for (const project of projects) {
    for (const config of ["debug", "release"]) {
      tasks.push(configureTask(project, config));
      tasks.push(buildTask(project, "game_asset_packs", config, "build packs"));
      tasks.push(buildTask(project, "game", config, "build"));
      tasks.push(runTask(project, config));
    }
    tasks.push(captureTask(project));
  }
  return {
    version: "2.0.0",
    tasks,
  };
}

function launchConfig(project, config) {
  const debug = config === "debug";
  return {
    name: `${debug ? "Debug" : "Run"} ${titleKind(project.kind)}: ${project.id} (native ${config})`,
    type: "cppvsdbg",
    request: "launch",
    program: exePath(project, config),
    args: [],
    stopAtEntry: false,
    cwd: sourceDir(project),
    environment: [],
    console: "integratedTerminal",
    preLaunchTask: `${taskPrefix(project)}: build native ${config}`,
  };
}

function renderLaunch(projects) {
  return {
    version: "0.2.0",
    configurations: projects.flatMap((project) => [
      launchConfig(project, "debug"),
      launchConfig(project, "release"),
    ]),
  };
}

function writeJson(path, data) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, `${JSON.stringify(data, null, 2)}\n`, "utf8");
}

function writeVscodeProjectFiles(root = defaultRepoRoot) {
  const projects = collectPlayableProjects(root);
  const vscodeDir = join(root, ".vscode");
  const tasksPath = join(vscodeDir, "tasks.json");
  const launchPath = join(vscodeDir, "launch.json");
  writeJson(tasksPath, renderTasks(projects));
  writeJson(launchPath, renderLaunch(projects));
  return {
    projects,
    tasksPath,
    launchPath,
  };
}

function privateGameProject(root, gameId) {
  const mounts = listGameMounts(root, { activeGameId: gameId });
  const mount = mounts.find((item) => item.gameId === gameId);
  if (!mount) throw new Error(`unknown private game mount: ${gameId}`);
  if (mount.visibility === "public") {
    throw new Error(`game '${gameId}' is public; use the parent .vscode generator`);
  }
  const gameRoot = join(root, mount.root);
  const project = {
    kind: "game",
    id: mount.gameId,
    title: mount.publicAlias || mount.title || mount.gameId,
    folder: ".",
  };
  if (!projectExists(gameRoot, project)) {
    throw new Error(`private game is not playable: ${mount.root}/CMakeLists.txt`);
  }
  return { mount, gameRoot, project };
}

function writePrivateGameVscodeProjectFiles(root = defaultRepoRoot, gameId = "") {
  const id = String(gameId || "").trim();
  if (!id) throw new Error("--game requires a game id");
  const { gameRoot, project } = privateGameProject(root, id);
  const vscodeDir = join(gameRoot, ".vscode");
  const tasksPath = join(vscodeDir, "tasks.json");
  const launchPath = join(vscodeDir, "launch.json");
  writeJson(tasksPath, renderTasks([project]));
  writeJson(launchPath, renderLaunch([project]));
  return {
    project,
    projects: [project],
    tasksPath,
    launchPath,
  };
}

function printUsage() {
  console.log(`usage:
  node ai_studio/dev_environment/vscode_projects.mjs [--root <repo>]
  node ai_studio/dev_environment/vscode_projects.mjs [--root <repo>] --game <private-game-id>

Regenerates parent .vscode/tasks.json and .vscode/launch.json from templates/templates.json and games/games.json.
With --game, writes game-local .vscode files inside a private mounted game after workspace preflight.`);
}

if (import.meta.url === pathToFileURL(process.argv[1]).href) {
  try {
    const args = parseArgs(process.argv.slice(2));
    if (args.help) {
      printUsage();
      process.exit(0);
    }
    const root = args.root ? resolve(args.root) : defaultRepoRoot;
    const result = args.game ? writePrivateGameVscodeProjectFiles(root, args.game) : writeVscodeProjectFiles(root);
    console.log(`wrote ${result.tasksPath}`);
    console.log(`wrote ${result.launchPath}`);
    console.log(`projects=${result.projects.map((project) => `${project.kind}:${project.id}`).join(", ") || "none"}`);
  } catch (error) {
    console.error(error && error.message ? error.message : String(error));
    process.exit(1);
  }
}

export {
  collectPlayableProjects,
  renderLaunch,
  renderTasks,
  writePrivateGameVscodeProjectFiles,
  writeVscodeProjectFiles,
};
