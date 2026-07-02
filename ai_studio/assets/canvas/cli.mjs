#!/usr/bin/env node
// Canvas agent client. Same ops layer as the browser page (ops.mjs) — no
// separate logic. Prints compact JSON so agents can pipe results.
//
// usage:
//   node ai_studio/assets/canvas/cli.mjs list
//   node ai_studio/assets/canvas/cli.mjs create --title "My canvas"
//   node ai_studio/assets/canvas/cli.mjs show <id>
//   node ai_studio/assets/canvas/cli.mjs add-image <id> --file path.png
//   node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
//   node ai_studio/assets/canvas/cli.mjs slice <id> --element <eid> [--regions r1,r2]
//   node ai_studio/assets/canvas/cli.mjs export <id> --elements e1,e2 | --all
//   node ai_studio/assets/canvas/cli.mjs undo|redo|history <id>
import { readFileSync } from "node:fs";
import { basename, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { fail, isMain } from "../../core_harness/tool_lib/cli.mjs";
import {
  addImage,
  createProject,
  detectRegions,
  exportElements,
  getProject,
  listProjects,
  patchElement,
  readHistory,
  redoOp,
  sliceRegions,
  undoOp,
} from "./ops.mjs";

const repoRoot = resolve(fileURLToPath(new URL("../../..", import.meta.url)));

function parseFlags(args) {
  const positional = [];
  const flags = {};
  for (let i = 0; i < args.length; i += 1) {
    const arg = args[i];
    if (arg.startsWith("--")) {
      const key = arg.slice(2);
      const value = args[i + 1] && !args[i + 1].startsWith("--") ? args[(i += 1)] : "true";
      flags[key] = value;
    } else {
      positional.push(arg);
    }
  }
  return { positional, flags };
}

function print(value) {
  process.stdout.write(`${JSON.stringify(value)}\n`);
}

function usage() {
  console.log(`usage: cli.mjs <list|create|show|add-image|detect-regions|move|slice|export|undo|redo|history>
  list
  create --title <title>
  show <id>
  add-image <id> --file <path>
  detect-regions <id> --element <eid>
  move <id> --element <eid> --x <n> --y <n>
  slice <id> --element <eid> [--regions r1,r2]
  export <id> --elements e1,e2 | --all
  undo <id>
  redo <id>
  history <id>`);
}

async function main(argv) {
  const [command, ...rest] = argv;
  const { positional, flags } = parseFlags(rest);
  const id = positional[0];

  switch (command) {
    case "list":
      return print({ projects: listProjects(repoRoot) });
    case "create":
      if (!flags.title) fail("create requires --title");
      return print({ project: createProject(repoRoot, { title: flags.title }) });
    case "show":
      if (!id) fail("show requires <id>");
      return print({ project: getProject(repoRoot, id) });
    case "add-image": {
      if (!id) fail("add-image requires <id>");
      if (!flags.file) fail("add-image requires --file <path>");
      const filePath = resolve(flags.file);
      const bytes = readFileSync(filePath);
      return print(addImage(repoRoot, id, { name: basename(filePath), bytes }));
    }
    case "detect-regions": {
      if (!id) fail("detect-regions requires <id>");
      if (!flags.element) fail("detect-regions requires --element <eid>");
      return print(await detectRegions(repoRoot, { projectId: id, elementId: flags.element }));
    }
    case "move": {
      if (!id) fail("move requires <id>");
      if (!flags.element) fail("move requires --element <eid>");
      const patch = {};
      if (flags.x !== undefined) patch.x = Number(flags.x);
      if (flags.y !== undefined) patch.y = Number(flags.y);
      return print(patchElement(repoRoot, id, flags.element, patch));
    }
    case "slice": {
      if (!id) fail("slice requires <id>");
      if (!flags.element) fail("slice requires --element <eid>");
      const regionIds = flags.regions && flags.regions !== "true"
        ? String(flags.regions).split(",").map((value) => value.trim()).filter(Boolean)
        : undefined;
      return print(await sliceRegions(repoRoot, { projectId: id, elementId: flags.element, regionIds }));
    }
    case "export": {
      if (!id) fail("export requires <id>");
      let elementIds;
      if (flags.all === "true") {
        elementIds = (getProject(repoRoot, id).elements || []).map((element) => element.id);
      } else if (flags.elements && flags.elements !== "true") {
        elementIds = String(flags.elements).split(",").map((value) => value.trim()).filter(Boolean);
      } else {
        fail("export requires --elements <e1,e2> or --all");
      }
      return print(exportElements(repoRoot, { projectId: id, elementIds }));
    }
    case "undo":
      if (!id) fail("undo requires <id>");
      return print(undoOp(repoRoot, { projectId: id }));
    case "redo":
      if (!id) fail("redo requires <id>");
      return print(redoOp(repoRoot, { projectId: id }));
    case "history":
      if (!id) fail("history requires <id>");
      return print(readHistory(repoRoot, { projectId: id }));
    case "help":
    case "--help":
    case "-h":
    case undefined:
      return usage();
    default:
      usage();
      return fail(`unknown command: ${command}`);
  }
}

if (isMain(import.meta.url)) {
  main(process.argv.slice(2)).catch((error) => fail(error && error.message ? error.message : String(error)));
}
