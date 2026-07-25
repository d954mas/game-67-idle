#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";
import process from "node:process";
import { fileURLToPath } from "node:url";

const INCLUDE_ANCHOR = "/* scene-scaffold:includes */";
const CATALOG_ANCHOR = "    /* scene-scaffold:catalog */";
const SOURCE_ANCHOR = "    # scene-scaffold:sources";
const DEBUG_SOURCE_ANCHOR = "# scene-scaffold:debug-sources";

function identifier(sceneId) {
  if (!/^[a-z_][a-z0-9._-]{0,126}$/.test(sceneId)) {
    throw new Error(
      "scene id must match [a-z_][a-z0-9._-]{0,126}",
    );
  }
  const value = sceneId.replace(/[^A-Za-z0-9_]/g, "_");
  return value;
}

function findGameScenes(projectRoot) {
  const candidates = [
    path.join(projectRoot, "src", "game_scenes.c"),
    path.join(projectRoot, "src", "scene", "game_scenes.c"),
  ];
  const found = candidates.find((candidate) => fs.existsSync(candidate));
  if (!found) {
    throw new Error("game_scenes.c was not found");
  }
  return found;
}

function insertBefore(source, anchor, text, owner) {
  const first = source.indexOf(anchor);
  if (first < 0 || source.indexOf(anchor, first + anchor.length) >= 0) {
    throw new Error(`${owner}: expected exactly one ${anchor} anchor`);
  }
  return source.slice(0, first) + text + source.slice(first);
}

function generatedHeader(name, symbol) {
  const guard = `GAME_SCENE_${name.toUpperCase()}_H`;
  return `#ifndef ${guard}
#define ${guard}

#include "features/scenes/scene_manager.h"

#include <stdbool.h>

typedef struct ${symbol}_scene {
    bool visible;
} ${symbol}_scene_t;

extern ${symbol}_scene_t g_${symbol}_scene;
extern const scene_api_t g_${symbol}_scene_api;

#endif
`;
}

function generatedSource(name, symbol) {
  return `#include "scenes/${name}_scene.h"

static scene_load_result_t load_step(void *instance) {
    (void)instance;
    return SCENE_LOAD_READY;
}

static void on_show(void *instance, scene_route_args_view_t args) {
    ${symbol}_scene_t *scene = instance;
    (void)args;
    scene->visible = true;
}

static void on_hide(void *instance) {
    ${symbol}_scene_t *scene = instance;
    scene->visible = false;
}

${symbol}_scene_t g_${symbol}_scene;

const scene_api_t g_${symbol}_scene_api = {
    .load_step = load_step,
    .on_show = on_show,
    .on_hide = on_hide,
};
`;
}

function catalogEntry({ sceneId, name, kind, debug }) {
  const entry = `    {
        .id = "${sceneId}",
        .kind = ${kind === "modal" ? "SCENE_KIND_MODAL" : "SCENE_KIND_SCREEN"},
        .debug_only = ${debug ? "true" : "false"},
        .modal_update_policy = SCENE_MODAL_PAUSE_BELOW,
        .instance = &g_${name}_scene,
        .api = &g_${name}_scene_api,
    },
`;
  return debug
    ? `#if GAME_DEBUG_SCENES_ENABLED\n${entry}#endif\n`
    : entry;
}

export function scaffoldScene(options) {
  const projectRoot = path.resolve(options.projectRoot);
  const sceneId = options.sceneId;
  const kind = options.kind ?? "screen";
  const debug = options.debug ?? false;
  const mode = options.mode ?? "dry-run";
  if (
    !sceneId ||
    !["screen", "modal"].includes(kind) ||
    !["dry-run", "check", "apply"].includes(mode)
  ) {
    throw new Error("scene id and kind=screen|modal are required");
  }

  const name = identifier(sceneId);
  const symbol = `game_${name}`;
  const gameScenesPath = findGameScenes(projectRoot);
  const cmakePath = path.join(projectRoot, "CMakeLists.txt");
  const scenesDir = path.join(projectRoot, "src", "scenes");
  const headerPath = path.join(scenesDir, `${name}_scene.h`);
  const sourcePath = path.join(scenesDir, `${name}_scene.c`);
  const relativeSource = `src/scenes/${name}_scene.c`;

  if (!fs.existsSync(cmakePath)) {
    throw new Error("CMakeLists.txt was not found");
  }

  let gameScenes = fs.readFileSync(gameScenesPath, "utf8");
  let cmake = fs.readFileSync(cmakePath, "utf8");
  const conflictingId = [
    ...gameScenes.matchAll(/\.id\s*=\s*"([^"]+)"/g),
  ]
    .map((match) => match[1])
    .find(
      (existingId) =>
        existingId !== sceneId && identifier(existingId) === name,
    );
  if (conflictingId) {
    throw new Error(
      `scene id '${sceneId}' normalizes to '${name}', already used by '${conflictingId}'`,
    );
  }
  const state = [
    fs.existsSync(headerPath),
    fs.existsSync(sourcePath),
    gameScenes.includes(`#include "scenes/${name}_scene.h"`),
    gameScenes.includes(`.id = "${sceneId}"`),
    cmake.includes(relativeSource),
  ];
  if (state.every(Boolean)) {
    return {
      status: "already_exists",
      sceneId,
      projectRoot,
      files: [headerPath, sourcePath],
    };
  }
  if (state.some(Boolean)) {
    throw new Error(`incomplete scaffold state for scene '${sceneId}'`);
  }

  const originalGameScenes = gameScenes;
  const originalCmake = cmake;
  gameScenes = insertBefore(
    gameScenes,
    INCLUDE_ANCHOR,
    `#include "scenes/${name}_scene.h"\n`,
    gameScenesPath,
  );
  gameScenes = insertBefore(
    gameScenes,
    CATALOG_ANCHOR,
    catalogEntry({ sceneId, name: symbol, kind, debug }),
    gameScenesPath,
  );
  if (debug) {
    cmake = insertBefore(
      cmake,
      DEBUG_SOURCE_ANCHOR,
      `if(GAME_DEBUG_SCENES_ENABLED)\n    target_sources(\${GAME_TARGET} PRIVATE ${relativeSource})\nendif()\n`,
      cmakePath,
    );
  } else {
    cmake = insertBefore(cmake, SOURCE_ANCHOR, `    ${relativeSource}\n`, cmakePath);
  }

  const writes = [
    { file: headerPath, content: generatedHeader(name, symbol) },
    { file: sourcePath, content: generatedSource(name, symbol) },
    { file: gameScenesPath, content: gameScenes },
    { file: cmakePath, content: cmake },
  ];
  const result = {
    status: mode === "apply" ? "created" : "would_create",
    sceneId,
    kind,
    debug,
    projectRoot,
    files: writes.map((write) => write.file),
  };
  if (mode !== "apply") {
    return result;
  }

  const scenesDirExisted = fs.existsSync(scenesDir);
  fs.mkdirSync(scenesDir, { recursive: true });
  const writeFileSync = options.writeFileSync ?? fs.writeFileSync;
  const renameSync = options.renameSync ?? fs.renameSync;
  const staged = writes.map((write, index) => ({
    ...write,
    temporary: `${write.file}.scene-scaffold-${process.pid}-${index}.tmp`,
  }));
  const committed = [];
  try {
    for (const write of staged) {
      writeFileSync(write.temporary, write.content, { flag: "wx" });
    }
    for (const write of staged) {
      renameSync(write.temporary, write.file);
      committed.push(write.file);
    }
  } catch (error) {
    if (committed.includes(gameScenesPath)) {
      fs.writeFileSync(gameScenesPath, originalGameScenes);
    }
    if (committed.includes(cmakePath)) {
      fs.writeFileSync(cmakePath, originalCmake);
    }
    for (const file of [headerPath, sourcePath]) {
      if (committed.includes(file) && fs.existsSync(file)) {
        fs.rmSync(file);
      }
    }
    for (const write of staged) {
      if (fs.existsSync(write.temporary)) {
        fs.rmSync(write.temporary);
      }
    }
    if (!scenesDirExisted &&
        fs.existsSync(scenesDir) &&
        fs.readdirSync(scenesDir).length === 0) {
      fs.rmdirSync(scenesDir);
    }
    throw error;
  }
  return result;
}

function parseArgs(argv) {
  const options = { mode: "dry-run", kind: "screen", debug: false };
  for (let i = 0; i < argv.length; ++i) {
    const arg = argv[i];
    if (arg === "--project") options.projectRoot = argv[++i];
    else if (arg === "--id") options.sceneId = argv[++i];
    else if (arg === "--kind") options.kind = argv[++i];
    else if (arg === "--debug") options.debug = true;
    else if (arg === "--apply") options.mode = "apply";
    else if (arg === "--check") options.mode = "check";
    else throw new Error(`unknown argument: ${arg}`);
  }
  if (!options.projectRoot || !options.sceneId) {
    throw new Error("usage: scaffold_scene.mjs --project <game> --id <scene> [--kind screen|modal] [--debug] [--check|--apply]");
  }
  return options;
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  try {
    const result = scaffoldScene(parseArgs(process.argv.slice(2)));
    process.stdout.write(`${JSON.stringify(result)}\n`);
  } catch (error) {
    process.stderr.write(`${JSON.stringify({ status: "error", message: error.message })}\n`);
    process.exitCode = 1;
  }
}
