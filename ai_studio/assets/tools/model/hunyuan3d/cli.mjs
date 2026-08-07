#!/usr/bin/env node
import { isMain } from "../../../../core_harness/tool_lib/cli.mjs";
import { buildPlan, doctor, runGeneration, startServer } from "./client.mjs";

function usage() {
  return `usage:
  node ai_studio/assets/tools/model/hunyuan3d/cli.mjs doctor
  node ai_studio/assets/tools/model/hunyuan3d/cli.mjs server-start
  node ai_studio/assets/tools/model/hunyuan3d/cli.mjs plan image --image <png|jpg> [options]
  node ai_studio/assets/tools/model/hunyuan3d/cli.mjs run image --image <png|jpg> --execute [options]
  node ai_studio/assets/tools/model/hunyuan3d/cli.mjs plan multiview --views <folder> [options]
  node ai_studio/assets/tools/model/hunyuan3d/cli.mjs run multiview --views <folder> --execute [options]
  node ai_studio/assets/tools/model/hunyuan3d/cli.mjs plan texture --mesh <glb> --image <png|jpg>
  node ai_studio/assets/tools/model/hunyuan3d/cli.mjs run texture --mesh <glb> --image <png|jpg> --execute

Options:
  --seed <n>                  Default 1234.
  --steps <n>                 Default 5 for image, 30 for multiview.
  --octree-resolution <n>     Default 128 for image, 256 for multiview.
  --guidance-scale <n>        Default 5.
  --num-chunks <n>            Multiview default 20000.
  --timeout-seconds <n>       Defaults: image 900, multiview 1800, texture 3600.

Environment overrides:
  HUNYUAN3D_HOME, HUNYUAN3D_URL, HUNYUAN3D_WORK_ROOT`;
}

export function parseArgs(argv) {
  const [command, kind, ...rest] = argv;
  if (["doctor", "server-start"].includes(command) && !kind && rest.length === 0) return { command };
  if (!["plan", "run"].includes(command) || !["image", "multiview", "texture"].includes(kind)) throw new Error(usage());
  const options = {};
  for (let index = 0; index < rest.length; index += 1) {
    const arg = rest[index];
    if (arg === "--execute") { options.execute = true; continue; }
    if (arg === "--help" || arg === "-h") throw new Error(usage());
    const value = rest[index + 1];
    if (!value || value.startsWith("--")) throw new Error(`missing value for ${arg}`);
    index += 1;
    if (arg === "--image") options.image = value;
    else if (arg === "--mesh") options.mesh = value;
    else if (arg === "--views") options.views = value;
    else if (arg === "--seed") options.seed = value;
    else if (arg === "--steps") options.steps = value;
    else if (arg === "--octree-resolution") options.octreeResolution = value;
    else if (arg === "--guidance-scale") options.guidanceScale = value;
    else if (arg === "--num-chunks") options.numChunks = value;
    else if (arg === "--timeout-seconds") options.timeoutSeconds = value;
    else throw new Error(`unknown option: ${arg}`);
  }
  return { command, kind, options };
}

export async function main(argv = process.argv.slice(2)) {
  const args = parseArgs(argv);
  if (args.command === "doctor") return doctor();
  if (args.command === "server-start") return startServer();
  if (args.command === "plan") return buildPlan(args.kind, args.options);
  return runGeneration(args.kind, args.options);
}

if (isMain(import.meta.url)) {
  main().then((result) => console.log(JSON.stringify(result, null, 2))).catch((error) => {
    console.error(`error: ${error.message}`);
    process.exit(1);
  });
}
