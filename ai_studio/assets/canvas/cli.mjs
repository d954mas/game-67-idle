#!/usr/bin/env node
// Canvas agent client. Same ops layer as the browser page (ops.mjs) — no
// separate logic. Prints compact JSON so agents can pipe results.
//
// usage:
//   node ai_studio/assets/canvas/cli.mjs list [--full]   (summary by default: id/title/created/updated/counts/head; --full = every project in full)
//   node ai_studio/assets/canvas/cli.mjs create [--title "My canvas"]
//   node ai_studio/assets/canvas/cli.mjs show <id>
//   node ai_studio/assets/canvas/cli.mjs rename <id> --title "New title"
//   node ai_studio/assets/canvas/cli.mjs delete <id>
//   node ai_studio/assets/canvas/cli.mjs add-image <id> --file path.png
//   node ai_studio/assets/canvas/cli.mjs add-images <id> --files a.png,b.png   (batched; one undo)
//   node ai_studio/assets/canvas/cli.mjs add-text <id> [--x n --y n] [--content "..."] [--style-json path] [--group gid]
//   node ai_studio/assets/canvas/cli.mjs add-note <id> [--x n --y n] [--w n --h n] [--content "..."] [--style-json path] [--background '#rrggbb'|none] [--group gid]   (T0268: sticky-note annotation; fixed clipped box; excluded from renders)
//   node ai_studio/assets/canvas/cli.mjs detect-regions <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs move <id> --element <eid> --x 10 --y 20
//   node ai_studio/assets/canvas/cli.mjs element-set <id> --element <eid> [--w n --h n] [--x n --y n] [--rotation <deg>] [--flip-h true|false] [--flip-v true|false] [--opacity <0..1>] [--filters-json '<json|null>']   (T0232 3a: rotation = degrees CW about the box center, normalized to [0,360); flip is image-only; T0260: opacity in [0,1], stored only when != 1; T0273: filters-json = {brightness?,saturation?,contrast?,tint?:{color,strength}} non-destructive image color adjustments, image-only, whole-object replace, "null" clears — see README "Image filters"; --w/--h/--x/--y are the same finite-number patchElement fields the API PATCH route accepts)
//   node ai_studio/assets/canvas/cli.mjs asset-status-show <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs asset-status-set <id> --element <eid> --status quarantine|checked|accepted
//   node ai_studio/assets/canvas/cli.mjs asset-status-check <id> --element <eid>   (trusted technical gate; PASS -> checked, FAIL -> quarantine)
//   node ai_studio/assets/canvas/cli.mjs asset-style-check <id> --element <eid>   (trusted advisory vision verdict; status stays checked/accepted)
//   node ai_studio/assets/canvas/cli.mjs regions-set <id> --element <eid> --json path.json
//   node ai_studio/assets/canvas/cli.mjs regions-show <id> --element <eid>
//   node ai_studio/assets/canvas/cli.mjs slice9-set <id> --element <eid> [--left n --top n --right n --bottom n] [--scale n] | --clear   (T0233: 9-slice insets, SOURCE pixels; missing flags merge over the element's current slice9 (or 0s); --scale multiplies the DESTINATION corner/edge band only, default 1; --clear sends null)
//   node ai_studio/assets/canvas/cli.mjs animation-set <id> --element <eid> --json spec.json | --clear   (T0260: set the ai_studio.canvas.animation.v1 spec {channels:[...]} from a file, or --clear; image + text)
//   node ai_studio/assets/canvas/cli.mjs element-reorder <id> --element <eid> --index <n>
//   node ai_studio/assets/canvas/cli.mjs node-reorder <id> --node <id> --index <n>
//   node ai_studio/assets/canvas/cli.mjs nodes-move <id> --json moves.json
//   node ai_studio/assets/canvas/cli.mjs nodes-reorder <id> --nodes n1,n2 --direction front|back|forward|backward | --index <n>
//   node ai_studio/assets/canvas/cli.mjs nodes-align <id> --nodes n1,n2 --align left|hcenter|right|top|vcenter|bottom|center [--reference auto|selection|parent]
//   node ai_studio/assets/canvas/cli.mjs nodes-distribute <id> --nodes n1,n2,n3 --axis h|v
//   node ai_studio/assets/canvas/cli.mjs nodes-paste <id> --spec spec.json [--dx n --dy n] [--group <gid>|none]
//   node ai_studio/assets/canvas/cli.mjs nodes-duplicate <id> --nodes id1,id2 [--dx n --dy n] [--group <gid>|none]
//   node ai_studio/assets/canvas/cli.mjs nodes-delete <id> --nodes id1,id2
//   node ai_studio/assets/canvas/cli.mjs slice <id> --element <eid> [--regions r1,r2]
//   node ai_studio/assets/canvas/cli.mjs alpha <id> --element <eid> [--method auto|matte|corridorkey|vitmatte|birefnet] [--regions r1,r2]   (cutout -> NEW element beside the source; original untouched; one undo)
//   node ai_studio/assets/canvas/cli.mjs alpha <id> --elements e1,e2 [--method auto|matte|corridorkey|vitmatte|birefnet]   (batch; N new copies; one undo)
//   node ai_studio/assets/canvas/cli.mjs alpha-dual <id> --elements a,b   (white+black plate pair -> new element; one undo)
//   node ai_studio/assets/canvas/cli.mjs alpha-dual-generate <id> --element <el> [--prompt "..."] [--no-lock]   (AUTOMATIC: element = light plate, generates the dark plate, gates, cuts; one undo)
//   node ai_studio/assets/canvas/cli.mjs quantize <id> --element <eid> --colors N [--dither] [--preview <out.png>]   (T0207: with --preview, writes the preview PNG + prints the report, NO journal entry; without it, applies — one undo step)
//   node ai_studio/assets/canvas/cli.mjs denoise <id> --element <eid> --strength 1|2|3 [--preview <out.png>]   (T0207: same preview/apply split as quantize)
//   node ai_studio/assets/canvas/cli.mjs filters-bake <id> --element <eid>   (T0274 "Apply": rasterize the element's CURRENT filters+opacity into a new source file, then clear both; one undo)
//   node ai_studio/assets/canvas/cli.mjs filters-bake <id> --elements e1,e2   (batch; one undo)
//   node ai_studio/assets/canvas/cli.mjs add-image-from-file <id> --src files/<hash>.png [--name X] [--x n --y n]   (mint an element from an EXISTING project file, no re-upload)
//   node ai_studio/assets/canvas/cli.mjs export-set <id> --element <eid> --json rows.json | --scale 2x [--format --quality --resample --base]
//   node ai_studio/assets/canvas/cli.mjs export <id> --elements e1,e2 | --all | --project [--scale --format --quality --resample --base] [--to <dir>] [--zip <path>]   (--project: T0332 B1 — exports only top-level groups with the explicit screen:true flag; group-set --screen sets it)
//   node ai_studio/assets/canvas/cli.mjs group-create <id> --name X [--elements e1,e2 | --x --y --w --h] [--parent <gid>|none]
//   node ai_studio/assets/canvas/cli.mjs group-reparent <id> --group g --parent <gid>|none [--index n]
//   node ai_studio/assets/canvas/cli.mjs group-move <id> --group g --x --y
//   node ai_studio/assets/canvas/cli.mjs group-set <id> --group g [--name] [--visible true|false] [--w --h] [--background '#rrggbb'|none] [--clip true|false] [--screen true|false]   (T0332 B1: --screen is the export opt-in flag — ONLY a screen:true top-level group is composited by `export --project`/counted by the page's Export button; absent by default, even on a freshly created group)
//   node ai_studio/assets/canvas/cli.mjs groups-set <id> --groups g1,g2 [--visible true|false] [--clip true|false]   (batched shared toggles; one undo)
//   node ai_studio/assets/canvas/cli.mjs group-fit <id> --group g [--padding n]
//   node ai_studio/assets/canvas/cli.mjs group-scale <id> --group g --x n --y n --w n --h n   (T0271: scale the group's full subtree -- frame + every descendant + text fontSize -- to a new frame; one undo step)
//   node ai_studio/assets/canvas/cli.mjs group-assign <id> --elements e1,e2 --group g|none
//   node ai_studio/assets/canvas/cli.mjs group-ungroup <id> --group g
//   node ai_studio/assets/canvas/cli.mjs group-delete <id> --group g
//   node ai_studio/assets/canvas/cli.mjs recipe-create <id> [--name X] [--x n --y n --w n --h n] [--parent <gid>|none]   (T0239 increment 1: mint a recipe card — a group with an additive `recipe` blob; no generation yet)
//   node ai_studio/assets/canvas/cli.mjs recipe-set <id> --group g [--prompt "..."] [--engine codex|gemini|both] [--style <id>|none] [--axes-json path] [--vary axisName] [--grid RxC] [--max-jobs n] [--pack none] [--bg-key '#rrggbb'] [--n-candidates n] [--size s] [--quality q]   (--style is a style-card group id from style-create; none clears it; T0332 v2: --axes-json/--vary/--grid/--max-jobs/--pack none patch recipe.pack — pack mode, full-replace, merged with the CURRENT pack in the CLI before sending; --bg-key/--n-candidates/--size/--quality patch recipe.params — partial, merges onto the existing params)
//   node ai_studio/assets/canvas/cli.mjs recipe-generate <id> --group g [--no-lock]   (game-owned production requires an accepted style lock; --no-lock creates tainted explore output)
//   node ai_studio/assets/canvas/cli.mjs recipe-expand <id> --group g   (T0239 increment 4: Expand-prompt — ONE codex TEXT call, real seconds/minutes; writes recipe.expanded only, no card minted; Generate sends it when the card's use_expanded is true)
//   node ai_studio/assets/canvas/cli.mjs recipe-pack-preview <id> --group g   (T0332 v2: EPHEMERAL preview — assembles a config from recipe.pack + recipe.prompt/style_ref/params and runs the REAL expand_jobs.py expander; NOT journaled, writes nothing to the blob; prints sheet count + per-sheet prompts)
//   node ai_studio/assets/canvas/cli.mjs recipe-pack-generate <id> --group g [--run <groupId>] [--sheet <slug>] [--no-lock]   (pack generation shares recipe generation's production lock/origin contract)
//   node ai_studio/assets/canvas/cli.mjs recipe-pack-slice <id> --group g [--run <groupId>]   (T0332 B3: slice every sheet of a pack run — detectRegions -> hard gate region_count===cells.length -> sliceRegions, reparented into the run group; --run selects an explicit run group (must carry pack_run for this card), omitted resolves recipe.last_run.run_group_id; real region-detector/crop_regions.py spawns, no fake seam; never throws mid-sheet — prints one name/verdict/got-expected line per sheet + the final {contract:[{sheet_element_id,verdict,region_count,cells_len,cut_ids}]} JSON)
//   node ai_studio/assets/canvas/cli.mjs style-create <id> [--name X] [--x n --y n --w n --h n] [--parent <gid>|none]   (T0239 increment 3: mint a style card — a group with an additive `style` blob: prompt + ONE ref image; no generation, style cards never generate)
//   node ai_studio/assets/canvas/cli.mjs style-set <id> --group g [--prompt "..."] [--ref <elementId>|none]   (partial style blob update; --ref must be a member IMAGE element id of THIS card, or none to clear — the "Make ref" gesture)
//   node ai_studio/assets/canvas/cli.mjs extract <id> --element el   (T0239 increment 4: ONE codex VISION call, real seconds/minutes -> element.meta.extracted {prompt_full, prompt_subject, style, description}; no card minted; re-running overwrites)
//   node ai_studio/assets/canvas/cli.mjs animate <id> --element el --text "..."   (T0264: ONE codex TEXT/VISION call, real seconds/minutes -> element.animation (the ai_studio.canvas.animation.v1 spec); authors fresh or minimally patches an existing spec; image + text)
//   node ai_studio/assets/canvas/cli.mjs promote-recipe <id> --element el   (mint a RECIPE card BELOW the element from its ALREADY-STORED meta.extracted; NO codex call; run extract first)
//   node ai_studio/assets/canvas/cli.mjs promote-style <id> --element el   (mint a STYLE card RIGHT of the element from its ALREADY-STORED meta.extracted; NO codex call; run extract first)
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
  addNote,
  addText,
  alignNodes,
  alphaCutout,
  alphaDualPlate,
  alphaDualPlateGenerate,
  animateElementFromText,
  assignToGroup,
  bakeFilters,
  cleanupApply,
  cleanupPreview,
  createAnimCard,
  createGroup,
  createProject,
  createRecipeCard,
  createStyleCard,
  deleteGroup,
  deleteNodes,
  deleteProject,
  detectRegions,
  distributeNodes,
  duplicateNodes,
  expandRecipePrompt,
  exportElements,
  exportProject,
  extractFromElement,
  fitGroup,
  generateAnimFromCard,
  generateFromRecipe,
  getAssetStatus,
  getProject,
  jumpHistory,
  listHistory,
  listProjects,
  moveNodes,
  opsStats,
  packPreview,
  packSlice,
  pasteNodes,
  patchAnim,
  patchElement,
  patchElements,
  patchGroup,
  patchGroups,
  patchProject,
  patchRecipe,
  patchStyle,
  promoteExtractedRecipe,
  promoteExtractedStyle,
  readHistory,
  recordOpFailure,
  redoOp,
  removeElement,
  removeElements,
  renderGroup,
  runAssetTechnicalGate,
  runAssetStyleVerdict,
  reorderElement,
  reorderNode,
  reorderNodes,
  reparentGroup,
  scaleGroup,
  setElementAnimation,
  setAssetStatus,
  setExportSettings,
  setOpsActor,
  setRegions,
  setSlice9,
  sliceRegions,
  undoOp,
  ungroupGroup,
  withProjectLock,
  zipExport,
} from "./ops.mjs";
import {
  assertBareCanvasProjectIdIsUnambiguous,
  assertCanvasExportDestination,
  canvasStoreArgs,
  canvasStoresForQuery,
  canvasStoreSummary,
  decorateCanvasProject,
  selectCanvasStore,
  withCanvasStore,
} from "./stores.mjs";

const DEFAULT_REPO_ROOT = resolve(process.env.AI_STUDIO_ROOT || fileURLToPath(new URL("../../..", import.meta.url)));

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

function defaultPrint(value) {
  process.stdout.write(`${JSON.stringify(value)}\n`);
  return value;
}

function selectedCanvasStore(repoRoot, flags) {
  return selectCanvasStore(repoRoot, canvasStoreArgs(flags));
}

function withSelectedCanvasStore(repoRoot, flags, fn) {
  return withCanvasStore(selectedCanvasStore(repoRoot, flags), fn);
}

// Strict boolean flag parser (T0254 Tier 1 #4). Accepts true/false (case-insensitive)
// or 1/0; anything else is a loud error naming the flag. Replaces the previous silent,
// DIRECTION-DEPENDENT coercion: junk resolved to *false* in element-set (`=== "true"`)
// but *true* in group-set (raw string forwarded to the op's own lenient check) — the
// same flag, two opposite wrong answers, no error either way. Every boolean flag site
// in this file routes through this one helper now, so junk is always a loud refusal,
// never a guess. Call only when the flag IS present (`flags[x] !== undefined`); a
// missing flag is the caller's own default-handling, not this function's concern.
function parseBool(flag, value) {
  if (typeof value === "boolean") return value;
  const normalized = String(value).trim().toLowerCase();
  if (normalized === "true" || normalized === "1") return true;
  if (normalized === "false" || normalized === "0") return false;
  fail(`--${flag} must be true, false, 1, or 0 (got ${JSON.stringify(value)})`);
  return undefined; // unreachable: fail() exits the process
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
  console.log(`usage: cli.mjs <list|create|show|rename|project-set|delete|add-image|add-images|add-image-from-file|add-text|add-note|detect-regions|move|element-set|asset-status-show|asset-status-set|asset-status-check|asset-style-check|element-remove|elements-set|elements-remove|element-reorder|node-reorder|nodes-move|nodes-reorder|nodes-align|nodes-distribute|nodes-paste|nodes-duplicate|nodes-delete|regions-set|regions-show|slice9-set|animation-set|slice|alpha|alpha-dual|alpha-dual-generate|quantize|denoise|filters-bake|export-set|export|group-create|group-reparent|group-move|group-set|groups-set|group-fit|group-scale|group-assign|group-ungroup|group-delete|recipe-create|recipe-set|recipe-generate|recipe-expand|recipe-pack-preview|recipe-pack-generate|recipe-pack-slice|style-create|style-set|extract|promote-recipe|promote-style|render-group|undo|redo|history|history-list|history-jump>
  list [--full] [--owner-game <gameId>] [--include-archived]   (summary by default: [{id,title,ownerGame,created,updated,elements,groups,head}]; --include-archived adds archived; --full = every project in full, today's original dump)
  create [--title <title>] [--owner-game <gameId>]     (omit --title for a random default)
  show <id>
  rename <id> --title <title>
  project-set <id> [--title <title>] [--owner-game <gameId|none>] [--archived true|false]
  delete <id>
  add-image <id> --file <path>
  add-images <id> --files a.png,b.png   (batched multi-image add; one undo step)
  add-image-from-file <id> --src <files/hash.png> [--name <name>] [--x <n> --y <n>]   (mint an element from an EXISTING project file; no re-upload, no duplicate bytes)
  add-text <id> [--x <n> --y <n>] [--content "<text>"] [--style-json <path>] [--group <gid>]
  add-note <id> [--x <n> --y <n>] [--w <n> --h <n>] [--content "<text>"] [--style-json <path>] [--background '#rrggbb'|none] [--group <gid>]   (T0268: sticky-note annotation — plain text, fixed box + browser wrap/clip, background fill; excluded from renderGroup/exportProject)
  detect-regions <id> --element <eid>
  move <id> --element <eid> --x <n> --y <n>
  element-set <id> --element <eid> [--name <name>] [--visible true|false] [--content "<text>"] [--style-json <path>] [--background '#rrggbb'|none] [--w <n> --h <n>] [--x <n> --y <n>] [--rotation <deg>] [--flip-h true|false] [--flip-v true|false] [--opacity <0..1>] [--filters-json '<json|null>']   (--content/--style-json patch a text OR note; --background is note-only)   (--w/--h/--x/--y are the same finite-number fields the API PATCH route + move accept — single-element resize/reposition from the CLI, T0254; --opacity in [0,1] stored only when != 1, T0260; --filters-json = {brightness?,saturation?,contrast?,tint?:{color,strength}} non-destructive image color adjustments, image-only, whole-object replace, "null" clears, T0273 — see README "Image filters")
  asset-status-show <id> --element <eid>
  asset-status-set <id> --element <eid> --status quarantine|checked|accepted   (image-only; one undo step; initializes quarantine or downgrades; upward transitions require gate evidence)
  asset-status-check <id> --element <eid>   (runs the accepted style lock's deterministic technical gate; PASS -> checked, FAIL -> quarantine; report + problem thumbnail stored as evidence)
  asset-style-check <id> --element <eid>   (vision-compares current technically-checked art with lock exemplars + Do/Don't; stores accept|revise|reject advisory evidence without minting accepted)
  element-remove <id> --element <eid>
  elements-set <id> --json <path>   (batched patch: [{elementId,x?,y?,w?,h?,name?,visible?,rotation?,flipH?,flipV?,opacity?,filters?}] or {patches:[...]}; one undo step)
  elements-remove <id> --elements e1,e2   (batched delete; one undo step)
  element-reorder <id> --element <eid> --index <n>   (z-order among siblings; 0 = back)
  node-reorder <id> --node <id> --index <n>   (z-order of an element OR group among merged siblings; 0 = back)
  nodes-move <id> --json <path>   (batched mixed element+group move: [{nodeId,x,y}] or {moves:[...]}; one undo step)
  nodes-reorder <id> --nodes n1,n2 --direction front|back|forward|backward | --index <n>   (multi-node z-order block; one undo step)
  nodes-align <id> --nodes n1,n2 --align left|hcenter|right|top|vcenter|bottom|center [--reference auto|selection|parent]   (2+ nodes -> selection bbox; 1 node in a group -> the group frame; center = both axes at once; one undo step)
  nodes-distribute <id> --nodes n1,n2,n3 --axis h|v   (equal-gap distribute; 3+ nodes; endpoints fixed; one undo step)
  nodes-paste <id> --spec <path> [--dx <n> --dy <n>] [--group <gid>|none]   (instantiate a copied node spec; new ids; one undo step)
  nodes-duplicate <id> --nodes id1,id2 [--dx <n> --dy <n>] [--group <gid>|none]   (duplicate live nodes in place +offset; one undo step)
  nodes-delete <id> --nodes id1,id2   (batched mixed element+group subtree delete; one undo step)
  regions-set <id> --element <eid> --json <path>   (JSON: a regions array or {regions:[...]})
  regions-show <id> --element <eid>
  slice9-set <id> --element <eid> [--left <n> --top <n> --right <n> --bottom <n>] [--scale <n>] | --clear   (T0233: 9-slice insets in SOURCE pixels; corners stay fixed size, edges stretch one axis, center stretches both, when the element is resized; image-only; omitted flags merge over the element's current slice9, or 0 if unset; --scale multiplies the DESTINATION corner/edge band only (default 1, source pixels never move); --clear sends insets:null)
  animation-set <id> --element <eid> --json <path> | --clear   (T0260: set an element's procedural animation — the ai_studio.canvas.animation.v1 spec {channels:[...]} (a spec object or a bare channels array) from a file; --clear sends animation:null; image + text; validated loudly at set time)
  slice <id> --element <eid> [--regions r1,r2]
  alpha <id> --element <eid> [--method auto|matte|corridorkey|vitmatte|birefnet] [--regions r1,r2]   (alpha-cutout -> a NEW element beside the source (original untouched, for side-by-side A/B); auto routes, matte forces key_matte, corridorkey = neural green-screen matte for soft glow (green native, magenta via hue180 shim; regions composite the whole-frame CK result into the requested regions; ~15s GPU); vitmatte = neural THIN detail (spider-web/mesh/fur/hair) on a green/magenta key + 2nd-choice glow, its OWN GPU venv, ~1-3s, whole-element only; birefnet = SOD cutout for ANY/unknown background, NO key, shared repo venv CPU ~10-30s, whole-element only; one undo)
  alpha <id> --elements e1,e2 [--method auto|matte|corridorkey|vitmatte|birefnet]   (batch: 2+ images each cut into a NEW copy beside itself, in ONE journal entry/undo; sources untouched; no --regions with a batch; corridorkey/vitmatte/birefnet each pay their per-image cost — the batch runs sequentially)
  alpha-dual <id> --elements a,b   (white-plate + black-plate pair -> ONE new cut element; either order; plates untouched; one undo step)
  alpha-dual-generate <id> --element <eid> [--prompt "<extra subject description>"] [--no-lock]   (AUTOMATIC: the element's current pixels are the LIGHT plate; generates the DARK plate via codex edit, gates it (one automatic retry), cuts -> ONE new element beside the source; source untouched; one undo step)
  quantize <id> --element <eid> --colors <2-256> [--dither] [--preview <out.png>]   (T0207: palette-quantize the element's CURRENT pixels, alpha untouched; --preview writes the result PNG to <out.png> + prints the report, NO journal entry; without --preview, commits a new file + src swap as one undo step)
  denoise <id> --element <eid> --strength <1|2|3> [--preview <out.png>]   (T0207: light median denoise, alpha NEVER filtered; same --preview/apply split as quantize)
  filters-bake <id> --element <eid>   (T0274 "Apply": rasterize the element's CURRENT filters+opacity — T0273/T0260 — into a NEW content-addressed source file, then clear both (sliders reset); loud when there is nothing to bake; one undo step)
  filters-bake <id> --elements e1,e2   (batch: 2+ images baked into ONE journal entry/undo; atomic — any refusal rejects the whole batch)
  export-set <id> --element <eid> --json <path> | --scale <t> [--format png|jpg|webp] [--quality 1-100] [--resample lanczos|nearest] [--base source|canvas]
  export <id> --elements e1,e2 | --all | --project [--scale <t> --format <f> --quality <n> --resample <r> --base source|canvas] [--to <dir>] [--zip <path>]   (--project: T0332 B1 — composites only top-level groups with screen:true; see group-set --screen)
  group-create <id> --name <name> [--elements e1,e2 | --x <n> --y <n> --w <n> --h <n>] [--parent <gid>|none]
  group-reparent <id> --group <gid> --parent <gid>|none [--index <n>]   (nest a group; none = top level)
  group-move <id> --group <gid> --x <n> --y <n>
  group-set <id> --group <gid> [--name <name>] [--visible true|false] [--w <n> --h <n>] [--background '#rrggbb'|none] [--clip true|false] [--screen true|false]   (T0332 B1: --screen is the export opt-in flag, absent by default — see the "export" command's --project)
  groups-set <id> --groups g1,g2 [--visible true|false] [--clip true|false]   (batched shared toggles; one undo step)
  group-fit <id> --group <gid> [--padding <n>]   (resize the frame to fit its content; padding default 24)
  group-scale <id> --group <gid> --x <n> --y <n> --w <n> --h <n>   (T0271: scale the group's FULL subtree — the frame AND every descendant element/nested-group box, text fontSize scaled too — to the given frame; server computes every descendant patch, so page and CLI can't disagree; one undo step. Distinct from group-set's frame-only --w/--h, which pins children in place.)
  group-assign <id> --elements e1,e2 --group <gid>|none
  group-ungroup <id> --group <gid>   (dissolve one level; children keep the group's z-slot; one undo step)
  group-delete <id> --group <gid>
  recipe-create <id> [--name <name>] [--x <n> --y <n> --w <n> --h <n>] [--parent <gid>|none]   (T0239 increment 1: mint a recipe card — a group with an additive 'recipe' blob; omitted w/h default to a 360x280 frame; no generation yet)
  recipe-set <id> --group <gid> [--prompt "<text>"] [--engine codex|gemini|both] [--style <id>|none] [--axes-json <path>] [--vary <axisName>] [--grid <RxC>] [--max-jobs <n>] [--pack none] [--bg-key '#rrggbb'] [--n-candidates <n>] [--size <s>] [--quality <q>]   (partial recipe blob update; --style is an existing style-card group id from style-create, or none to clear; T0332 v2 pack mode — --axes-json reads {axisName:[values...]} from a file; --axes-json/--vary/--grid/--max-jobs assemble a FULL recipe.pack: the CLI reads the card's CURRENT pack (or a default template if pack mode is currently off) and merges the given flags on top before sending, since the op itself REPLACES pack wholesale, not a partial merge; --pack none clears pack mode (sets recipe.pack = null), taking priority over any axes/vary/grid/max-jobs given in the same call; --bg-key/--n-candidates/--size/--quality patch recipe.params instead — a PARTIAL patch, merged onto the existing params by the op itself)
  recipe-generate <id> --group <gid> [--no-lock]   (generate from the card; game-owned production requires an accepted style lock; --no-lock creates tainted explore output)
  recipe-expand <id> --group <gid>   (T0239 increment 4: Expand-prompt — ONE codex TEXT call, real seconds/minutes; writes recipe.expanded only, no card minted; the CLI always runs the DEFAULT codex impl, no fake-assistant injection)
  recipe-pack-preview <id> --group <gid>   (T0332 v2: EPHEMERAL preview — requires recipe.pack; builds a config from recipe.pack (axes/vary/grid/max_jobs) + recipe.prompt (VERBATIM subject_template) + recipe.style_ref (style_prefix) + recipe.params (background from bg_key, candidates from n_candidates, size/quality/model) and runs the REAL expand_jobs.py expander, the ONE expander shared with the later pack-branch of recipe-generate; NOT journaled, writes nothing to the recipe blob; engine codex|gemini|both all legal (both = every sheet on BOTH engines, 2x the paid calls); prints sheet count + style_ref_image flag + per-sheet name/prompt head, plus the full {sheets,style_ref_image,jobs} JSON)
  recipe-pack-generate <id> --group <gid> [--run <groupId>] [--sheet <slug>] [--no-lock]   (T0332 v2: the pack branch of generateFromRecipe — requires recipe.pack; re-expands FRESH (never packPreview's stale run), resolves refs (member images + style card ref image) ONCE, then generates sheets via the card's own engine SEQUENTIALLY (codex, agy, or both — both fans every job out to the two engines at 2x the calls; sheet identity everywhere is (sheet_axes, engine), so resume skips and --sheet replaces per engine), minting each finished sheet under its OWN short commit as it lands — a crash on sheet 3 never loses sheets 1-2; the FIRST minted sheet of a fresh run creates a result-group beside the card named "<style name|no-style>/<vary> <ts>" carrying a pack_run provenance marker, later sheets land in it; recipe.last_run updates after EVERY sheet (verdict partial while running/on any failure, ok once every requested sheet lands) plus one tool_runs entry at the end; --run <groupId> resumes into that group, skipping sheets whose axes already landed (gen_batch skip-if-exists parity — a killed/timed-out run is not repaid); --sheet <slug> force-regenerates exactly one sheet, by the expander's own job name, into the same group even if already present; codex/agy run for real minutes x N sheets — no --dry-run, agents should pass a generous timeout and resume via --run on a kill; prints one "<name>: ok|failed|skipped" line per sheet, then the final recipe.last_run JSON)
  recipe-pack-slice <id> --group <gid> [--run <groupId>]   (T0332 B3: slice EVERY sheet of a pack run — for each sheet element (meta.pack with a cells manifest) in the run group: detectRegions, then a HARD gate region_count === cells.length (mismatch -> REJECT that sheet, others still slice); on a match, sliceRegions mints the cuts with a MINIMAL per-cut meta ({cardId, sheet_element_id, cell, axes} — the full manifest/prompt stay on the sheet, its provenance anchor) and reparents the fresh slices-group into the run group; --run <groupId> selects an explicit run group (must carry pack_run for this card), omitted resolves recipe.last_run.run_group_id; real region-detector/crop_regions.py spawns, same pipeline as plain detect-regions/slice — no fake seam; never throws mid-sheet (a detection failure lands that one sheet as MISSING); prints one "<name>: OK|REJECT|MISSING (got/expected)" line per sheet, then the final {contract:[{sheet_element_id,verdict,region_count,cells_len,cut_ids}], run_group_id} JSON)
  style-create <id> [--name <name>] [--x <n> --y <n> --w <n> --h <n>] [--parent <gid>|none]   (T0239 increment 3: mint a style card — a group with an additive 'style' blob: prompt + ONE ref image; omitted w/h default to a 360x280 frame; style cards never generate)
  style-set <id> --group <gid> [--prompt "<text>"] [--ref <elementId>|none]   (partial style blob update; --ref must be an existing member IMAGE element id of THIS card, or none to clear — the "Make ref" gesture; the first image dropped into an empty card auto-claims the ref)
  anim-card <id> [--name <name>] [--member <eid>] [--x <n> --y <n> --w <n> --h <n>] [--parent <gid>|none]   (T0265 increment 1: mint an animation card — a group with an additive 'anim' blob; keyframes are its member IMAGE elements; omitted w/h default to a 360x280 frame. --member is the "Animate this image" promotion: fit the card around that image + move it in as the first keyframe in ONE journal entry; not combinable with --x/--y/--w/--h; no generation yet)
  anim-patch <id> --group <gid> [--motion "<text>"] [--profile draft|final] [--seed <n>|none] [--matte corridorkey|key_matte] [--gen-fps <n>|none] [--loop true|false] [--columns <n>|none] [--trim true|false] [--style <id>|none] [--accepted <eid>|none]   (partial anim blob update)
  anim-generate <id> --group <gid> [--no-lock]   (T0265 increment 1: generate via the Track B video route — 1 keyframe = plain I2V; mints ONE flipbook element beside the card frame in its PARENT scope; one undo step; ComfyUI/CorridorKey run for real minutes — no --dry-run; the CLI always runs the DEFAULT generator, no fake injection)
  extract <id> --element <eid>   (T0239 increment 4: ONE codex VISION call, real seconds/minutes -> element.meta.extracted {prompt_full, prompt_subject, style, description}; no card minted; re-running overwrites; no fake-assistant injection from the CLI)
  animate <id> --element <eid> --text "<description>"   (T0264: ONE codex TEXT/VISION call, real seconds/minutes -> element.animation (the ai_studio.canvas.animation.v1 spec); authors fresh or minimally patches an existing spec; image (vision) + text; no fake-runner injection from the CLI)
  promote-recipe <id> --element <eid>   (mint a RECIPE card BELOW the element from its ALREADY-STORED meta.extracted; NO codex call; run extract first — loud otherwise)
  promote-style <id> --element <eid>   (mint a STYLE card RIGHT of the element from its ALREADY-STORED meta.extracted; NO codex call; run extract first — loud otherwise)
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

async function runCommand(command, id, positional, flags, { repoRoot, print }) {
  switch (command) {
    case "list": {
      // T0254 Tier 1 #3: a summary by default — id/title/created/updated/counts/head,
      // not every element+region+tool_run of every project (the CLI F1 95KB-overflow
      // finding: `list` scales with total canvas content across ALL projects,
      // unbounded, and no consumer parses element data out of it — that's what `show`
      // is for). --full restores today's exact full-project dump (additive contract:
      // nothing that worked before stops working).
      const stores = canvasStoresForQuery(repoRoot, canvasStoreArgs(flags));
      const includeArchived = flags["include-archived"] === "true";
      const projects = stores.flatMap((store) =>
        withCanvasStore(store, () => listProjects(repoRoot, { includeArchived }).map((project) => ({ project, store })))
      );
      const ownerGame = flags["owner-game"] && flags["owner-game"] !== "true" ? String(flags["owner-game"]).trim() : "";
      const filteredProjects = ownerGame
        ? projects.filter(({ project }) => project.ownership?.kind === "game" && project.ownership.gameId === ownerGame)
        : projects;
      if (flags.full === "true") {
        return print({
          stores: stores.map(canvasStoreSummary),
          projects: filteredProjects.map(({ project, store }) => decorateCanvasProject(project, store)),
        });
      }
      const summary = filteredProjects.map(({ project, store }) => ({
        id: project.id,
        title: project.title,
        ownerGame: project.ownership?.kind === "game" ? project.ownership.gameId : "",
        ...(includeArchived ? { archived: project.archived === true } : {}),
        created: project.created,
        updated: project.updated,
        elements: (project.elements || []).length,
        groups: (project.groups || []).length,
        head: Number(project.history_seq) || 0,
        storeId: store.storeId,
        visibility: store.visibility,
        qualifiedId: `${store.storeId}:${project.id}`,
      }));
      return print({ stores: stores.map(canvasStoreSummary), projects: summary });
    }
    case "create":
      // --title is optional: a missing/empty title gets a random default
      // ("Amber Fox"-style) from the op layer, matching the page's instant-create.
      if (flags["owner-game"] === "true") fail("create --owner-game requires a game id");
      return withSelectedCanvasStore(repoRoot, flags, () => {
        const store = selectedCanvasStore(repoRoot, flags);
        return print({ project: decorateCanvasProject(createProject(repoRoot, { title: flags.title, gameId: flags["owner-game"] }), store) });
      });
    case "show":
      if (!id) fail("show requires <id>");
      return withSelectedCanvasStore(repoRoot, flags, () => {
        const store = selectedCanvasStore(repoRoot, flags);
        return print({ project: decorateCanvasProject(getProject(repoRoot, id), store) });
      });
    case "rename":
      if (!id) fail("rename requires <id>");
      if (!flags.title || flags.title === "true") fail("rename requires --title <title>");
      return print(patchProject(repoRoot, { projectId: id, title: flags.title }));
    case "project-set": {
      if (!id) fail("project-set requires <id>");
      const patch = { projectId: id };
      if (flags.title !== undefined && flags.title !== "true") patch.title = flags.title;
      if (flags["owner-game"] !== undefined) {
        if (flags["owner-game"] === "true") fail("project-set --owner-game requires a game id or 'none'");
        patch.gameId = flags["owner-game"];
      }
      if (flags.archived !== undefined) patch.archived = parseBool("archived", flags.archived);
      if (patch.title === undefined && !Object.hasOwn(patch, "gameId") && !Object.hasOwn(patch, "archived")) {
        fail("project-set requires --title, --owner-game, and/or --archived");
      }
      return print(patchProject(repoRoot, patch));
    }
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
    case "add-note": {
      // T0268: add a sticky-note annotation. --w/--h set the FULLY FIXED box (default
      // sticky size otherwise); --content is plain text (\n for newlines, wrapped on the
      // page); --style-json is the note font subset (validated against fonts.json);
      // --background '#rrggbb' sets a solid fill, 'none' = no fill (default = yellow preset).
      if (!id) fail("add-note requires <id>");
      const args = {};
      if (flags.x !== undefined) args.x = Number(flags.x);
      if (flags.y !== undefined) args.y = Number(flags.y);
      if (flags.w !== undefined && flags.w !== "true") args.w = Number(flags.w);
      if (flags.h !== undefined && flags.h !== "true") args.h = Number(flags.h);
      if (flags.content && flags.content !== "true") args.content = flags.content;
      if (flags["style-json"] && flags["style-json"] !== "true") {
        args.style = JSON.parse(readFileSync(resolve(flags["style-json"]), "utf8"));
      }
      // --background '#rrggbb' -> solid fill; 'none' -> no fill; omitted -> op default preset.
      if (flags.background !== undefined) {
        args.background = flags.background === "none" ? null : { type: "color", color: flags.background };
      }
      if (flags.group && flags.group !== "true" && flags.group !== "none") args.groupId = flags.group;
      // addNote(root, projectId, args) — the op validates style + background loudly.
      return print(addNote(repoRoot, id, args));
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
      if (flags.visible !== undefined) patch.visible = parseBool("visible", flags.visible);
      // Text-element edits: --content sets the string (\n for newlines); --style-json is
      // a partial or full style object shallow-merged + validated against fonts.json.
      if (flags.content !== undefined && flags.content !== "true") patch.content = flags.content;
      if (flags["style-json"] && flags["style-json"] !== "true") {
        patch.style = JSON.parse(readFileSync(resolve(flags["style-json"]), "utf8"));
      }
      // T0268: --background '#rrggbb' sets a NOTE's solid fill; 'none' clears it. The op
      // refuses it loudly on a non-note element (background is note-only), same shape as
      // group-set's --background. Content/style above apply to a text OR a note element.
      if (flags.background !== undefined) {
        patch.background = flags.background === "none" ? null : { type: "color", color: flags.background };
      }
      // T0232 increment 3a: rotation (degrees CW about the box center; the op normalizes
      // to [0,360) and throws on a non-finite value) + flip (image-only booleans; a loud
      // error on a text element) — the SAME patchElement fields the page's inspector
      // Rotation input / Flip H/Flip V buttons commit (strict tool parity).
      if (flags.rotation !== undefined && flags.rotation !== "true") patch.rotation = Number(flags.rotation);
      if (flags["flip-h"] !== undefined) patch.flipH = parseBool("flip-h", flags["flip-h"]);
      if (flags["flip-v"] !== undefined) patch.flipV = parseBool("flip-v", flags["flip-v"]);
      // T0254 Tier 1 #5: size/position — the SAME patchElement fields `move`/the API PATCH
      // route already accept (finite numbers; the op layer applies no positivity check —
      // export.test.mjs:338 relies on w:0/h:0 being acceptable at patch time, the export
      // step is where a zero canvas size is refused — so the CLI stays byte-for-byte at
      // parity with the API instead of inventing a stricter CLI-only rule).
      if (flags.w !== undefined && flags.w !== "true") patch.w = Number(flags.w);
      if (flags.h !== undefined && flags.h !== "true") patch.h = Number(flags.h);
      if (flags.x !== undefined && flags.x !== "true") patch.x = Number(flags.x);
      if (flags.y !== undefined && flags.y !== "true") patch.y = Number(flags.y);
      // T0260: --opacity in [0,1] (the op validates loudly, stores only when != 1) — the
      // same patchElement field the inspector's Opacity input / the API PATCH route accept.
      if (flags.opacity !== undefined && flags.opacity !== "true") patch.opacity = Number(flags.opacity);
      // T0273: --filters-json is an INLINE JSON value (not a file path, unlike --style-json)
      // — {brightness?,saturation?,contrast?,tint?:{color,strength}}, validated + normalized
      // loudly by the op layer (image-only, whole-object replace); the literal string "null"
      // clears filters to absent — the same patchElement field the inspector's Filters
      // section / the API PATCH route accept.
      if (flags["filters-json"] !== undefined && flags["filters-json"] !== "true") {
        patch.filters = flags["filters-json"] === "null" ? null : JSON.parse(flags["filters-json"]);
      }
      if (!Object.keys(patch).length) {
        fail("element-set requires --name, --visible, --content, --style-json, --background, --rotation, --flip-h, --flip-v, --opacity, --filters-json, --w, --h, --x, and/or --y");
      }
      return print(patchElement(repoRoot, id, flags.element, patch));
    }
    case "asset-status-show": {
      if (!id) fail("asset-status-show requires <id>");
      if (!flags.element || flags.element === "true") fail("asset-status-show requires --element <eid>");
      return print(getAssetStatus(repoRoot, { projectId: id, elementId: flags.element }));
    }
    case "asset-status-set": {
      if (!id) fail("asset-status-set requires <id>");
      if (!flags.element || flags.element === "true") fail("asset-status-set requires --element <eid>");
      if (!flags.status || flags.status === "true") fail("asset-status-set requires --status quarantine|checked|accepted");
      return print(setAssetStatus(repoRoot, { projectId: id, elementId: flags.element, status: flags.status }));
    }
    case "asset-status-check": {
      if (!id) fail("asset-status-check requires <id>");
      if (!flags.element || flags.element === "true") fail("asset-status-check requires --element <eid>");
      return print(await runAssetTechnicalGate(repoRoot, { projectId: id, elementId: flags.element }));
    }
    case "asset-style-check": {
      if (!id) fail("asset-style-check requires <id>");
      if (!flags.element || flags.element === "true") fail("asset-style-check requires --element <eid>");
      return print(await runAssetStyleVerdict(repoRoot, { projectId: id, elementId: flags.element }));
    }
    case "element-remove": {
      if (!id) fail("element-remove requires <id>");
      if (!flags.element) fail("element-remove requires --element <eid>");
      return print(removeElement(repoRoot, id, flags.element));
    }
    case "elements-set": {
      // Batched multi-element patch (one journal entry). --json is a patches array
      // or a { patches: [...] } wrapper; each patch is {elementId, x?, y?, w?, h?,
      // name?, visible?, rotation?, flipH?, flipV?, opacity?, filters?} — same fields as
      // `move`/`element-set` (incl. T0232 3a rotation/flip and T0273 filters/opacity),
      // applied together.
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
    case "slice9-set": {
      // T0233: 9-slice insets — a DEDICATED op (not element-set), matching the
      // regions-set/export-set precedent. --clear sends insets:null. Otherwise,
      // omitted --left/--top/--right/--bottom/--scale flags merge over the
      // element's CURRENT slice9 (or 0 for the insets / absent for --scale) —
      // ergonomic partial edits (e.g. bump just --left), same contract as CLI
      // partial merges elsewhere in this file.
      if (!id) fail("slice9-set requires <id>");
      if (!flags.element) fail("slice9-set requires --element <eid>");
      if (flags.clear === "true") {
        return print(setSlice9(repoRoot, { projectId: id, elementId: flags.element, insets: null }));
      }
      const element = (getProject(repoRoot, id).elements || []).find((item) => item.id === flags.element);
      if (!element) fail(`element not found: ${flags.element}`);
      const current = element.slice9 || {};
      const flagOrCurrent = (flag, fallback) =>
        flags[flag] !== undefined && flags[flag] !== "true" ? Number(flags[flag]) : fallback;
      const insets = {
        left: flagOrCurrent("left", current.left || 0),
        top: flagOrCurrent("top", current.top || 0),
        right: flagOrCurrent("right", current.right || 0),
        bottom: flagOrCurrent("bottom", current.bottom || 0),
      };
      const scale = flagOrCurrent("scale", current.scale);
      if (scale !== undefined) insets.scale = scale;
      return print(setSlice9(repoRoot, { projectId: id, elementId: flags.element, insets }));
    }
    case "animation-set": {
      // T0260 Track A: set/clear an element's procedural animation spec — a DEDICATED op
      // (matching slice9-set/export-set). --json reads the ai_studio.canvas.animation.v1
      // spec (a {channels:[...]} object or a bare channels array) from a file and the op
      // validates it loudly; --clear sends animation:null. Image AND text both accept one.
      if (!id) fail("animation-set requires <id>");
      if (!flags.element || flags.element === "true") fail("animation-set requires --element <eid>");
      if (flags.clear === "true") {
        return print(setElementAnimation(repoRoot, { projectId: id, elementId: flags.element, animation: null }));
      }
      if (!flags.json || flags.json === "true") fail("animation-set requires --json <path> or --clear");
      const raw = JSON.parse(readFileSync(resolve(flags.json), "utf8"));
      const animation = Array.isArray(raw) ? { channels: raw } : raw;
      return print(setElementAnimation(repoRoot, { projectId: id, elementId: flags.element, animation }));
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
      // via the image-tools matte pipeline; mints the cutout as a NEW element beside the
      // source (original untouched, for side-by-side A/B — T0336), one undo.
      // --method auto (route; refuses a dual-plate soft zone), matte (force key_matte),
      // corridorkey (T0261/T0262 — neural GREEN-screen matte for soft glow art; green native,
      // magenta via a hue180 shim, ~15s GPU; a key that is neither is a loud refusal; --regions
      // composites the whole-frame CK result into the requested regions), vitmatte (T0335 — neural
      // THIN detail on a green/magenta key + 2nd-choice glow, own GPU venv, ~1-3s, whole-element
      // only), or birefnet (T0335 — SOD cutout for ANY/unknown background, no key, shared repo venv
      // CPU ~10-30s, whole-element only). --elements e1,e2 batches 2+ images into ONE journal
      // entry/undo (regions are not allowed with a batch — regions stay single-element, use
      // --element).
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
      // AUTOMATIC dual-plate alpha (T0238/T0248): works from ANY art — a flat-light
      // element's own pixels are the LIGHT plate, any other background generates the
      // WHITE plate first (codex edit of the element); then the DARK plate as a codex
      // edit of the light plate, gate (one automatic retry on the dark only), key ->
      // ONE new element beside the source. --prompt is an optional extra subject
      // description appended to both subject-lock prompts.
      if (!id) fail("alpha-dual-generate requires <id>");
      if (!flags.element || flags.element === "true") fail("alpha-dual-generate requires --element <eid>");
      if (flags["no-lock"] !== undefined && flags["no-lock"] !== true) fail("--no-lock does not take a value");
      const args = { projectId: id, elementId: flags.element, noLock: flags["no-lock"] === true };
      if (flags.prompt && flags.prompt !== "true") args.prompt = flags.prompt;
      return print(await alphaDualPlateGenerate(repoRoot, args));
    }
    case "quantize": {
      // T0207: palette-quantize the element's CURRENT pixels (alpha untouched). With
      // --preview, computes the result and writes it to the given PNG path + prints the
      // report — NOTHING is journaled (Cancel is free). Without --preview, applies: one
      // new content-addressed file + one journaled src swap.
      if (!id) fail("quantize requires <id>");
      if (!flags.element || flags.element === "true") fail("quantize requires --element <eid>");
      if (flags.colors === undefined || flags.colors === "true") fail("quantize requires --colors <n>");
      const params = { colors: Number(flags.colors) };
      if (flags.dither !== undefined) params.dither = parseBool("dither", flags.dither);
      if (flags.preview && flags.preview !== "true") {
        const result = await cleanupPreview(repoRoot, { projectId: id, elementId: flags.element, tool: "quantize", params });
        const previewPath = resolve(flags.preview);
        writeFileSync(previewPath, Buffer.from(result.previewBase64, "base64"));
        return print({ preview: previewPath, tool: result.tool, params: result.params, report: result.report });
      }
      return print(await cleanupApply(repoRoot, { projectId: id, elementId: flags.element, tool: "quantize", params }));
    }
    case "denoise": {
      // T0207: light median denoise (alpha NEVER filtered). Same --preview/apply split as
      // quantize above.
      if (!id) fail("denoise requires <id>");
      if (!flags.element || flags.element === "true") fail("denoise requires --element <eid>");
      if (flags.strength === undefined || flags.strength === "true") fail("denoise requires --strength 1|2|3");
      const params = { strength: Number(flags.strength) };
      if (flags.preview && flags.preview !== "true") {
        const result = await cleanupPreview(repoRoot, { projectId: id, elementId: flags.element, tool: "denoise", params });
        const previewPath = resolve(flags.preview);
        writeFileSync(previewPath, Buffer.from(result.previewBase64, "base64"));
        return print({ preview: previewPath, tool: result.tool, params: result.params, report: result.report });
      }
      return print(await cleanupApply(repoRoot, { projectId: id, elementId: flags.element, tool: "denoise", params }));
    }
    case "filters-bake": {
      // T0274 "Apply": rasterize the element's CURRENT filters+opacity into a new source
      // file, then clear both (one undo step). --elements batches 2+ images into ONE
      // journal entry (atomic — mirrors the alpha verb's single-vs-batch shape).
      if (!id) fail("filters-bake requires <id>");
      const hasElements = flags.elements && flags.elements !== "true";
      const hasElement = flags.element && flags.element !== "true";
      if (!hasElement && !hasElements) fail("filters-bake requires --element <eid> or --elements e1,e2");
      if (hasElement && hasElements) fail("filters-bake accepts --element or --elements, not both");
      const args = { projectId: id };
      if (hasElements) {
        args.elementIds = String(flags.elements).split(",").map((value) => value.trim()).filter(Boolean);
      } else {
        args.elementId = flags.element;
      }
      return print(await bakeFilters(repoRoot, args));
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
      const store = selectedCanvasStore(repoRoot, flags);
      if (flags.to && flags.to !== "true") {
        assertCanvasExportDestination(repoRoot, store, resolve(flags.to));
      }
      if (flags.zip && flags.zip !== "true") {
        assertCanvasExportDestination(repoRoot, store, resolve(flags.zip));
      }
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
      if (flags.visible !== undefined) args.visible = parseBool("visible", flags.visible);
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
      // boolean here via the shared strict parser; the op validates strictly too (a
      // boolean, no silent coercion) — belt and suspenders.
      if (flags.clip !== undefined) args.clip = parseBool("clip", flags.clip);
      // --screen true|false (T0332 B1): the export opt-in flag — same strict-boolean
      // handling as --clip, belt and suspenders with the op's own check.
      if (flags.screen !== undefined) args.screen = parseBool("screen", flags.screen);
      return print(patchGroup(repoRoot, args));
    }
    case "groups-set": {
      // Batched shared-toggle set across several groups (one journal entry; one undo) —
      // the CLI parity for the multi-group inspector. Only Visible / Clip are shared.
      if (!id) fail("groups-set requires <id>");
      if (!flags.groups || flags.groups === "true") fail("groups-set requires --groups g1,g2");
      const groupIds = String(flags.groups).split(",").map((value) => value.trim()).filter(Boolean);
      const args = { projectId: id, groupIds };
      if (flags.visible !== undefined) args.visible = parseBool("visible", flags.visible);
      if (flags.clip !== undefined) args.clip = parseBool("clip", flags.clip);
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
    case "group-scale": {
      // T0271: scale the group's full subtree (frame + descendants + text fontSize) to a
      // new frame — the default drag-mode counterpart to group-set's frame-only --w/--h.
      // All four dims are required (this IS the final frame, not a partial patch); the op
      // itself validates finite x/y and positive w/h, but fail early here with a clearer
      // message than the op's own per-field error.
      if (!id) fail("group-scale requires <id>");
      if (!flags.group) fail("group-scale requires --group <gid>");
      if (flags.x === undefined || flags.y === undefined || flags.w === undefined || flags.h === undefined) {
        fail("group-scale requires --x --y --w --h");
      }
      return print(scaleGroup(repoRoot, {
        projectId: id,
        groupId: flags.group,
        x: Number(flags.x),
        y: Number(flags.y),
        w: Number(flags.w),
        h: Number(flags.h),
      }));
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
    case "recipe-create": {
      if (!id) fail("recipe-create requires <id>");
      const args = { projectId: id };
      if (flags.name !== undefined && flags.name !== "true") args.name = flags.name;
      if (flags.x !== undefined) args.x = Number(flags.x);
      if (flags.y !== undefined) args.y = Number(flags.y);
      if (flags.w !== undefined) args.w = Number(flags.w);
      if (flags.h !== undefined) args.h = Number(flags.h);
      // --parent <gid> nests the new card; --parent none (or omitted) = top level.
      if (flags.parent !== undefined && flags.parent !== "true") {
        args.parentId = flags.parent === "none" ? null : flags.parent;
      }
      return print(createRecipeCard(repoRoot, args));
    }
    case "recipe-set": {
      if (!id) fail("recipe-set requires <id>");
      if (!flags.group) fail("recipe-set requires --group <gid>");
      const patch = {};
      if (flags.prompt !== undefined && flags.prompt !== "true") patch.prompt = flags.prompt;
      if (flags.engine !== undefined && flags.engine !== "true") patch.engine = flags.engine;
      // --style <id> sets the reserved by-id pointer; --style none clears it.
      if (flags.style !== undefined) patch.style_ref = flags.style === "none" || flags.style === "true" ? null : flags.style;

      // T0332 v2 pack-mode flags. `patchRecipe`'s own `pack` field REPLACES recipe.pack
      // WHOLESALE (it never merges onto the stored value — see ops.mjs's normalizeRecipePack
      // doc comment), so a caller that only means to tweak ONE field must assemble the FULL
      // object itself; this CLI does that by reading the card's CURRENT recipe.pack (or a
      // default template, when pack mode is currently off) and overlaying just the flags
      // given in this call. --pack none clears pack mode outright (sets recipe.pack = null),
      // and takes priority over any axes/vary/grid/max-jobs given in the SAME call — there is
      // nothing sensible to merge them onto once the mode is off.
      if (flags.pack !== undefined && flags.pack !== "true" && flags.pack !== "none") {
        fail(`recipe-set --pack only accepts "none" (to clear pack mode), got ${JSON.stringify(flags.pack)}`);
      }
      const packFlagsGiven = ["axes-json", "vary", "grid", "max-jobs"].some((flag) => flags[flag] !== undefined);
      if (flags.pack === "none") {
        patch.pack = null;
      } else if (packFlagsGiven) {
        const current = getProject(repoRoot, id);
        const card = (current.groups || []).find((group) => group.id === flags.group);
        const currentPack = card && card.recipe ? card.recipe.pack : undefined;
        const base = currentPack || { axes: {}, vary: "", grid: [3, 3], max_jobs: 12 };
        const nextPack = { axes: base.axes, vary: base.vary, grid: base.grid, max_jobs: base.max_jobs };
        // --axes-json reads {axisName: ["val1", "val2", ...], ...} from a file — the op itself
        // validates every axis is a non-empty array of non-empty strings.
        if (flags["axes-json"] !== undefined && flags["axes-json"] !== "true") {
          nextPack.axes = JSON.parse(readFileSync(resolve(flags["axes-json"]), "utf8"));
        }
        if (flags.vary !== undefined && flags.vary !== "true") nextPack.vary = flags.vary;
        if (flags.grid !== undefined && flags.grid !== "true") {
          const match = /^(\d+)x(\d+)$/i.exec(String(flags.grid).trim());
          if (!match) fail(`recipe-set --grid must look like RxC (e.g. 3x3), got ${JSON.stringify(flags.grid)}`);
          nextPack.grid = [Number(match[1]), Number(match[2])];
        }
        if (flags["max-jobs"] !== undefined && flags["max-jobs"] !== "true") nextPack.max_jobs = Number(flags["max-jobs"]);
        patch.pack = nextPack;
      }

      // T0332 v2 params flags — UNLIKE pack above, this is a PARTIAL patch: the op itself
      // merges it onto the stored recipe.params one level deep (ops.mjs's patchRecipe), so
      // there is nothing to assemble/read-back here.
      const paramsPatch = {};
      if (flags["bg-key"] !== undefined && flags["bg-key"] !== "true") paramsPatch.bg_key = flags["bg-key"];
      if (flags["n-candidates"] !== undefined && flags["n-candidates"] !== "true") paramsPatch.n_candidates = Number(flags["n-candidates"]);
      if (flags.size !== undefined && flags.size !== "true") paramsPatch.size = flags.size;
      if (flags.quality !== undefined && flags.quality !== "true") paramsPatch.quality = flags.quality;
      if (Object.keys(paramsPatch).length) patch.params = paramsPatch;

      if (!Object.keys(patch).length) {
        fail(
          "recipe-set requires at least one of --prompt, --engine, --style, --axes-json, --vary, --grid, --max-jobs, --pack none, --bg-key, --n-candidates, --size, --quality",
        );
      }
      return print(patchRecipe(repoRoot, { projectId: id, groupId: flags.group, patch }));
    }
    case "recipe-generate": {
      // T0239 increment 2: real codex/agy spawn (minutes) — no fake-generator injection
      // from the CLI (that is a test-only seam), so this always runs the DEFAULT engine(s).
      if (!id) fail("recipe-generate requires <id>");
      if (!flags.group) fail("recipe-generate requires --group <gid>");
      if (flags["no-lock"] !== undefined && flags["no-lock"] !== true) fail("--no-lock does not take a value");
      return print(await generateFromRecipe(repoRoot, { projectId: id, groupId: flags.group, noLock: flags["no-lock"] === true }));
    }
    case "recipe-expand": {
      // T0239 increment 4: real codex TEXT spawn — no fake-assistant injection from the
      // CLI (test-only seam), so this always runs the DEFAULT expandPrompt impl.
      if (!id) fail("recipe-expand requires <id>");
      if (!flags.group) fail("recipe-expand requires --group <gid>");
      return print(await expandRecipePrompt(repoRoot, { projectId: id, groupId: flags.group }));
    }
    case "recipe-pack-generate": {
      // T0332 v2: the pack-mode branch of generateFromRecipe — requires recipe.pack. Real
      // engine spawns (codex or agy), sequential, one PER SHEET (N x 30-60s+; timeout=max, resume via --run
      // on a kill/timeout) — no fake-generator injection from the CLI, same law as
      // recipe-generate. --run <groupId> resumes an existing pack run group (sheets whose
      // axes already landed there are skipped); --sheet <slug> force-regenerates exactly
      // one sheet (by the expander's own job name) into that same group even if present.
      if (!id) fail("recipe-pack-generate requires <id>");
      if (!flags.group) fail("recipe-pack-generate requires --group <gid>");
      if (flags["no-lock"] !== undefined && flags["no-lock"] !== true) fail("--no-lock does not take a value");
      const args = { projectId: id, groupId: flags.group, noLock: flags["no-lock"] === true };
      if (flags.run !== undefined && flags.run !== "true") args.runGroupId = flags.run;
      if (flags.sheet !== undefined && flags.sheet !== "true") args.sheetSlug = flags.sheet;
      const result = await generateFromRecipe(repoRoot, args);
      for (const sheet of result.results || []) {
        console.log(`${sheet.name}: ${sheet.status}`);
      }
      return print(result.last_run);
    }
    case "recipe-pack-preview": {
      // T0332 v2: runs the REAL expand_jobs.py expander (no fake injection seam; it is
      // offline/deterministic/stdlib, unlike recipe-generate/-expand's codex spawns). Not
      // journaled; writes nothing to the recipe blob.
      if (!id) fail("recipe-pack-preview requires <id>");
      if (!flags.group) fail("recipe-pack-preview requires --group <gid>");
      const result = await packPreview(repoRoot, { projectId: id, groupId: flags.group });
      console.log(`sheets: ${result.sheets}`);
      console.log(`style_ref_image: ${result.style_ref_image}`);
      for (const job of result.jobs) {
        const head = (job.prompt.split(/\r?\n/).find((line) => line.trim()) || "").slice(0, 120);
        console.log(`- ${job.name}: ${head}`);
      }
      return print(result);
    }
    case "recipe-pack-slice": {
      // T0332 B3 (build-spec §4): slice every sheet of a pack run into cuts — detectRegions
      // -> hard gate region_count === meta.pack.cells.length -> sliceRegions, reparented
      // into the run group. Real region-detector + crop_regions.py spawns (same pipeline as
      // plain `detect-regions`/`slice` — no fake seam, unlike the codex-calling recipe-
      // pack-generate). --run <groupId> selects an explicit run group (must carry a
      // pack_run marker for THIS card); omitted resolves recipe.last_run.run_group_id.
      // NOT in SELF_LOCKING_COMMANDS: unlike recipe-pack-generate's own engine-per-sheet
      // branch, detectRegions/sliceRegions do not acquire the project lock themselves, so
      // the WHOLE multi-sheet slice pass is meant to run under ONE outer lock (main()'s own
      // wrap below) — the same shape plain detect-regions/slice already rely on.
      if (!id) fail("recipe-pack-slice requires <id>");
      if (!flags.group) fail("recipe-pack-slice requires --group <gid>");
      const args = { projectId: id, groupId: flags.group };
      if (flags.run !== undefined && flags.run !== "true") args.runGroupId = flags.run;
      const result = await packSlice(repoRoot, args);
      const project = getProject(repoRoot, id);
      for (const sheet of result.contract) {
        const element = (project.elements || []).find((el) => el.id === sheet.sheet_element_id);
        const name = element ? element.name : sheet.sheet_element_id;
        console.log(`${name}: ${sheet.verdict} (${sheet.region_count}/${sheet.cells_len})`);
      }
      return print(result);
    }
    case "style-create": {
      if (!id) fail("style-create requires <id>");
      const args = { projectId: id };
      if (flags.name !== undefined && flags.name !== "true") args.name = flags.name;
      if (flags.x !== undefined) args.x = Number(flags.x);
      if (flags.y !== undefined) args.y = Number(flags.y);
      if (flags.w !== undefined) args.w = Number(flags.w);
      if (flags.h !== undefined) args.h = Number(flags.h);
      // --parent <gid> nests the new card; --parent none (or omitted) = top level.
      if (flags.parent !== undefined && flags.parent !== "true") {
        args.parentId = flags.parent === "none" ? null : flags.parent;
      }
      return print(createStyleCard(repoRoot, args));
    }
    case "style-set": {
      if (!id) fail("style-set requires <id>");
      if (!flags.group) fail("style-set requires --group <gid>");
      const patch = {};
      if (flags.prompt !== undefined && flags.prompt !== "true") patch.prompt = flags.prompt;
      // --ref <elementId> sets the ONE image sent to generation; --ref none clears it.
      if (flags.ref !== undefined) patch.ref = flags.ref === "none" || flags.ref === "true" ? null : flags.ref;
      if (!Object.keys(patch).length) fail("style-set requires --prompt and/or --ref");
      return print(patchStyle(repoRoot, { projectId: id, groupId: flags.group, patch }));
    }
    case "anim-card": {
      if (!id) fail("anim-card requires <id>");
      const args = { projectId: id };
      if (flags.name !== undefined && flags.name !== "true") args.name = flags.name;
      // --member <eid> is the "Animate this image" promotion: fit the card around that image and
      // move it in as the first keyframe (ONE journal entry). The op refuses it combined with
      // explicit x/y/w/h (fit owns the box).
      if (flags.member !== undefined) {
        if (flags.member === "true") fail("anim-card --member requires an element id");
        args.memberId = flags.member;
      }
      if (flags.x !== undefined) args.x = Number(flags.x);
      if (flags.y !== undefined) args.y = Number(flags.y);
      if (flags.w !== undefined) args.w = Number(flags.w);
      if (flags.h !== undefined) args.h = Number(flags.h);
      // --parent <gid> nests the new card; --parent none (or omitted) = top level.
      if (flags.parent !== undefined && flags.parent !== "true") {
        args.parentId = flags.parent === "none" ? null : flags.parent;
      }
      return print(createAnimCard(repoRoot, args));
    }
    case "anim-patch": {
      if (!id) fail("anim-patch requires <id>");
      if (!flags.group) fail("anim-patch requires --group <gid>");
      const patch = {};
      if (flags.motion !== undefined && flags.motion !== "true") patch.motion = flags.motion;
      if (flags.profile !== undefined && flags.profile !== "true") patch.profile = flags.profile;
      // --seed <n> pins the seed; --seed none|null = a fresh random seed on every Generate. A bare
      // --seed (no value; the parser hands "true") is a LOUD error, not a silent NaN (F5).
      if (flags.seed !== undefined) {
        if (flags.seed === "true") fail("--seed requires a value or 'none'");
        patch.seed = flags.seed === "none" || flags.seed === "null" ? null : Number(flags.seed);
      }
      if (flags.matte !== undefined && flags.matte !== "true") patch.matte = flags.matte;
      // --gen-fps <n> overrides the workflow generation fps; --gen-fps none|null = workflow default.
      if (flags["gen-fps"] !== undefined) {
        if (flags["gen-fps"] === "true") fail("--gen-fps requires a value or 'none'");
        patch.gen_fps = flags["gen-fps"] === "none" || flags["gen-fps"] === "null" ? null : Number(flags["gen-fps"]);
      }
      if (flags.loop !== undefined) patch.loop = parseBool("loop", flags.loop);
      if (flags.columns !== undefined) {
        if (flags.columns === "true") fail("--columns requires a value or 'none'");
        patch.columns = flags.columns === "none" || flags.columns === "null" ? null : Number(flags.columns);
      }
      if (flags.trim !== undefined) patch.trim = parseBool("trim", flags.trim);
      // --style <id> sets the style-card pointer; --style none clears it. A bare --style is a LOUD
      // error, not a silent clear (F5) — an explicit clear stays 'none'.
      if (flags.style !== undefined) {
        if (flags.style === "true") fail("--style requires a value or 'none'");
        patch.style_ref = flags.style === "none" ? null : flags.style;
      }
      // --accepted <eid> pins the accepted flipbook take; --accepted none clears it. Bare -> LOUD.
      if (flags.accepted !== undefined) {
        if (flags.accepted === "true") fail("--accepted requires a value or 'none'");
        patch.accepted_ref = flags.accepted === "none" ? null : flags.accepted;
      }
      if (!Object.keys(patch).length) {
        fail("anim-patch requires at least one of --motion, --profile, --seed, --matte, --gen-fps, --loop, --columns, --trim, --style, --accepted");
      }
      return print(patchAnim(repoRoot, { projectId: id, groupId: flags.group, patch }));
    }
    case "anim-generate": {
      // T0265 increment 1: the real Track B video pipeline (ComfyUI + CorridorKey, minutes) —
      // no fake-generator injection from the CLI (that is a test-only seam), so this always
      // runs the DEFAULT generator (tools/anim_generate.mjs).
      if (!id) fail("anim-generate requires <id>");
      if (!flags.group) fail("anim-generate requires --group <gid>");
      if (flags["no-lock"] !== undefined && flags["no-lock"] !== true) fail("--no-lock does not take a value");
      return print(await generateAnimFromCard(repoRoot, {
        projectId: id,
        groupId: flags.group,
        noLock: flags["no-lock"] === true,
      }));
    }
    case "extract": {
      // T0239 increment 4: real codex VISION spawn — no fake-assistant injection from the
      // CLI (test-only seam), so this always runs the DEFAULT extractFromImage impl.
      if (!id) fail("extract requires <id>");
      if (!flags.element || flags.element === "true") fail("extract requires --element <eid>");
      return print(await extractFromElement(repoRoot, { projectId: id, elementId: flags.element }));
    }
    case "animate": {
      // T0264: the text->animation bridge — a real codex TEXT/VISION spawn (no fake-runner
      // injection from the CLI, test-only seam), so this always runs the DEFAULT codex runner.
      if (!id) fail("animate requires <id>");
      if (!flags.element || flags.element === "true") fail("animate requires --element <eid>");
      if (!flags.text || flags.text === "true") fail("animate requires --text \"<description>\"");
      return print(await animateElementFromText(repoRoot, { projectId: id, elementId: flags.element, text: flags.text }));
    }
    case "promote-recipe": {
      if (!id) fail("promote-recipe requires <id>");
      if (!flags.element || flags.element === "true") fail("promote-recipe requires --element <eid>");
      return print(promoteExtractedRecipe(repoRoot, { projectId: id, elementId: flags.element }));
    }
    case "promote-style": {
      if (!id) fail("promote-style requires <id>");
      if (!flags.element || flags.element === "true") fail("promote-style requires --element <eid>");
      return print(promoteExtractedStyle(repoRoot, { projectId: id, elementId: flags.element }));
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

// T0254 Tier 1 #1: slow codex/agy generation commands lock only their OWN
// final commit internally (ops.mjs — generateFromRecipe/expandRecipePrompt/
// extractFromElement/animateElementFromText/alphaDualPlateGenerate), so a multi-minute
// generation never blocks other mutations on the project. Wrapping them AGAIN here would try to
// acquire the SAME per-project lock twice in one call stack (the outer acquire would
// never release until the inner one finishes, and the inner one waits on the outer)
// — excluded on purpose. Every other project-scoped command is wrapped in main()
// Additional exceptions: alpha and alpha-dual are self-locking now that their evaluators run
// before one guarded final mint. Non-self-locking commands are wrapped below so the CLI (a SEPARATE process from
// the server) respects the same
// cross-process lockfile the API adapter does.
// recipe-pack-generate (T0332 v2) is likewise self-locking — its own generateFromRecipe pack
// branch acquires the per-project lock ONCE PER SHEET (short, per-sheet commits), so wrapping
// the WHOLE multi-sheet, multi-minute call in an outer lock here would deadlock the same way
// (the outer acquire never releases until every per-sheet inner acquire finishes, and the first
// inner acquire waits on the outer).
// recipe-pack-slice (T0332 B3) is the OPPOSITE case, deliberately left OUT of this set: it
// calls detectRegions/sliceRegions in a loop, and NEITHER of those acquires the project lock
// itself (unlike the five codex/agy ops above) — exactly like the plain `detect-regions`/
// `slice` commands, which are also not self-locking. So the whole multi-sheet slice pass
// needs the SAME single outer lock main() already gives every non-self-locking command; no
// deadlock risk here since there is no inner acquire to collide with.
// anim-generate (T0265) is self-locking too — generateAnimFromCard acquires the per-project
// lock ONCE internally (short commit), so wrapping it here would deadlock the same way.
const SELF_LOCKING_COMMANDS = new Set(["recipe-generate", "recipe-pack-generate", "recipe-expand", "extract", "animate", "alpha", "alpha-dual", "alpha-dual-generate", "anim-generate", "asset-status-check", "asset-style-check"]);

export async function main(argv, { repoRoot = DEFAULT_REPO_ROOT, print = defaultPrint } = {}) {
  const [command, ...rest] = argv;
  const { positional, flags } = parseFlags(rest);
  const id = positional[0];
  const startedAt = performance.now();
  const selectedArgs = canvasStoreArgs(flags);
  const hasStoreSelector = Boolean(selectedArgs.store || selectedArgs.game);
  const runInSelectedStore = (fn) => {
    if (!hasStoreSelector) return fn();
    return withCanvasStore(selectCanvasStore(repoRoot, selectedArgs), fn);
  };
  try {
    if (id && !hasStoreSelector) {
      assertBareCanvasProjectIdIsUnambiguous(repoRoot, id);
    }
    // `id` is undefined for project-less commands (list/create/help/...) —
    // withProjectLock is a no-op pass-through in that case (see its doc in
    // store.mjs), so this is safe to apply unconditionally to everything else.
    if (id && !SELF_LOCKING_COMMANDS.has(command)) {
      return await runInSelectedStore(() => withProjectLock(repoRoot, id, () => runCommand(command, id, positional, flags, { repoRoot, print })));
    }
    return await runInSelectedStore(() => runCommand(command, id, positional, flags, { repoRoot, print }));
  } catch (error) {
    // Mirror the API: a project-resolvable failure leaves a trail in
    // <project>/errors.jsonl (recordOpFailure no-ops when id can't resolve).
    try {
      await runInSelectedStore(() => recordOpFailure(repoRoot, id, {
        op: command || "",
        args_summary: flags || {},
        error,
        duration_ms: performance.now() - startedAt,
      }));
    } catch {
      // Keep the original command error intact; failure logging is best effort.
    }
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
