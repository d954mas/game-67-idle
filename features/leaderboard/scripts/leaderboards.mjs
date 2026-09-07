#!/usr/bin/env node
/* CLI over the board manifest: validate it, generate the C table for one
   publish target, print the boards a human must create, or emit the Playgama
   config block. The build calls `generate`; the rest are for people. */

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname } from "node:path";
import {
  consoleChecklist,
  generateHeader,
  mergeLeaderboardsBlock,
  playgamaLeaderboards,
  validateManifest,
} from "../lib/leaderboards.mjs";

function usage() {
  return [
    "usage: leaderboards.mjs <command> --manifest <leaderboards.json> [options]",
    "",
    "  validate                       report every problem in the manifest",
    "  generate --target <t> --out <header>",
    "  checklist                      boards a human must create in a console",
    "  playgama-config --config <playgama-bridge-config.json>",
  ].join("\n");
}

function parseArgs(argv) {
  const args = { _: [] };
  for (let i = 0; i < argv.length; i += 1) {
    const token = argv[i];
    if (token.startsWith("--")) args[token.slice(2)] = argv[++i];
    else args._.push(token);
  }
  return args;
}

function loadManifest(path) {
  const text = readFileSync(path, "utf8");
  return JSON.parse(text);
}

function reportOrExit(manifest, path) {
  const errors = validateManifest(manifest, { file: path });
  if (errors.length === 0) return;
  for (const error of errors) console.error(error);
  process.exit(1);
}

function writeIfChanged(path, content) {
  try {
    if (readFileSync(path, "utf8") === content) return false;
  } catch {
    /* absent is just a first write */
  }
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, content);
  return true;
}

const args = parseArgs(process.argv.slice(2));
const command = args._[0];
if (!command || !args.manifest) {
  console.error(usage());
  process.exit(2);
}

const manifestPath = args.manifest;
const manifest = loadManifest(manifestPath);

if (command === "validate") {
  reportOrExit(manifest, manifestPath);
  console.log(`ok: ${manifest.boards.length} board(s)`);
} else if (command === "generate") {
  reportOrExit(manifest, manifestPath);
  if (!args.target || !args.out) {
    console.error(usage());
    process.exit(2);
  }
  writeIfChanged(args.out, generateHeader(manifest, args.target));
} else if (command === "checklist") {
  reportOrExit(manifest, manifestPath);
  const lines = consoleChecklist(manifest);
  if (lines.length === 0) console.log("no board needs a console");
  for (const line of lines) console.log(line);
} else if (command === "playgama-config") {
  reportOrExit(manifest, manifestPath);
  if (!args.config) {
    console.error(usage());
    process.exit(2);
  }
  const before = readFileSync(args.config, "utf8");
  writeIfChanged(args.config, mergeLeaderboardsBlock(before, playgamaLeaderboards(manifest)));
} else {
  console.error(usage());
  process.exit(2);
}
