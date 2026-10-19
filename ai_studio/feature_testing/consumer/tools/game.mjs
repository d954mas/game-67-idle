#!/usr/bin/env node

import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { findStudioRoot } from "./lib/studio_root.mjs";

// The CLI itself is Studio-owned (ai_studio/game_tools); this file names the game
// and is where the game adds its own commands.
const GAME_DIR = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const { main } = await import(pathToFileURL(join(findStudioRoot(GAME_DIR), "ai_studio", "game_tools", "game_cli.mjs")).href);

process.exitCode = await main(process.argv.slice(2), { gameDir: GAME_DIR, commands: {} });
