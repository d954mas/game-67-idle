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
//   node ai_studio/assets/canvas/cli.mjs add-text <id> [--x n --y n] [--content "..."] [--style-json path] [--group gid]
//   node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
//   node ai_studio/assets/canvas/cli.mjs regions-set <id> --element <eid> --json path.json
//   node ai_studio/assets/canvas/cli.mjs regions-show <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs element-reorder <id> --element <eid> --index <n>
//   node ai_studio/assets/canvas/cli.mjs node-reorder <id> --node <id> --index <n>
//   node ai_studio/assets/canvas/cli.mjs nodes-move <id> --json moves.json
//   node ai_studio/assets/canvas/cli.mjs nodes-reorder <id> --nodes n1,n2 --direction front|back|forward|backward | --index <n>
//   node ai_studio/assets/canvas/cli.mjs slice <id> --element <eid> [--regions r1,r2]
//   node ai_studio/assets/canvas/cli.mjs export-set <id> --element <eid> --json rows.json | --scale 2x [--format --quality --suffix --resample]
//   node ai_studio/assets/canvas/cli.mjs export <id> --elements e1,e2 | --all | --project [--scale --format --quality --suffix --resample] [--to <dir>]
//   node ai_studio/assets/canvas/cli.mjs group-create <id> --name X [--elements e1,e2 | --x --y --w --h] [--parent <gid>|none]
//   node ai_studio/assets/canvas/cli.mjs group-reparent <id> --group g --parent <gid>|none [--index n]
//   node ai_studio/assets/canvas/cli.mjs group-move <id> --group g --x --y
//   node ai_studio/assets/canvas/cli.mjs group-set <id> --group g [--name] [--visible true|false] [--w --h] [--background '#rrggbb'|none] [--clip true|false]
//   node ai_studio/assets/canvas/cli.mjs group-fit <id> --group g [--padding n]
//   node ai_studio/assets/canvas/cli.mjs group-assign <id> --elements e1,e2 --group g|none
//   node ai_studio/assets/canvas/cli.mjs group-ungroup <id> --group g
//   node ai_studio/assets/canvas/cli.mjs group-delete <id> --group g
//   node ai_studio/assets/canvas/cli.mjs render-group <id> --group g [--scale 2] [--background "#rrggbb"]
//   node ai_studio/assets/canvas/cli.mjs undo|redo|history <id>
import { copyFileSync, mkdirSync, readFileSync } from "node:fs";
import { basename, join, resolve } from "node:path";
import { performance } from "node:perf_hooks";
import { fileURLToPath } from "node:url";
import { fail, isMain } from "../../core_harness/tool_lib/cli.mjs";
import {
  addImage,
  addText,
  assignToGroup,
  createGroup,
  createProject,
  deleteGroup,
  deleteProject,
  detectRegions,
  exportElements,
  exportProject,
  fitGroup,
  getProject,
  listProjects,
  moveNodes,
  opsStats,
  patchElement,
  patchElements,
  patchGroup,
  patchProject,
  readHistory,
  recordOpFailure,
  redoOp,
  removeElement,
  removeElements,
  renderGroup,
  reorderElement,
  reorderNode,
  reorderNodes,
  reparentGroup,
  setExportSettings,
  setRegions,
  sliceRegions,
  undoOp,
  ungroupGroup,
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

// A single ad-hoc export row from inline flags (--scale is the trigger). Returns
// undefined when no --scale is given, so `export` then honors each element's stored
// rows and `export-set` demands an explicit source.
function exportRowFromFlags(flags) {
  if (!flags.scale || flags.scale === "true") return undefined;
  const row = { scale: flags.scale };
  if (flags.suffix && flags.suffix !== "true") row.suffix = flags.suffix;
  if (flags.format && flags.format !== "true") row.format = flags.format;
  if (flags.quality && flags.quality !== "true") row.quality = Number(flags.quality);
  if (flags.resample && flags.resample !== "true") row.resample = flags.resample;
  return [row];
}

// Deliver-to-disk for the CLI: copy an export result's files (+ manifest) into an
// explicit --to directory. The op already produced them in the confined
// <project>/export/<stamp>/; this is the agent-side destination, uncofined by design.
function copyExportTo(result, toDir) {
  mkdirSync(toDir, { recursive: true });
  const files = new Set(["manifest.json"]);
  for (const entry of result.items || result.screens || []) if (entry.file) files.add(entry.file);
  const copied = [];
  for (const file of files) {
    copyFileSync(join(result.folder, file), join(toDir, file));
    copied.push(file);
  }
  return copied;
}

function usage() {
  console.log(`usage: cli.mjs <list|create|show|rename|delete|add-image|add-text|detect-regions|move|element-set|element-remove|elements-set|elements-remove|element-reorder|node-reorder|nodes-move|nodes-reorder|regions-set|regions-show|slice|export-set|export|group-create|group-reparent|group-move|group-set|group-fit|group-assign|group-ungroup|group-delete|render-group|undo|redo|history>
  list
  create [--title <title>]     (omit --title for a random default)
  show <id>
  rename <id> --title <title>
  delete <id>
  add-image <id> --file <path>
  add-text <id> [--x <n> --y <n>] [--content "<text>"] [--style-json <path>] [--group <gid>]
  detect-regions <id> --element <eid>
  move <id> --element <eid> --x <n> --y <n>
  element-set <id> --element <eid> [--name <name>] [--visible true|false] [--content "<text>"] [--style-json <path>]
  element-remove <id> --element <eid>
  elements-set <id> --json <path>   (batched patch: [{elementId,x?,y?,w?,h?,name?,visible?}] or {patches:[...]}; one undo step)
  elements-remove <id> --elements e1,e2   (batched delete; one undo step)
  element-reorder <id> --element <eid> --index <n>   (z-order among siblings; 0 = back)
  node-reorder <id> --node <id> --index <n>   (z-order of an element OR group among merged siblings; 0 = back)
  nodes-move <id> --json <path>   (batched mixed element+group move: [{nodeId,x,y}] or {moves:[...]}; one undo step)
  nodes-reorder <id> --nodes n1,n2 --direction front|back|forward|backward | --index <n>   (multi-node z-order block; one undo step)
  regions-set <id> --element <eid> --json <path>   (JSON: a regions array or {regions:[...]})
  regions-show <id> --element <eid>
  slice <id> --element <eid> [--regions r1,r2]
  export-set <id> --element <eid> --json <path> | --scale <t> [--suffix <s>] [--format png|jpg|webp] [--quality 1-100] [--resample lanczos|nearest]
  export <id> --elements e1,e2 | --all | --project [--scale <t> --format <f> --quality <n> --suffix <s> --resample <r>] [--to <dir>]
  group-create <id> --name <name> [--elements e1,e2 | --x <n> --y <n> --w <n> --h <n>] [--parent <gid>|none]
  group-reparent <id> --group <gid> --parent <gid>|none [--index <n>]   (nest a group; none = top level)
  group-move <id> --group <gid> --x <n> --y <n>
  group-set <id> --group <gid> [--name <name>] [--visible true|false] [--w <n> --h <n>] [--background '#rrggbb'|none] [--clip true|false]
  group-fit <id> --group <gid> [--padding <n>]   (resize the frame to fit its content; padding default 24)
  group-assign <id> --elements e1,e2 --group <gid>|none
  group-ungroup <id> --group <gid>   (dissolve one level; children keep the group's z-slot; one undo step)
  group-delete <id> --group <gid>
  render-group <id> --group <gid>  (alias: render-screen) [--scale <n>] [--background '#rrggbb']
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
    case "add-text": {
      if (!id) fail("add-text requires <id>");
      const args = {};
      if (flags.x !== undefined) args.x = Number(flags.x);
      if (flags.y !== undefined) args.y = Number(flags.y);
      if (flags.content && flags.content !== "true") args.content = flags.content;
      if (flags["style-json"] && flags["style-json"] !== "true") {
        args.style = JSON.parse(readFileSync(resolve(flags["style-json"]), "utf8"));
      }
      if (flags.group && flags.group !== "true" && flags.group !== "none") args.groupId = flags.group;
      // addText(root, projectId, args) — the op validates style against fonts.json.
      return print(addText(repoRoot, id, args));
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
      // Text-element edits: --content sets the string (\n for newlines); --style-json is
      // a partial or full style object shallow-merged + validated against fonts.json.
      if (flags.content !== undefined && flags.content !== "true") patch.content = flags.content;
      if (flags["style-json"] && flags["style-json"] !== "true") {
        patch.style = JSON.parse(readFileSync(resolve(flags["style-json"]), "utf8"));
      }
      if (!Object.keys(patch).length) fail("element-set requires --name, --visible, --content, and/or --style-json");
      return print(patchElement(repoRoot, id, flags.element, patch));
    }
    case "element-remove": {
      if (!id) fail("element-remove requires <id>");
      if (!flags.element) fail("element-remove requires --element <eid>");
      return print(removeElement(repoRoot, id, flags.element));
    }
    case "elements-set": {
      // Batched multi-element patch (one journal entry). --json is a patches array
      // or a { patches: [...] } wrapper; each patch is {elementId, x?, y?, w?, h?,
      // name?, visible?} — same fields as `move`/`element-set`, applied together.
      if (!id) fail("elements-set requires <id>");
      if (!flags.json || flags.json === "true") fail("elements-set requires --json <path> (a patches array or {patches:[...]})");
      const raw = JSON.parse(readFileSync(resolve(flags.json), "utf8"));
      const patches = Array.isArray(raw) ? raw : raw.patches;
      return print(patchElements(repoRoot, { projectId: id, patches }));
    }
    case "elements-remove": {
      // Batched multi-element delete (one journal entry; one undo restores all).
      if (!id) fail("elements-remove requires <id>");
      if (!flags.elements || flags.elements === "true") fail("elements-remove requires --elements e1,e2");
      const elementIds = String(flags.elements).split(",").map((value) => value.trim()).filter(Boolean);
      return print(removeElements(repoRoot, { projectId: id, elementIds }));
    }
    case "element-reorder": {
      if (!id) fail("element-reorder requires <id>");
      if (!flags.element) fail("element-reorder requires --element <eid>");
      if (flags.index === undefined || flags.index === "true") fail("element-reorder requires --index <n>");
      return print(reorderElement(repoRoot, { projectId: id, elementId: flags.element, index: Number(flags.index) }));
    }
    case "node-reorder": {
      if (!id) fail("node-reorder requires <id>");
      if (!flags.node || flags.node === "true") fail("node-reorder requires --node <id>");
      if (flags.index === undefined || flags.index === "true") fail("node-reorder requires --index <n>");
      return print(reorderNode(repoRoot, { projectId: id, nodeId: flags.node, index: Number(flags.index) }));
    }
    case "nodes-move": {
      // Batched mixed element+group move (one journal entry). --json is a moves array
      // or a { moves: [...] } wrapper; each move is {nodeId, x, y} (absolute top-left).
      if (!id) fail("nodes-move requires <id>");
      if (!flags.json || flags.json === "true") fail("nodes-move requires --json <path> (a moves array or {moves:[...]})");
      const raw = JSON.parse(readFileSync(resolve(flags.json), "utf8"));
      const moves = Array.isArray(raw) ? raw : raw.moves;
      return print(moveNodes(repoRoot, { projectId: id, moves }));
    }
    case "nodes-reorder": {
      // Batched multi-node z-order (one journal entry): the selected same-scope siblings
      // move as a block via --direction, or to an absolute --index (single scope only).
      if (!id) fail("nodes-reorder requires <id>");
      if (!flags.nodes || flags.nodes === "true") fail("nodes-reorder requires --nodes n1,n2");
      const nodeIds = String(flags.nodes).split(",").map((value) => value.trim()).filter(Boolean);
      const args = { projectId: id, nodeIds };
      if (flags.direction && flags.direction !== "true") args.direction = flags.direction;
      if (flags.index !== undefined && flags.index !== "true") args.index = Number(flags.index);
      return print(reorderNodes(repoRoot, args));
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
      const regions = element.regions || [];
      // `shapes` annotates each region as rect or polygon(<vertex-count>) without
      // touching the round-tripping `regions` array.
      const shapes = regions.map((region) => ({
        id: region.id,
        shape: Array.isArray(region.polygon) && region.polygon.length >= 3 ? `polygon(${region.polygon.length})` : "rect",
      }));
      return print({ elementId: element.id, regions, shapes });
    }
    case "slice": {
      if (!id) fail("slice requires <id>");
      if (!flags.element) fail("slice requires --element <eid>");
      const regionIds = flags.regions && flags.regions !== "true"
        ? String(flags.regions).split(",").map((value) => value.trim()).filter(Boolean)
        : undefined;
      return print(await sliceRegions(repoRoot, { projectId: id, elementId: flags.element, regionIds }));
    }
    case "export-set": {
      if (!id) fail("export-set requires <id>");
      if (!flags.element) fail("export-set requires --element <eid>");
      let rows;
      if (flags.json && flags.json !== "true") {
        // A bare rows array or a { rows: [...] } wrapper.
        const raw = JSON.parse(readFileSync(resolve(flags.json), "utf8"));
        rows = Array.isArray(raw) ? raw : raw.rows;
      } else {
        rows = exportRowFromFlags(flags);
        if (!rows) fail("export-set requires --json <path> or --scale <t> [--suffix --format --quality --resample]");
      }
      return print(setExportSettings(repoRoot, { projectId: id, elementId: flags.element, rows }));
    }
    case "export": {
      if (!id) fail("export requires <id>");
      const rows = exportRowFromFlags(flags); // undefined => honor each element's stored rows
      let result;
      if (flags.project === "true") {
        result = await exportProject(repoRoot, { projectId: id });
      } else {
        let elementIds;
        if (flags.all === "true") {
          elementIds = (getProject(repoRoot, id).elements || []).map((element) => element.id);
        } else if (flags.elements && flags.elements !== "true") {
          elementIds = String(flags.elements).split(",").map((value) => value.trim()).filter(Boolean);
        } else {
          fail("export requires --elements <e1,e2>, --all, or --project");
        }
        result = await exportElements(repoRoot, { projectId: id, elementIds, rows });
      }
      // --to writes the produced files to an explicit path; without it the confined
      // <project>/export/<stamp>/ automation default stays (agents rely on it).
      if (flags.to && flags.to !== "true") {
        const toDir = resolve(flags.to);
        result = { ...result, to: toDir, copied: copyExportTo(result, toDir) };
      }
      return print(result);
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
      // --parent <gid> nests the new group; --parent none forces top level. Omit to
      // let fromElements default to the members' common parent (else root).
      if (flags.parent !== undefined && flags.parent !== "true") {
        args.parentId = flags.parent === "none" ? null : flags.parent;
      }
      return print(createGroup(repoRoot, args));
    }
    case "group-reparent": {
      if (!id) fail("group-reparent requires <id>");
      if (!flags.group) fail("group-reparent requires --group <gid>");
      const args = { projectId: id, groupId: flags.group };
      // --parent <gid> nests under that group; --parent none (or omitted) = top level.
      args.parentId = !flags.parent || flags.parent === "none" || flags.parent === "true" ? null : flags.parent;
      if (flags.index !== undefined && flags.index !== "true") args.index = Number(flags.index);
      return print(reparentGroup(repoRoot, args));
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
      // --background '#rrggbb' sets a solid group fill; --background none clears it.
      // Any other value reaches the op and fails loudly (no silent fallback).
      if (flags.background !== undefined) {
        args.background = flags.background === "none" ? null : { type: "color", color: flags.background };
      }
      // --clip true|false toggles the Figma frame clip. Convert the string flag to a real
      // boolean here; the op validates strictly (a boolean, no silent coercion).
      if (flags.clip !== undefined) {
        if (flags.clip !== "true" && flags.clip !== "false") fail("group-set --clip must be true or false");
        args.clip = flags.clip === "true";
      }
      return print(patchGroup(repoRoot, args));
    }
    case "group-fit": {
      if (!id) fail("group-fit requires <id>");
      if (!flags.group) fail("group-fit requires --group <gid>");
      const args = { projectId: id, groupId: flags.group };
      // --padding overrides the default (24). Passed through as a number so the op
      // validates it (finite >= 0, else a loud error — no silent fallback).
      if (flags.padding !== undefined && flags.padding !== "true") args.padding = Number(flags.padding);
      return print(fitGroup(repoRoot, args));
    }
    case "group-assign": {
      if (!id) fail("group-assign requires <id>");
      if (!flags.elements || flags.elements === "true") fail("group-assign requires --elements e1,e2");
      const elementIds = String(flags.elements).split(",").map((value) => value.trim()).filter(Boolean);
      const groupId = !flags.group || flags.group === "none" ? null : flags.group;
      return print(assignToGroup(repoRoot, { projectId: id, elementIds, groupId }));
    }
    case "group-ungroup": {
      if (!id) fail("group-ungroup requires <id>");
      if (!flags.group) fail("group-ungroup requires --group <gid>");
      return print(ungroupGroup(repoRoot, { projectId: id, groupId: flags.group }));
    }
    case "group-delete": {
      if (!id) fail("group-delete requires <id>");
      if (!flags.group) fail("group-delete requires --group <gid>");
      return print(deleteGroup(repoRoot, { projectId: id, groupId: flags.group }));
    }
    case "render-group":
    case "render-screen": {
      if (!id) fail("render-group requires <id>");
      if (!flags.group) fail("render-group requires --group <gid>");
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
