#!/usr/bin/env node
import { existsSync } from "node:fs";
import { resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { findDoc, findRoot } from "../taskboard/lib.mjs";
import { fail } from "../lib/cli.mjs";
import { readJson } from "../lib/json.mjs";

function usage() {
  console.error(`usage:
  node tools/product_gate/close_slice.mjs --task <task-id> --project <game-id> --gate <gate.json> --evidence <text> [options]

Options:
  --evidence <text>      repeatable validation evidence line
  --next <text>          next action or handoff note
  --resolved-rejection <text>
                         required for strict close of lead-rejection tasks
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

// A single path-like token (no spaces, has a separator and an extension), so a
// command line passed as evidence ("node --test tools/x.mjs") is not mistaken
// for a file path.
function looksLikePath(value) {
  const v = String(value || "");
  return !/\s/.test(v) && /[\\/]/.test(v) && /\.[a-z0-9]{2,5}$/i.test(v);
}

function latestGatePath(project) {
  const safe = String(project || "")
    .toLowerCase()
    .replace(/[^a-z0-9_-]+/g, "-")
    .replace(/^-+|-+$/g, "");
  return `gamedesign/projects/${safe}/reviews/product_read_gate_latest.json`;
}

function tagsOf(doc) {
  return Array.isArray(doc?.fields?.tags) ? doc.fields.tags.map((tag) => String(tag).toLowerCase()) : [];
}

function taskHasLeadRejection(doc) {
  const tags = tagsOf(doc);
  const text = `${doc?.fields?.title || ""}\n${doc?.body || ""}`.toLowerCase();
  return tags.includes("lead-rejection") ||
    /\blead[- ]rejection\b/.test(text) ||
    /\blead[- ]rejected\b/.test(text) ||
    /\blead feedback\b/.test(text);
}

function resolvedRejectionText(values) {
  return String(values["resolved-rejection"] || "").trim();
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

const gate = readJson(resolve(gatePath));
if (values.strict && gate.verdict !== "pass" && !values.allowFail) {
  const hint = gate.verdict === "review"
    ? "lead must convert it to pass/fail, or rerun with --allow-fail only for an explicit partial handoff"
    : "rerun with --allow-fail only for an explicit partial handoff";
  fail(`product gate is ${gate.verdict}; ${hint}`);
}

const taskRoot = findRoot();
const taskDoc = findDoc(taskRoot, values.task);
if (!taskDoc) fail(`task not found: ${values.task}`);
const resolvedRejection = resolvedRejectionText(values);
if (taskHasLeadRejection(taskDoc) && !values.allowFail && resolvedRejection.length < 12) {
  fail(`task ${values.task} is lead-rejection work; strict close requires --resolved-rejection <evidence> naming the exact rejected issue and proof`);
}

// Evidence-as-arbiter: a real (non-partial) close must reference artifacts that
// actually exist on disk, so a green slice cannot point at a screenshot or
// evidence file that was never produced. --allow-fail is the explicit
// partial-handoff escape that waives this.
if (!values.allowFail) {
  const missing = [];
  if (gate.verdict === "pass" && !gate.screenshot) {
    missing.push("gate.screenshot (a passing slice must name a screenshot)");
  }
  if (gate.screenshot && !existsSync(resolve(gate.screenshot))) {
    missing.push(gate.screenshot);
  }
  for (const item of values.evidence) {
    if (looksLikePath(item) && !existsSync(resolve(item))) missing.push(item);
  }
  if (missing.length > 0) {
    fail(`close references artifacts that do not exist on disk: ${missing.join(", ")}. Create them, or use --allow-fail for an explicit partial handoff.`);
  }
}

const log = [
  `close-slice ${gate.verdict.toUpperCase()} gate (${gate.surface || "surface"})`,
  `gate: ${gate.markdown || gatePath}`,
  `screenshot: ${gate.screenshot || "(missing)"}`,
  `evidence: ${values.evidence.join(" | ")}`,
  resolvedRejection ? `resolved rejection: ${resolvedRejection}` : null,
  `next: ${values.next || gate.next || "(none)"}`,
].filter(Boolean).join("; ");

taskboardSet(values.task, ["--log", log]);
if (values.status) taskboardSet(values.task, ["--status", values.status]);

console.log("# Close Slice");
console.log(`Task: ${values.task}`);
console.log(`Gate: ${gate.verdict}`);
console.log(`Evidence: ${values.evidence.length}`);
console.log(`Next: ${values.next || gate.next || "(none)"}`);
