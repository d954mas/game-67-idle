#!/usr/bin/env node
// Scaffold one reproducible generation/provenance record for accepted source art.

import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { fail } from "../../lib/cli.mjs";

const root = resolve(fileURLToPath(new URL("../../..", import.meta.url)));

function usage() {
  console.log(`usage:
  node tools/assets/job/new_generation_record.mjs --id <record-id> --project-dir <project-dir> --source-family <family> --accepted-source <path> --provider <name> --model <name> (--seed <seed> | --no-seed-reason <text>) --prompt <text> --negative-prompt <text> [options]

Options:
  --output <path>                  Override output path.
  --workflow-path <path>           Path to ComfyUI/SD/OpenAI/etc workflow JSON.
  --workflow-json <json>           Inline workflow JSON; empty object is only
                                   allowed for procedural debug records.
  --prompt-packet <path>           Source-sheet prompt packet used for this run.
  --source-family-role <text>      More specific role/variant inside the source
                                   family. Defaults to --source-family.
  --seed <seed>                    Stable generation seed when the provider exposes one.
  --no-seed-reason <text>           Required instead of --seed when the provider
                                   does not expose stable seed data.
  --final-art-source <kind>         generated | artist | procedural. Default: generated.
  --procedural-exception <text>     Required when final-art-source is procedural.
  --rejected-notes <text>           Candidate/rejection notes.
  --dry-run                         Print without writing.`);
}

function parseArgs(argv) {
  const values = { dryRun: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg === "--dry-run") values.dryRun = true;
    else if (arg.startsWith("--")) {
      const key = arg.slice(2).replaceAll("-", "_");
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values[key] = value;
      index += 1;
    } else {
      fail(`unknown argument: ${arg}`);
    }
  }
  return values;
}

function parseWorkflowJson(value) {
  if (!value) return undefined;
  try {
    return JSON.parse(value);
  } catch (error) {
    fail(`--workflow-json must be valid JSON: ${error.message}`);
  }
}

function writeJson(path, data, dryRun) {
  const fullPath = join(root, path);
  if (existsSync(fullPath)) {
    fail(`refusing to overwrite existing file: ${path}`);
  }
  const text = `${JSON.stringify(data, null, 2)}\n`;
  if (dryRun) {
    console.log(`would write ${path}`);
    console.log(text);
    return;
  }
  mkdirSync(dirname(fullPath), { recursive: true });
  writeFileSync(fullPath, text, "utf8");
  console.log(`wrote ${path}`);
}

const args = parseArgs(process.argv.slice(2));
if (args.help) {
  usage();
  process.exit(0);
}

for (const key of ["id", "project_dir", "source_family", "accepted_source", "provider", "model", "prompt", "negative_prompt"]) {
  if (!args[key]) fail(`--${key.replaceAll("_", "-")} is required`);
}
if (!args.seed && !args.no_seed_reason) {
  fail("--seed or --no-seed-reason is required");
}
if (!/^[a-z0-9][a-z0-9_-]*[a-z0-9]$/.test(args.id)) {
  fail("--id must use lowercase letters, numbers, hyphen, or underscore");
}

const finalArtSource = args.final_art_source || "generated";
if (!["generated", "artist", "procedural"].includes(finalArtSource)) {
  fail("--final-art-source must be generated, artist, or procedural");
}
if (finalArtSource === "procedural" && !args.procedural_exception) {
  fail("--procedural-exception is required when --final-art-source procedural");
}
if (!args.workflow_path && !args.workflow_json) {
  fail("--workflow-path or --workflow-json is required");
}
const workflowJson = parseWorkflowJson(args.workflow_json);
if (workflowJson && typeof workflowJson === "object" && Object.keys(workflowJson).length === 0 && finalArtSource !== "procedural") {
  fail("--workflow-json must not be empty for generated or artist records; capture workflow metadata or use --workflow-path");
}

const projectDir = args.project_dir.replaceAll("\\", "/").replace(/\/+$/g, "");
const outputPath = args.output ? args.output.replaceAll("\\", "/") : `${projectDir}/art/generation_records/${args.id}.json`;

const record = {
  schema: "game.art_generation_record",
  version: 1,
  id: args.id,
  provider: args.provider,
  model_or_workflow: args.model,
  workflow_path: args.workflow_path ? args.workflow_path.replaceAll("\\", "/") : undefined,
  workflow_json: workflowJson,
  prompt_packet: args.prompt_packet ? args.prompt_packet.replaceAll("\\", "/") : undefined,
  seed: args.seed ? Number.isNaN(Number(args.seed)) ? args.seed : Number(args.seed) : undefined,
  no_seed_reason: args.no_seed_reason || undefined,
  prompt: args.prompt,
  negative_prompt: args.negative_prompt,
  source_family: args.source_family,
  source_family_role: args.source_family_role || args.source_family,
  accepted_source_image: args.accepted_source.replaceAll("\\", "/"),
  final_art_source: finalArtSource,
  procedural_exception: args.procedural_exception || undefined,
  rejected_candidate_notes: args.rejected_notes || "",
};

for (const [key, value] of Object.entries(record)) {
  if (value === undefined) delete record[key];
}

writeJson(outputPath, record, args.dryRun);
