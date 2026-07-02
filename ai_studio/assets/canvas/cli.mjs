#!/usr/bin/env node
// Canvas agent client. Same ops layer as the browser page (ops.mjs) — no
// separate logic. Prints compact JSON so agents can pipe results.
//
// usage:
//   node ai_studio/assets/canvas/cli.mjs list
//   node ai_studio/assets/canvas/cli.mjs create [--title "My canvas"]
//   node ai_studio/assets/canvas/cli.mjs show <id>
//   node ai_studio/assets/canvas/cli.mjs rename <id> --title "New title"
//   node ai_studio/assets/canvas/cli.mjs delete <id>
//   node ai_studio/assets/canvas/cli.mjs add-image <id> --file path.png
//   node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
//   node ai_studio/assets/canvas/cli.mjs regions-set <id> --element <eid> --json path.json
//   node ai_studio/assets/canvas/cli.mjs regions-show <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs slice <id> --element <eid> [--regions r1,r2]
//   node ai_studio/assets/canvas/cli.mjs export <id> --elements e1,e2 | --all
//   node ai_studio/assets/canvas/cli.mjs group-create <id> --name X [--elements e1,e2 | --x --y --w --h]
//   node ai_studio/assets/canvas/cli.mjs group-move <id> --group g --x --y
//   node ai_studio/assets/canvas/cli.mjs group-set <id> --group g [--name] [--visible true|false] [--w --h]
//   node ai_studio/assets/canvas/cli.mjs group-assign <id> --elements e1,e2 --group g|none
//   node ai_studio/assets/canvas/cli.mjs group-delete <id> --group g
//   node ai_studio/assets/canvas/cli.mjs render-screen <id> --group g [--scale 2] [--background '#rrggbb']
//   node ai_studio/assets/canvas/cli.mjs undo|redo|history <id>
import { readFileSync } from "node:fs";
import { basename, resolve } from "node:path";
import { performance } from "node:perf_hooks";
import { fileURLToPath } from "node:url";
import { fail, isMain } from "../../core_harness/tool_lib/cli.mjs";
import {
  addImage,
  assignToGroup,
  createGroup,
  createProject,
  deleteGroup,
  deleteProject,
  detectRegions,
  exportElements,
  getProject,
  listProjects,
  opsStats,
  patchElement,
  patchGroup,
  patchProject,
  readHistory,
  recordOpFailure,
  redoOp,
  removeElement,
  renderGroup,
  setRegions,
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
  console.log(`usage: cli.mjs <list|create|show|rename|delete|add-image|detect-regions|move|element-set|element-remove|regions-set|regions-show|slice|export|group-create|group-move|group-set|group-assign|group-delete|render-screen|undo|redo|history>
  list
  create [--title <title>]     (omit --title for a random default)
  show <id>
  rename <id> --title <title>
  delete <id>
  add-image <id> --file <path>
  detect-regions <id> --element <eid>
  move <id> --element <eid> --x <n> --y <n>
  element-set <id> --element <eid> [--name <name>] [--visible true|false]
  element-remove <id> --element <eid>
  regions-set <id> --element <eid> --json <path>   (JSON: a regions array or {regions:[...]})
  regions-show <id> --element <eid>
  slice <id> --element <eid> [--regions r1,r2]
  export <id> --elements e1,e2 | --all
  group-create <id> --name <name> [--elements e1,e2 | --x <n> --y <n> --w <n> --h <n>]
  group-move <id> --group <gid> --x <n> --y <n>
  group-set <id> --group <gid> [--name <name>] [--visible true|false] [--w <n> --h <n>]
  group-assign <id> --elements e1,e2 --group <gid>|none
  group-delete <id> --group <gid>
  render-screen <id> --group <gid> [--scale <n>] [--background '#rrggbb']
  undo <id>
  redo <id>
  history <id>`);
}

async function runCommand(command, id, positional, flags) {
  switch (command) {
    case "list":
      return print({ projects: listProjects(repoRoot) });
    case "create":
      // --title is optional: a missing/empty title gets a random default
      // ("Amber Fox"-style) from the op layer, matching the page's instant-create.
      return print({ project: createProject(repoRoot, { title: flags.title }) });
    case "show":
      if (!id) fail("show requires <id>");
      return print({ project: getProject(repoRoot, id) });
    case "rename":
      if (!id) fail("rename requires <id>");
      if (!flags.title || flags.title === "true") fail("rename requires --title <title>");
      return print(patchProject(repoRoot, { projectId: id, title: flags.title }));
    case "delete":
      if (!id) fail("delete requires <id>");
      return print(deleteProject(repoRoot, { projectId: id }));
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
    case "element-set": {
      if (!id) fail("element-set requires <id>");
      if (!flags.element) fail("element-set requires --element <eid>");
      const patch = {};
      if (flags.name && flags.name !== "true") patch.name = flags.name;
      if (flags.visible !== undefined) patch.visible = flags.visible === "true";
      if (!Object.keys(patch).length) fail("element-set requires --name and/or --visible");
      return print(patchElement(repoRoot, id, flags.element, patch));
    }
    case "element-remove": {
      if (!id) fail("element-remove requires <id>");
      if (!flags.element) fail("element-remove requires --element <eid>");
      return print(removeElement(repoRoot, id, flags.element));
    }
    case "regions-set": {
      if (!id) fail("regions-set requires <id>");
      if (!flags.element) fail("regions-set requires --element <eid>");
      if (!flags.json || flags.json === "true") fail("regions-set requires --json <path>");
      // The JSON file may be a bare regions array or a { regions: [...] } wrapper
      // (e.g. a detect-regions dump), so accept either shape.
      const raw = JSON.parse(readFileSync(resolve(flags.json), "utf8"));
      const regions = Array.isArray(raw) ? raw : raw.regions;
      return print(setRegions(repoRoot, { projectId: id, elementId: flags.element, regions }));
    }
    case "regions-show": {
      if (!id) fail("regions-show requires <id>");
      if (!flags.element) fail("regions-show requires --element <eid>");
      const element = (getProject(repoRoot, id).elements || []).find((item) => item.id === flags.element);
      if (!element) fail(`element not found: ${flags.element}`);
      return print({ elementId: element.id, regions: element.regions || [] });
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
    case "group-create": {
      if (!id) fail("group-create requires <id>");
      if (!flags.name) fail("group-create requires --name <name>");
      const args = { projectId: id, name: flags.name };
      if (flags.elements && flags.elements !== "true") {
        args.fromElements = String(flags.elements).split(",").map((value) => value.trim()).filter(Boolean);
      } else {
        if (flags.x !== undefined) args.x = Number(flags.x);
        if (flags.y !== undefined) args.y = Number(flags.y);
        if (flags.w !== undefined) args.w = Number(flags.w);
        if (flags.h !== undefined) args.h = Number(flags.h);
      }
      return print(createGroup(repoRoot, args));
    }
    case "group-move": {
      if (!id) fail("group-move requires <id>");
      if (!flags.group) fail("group-move requires --group <gid>");
      const args = { projectId: id, groupId: flags.group };
      if (flags.x !== undefined) args.x = Number(flags.x);
      if (flags.y !== undefined) args.y = Number(flags.y);
      return print(patchGroup(repoRoot, args));
    }
    case "group-set": {
      if (!id) fail("group-set requires <id>");
      if (!flags.group) fail("group-set requires --group <gid>");
      const args = { projectId: id, groupId: flags.group };
      if (flags.name !== undefined && flags.name !== "true") args.name = flags.name;
      if (flags.visible !== undefined) args.visible = flags.visible;
      if (flags.w !== undefined) args.w = Number(flags.w);
      if (flags.h !== undefined) args.h = Number(flags.h);
      if (flags.x !== undefined) args.x = Number(flags.x);
      if (flags.y !== undefined) args.y = Number(flags.y);
      return print(patchGroup(repoRoot, args));
    }
    case "group-assign": {
      if (!id) fail("group-assign requires <id>");
      if (!flags.elements || flags.elements === "true") fail("group-assign requires --elements e1,e2");
      const elementIds = String(flags.elements).split(",").map((value) => value.trim()).filter(Boolean);
      const groupId = !flags.group || flags.group === "none" ? null : flags.group;
      return print(assignToGroup(repoRoot, { projectId: id, elementIds, groupId }));
    }
    case "group-delete": {
      if (!id) fail("group-delete requires <id>");
      if (!flags.group) fail("group-delete requires --group <gid>");
      return print(deleteGroup(repoRoot, { projectId: id, groupId: flags.group }));
    }
    case "render-screen": {
      if (!id) fail("render-screen requires <id>");
      if (!flags.group) fail("render-screen requires --group <gid>");
      const args = { projectId: id, groupId: flags.group };
      if (flags.scale !== undefined) args.scale = Number(flags.scale);
      if (flags.background !== undefined && flags.background !== "true") args.background = flags.background;
      return print(await renderGroup(repoRoot, args));
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
    case "ops-stats":
      if (!id) fail("ops-stats requires <id>");
      return print(opsStats(repoRoot, { projectId: id }));
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

async function main(argv) {
  const [command, ...rest] = argv;
  const { positional, flags } = parseFlags(rest);
  const id = positional[0];
  const startedAt = performance.now();
  try {
    return await runCommand(command, id, positional, flags);
  } catch (error) {
    // Mirror the API: a project-resolvable failure leaves a trail in
    // <project>/errors.jsonl (recordOpFailure no-ops when id can't resolve).
    recordOpFailure(repoRoot, id, {
      op: command || "",
      args_summary: flags || {},
      error,
      duration_ms: performance.now() - startedAt,
    });
    throw error;
  }
}

if (isMain(import.meta.url)) {
  main(process.argv.slice(2)).catch((error) => fail(error && error.message ? error.message : String(error)));
}
