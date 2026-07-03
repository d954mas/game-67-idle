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
//   node ai_studio/assets/canvas/cli.mjs add-images <id> --files a.png,b.png   (batched; one undo)
//   node ai_studio/assets/canvas/cli.mjs add-text <id> [--x n --y n] [--content "..."] [--style-json path] [--group gid]
//   node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
//   node ai_studio/assets/canvas/cli.mjs regions-set <id> --element <eid> --json path.json
//   node ai_studio/assets/canvas/cli.mjs regions-show <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs element-reorder <id> --element <eid> --index <n>
//   node ai_studio/assets/canvas/cli.mjs node-reorder <id> --node <id> --index <n>
//   node ai_studio/assets/canvas/cli.mjs nodes-move <id> --json moves.json
//   node ai_studio/assets/canvas/cli.mjs nodes-reorder <id> --nodes n1,n2 --direction front|back|forward|backward | --index <n>
//   node ai_studio/assets/canvas/cli.mjs nodes-align <id> --nodes n1,n2 --align left|hcenter|right|top|vcenter|bottom [--reference auto|selection|parent]
//   node ai_studio/assets/canvas/cli.mjs nodes-distribute <id> --nodes n1,n2,n3 --axis h|v
//   node ai_studio/assets/canvas/cli.mjs nodes-paste <id> --spec spec.json [--dx n --dy n] [--group <gid>|none]
//   node ai_studio/assets/canvas/cli.mjs nodes-duplicate <id> --nodes id1,id2 [--dx n --dy n] [--group <gid>|none]
//   node ai_studio/assets/canvas/cli.mjs nodes-delete <id> --nodes id1,id2
//   node ai_studio/assets/canvas/cli.mjs slice <id> --element <eid> [--regions r1,r2]
//   node ai_studio/assets/canvas/cli.mjs alpha <id> --element <eid> [--method auto|matte] [--regions r1,r2]
//   node ai_studio/assets/canvas/cli.mjs alpha <id> --elements e1,e2 [--method auto|matte]   (batch; one undo)
//   node ai_studio/assets/canvas/cli.mjs alpha-dual <id> --elements a,b   (white+black plate pair -> new element; one undo)
//   node ai_studio/assets/canvas/cli.mjs alpha-dual-generate <id> --element <el> [--prompt "..."]   (AUTOMATIC: element = light plate, generates the dark plate, gates, cuts; one undo)
//   node ai_studio/assets/canvas/cli.mjs add-image-from-file <id> --src files/<hash>.png [--name X] [--x n --y n]   (mint an element from an EXISTING project file, no re-upload)
//   node ai_studio/assets/canvas/cli.mjs export-set <id> --element <eid> --json rows.json | --scale 2x [--format --quality --resample --base]
//   node ai_studio/assets/canvas/cli.mjs export <id> --elements e1,e2 | --all | --project [--scale --format --quality --resample --base] [--to <dir>] [--zip <path>]
//   node ai_studio/assets/canvas/cli.mjs group-create <id> --name X [--elements e1,e2 | --x --y --w --h] [--parent <gid>|none]
//   node ai_studio/assets/canvas/cli.mjs group-reparent <id> --group g --parent <gid>|none [--index n]
//   node ai_studio/assets/canvas/cli.mjs group-move <id> --group g --x --y
//   node ai_studio/assets/canvas/cli.mjs group-set <id> --group g [--name] [--visible true|false] [--w --h] [--background '#rrggbb'|none] [--clip true|false]
//   node ai_studio/assets/canvas/cli.mjs groups-set <id> --groups g1,g2 [--visible true|false] [--clip true|false]   (batched shared toggles; one undo)
//   node ai_studio/assets/canvas/cli.mjs group-fit <id> --group g [--padding n]
//   node ai_studio/assets/canvas/cli.mjs group-assign <id> --elements e1,e2 --group g|none
//   node ai_studio/assets/canvas/cli.mjs group-ungroup <id> --group g
//   node ai_studio/assets/canvas/cli.mjs group-delete <id> --group g
//   node ai_studio/assets/canvas/cli.mjs render-group <id> --group g [--scale 2] [--background "#rrggbb"]
//   node ai_studio/assets/canvas/cli.mjs undo <id> --expect-head <n>
//   node ai_studio/assets/canvas/cli.mjs redo <id> --expect-head <n>
//   node ai_studio/assets/canvas/cli.mjs history <id>
//   node ai_studio/assets/canvas/cli.mjs history-list <id>   (prints "head: N" then the JSON; note N)
//   node ai_studio/assets/canvas/cli.mjs history-jump <id> --seq <n> --expect-head <n>   (0 = base; like N undos/redos, undoable)
//   (undo/redo/history-jump require --expect-head N: the project may be live, so read
//   the current head from history-list right before acting — see README "History panel
//   + jump" / T0234)
import { copyFileSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, join, resolve } from "node:path";
import { performance } from "node:perf_hooks";
import { fileURLToPath } from "node:url";
import { fail, isMain } from "../../core_harness/tool_lib/cli.mjs";
import {
  addImage,
  addImageFromFile,
  addImages,
  addText,
  alignNodes,
  alphaCutout,
  alphaDualPlate,
  alphaDualPlateGenerate,
  assignToGroup,
  createGroup,
  createProject,
  deleteGroup,
  deleteNodes,
  deleteProject,
  detectRegions,
  distributeNodes,
  duplicateNodes,
  exportElements,
  exportProject,
  fitGroup,
  getProject,
  jumpHistory,
  listHistory,
  listProjects,
  moveNodes,
  opsStats,
  pasteNodes,
  patchElement,
  patchElements,
  patchGroup,
  patchGroups,
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
  setOpsActor,
  setRegions,
  sliceRegions,
  undoOp,
  ungroupGroup,
  zipExport,
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
// rows and `export-set` demands an explicit source. (T0229: --suffix is gone — export
// file names are automatic. T0235: --base source|canvas selects which dims the scale
// token resolves against; the row layer (ops.cleanExportRows) validates it loudly.)
function exportRowFromFlags(flags) {
  if (!flags.scale || flags.scale === "true") return undefined;
  const row = { scale: flags.scale };
  if (flags.format && flags.format !== "true") row.format = flags.format;
  if (flags.quality && flags.quality !== "true") row.quality = Number(flags.quality);
  if (flags.resample && flags.resample !== "true") row.resample = flags.resample;
  if (flags.base && flags.base !== "true") row.base = flags.base;
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
  console.log(`usage: cli.mjs <list|create|show|rename|delete|add-image|add-images|add-image-from-file|add-text|detect-regions|move|element-set|element-remove|elements-set|elements-remove|element-reorder|node-reorder|nodes-move|nodes-reorder|nodes-align|nodes-distribute|nodes-paste|nodes-duplicate|nodes-delete|regions-set|regions-show|slice|alpha|alpha-dual|alpha-dual-generate|export-set|export|group-create|group-reparent|group-move|group-set|groups-set|group-fit|group-assign|group-ungroup|group-delete|render-group|undo|redo|history|history-list|history-jump>
  list
  create [--title <title>]     (omit --title for a random default)
  show <id>
  rename <id> --title <title>
  delete <id>
  add-image <id> --file <path>
  add-images <id> --files a.png,b.png   (batched multi-image add; one undo step)
  add-image-from-file <id> --src <files/hash.png> [--name <name>] [--x <n> --y <n>]   (mint an element from an EXISTING project file; no re-upload, no duplicate bytes)
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
  nodes-align <id> --nodes n1,n2 --align left|hcenter|right|top|vcenter|bottom [--reference auto|selection|parent]   (2+ nodes -> selection bbox; 1 node in a group -> the group frame; one undo step)
  nodes-distribute <id> --nodes n1,n2,n3 --axis h|v   (equal-gap distribute; 3+ nodes; endpoints fixed; one undo step)
  nodes-paste <id> --spec <path> [--dx <n> --dy <n>] [--group <gid>|none]   (instantiate a copied node spec; new ids; one undo step)
  nodes-duplicate <id> --nodes id1,id2 [--dx <n> --dy <n>] [--group <gid>|none]   (duplicate live nodes in place +offset; one undo step)
  nodes-delete <id> --nodes id1,id2   (batched mixed element+group subtree delete; one undo step)
  regions-set <id> --element <eid> --json <path>   (JSON: a regions array or {regions:[...]})
  regions-show <id> --element <eid>
  slice <id> --element <eid> [--regions r1,r2]
  alpha <id> --element <eid> [--method auto|matte] [--regions r1,r2]   (alpha-cutout the element; auto routes, matte forces key_matte; one undo)
  alpha <id> --elements e1,e2 [--method auto|matte]   (batch: 2+ images keyed into ONE journal entry/undo; no --regions with a batch)
  alpha-dual <id> --elements a,b   (white-plate + black-plate pair -> ONE new cut element; either order; plates untouched; one undo step)
  alpha-dual-generate <id> --element <eid> [--prompt "<extra subject description>"]   (AUTOMATIC: the element's current pixels are the LIGHT plate; generates the DARK plate via codex edit, gates it (one automatic retry), cuts -> ONE new element beside the source; source untouched; one undo step)
  export-set <id> --element <eid> --json <path> | --scale <t> [--format png|jpg|webp] [--quality 1-100] [--resample lanczos|nearest] [--base source|canvas]
  export <id> --elements e1,e2 | --all | --project [--scale <t> --format <f> --quality <n> --resample <r> --base source|canvas] [--to <dir>] [--zip <path>]
  group-create <id> --name <name> [--elements e1,e2 | --x <n> --y <n> --w <n> --h <n>] [--parent <gid>|none]
  group-reparent <id> --group <gid> --parent <gid>|none [--index <n>]   (nest a group; none = top level)
  group-move <id> --group <gid> --x <n> --y <n>
  group-set <id> --group <gid> [--name <name>] [--visible true|false] [--w <n> --h <n>] [--background '#rrggbb'|none] [--clip true|false]
  groups-set <id> --groups g1,g2 [--visible true|false] [--clip true|false]   (batched shared toggles; one undo step)
  group-fit <id> --group <gid> [--padding <n>]   (resize the frame to fit its content; padding default 24)
  group-assign <id> --elements e1,e2 --group <gid>|none
  group-ungroup <id> --group <gid>   (dissolve one level; children keep the group's z-slot; one undo step)
  group-delete <id> --group <gid>
  render-group <id> --group <gid>  (alias: render-screen) [--scale <n>] [--background '#rrggbb']
  undo <id> --expect-head <n>
  redo <id> --expect-head <n>
  history <id>
  history-list <id>   (labeled linear history the panel shows: Base + undo chain + dimmed redo tail; prints "head: N" first)
  history-jump <id> --seq <n> --expect-head <n>   (jump the applied head to a spine seq; 0 = base; like N undos/redos, undoable)
  (undo/redo/history-jump require --expect-head N — the project may be live; run
  history-list to read the current head, then pass it right before acting; a stale
  value refuses loudly and writes nothing)`);
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
    case "add-images": {
      // Batched multi-image add (one journal entry; one undo restores all) — the CLI
      // parity for the page's multi-file drop/paste. --files is a comma-separated list.
      if (!id) fail("add-images requires <id>");
      if (!flags.files || flags.files === "true") fail("add-images requires --files a.png,b.png");
      const paths = String(flags.files).split(",").map((value) => value.trim()).filter(Boolean);
      const images = paths.map((p) => {
        const filePath = resolve(p);
        return { name: basename(filePath), bytes: readFileSync(filePath) };
      });
      return print(addImages(repoRoot, id, { images }));
    }
    case "add-image-from-file": {
      // Mint a normal journaled image element from an EXISTING project file src — no
      // re-upload, no duplicate bytes (backs the inspector's per-plate "Add to canvas"
      // button, T0238). --src is a project-relative ref like "files/<hash>.png".
      if (!id) fail("add-image-from-file requires <id>");
      if (!flags.src || flags.src === "true") fail("add-image-from-file requires --src <files/hash.png>");
      const args = { src: flags.src };
      if (flags.name && flags.name !== "true") args.name = flags.name;
      if (flags.x !== undefined) args.x = Number(flags.x);
      if (flags.y !== undefined) args.y = Number(flags.y);
      return print(addImageFromFile(repoRoot, id, args));
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
    case "nodes-align": {
      // Align 2+ nodes (elements AND/OR groups), or exactly 1 node inside a parent group,
      // to a shared reference frame in ONE journaled gesture. --reference forces auto
      // (default)/selection/parent; auto already covers both the 2+ union-bbox case and the
      // "1 node inside a group" parent-frame case, so it rarely needs to be set explicitly.
      if (!id) fail("nodes-align requires <id>");
      if (!flags.nodes || flags.nodes === "true") fail("nodes-align requires --nodes n1,n2");
      if (!flags.align || flags.align === "true") fail("nodes-align requires --align left|hcenter|right|top|vcenter|bottom");
      const nodeIds = String(flags.nodes).split(",").map((value) => value.trim()).filter(Boolean);
      const args = { projectId: id, nodeIds, align: flags.align };
      if (flags.reference && flags.reference !== "true") args.reference = flags.reference;
      return print(alignNodes(repoRoot, args));
    }
    case "nodes-distribute": {
      // Distribute 3+ nodes (elements AND/OR groups) with equal gaps along an axis in ONE
      // journaled gesture; the two extreme (by sorted position) nodes stay put.
      if (!id) fail("nodes-distribute requires <id>");
      if (!flags.nodes || flags.nodes === "true") fail("nodes-distribute requires --nodes n1,n2,n3");
      if (!flags.axis || flags.axis === "true") fail("nodes-distribute requires --axis h|v");
      const nodeIds = String(flags.nodes).split(",").map((value) => value.trim()).filter(Boolean);
      return print(distributeNodes(repoRoot, { projectId: id, nodeIds, axis: flags.axis }));
    }
    case "nodes-paste": {
      // Instantiate a copied node spec (one journal entry; new ids). --spec is a JSON file
      // in the tree.buildNodesSpec shape ({schema, nodes:[...]}). --group <gid> pastes into
      // that group; --group none (or omitted) = root. --dx/--dy shift the paste (default 0).
      if (!id) fail("nodes-paste requires <id>");
      if (!flags.spec || flags.spec === "true") fail("nodes-paste requires --spec <path>");
      const spec = JSON.parse(readFileSync(resolve(flags.spec), "utf8"));
      const args = { projectId: id, spec };
      if (flags.dx !== undefined && flags.dx !== "true") args.dx = Number(flags.dx);
      if (flags.dy !== undefined && flags.dy !== "true") args.dy = Number(flags.dy);
      if (flags.group !== undefined && flags.group !== "true") args.scopeId = flags.group === "none" ? null : flags.group;
      return print(pasteNodes(repoRoot, args));
    }
    case "nodes-duplicate": {
      // Duplicate live nodes in place +offset (one journal entry; new ids). --nodes is a
      // comma list of element/group ids. Default offset +16,+16; default destination = the
      // originals' common scope. --group <gid>|none overrides the destination scope.
      if (!id) fail("nodes-duplicate requires <id>");
      if (!flags.nodes || flags.nodes === "true") fail("nodes-duplicate requires --nodes id1,id2");
      const nodeIds = String(flags.nodes).split(",").map((value) => value.trim()).filter(Boolean);
      const args = { projectId: id, nodeIds };
      if (flags.dx !== undefined && flags.dx !== "true") args.dx = Number(flags.dx);
      if (flags.dy !== undefined && flags.dy !== "true") args.dy = Number(flags.dy);
      if (flags.group !== undefined && flags.group !== "true") args.scopeId = flags.group === "none" ? null : flags.group;
      return print(duplicateNodes(repoRoot, args));
    }
    case "nodes-delete": {
      // Batched mixed element+group subtree delete (one journal entry; one undo restores all).
      if (!id) fail("nodes-delete requires <id>");
      if (!flags.nodes || flags.nodes === "true") fail("nodes-delete requires --nodes id1,id2");
      const nodeIds = String(flags.nodes).split(",").map((value) => value.trim()).filter(Boolean);
      return print(deleteNodes(repoRoot, { projectId: id, nodeIds }));
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
    case "alpha": {
      // Alpha-cutout the element's current pixels (whole element, or only --regions r1,r2)
      // via the image-tools matte pipeline; swaps the element to a new alpha PNG (one undo).
      // --method auto (route; refuses a dual-plate soft zone) or matte (force key_matte).
      // --elements e1,e2 batches 2+ images into ONE journal entry/undo (regions are not
      // allowed with a batch — regions stay single-element, use --element).
      if (!id) fail("alpha requires <id>");
      const hasElements = flags.elements && flags.elements !== "true";
      const hasElement = flags.element && flags.element !== "true";
      if (!hasElement && !hasElements) fail("alpha requires --element <eid> or --elements e1,e2");
      if (hasElement && hasElements) fail("alpha accepts --element or --elements, not both");
      const args = { projectId: id };
      if (flags.method && flags.method !== "true") args.method = flags.method;
      if (hasElements) {
        if (flags.regions) fail("--regions is not allowed with --elements (regions stay single-element, use --element)");
        args.elementIds = String(flags.elements).split(",").map((value) => value.trim()).filter(Boolean);
      } else {
        args.elementId = flags.element;
        if (flags.regions && flags.regions !== "true") {
          args.regions = String(flags.regions).split(",").map((value) => value.trim()).filter(Boolean);
        }
      }
      return print(await alphaCutout(repoRoot, args));
    }
    case "alpha-dual": {
      // Dual-plate alpha cutout (T0237): the SAME art on a white plate + a black plate
      // (either order — the tool auto-detects roles by overall brightness) -> ONE new
      // cut element in ONE undo step. Both plate elements stay untouched (non-destructive).
      if (!id) fail("alpha-dual requires <id>");
      if (!flags.elements || flags.elements === "true") fail("alpha-dual requires --elements a,b");
      const elementIds = String(flags.elements).split(",").map((value) => value.trim()).filter(Boolean);
      return print(await alphaDualPlate(repoRoot, { projectId: id, elementIds }));
    }
    case "alpha-dual-generate": {
      // AUTOMATIC dual-plate alpha (T0238): the element's CURRENT pixels are the LIGHT
      // plate (loudly refused unless the border is flat + light); generates the DARK plate
      // as a codex edit of it, gates the pair (one automatic retry), keys -> ONE new
      // element beside the source. --prompt is an optional extra subject description
      // appended to the subject-lock prompt.
      if (!id) fail("alpha-dual-generate requires <id>");
      if (!flags.element || flags.element === "true") fail("alpha-dual-generate requires --element <eid>");
      const args = { projectId: id, elementId: flags.element };
      if (flags.prompt && flags.prompt !== "true") args.prompt = flags.prompt;
      return print(await alphaDualPlateGenerate(repoRoot, args));
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
        if (!rows) fail("export-set requires --json <path> or --scale <t> [--format --quality --resample --base]");
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
      // --zip writes ONE STORE-mode .zip of the run's images (the same archive the page's
      // multi-output save-dialog builds) to an explicit path — tool parity, optional.
      if (flags.zip && flags.zip !== "true") {
        const zipPath = resolve(flags.zip);
        const { bytes, files } = zipExport(repoRoot, { projectId: id, stamp: result.stamp });
        writeFileSync(zipPath, bytes);
        result = { ...result, zip: zipPath, zipped: files };
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
    case "groups-set": {
      // Batched shared-toggle set across several groups (one journal entry; one undo) —
      // the CLI parity for the multi-group inspector. Only Visible / Clip are shared.
      if (!id) fail("groups-set requires <id>");
      if (!flags.groups || flags.groups === "true") fail("groups-set requires --groups g1,g2");
      const groupIds = String(flags.groups).split(",").map((value) => value.trim()).filter(Boolean);
      const args = { projectId: id, groupIds };
      if (flags.visible !== undefined) {
        if (flags.visible !== "true" && flags.visible !== "false") fail("groups-set --visible must be true or false");
        args.visible = flags.visible === "true";
      }
      if (flags.clip !== undefined) {
        if (flags.clip !== "true" && flags.clip !== "false") fail("groups-set --clip must be true or false");
        args.clip = flags.clip === "true";
      }
      if (args.visible === undefined && args.clip === undefined) fail("groups-set requires --visible and/or --clip");
      return print(patchGroups(repoRoot, args));
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
    case "undo": {
      if (!id) fail("undo requires <id>");
      // T0234: the CLI is the agent transport — undo must prove it read the CURRENT
      // head before applying (the project may be live; see history-jump below).
      if (flags["expect-head"] === undefined || flags["expect-head"] === "true") {
        fail("undo requires --expect-head <n> — the project may be live; run history-list to read the current head, then retry with it");
      }
      return print(undoOp(repoRoot, { projectId: id, expectHead: Number(flags["expect-head"]) }));
    }
    case "redo": {
      if (!id) fail("redo requires <id>");
      if (flags["expect-head"] === undefined || flags["expect-head"] === "true") {
        fail("redo requires --expect-head <n> — the project may be live; run history-list to read the current head, then retry with it");
      }
      return print(redoOp(repoRoot, { projectId: id, expectHead: Number(flags["expect-head"]) }));
    }
    case "history":
      if (!id) fail("history requires <id>");
      return print(readHistory(repoRoot, { projectId: id }));
    case "history-list": {
      // Labeled linear spine (Base + undo chain + redo tail) — the exact rows the page's
      // history panel renders; the parity list for undo/redo/history-jump's --expect-head.
      if (!id) fail("history-list requires <id>");
      const list = listHistory(repoRoot, { projectId: id });
      // T0234: print the current head prominently as its own line BEFORE the JSON, so
      // an agent reading the output can't miss it — the value to pass as --expect-head.
      console.log(`head: ${list.head}`);
      return print(list);
    }
    case "history-jump": {
      // Jump the applied head to a spine seq (0 = base) — one call, behaves like N
      // undos/redos, undoable. Loud on an unknown/out-of-range seq.
      if (!id) fail("history-jump requires <id>");
      if (flags.seq === undefined || flags.seq === "true") fail("history-jump requires --seq <n>");
      // T0234: same live-project guard as undo/redo — prove the caller read the
      // CURRENT head (history-list) before the jump is allowed to apply.
      if (flags["expect-head"] === undefined || flags["expect-head"] === "true") {
        fail("history-jump requires --expect-head <n> — the project may be live; run history-list to read the current head, then retry with it");
      }
      return print(jumpHistory(repoRoot, { projectId: id, seq: Number(flags.seq), expectHead: Number(flags["expect-head"]) }));
    }
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
  // The CLI is the AGENT transport: every mutation it commits is attributed as
  // agent-made so history marks it (🤖, T0228). Set ONLY when actually running as
  // the CLI — importing this module (tests do) must not flip the process actor.
  setOpsActor("agent");
  main(process.argv.slice(2)).catch((error) => fail(error && error.message ? error.message : String(error)));
}
