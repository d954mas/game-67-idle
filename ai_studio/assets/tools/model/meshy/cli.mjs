#!/usr/bin/env node
import { isMain } from "../../../../core_harness/tool_lib/cli.mjs";
import { buildPlan, getBalance, printablePlan, runGeneration } from "./client.mjs";

function usage() {
  return `usage:
  node ai_studio/assets/tools/model/meshy/cli.mjs plan <text-preview|text-refine|image> [options]
  node ai_studio/assets/tools/model/meshy/cli.mjs run <text-preview|text-refine|image> [options]
  node ai_studio/assets/tools/model/meshy/cli.mjs balance

Inputs:
  --prompt <text>                 Text-to-3D preview prompt.
  --preview-task-id <id>         Successful preview id for text refinement.
  --image <png|jpg>              Local reference for image-to-3D.
  --texture-prompt <text>        Optional texture guidance.
  --profile <name>               text-preview: draft|game; text-refine: draft|game|ultra;
                                 image: draft|game|quality|ultra.
  --target-polycount <n>         Override the profile's game-oriented default.

Paid execution gate (run only):
  --execute                      Explicitly allow one new paid task.
  --confirm-credits <n>          Must exactly match plan.estimated_credits.
  --max-credits <n>              Must exactly match the caller-approved estimate.
  --reserve-credits <n>          Credits that must remain after the estimate.
  --confirm-reserve-credits <n>  Required when lowering the configured reserve.`;
}

export function parseArgs(argv) {
  const [command, kind, ...rest] = argv;
  if (command === "balance" && !kind && rest.length === 0) return { command };
  if (!["plan", "run"].includes(command) || !kind) throw new Error(usage());
  const options = {};
  for (let index = 0; index < rest.length; index += 1) {
    const arg = rest[index];
    if (arg === "--execute") { options.execute = true; continue; }
    if (arg === "--help" || arg === "-h") throw new Error(usage());
    const value = rest[index + 1];
    if (!value || value.startsWith("--")) throw new Error(`missing value for ${arg}`);
    index += 1;
    if (arg === "--prompt") options.prompt = value;
    else if (arg === "--preview-task-id") options.previewTaskId = value;
    else if (arg === "--image") options.image = value;
    else if (arg === "--texture-prompt") options.texturePrompt = value;
    else if (arg === "--profile") options.profile = value;
    else if (arg === "--target-polycount") options.targetPolycount = value;
    else if (arg === "--confirm-credits") options.confirmCredits = value;
    else if (arg === "--max-credits") options.maxCredits = value;
    else if (arg === "--reserve-credits") options.reserveCredits = value;
    else if (arg === "--confirm-reserve-credits") options.confirmReserveCredits = value;
    else throw new Error(`unknown option: ${arg}`);
  }
  return { command, kind, options };
}

export async function main(argv = process.argv.slice(2)) {
  const args = parseArgs(argv);
  if (args.command === "balance") return getBalance();
  if (args.command === "plan") return printablePlan(await buildPlan(args.kind, args.options));
  return runGeneration(args.kind, args.options);
}

if (isMain(import.meta.url)) {
  main().then((result) => console.log(JSON.stringify(result, null, 2))).catch((error) => {
    console.error(`error: ${error.message}`);
    process.exit(1);
  });
}
