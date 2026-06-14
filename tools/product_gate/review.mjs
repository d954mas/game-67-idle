#!/usr/bin/env node
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { spawnSync } from "node:child_process";

function usage() {
  console.error(`usage:
  node tools/product_gate/review.mjs --project <game-id> --task <task-id> --screenshot <path> --verdict pass|fail [options]

Options:
  --surface <name>       desktop, portrait, tablet, web, or another short label
  --output <path>        markdown output path
  --json-output <path>   optional JSON output path
  --where <text>         answer: where am I?
  --action <text>        answer: what should I do now?
  --response <text>      answer: what changed after input?
  --reward <text>        answer: what did I get / why continue?
  --game-look <text>     answer: why does this look like a game?
  --problem <text>       required for strict fail
  --next <text>          required for strict fail
  --index-output <path>  latest-gate JSON index path
  --task-log             append a compact evidence line to the task log
  --strict               fail if a pass lacks strong answers or a fail lacks next action`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = {};
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg === "--strict") values.strict = true;
    else if (arg === "--task-log") values.taskLog = true;
    else if (arg.startsWith("--")) {
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

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function sanitizeToken(value) {
  return String(value || "")
    .toLowerCase()
    .replace(/[^a-z0-9_-]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 80) || "gate";
}

function defaultOutput(project, surface) {
  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  return `gamedesign/projects/${sanitizeToken(project)}/reviews/product_read_gate_${stamp}_${sanitizeToken(surface)}.md`;
}

function defaultJsonOutput(markdownPath) {
  return markdownPath.replace(/\.md$/i, ".json");
}

function defaultIndexOutput(project) {
  return `gamedesign/projects/${sanitizeToken(project)}/reviews/product_read_gate_latest.json`;
}

function hasUsefulAnswer(value) {
  return String(value || "").trim().length >= 8;
}

function relPath(path) {
  return resolve(path).startsWith(process.cwd()) ? resolve(path).slice(process.cwd().length + 1).replaceAll("\\", "/") : path;
}

function validate(values) {
  const errors = [];
  if (!values.project) errors.push("--project is required");
  if (!values.screenshot) errors.push("--screenshot is required");
  if (!values.verdict) errors.push("--verdict is required");
  if (values.verdict && !["pass", "fail"].includes(values.verdict)) errors.push("--verdict must be pass or fail");
  if (values.screenshot && !existsSync(resolve(values.screenshot))) errors.push(`screenshot does not exist: ${values.screenshot}`);

  const answers = ["where", "action", "response", "reward", "game-look"];
  if (values.strict || values.verdict === "pass") {
    for (const key of answers) {
      if (!hasUsefulAnswer(values[key])) errors.push(`--${key} needs a concrete player-read answer`);
    }
  }
  if (values.strict && values.verdict === "fail") {
    if (!hasUsefulAnswer(values.problem)) errors.push("--problem is required for strict fail");
    if (!hasUsefulAnswer(values.next)) errors.push("--next is required for strict fail");
  }
  return errors;
}

function renderMarkdown(record) {
  return [
    "---",
    "type: ProductReadGate",
    `project: ${record.project}`,
    `task: ${record.task || ""}`,
    `surface: ${record.surface}`,
    `verdict: ${record.verdict}`,
    `timestamp: ${record.timestamp}`,
    "---",
    "",
    `# Product Read Gate - ${record.project} / ${record.surface}`,
    "",
    `Verdict: **${record.verdict.toUpperCase()}**`,
    "",
    `Screenshot: \`${record.screenshot}\``,
    "",
    "## Player Read",
    "",
    `- Where am I? ${record.answers.where || "(missing)"}`,
    `- What should I do now? ${record.answers.action || "(missing)"}`,
    `- What changed after input? ${record.answers.response || "(missing)"}`,
    `- What is the reward / why continue? ${record.answers.reward || "(missing)"}`,
    `- Why does this look like a game? ${record.answers.game_look || "(missing)"}`,
    "",
    "## Review",
    "",
    `Problem: ${record.problem || "(none)"}`,
    "",
    `Next: ${record.next || "(none)"}`,
    "",
  ].join("\n");
}

function runTaskLog(record, markdownPath) {
  if (!record.task) fail("--task is required when --task-log is set");
  const log = [
    `product gate ${record.verdict.toUpperCase()} (${record.surface})`,
    `review: ${markdownPath}`,
    `screenshot: ${record.screenshot}`,
    `next: ${record.verdict === "pass" ? "continue to the next narrow slice" : (record.next || "fix the screen before adding content")}`,
  ].join("; ");
  const result = spawnSync(process.execPath, ["tools/taskboard/cli.mjs", "set", record.task, "--log", log], {
    cwd: process.cwd(),
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.status !== 0) {
    if (result.stdout) process.stdout.write(result.stdout);
    if (result.stderr) process.stderr.write(result.stderr);
    fail(`task log update failed for ${record.task}`);
  }
}

const values = parseArgs(process.argv.slice(2));
if (values.help) usage();
const surface = values.surface || "desktop";
const output = values.output || defaultOutput(values.project, surface);
const jsonOutput = values["json-output"] || defaultJsonOutput(output);
const indexOutput = values["index-output"] || defaultIndexOutput(values.project);
const errors = validate(values);
if (errors.length > 0) fail(errors.join("\n"));

const record = {
  schema: "game.product_read_gate",
  version: 1,
  project: values.project,
  task: values.task || "",
  surface,
  verdict: values.verdict,
  timestamp: new Date().toISOString(),
  screenshot: relPath(values.screenshot),
  answers: {
    where: values.where || "",
    action: values.action || "",
    response: values.response || "",
    reward: values.reward || "",
    game_look: values["game-look"] || "",
  },
  problem: values.problem || "",
  next: values.next || "",
};

mkdirSync(dirname(resolve(output)), { recursive: true });
writeFileSync(resolve(output), renderMarkdown(record), "utf8");
mkdirSync(dirname(resolve(jsonOutput)), { recursive: true });
writeFileSync(resolve(jsonOutput), `${JSON.stringify(record, null, 2)}\n`, "utf8");
mkdirSync(dirname(resolve(indexOutput)), { recursive: true });
writeFileSync(resolve(indexOutput), `${JSON.stringify({ ...record, markdown: output, json: jsonOutput }, null, 2)}\n`, "utf8");

if (values.taskLog) runTaskLog(record, output);

console.log(`# Product Read Gate`);
console.log(`Verdict: ${record.verdict}`);
console.log(`Markdown: ${output}`);
console.log(`JSON: ${jsonOutput}`);
console.log(`Latest: ${indexOutput}`);
console.log(`Next: ${record.verdict === "pass" ? "continue to the next narrow slice" : (record.next || "fix the screen before adding content")}`);
