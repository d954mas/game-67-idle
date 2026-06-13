import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { parseArgs, stringArg } from "./profile_lib.mjs";

const allowed = {
  category: new Set([
    "context",
    "planning",
    "research",
    "implementation",
    "art",
    "asset_pipeline",
    "validation",
    "release",
    "task_status",
    "reflection",
    "tooling",
    "handoff",
  ]),
  result: new Set(["pass", "fail", "mixed", "blocked", "skipped", "unknown"]),
  value: new Set(["productive", "necessary_overhead", "rework", "waste", "unknown"]),
  context_risk: new Set(["low", "medium", "high", "unknown", ""]),
};

function usage() {
  console.error(`usage:
  node tools/ai_profile/summarize_session_profile.mjs <profile.jsonl> [--output <summary.md>]

If --output is provided, the summary is written to that file and still printed
to stdout. Put generated summaries under tmp/session_profiles/ by default.`);
  process.exit(2);
}

function addCount(map, key, count = 1) {
  const normalized = key || "(empty)";
  map.set(normalized, (map.get(normalized) || 0) + count);
}

function addDuration(map, key, duration) {
  const normalized = key || "(empty)";
  map.set(normalized, (map.get(normalized) || 0) + duration);
}

function formatMs(ms) {
  if (!Number.isFinite(ms) || ms <= 0) return "0s";
  const seconds = ms / 1000;
  if (seconds < 90) return `${seconds.toFixed(1)}s`;
  const minutes = seconds / 60;
  if (minutes < 90) return `${minutes.toFixed(1)}m`;
  return `${(minutes / 60).toFixed(2)}h`;
}

function topEntries(map, limit = 10) {
  return [...map.entries()].sort((a, b) => b[1] - a[1]).slice(0, limit);
}

const output = [];

function emit(line = "") {
  output.push(line);
}

function printMap(title, map, formatter = (value) => String(value)) {
  emit(`\n## ${title}`);
  const entries = topEntries(map);
  if (entries.length === 0) {
    emit("- none");
    return;
  }
  for (const [key, value] of entries) {
    emit(`- ${key}: ${formatter(value)}`);
  }
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help) usage();
const file = positionals[0];
if (!file) usage();
const outputFile = stringArg(values, "output", "");

const text = readFileSync(file, "utf8");
const lines = text.split(/\r?\n/);
const records = [];
const errors = [];

for (let index = 0; index < lines.length; index += 1) {
  const line = lines[index].trim();
  if (!line) continue;
  try {
    const record = JSON.parse(line);
    records.push({ ...record, __line: index + 1 });
  } catch (error) {
    errors.push(`line ${index + 1}: invalid JSON: ${error.message}`);
  }
}

for (const record of records) {
  for (const field of ["ts", "phase", "category", "intent", "result", "value"]) {
    if (!record[field]) {
      errors.push(`line ${record.__line}: missing required field ${field}`);
    }
  }
  if (record.category && !allowed.category.has(record.category)) {
    errors.push(`line ${record.__line}: unknown category ${record.category}`);
  }
  if (record.result && !allowed.result.has(record.result)) {
    errors.push(`line ${record.__line}: unknown result ${record.result}`);
  }
  if (record.value && !allowed.value.has(record.value)) {
    errors.push(`line ${record.__line}: unknown value ${record.value}`);
  }
  const contextRisk = record.context_risk || "";
  if (!allowed.context_risk.has(contextRisk)) {
    errors.push(`line ${record.__line}: unknown context_risk ${record.context_risk}`);
  }
  if (record.duration_ms !== undefined && (!Number.isFinite(record.duration_ms) || record.duration_ms < 0)) {
    errors.push(`line ${record.__line}: duration_ms must be a non-negative number`);
  }
}

if (errors.length > 0) {
  console.error(`profile validation failed for ${file}`);
  for (const error of errors) console.error(`- ${error}`);
  process.exit(1);
}

const durationByPhase = new Map();
const durationByCategory = new Map();
const durationByValue = new Map();
const countByResult = new Map();
const countByRisk = new Map();
const toolCounts = new Map();
const fileReads = new Map();
const fileWrites = new Map();
const evidenceCounts = new Map();
const contextChars = new Map();
const wasteRecords = [];
const blockers = [];
let totalDuration = 0;
let commandCount = 0;

for (const record of records) {
  const duration = Number(record.duration_ms || 0);
  totalDuration += duration;
  addDuration(durationByPhase, record.phase, duration);
  addDuration(durationByCategory, record.category, duration);
  addDuration(durationByValue, record.value, duration);
  addCount(countByResult, record.result);
  addCount(countByRisk, record.context_risk || "unknown");

  for (const tool of record.tools || []) addCount(toolCounts, tool);
  for (const command of record.commands || []) {
    commandCount += 1;
    if (record.result === "fail") addCount(toolCounts, "failed_command");
    void command;
  }
  for (const path of record.files_read || []) addCount(fileReads, path);
  for (const path of record.files_written || []) addCount(fileWrites, path);
  for (const path of record.evidence || []) addCount(evidenceCounts, path);
  for (const input of record.context_inputs || []) {
    addCount(contextChars, input.path || "(inline)", Number(input.chars || 0));
  }
  if (["waste", "rework"].includes(record.value) || record.waste_reason) {
    wasteRecords.push(record);
  }
  if (record.blocked_by || record.result === "blocked") {
    blockers.push(record);
  }
}

emit(`# AI Session Profile Summary - ${basename(file)}`);
emit(`\nRecords: ${records.length}`);
emit(`Profiled duration: ${formatMs(totalDuration)}`);
emit(`Commands recorded: ${commandCount}`);

printMap("Duration By Phase", durationByPhase, formatMs);
printMap("Duration By Category", durationByCategory, formatMs);
printMap("Duration By Value", durationByValue, formatMs);
printMap("Result Counts", countByResult);
printMap("Context Risk Counts", countByRisk);
printMap("Tool Counts", toolCounts);
printMap("Most Read Files", fileReads);
printMap("Most Written Files", fileWrites);
printMap("Context Input Chars", contextChars);
printMap("Evidence Paths", evidenceCounts);

emit("\n## Waste And Rework");
if (wasteRecords.length === 0) {
  emit("- none");
} else {
  for (const record of wasteRecords) {
    const reason = record.waste_reason || record.notes || "no reason recorded";
    emit(`- line ${record.__line} [${record.phase}/${record.category}/${record.value}]: ${record.intent} -> ${reason}`);
  }
}

emit("\n## Blockers");
if (blockers.length === 0) {
  emit("- none");
} else {
  for (const record of blockers) {
    emit(`- line ${record.__line} [${record.phase}]: ${record.blocked_by || record.intent}`);
  }
}

const rendered = `${output.join("\n")}\n`;
if (outputFile) {
  const target = resolve(outputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, rendered, "utf8");
}
process.stdout.write(rendered);
