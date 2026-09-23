#!/usr/bin/env node
import { isMain } from "../../../../core_harness/tool_lib/cli.mjs";
import { doctor, generate, parseOffsets, retarget } from "./client.mjs";

function usage() {
  return `usage:
  node ai_studio/assets/tools/model/kimodo/cli.mjs doctor
  node ai_studio/assets/tools/model/kimodo/cli.mjs generate --prompt <text> [--prompt <text> ...] [options]
  node ai_studio/assets/tools/model/kimodo/cli.mjs retarget --motion <run-dir|bvh> --character <glb> --map <id|json> [options]

generate options:
  --frames <n>        2..300 at 30 fps. Default 150.
  --steps <n>         Denoising steps. Default 100.
  --seed <n>          Default 1.
  --model <id>        soma-rp-v1.1 (default) or soma-seed-v1.1.
  --backend <b>       auto (default), cpu, or vulkan.

retarget options:
  --clip-name <name>  Animation name in the exported GLB. Default: the run's prompt slug.
  --offset B=x,y,z    Extra local rotation in degrees for bone B; repeatable, merged over the map.
  --no-foot-lock      Skip the planted-foot IK pass.

Environment overrides:
  KIMODO_HOME, KIMODO_WORK_ROOT, KIMODO_BLENDER`;
}

export function parseArgs(argv) {
  const [command, ...rest] = argv;
  if (command === "doctor" && rest.length === 0) return { command };
  if (!["generate", "retarget"].includes(command)) throw new Error(usage());
  const options = { prompts: [], offsetEntries: [] };
  for (let index = 0; index < rest.length; index += 1) {
    const arg = rest[index];
    if (arg === "--help" || arg === "-h") throw new Error(usage());
    if (arg === "--no-foot-lock" && command === "retarget") { options.footLock = false; continue; }
    const value = rest[index + 1];
    if (value === undefined || value.startsWith("--")) throw new Error(`missing value for ${arg}`);
    index += 1;
    if (command === "generate" && arg === "--prompt") options.prompts.push(value);
    else if (command === "generate" && arg === "--frames") options.frames = value;
    else if (command === "generate" && arg === "--steps") options.steps = value;
    else if (command === "generate" && arg === "--seed") options.seed = value;
    else if (command === "generate" && arg === "--model") options.model = value;
    else if (command === "generate" && arg === "--backend") options.backend = value;
    else if (command === "retarget" && arg === "--motion") options.motion = value;
    else if (command === "retarget" && arg === "--character") options.character = value;
    else if (command === "retarget" && arg === "--map") options.map = value;
    else if (command === "retarget" && arg === "--clip-name") options.clipName = value;
    else if (command === "retarget" && arg === "--offset") options.offsetEntries.push(value);
    else throw new Error(`unknown option for ${command}: ${arg}`);
  }
  if (command === "retarget") options.offsets = parseOffsets(options.offsetEntries);
  delete options.offsetEntries;
  if (command === "retarget") delete options.prompts;
  return { command, options };
}

export async function main(argv = process.argv.slice(2)) {
  const args = parseArgs(argv);
  if (args.command === "doctor") return doctor();
  if (args.command === "generate") return generate(args.options);
  return retarget(args.options);
}

if (isMain(import.meta.url)) {
  main().then((result) => console.log(JSON.stringify(result, null, 2))).catch((error) => {
    console.error(`error: ${error.message}`);
    process.exit(1);
  });
}
