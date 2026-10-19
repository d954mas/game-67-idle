#!/usr/bin/env node

import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

import { findStudioRoot } from "./lib/studio_root.mjs";

const GAME_DIR = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const { main } = await import(pathToFileURL(join(findStudioRoot(GAME_DIR), "ai_studio", "game_tools", "build_web.mjs")).href);

process.exitCode = main(process.argv.slice(2), process.env, { gameDir: GAME_DIR });
