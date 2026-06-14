#!/usr/bin/env node
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import { spawnSync } from "node:child_process";

function usage() {
  console.error(`usage:
  node tools/product_gate/close_slice.mjs --task <task-id> --project <game-id> --gate <gate.json> --evidence <text> [options]

Options:
  --evidence <text>      repeatable validation evidence line
  --next <text>          next action or handoff note
  --status <status>      optional task status to set, e.g. review
  --allow-fail           allow closing a partial slice with a failed gate
  --strict               require a passing gate unless --allow-fail is set`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = { evidence: [] };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") values.help = true;
    else if (arg === "--strict") values.strict = true;
    else if (arg === "--allow-fail") values.allowFail = true;
    else if (arg === "--evidence") {
      const value = argv[index + 1];
      if (!value || value.startsWith("--")) fail("--evidence requires a value");
      values.evidence.push(value);
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

function fail(message) {
  console.error(`error: ${message}`);
  process.exit(1);
}

function latestGatePath(project) {
  const safe = String(project || "")
    .toLowerCase()
    .replace(/[^a-z0-9_-]+/g, "-")
    .replace(/^-+|-+$/g, "");
  return `gamedesign/projects/${safe}/reviews/product_read_gate_latest.json`;
}

function readJson(path) {
  return JSON.parse(readFileSync(resolve(path), "utf8"));
}

function taskboardSet(task, args) {
  const result = spawnSync(process.execPath, ["tools/taskboard/cli.mjs", "set", task, ...args], {
    cwd: process.cwd(),
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.status !== 0) {
    if (result.stdout) process.stdout.write(result.stdout);
    if (result.stderr) process.stderr.write(result.stderr);
    fail(`taskboard update failed for ${task}`);
  }
}

const values = parseArgs(process.argv.slice(2));
if (values.help) usage();
if (!values.task) fail("--task is required");
if (!values.project) fail("--project is required");
const gatePath = values.gate || latestGatePath(values.project);
if (!existsSync(resolve(gatePath))) fail(`gate JSON does not exist: ${gatePath}`);
if (values.evidence.length === 0) fail("--evidence is required");

const gate = readJson(gatePath);
if (values.strict && gate.verdict !== "pass" && !values.allowFail) {
  fail(`product gate is ${gate.verdict}; rerun with --allow-fail only for an explicit partial handoff`);
}

const log = [
  `close-slice ${gate.verdict.toUpperCase()} gate (${gate.surface || "surface"})`,
  `gate: ${gate.markdown || gatePath}`,
  `screenshot: ${gate.screenshot || "(missing)"}`,
  `evidence: ${values.evidence.join(" | ")}`,
  `next: ${values.next || gate.next || "(none)"}`,
].join("; ");

taskboardSet(values.task, ["--log", log]);
if (values.status) taskboardSet(values.task, ["--status", values.status]);

console.log("# Close Slice");
console.log(`Task: ${values.task}`);
console.log(`Gate: ${gate.verdict}`);
console.log(`Evidence: ${values.evidence.length}`);
console.log(`Next: ${values.next || gate.next || "(none)"}`);
