#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import { fail } from "../lib/cli.mjs";

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
  node tools/product_gate/visual_rejection_lock.mjs --project <game-id> --task <task-id> --screenshot <path> --problem <text> --next <text> [options]

Options:
  --surface <name>       desktop, portrait, tablet, web, or another short label
  --axis <name>          primary failed visual axis; default: art_quality
  --severity <name>      blocker, major, or minor; default: major
  --visual-issue <severity:axis:text> repeatable extra issue
  --visual-score <axis=n> repeatable score override, n is 1-5
  --where <text>         override player-read answer
  --action <text>        override player-read answer
  --response <text>      override player-read answer
  --reward <text>        override player-read answer
  --game-look <text>     override player-read answer
  --output <path>        markdown output path
  --json-output <path>   optional JSON output path
  --index-output <path>  latest-gate JSON index path
  --verify               request independent re-check after the lock is fixed`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = { visualIssues: [], visualScores: [] };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg === "--verify") values.verify = true;
    else if (arg === "--visual-issue") {
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values.visualIssues.push(value);
      index += 1;
    } else if (arg === "--visual-score") {
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail(`${arg} requires a value`);
      values.visualScores.push(value);
      index += 1;
    } else if (arg.startsWith("--")) {
      const key = arg.slice(2);
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

function scoreOverrides(rawScores) {
  const overrides = new Map();
  for (const raw of rawScores || []) {
    const [axis, score] = String(raw || "").split("=");
    if (VISUAL_AXES.includes(axis)) overrides.set(axis, score);
  }
  return overrides;
}

function pushArg(args, key, value) {
  if (value !== undefined && value !== "") args.push(key, String(value));
}

function defaultVisualScores(primaryAxis, rawScores) {
  const overrides = scoreOverrides(rawScores);
  return VISUAL_AXES.map((axis) => {
    const score = overrides.get(axis) || (axis === primaryAxis ? "1" : "2");
    return `${axis}=${score}`;
  });
}

const values = parseArgs(process.argv.slice(2));
if (values.help) usage();

const required = ["project", "task", "screenshot", "problem", "next"];
for (const key of required) {
  if (!String(values[key] || "").trim()) fail(`--${key} is required`);
}

const axis = values.axis || "art_quality";
if (!VISUAL_AXES.includes(axis)) fail(`--axis must be one of: ${VISUAL_AXES.join(", ")}`);
const severity = values.severity || "major";
if (!["blocker", "major", "minor"].includes(severity)) fail("--severity must be blocker, major, or minor");

const reviewArgs = [
  "tools/product_gate/review.mjs",
  "--project", values.project,
  "--task", values.task,
  "--surface", values.surface || "desktop",
  "--screenshot", values.screenshot,
  "--verdict", "fail",
  "--where", values.where || "Lead-rejected gameplay screenshot under review.",
  "--action", values.action || "Stop feature expansion and resolve the rejected visual issue.",
  "--response", values.response || "A strict fail gate records the rejection before more implementation.",
  "--reward", values.reward || "Next pass must close this visual mismatch before acceptance.",
  "--game-look", values["game-look"] || "The lead rejected the current game look, so it is not accepted yet.",
  "--problem", values.problem,
  "--next", values.next,
  "--visual-strict",
  "--strict",
  "--task-log",
  "--visual-issue", `${severity}:${axis}:${values.problem}`,
];

for (const score of defaultVisualScores(axis, values.visualScores)) {
  reviewArgs.push("--visual-score", score);
}
for (const issue of values.visualIssues) {
  reviewArgs.push("--visual-issue", issue);
}

pushArg(reviewArgs, "--output", values.output);
pushArg(reviewArgs, "--json-output", values["json-output"]);
pushArg(reviewArgs, "--index-output", values["index-output"]);
if (values.verify) reviewArgs.push("--verify");

const result = spawnSync(process.execPath, reviewArgs, {
  cwd: process.cwd(),
  env: process.env,
  encoding: "utf8",
  stdio: "inherit",
});

if (result.error) fail(result.error.message);
process.exit(result.status ?? 1);
