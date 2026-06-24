#!/usr/bin/env node
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { fail } from "../lib/cli.mjs";
import { relCwdPosix } from "../lib/paths.mjs";

const VISUAL_AXES = [
  "composition",
  "readability",
  "ui_controls",
  "action_direction",
  "art_quality",
  "audience_fit",
];

function usage() {
  console.error(`usage:
  node tools/product_gate/visual_critique_packet.mjs --project <game-id> --task <task-id> --screenshot <path> --target <path|text> --output <packet.md> [--json-output <packet.json>] [--surface <name>] [--brief <text>]

Creates a reusable visual/UI critic packet for a screenshot before product gate closeout.`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = {};
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") usage();
    if (!arg.startsWith("--")) fail(`unknown argument: ${arg}`);
    const key = arg.slice(2);
    const value = argv[index + 1];
    if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
    values[key] = value;
    index += 1;
  }
  return values;
}

function defaultJsonOutput(markdownPath) {
  return markdownPath.replace(/\.md$/i, ".json");
}

function validate(values) {
  const errors = [];
  for (const key of ["project", "task", "screenshot", "target", "output"]) {
    if (!values[key]) errors.push(`--${key} is required`);
  }
  if (values.screenshot && !existsSync(resolve(values.screenshot))) {
    errors.push(`screenshot does not exist: ${values.screenshot}`);
  }
  return errors;
}

function gateCommand(record) {
  return [
    "node tools/ai.mjs gate",
    `--project ${record.project}`,
    `--task ${record.task}`,
    `--surface ${record.surface}`,
    `--screenshot ${record.screenshot}`,
    "--verdict fail",
    "--strict",
    "--visual-strict",
    '--where "<where am I?>"',
    '--action "<what can I do?>"',
    '--response "<what changed?>"',
    '--reward "<why continue?>"',
    '--game-look "<why game?>"',
    '--problem "<specific visual/player-read problem>"',
    '--next "<smallest next visual fix>"',
    ...VISUAL_AXES.map((axis) => `--visual-score ${axis}=1`),
    '--visual-issue blocker:readability:"<concrete issue>"',
  ].join(" ");
}

function renderMarkdown(record) {
  const lines = [];
  lines.push("---");
  lines.push("type: VisualCritiquePacket");
  lines.push(`project: ${record.project}`);
  lines.push(`task: ${record.task}`);
  lines.push(`surface: ${record.surface}`);
  lines.push(`screenshot: ${record.screenshot}`);
  lines.push(`target: ${record.target}`);
  lines.push("---");
  lines.push("");
  lines.push(`# Visual Critic Packet - ${record.project} / ${record.surface}`);
  lines.push("");
  lines.push(`Task: \`${record.task}\``);
  lines.push(`Screenshot: \`${record.screenshot}\``);
  lines.push(`Target: \`${record.target}\``);
  lines.push(`Brief: ${record.brief || "(none)"}`);
  lines.push("");
  lines.push("## Critic Role");
  lines.push("");
  lines.push("Act as a harsh game visual/UI critic for a casual prototype. Judge the screenshot against the target and audience. Do not excuse placeholder art, unreadable text, debug controls, unclear action direction, or weak reward/progression feedback.");
  lines.push("");
  lines.push("## Required Output");
  lines.push("");
  lines.push("- Verdict: `pass` or `fail`.");
  lines.push("- One sentence: what the player should understand in the first 5 seconds.");
  lines.push("- Scores 1-5 for every axis below; pass requires all scores >= 4.");
  lines.push("- Blocker/major/minor issues with concrete visual evidence from the screenshot.");
  lines.push("- Smallest next visual fix before adding features/content.");
  lines.push("");
  lines.push("## Strict Visual Axes");
  lines.push("");
  for (const axis of VISUAL_AXES) {
    lines.push(`- ${axis}: score 1-5, evidence, fix if < 4`);
  }
  lines.push("");
  lines.push("Issue severities: `blocker`, `major`, `minor`. A pass cannot include blocker or major issues.");
  lines.push("");
  lines.push("## Product Gate Command Skeleton");
  lines.push("");
  lines.push("```powershell");
  lines.push(record.gate_command);
  lines.push("```");
  lines.push("");
  return lines.join("\n");
}

const values = parseArgs(process.argv.slice(2));
const errors = validate(values);
if (errors.length > 0) fail(errors.join("\n"));

const output = values.output;
const jsonOutput = values["json-output"] || defaultJsonOutput(output);
const record = {
  schema: "game.visual_critique_packet",
  version: 1,
  project: values.project,
  task: values.task,
  surface: values.surface || "desktop",
  screenshot: relCwdPosix(values.screenshot),
  target: values.target,
  brief: values.brief || "",
  axes: VISUAL_AXES,
};
record.gate_command = gateCommand(record);

mkdirSync(dirname(resolve(output)), { recursive: true });
writeFileSync(resolve(output), renderMarkdown(record), "utf8");
mkdirSync(dirname(resolve(jsonOutput)), { recursive: true });
writeFileSync(resolve(jsonOutput), `${JSON.stringify(record, null, 2)}\n`, "utf8");

console.log(`# Visual Critic Packet`);
console.log(`Markdown: ${output}`);
console.log(`JSON: ${jsonOutput}`);
console.log(`Next: run a critic pass, then convert findings into ${record.project} product gate evidence`);
