#!/usr/bin/env node
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { CATEGORIES, CONTEXT_RISKS, RESULTS, VALUES, parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/review.mjs <profile.jsonl> [--output <review.md>] [--json-output <review.json>]

Reads a session profile and produces reflection-ready findings: waste/rework,
failed commands, blocked records, context hotspots, repeated commands, and
missing telemetry. Keep generated reviews in tmp/session_profiles/ by default.`);
  process.exit(2);
}

function addCount(map, key, count = 1) {
  const normalized = key || "(empty)";
  map.set(normalized, (map.get(normalized) || 0) + count);
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

function mapEntries(map, limit = 10) {
  return topEntries(map, limit).map(([key, value]) => ({ key, value }));
}

function parseProfile(file) {
  const text = readFileSync(file, "utf8");
  const records = [];
  const errors = [];
  for (const [index, lineRaw] of text.split(/\r?\n/).entries()) {
    const line = lineRaw.trim();
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
      if (!record[field]) errors.push(`line ${record.__line}: missing required field ${field}`);
    }
    if (record.category && !CATEGORIES.has(record.category)) errors.push(`line ${record.__line}: unknown category ${record.category}`);
    if (record.result && !RESULTS.has(record.result)) errors.push(`line ${record.__line}: unknown result ${record.result}`);
    if (record.value && !VALUES.has(record.value)) errors.push(`line ${record.__line}: unknown value ${record.value}`);
    if (record.context_risk && !CONTEXT_RISKS.has(record.context_risk)) {
      errors.push(`line ${record.__line}: unknown context_risk ${record.context_risk}`);
    }
  }
  if (errors.length > 0) {
    throw new Error(errors.join("\n"));
  }
  return records;
}

function commandText(record) {
  return (record.commands || []).join(" && ");
}

function normalizeCommand(command) {
  return String(command || "").replaceAll("\\", "/").replace(/\s+/g, " ").trim();
}

function commandScope(command) {
  const normalized = normalizeCommand(command);
  if (!normalized) return "unknown";
  if (
    normalized === "git diff --check" ||
    normalized.startsWith("node --check ") ||
    normalized.startsWith("py -3.12 -m py_compile ") ||
    normalized.startsWith("python -m py_compile ")
  ) {
    return "preflight";
  }
  if (
    normalized === "node tools/pipeline_validate.mjs" ||
    normalized.includes("tools/release_candidate_audit.py") ||
    normalized.includes("tools/package_native_release.mjs")
  ) {
    return "broad/final";
  }
  if (
    normalized.includes("tools/taskboard/cli.mjs validate") ||
    normalized.includes("tools/taskboard/cli.mjs context") ||
    normalized.includes("tools/taskboard/test.mjs") ||
    normalized.includes("tools/skills_eval.mjs") ||
    normalized.includes("tools/skills_sync.mjs") ||
    normalized.includes("tools/ai_profile/") ||
    normalized.includes("tools/state_codegen/") ||
    normalized.startsWith("cmake --preset ") ||
    normalized.startsWith("cmake --build --preset native-debug")
  ) {
    return "scoped";
  }
  if (
    normalized.startsWith("cmake --build --preset native-release") ||
    normalized.includes("tools/devapi/scenarios/package_release_smoke.py")
  ) {
    return "broad/final";
  }
  if (
    normalized.includes("tools/devapi/scenarios/") ||
    normalized.includes("tools/devapi/smoke_test.py") ||
    normalized.includes("tools/devapi/full_probe.py") ||
    normalized.includes("tools/balance/")
  ) {
    return "scoped";
  }
  return "unknown";
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help) usage();
const file = positionals[0];
if (!file) usage();
const outputFile = stringArg(values, "output", "");
const jsonOutputFile = stringArg(values, "json-output", "");

let records;
try {
  records = parseProfile(file);
} catch (error) {
  console.error(`profile review failed for ${file}`);
  console.error(error.message);
  process.exit(1);
}

const output = [];
const emit = (line = "") => output.push(line);
const totalDuration = records.reduce((sum, record) => sum + Number(record.duration_ms || 0), 0);
const commandCounts = new Map();
const commandVariants = new Map();
const commandScopes = new Map();
const phaseDuration = new Map();
const contextChars = new Map();
const waste = [];
const failed = [];
const blocked = [];
const highContext = [];
const missingContextInputs = [];
let closeoutSeen = false;

for (const record of records) {
  addCount(phaseDuration, record.phase, Number(record.duration_ms || 0));
  for (const command of record.commands || []) {
    const normalized = normalizeCommand(command);
    addCount(commandCounts, normalized);
    if (!commandVariants.has(normalized)) commandVariants.set(normalized, new Set());
    commandVariants.get(normalized).add(command);
    commandScopes.set(normalized, commandScope(normalized));
  }
  for (const input of record.context_inputs || []) addCount(contextChars, input.path || "(inline)", Number(input.chars || 0));
  if (record.value === "waste" || record.value === "rework" || record.waste_reason) waste.push(record);
  if (record.result === "fail") failed.push(record);
  if (record.result === "blocked" || record.blocked_by) blocked.push(record);
  if (record.context_risk === "high") highContext.push(record);
  if (record.context_risk && record.context_risk !== "low" && !(record.context_inputs || []).length) missingContextInputs.push(record);
  if (record.phase === "session_closeout") closeoutSeen = true;
}

const repeatedCommands = topEntries(commandCounts, 20).filter(([, count]) => count > 1);
const repeatedScopeCounts = new Map();
for (const [command, count] of repeatedCommands) addCount(repeatedScopeCounts, commandScopes.get(command) || "unknown", count);
const repeatedBroadCommands = repeatedCommands.filter(([command]) => commandScopes.get(command) === "broad/final");
const topContext = topEntries(contextChars, 10).filter(([, chars]) => chars > 0);
const topPhaseDuration = topEntries(phaseDuration, 10);
const findings = [];
if (waste.length > 0) findings.push({ type: "waste_or_rework", message: `${waste.length} waste/rework record(s) need process fixes.` });
if (failed.length > 0) findings.push({ type: "failed_records", message: `${failed.length} failed command/event record(s) need failure analysis.` });
if (blocked.length > 0) findings.push({ type: "blocked_records", message: `${blocked.length} blocker record(s) need owner or decision.` });
if (highContext.length > 0) findings.push({ type: "high_context", message: `${highContext.length} high-context record(s) indicate context pressure.` });
if (repeatedCommands.length > 0) findings.push({ type: "repeated_commands", message: `${repeatedCommands.length} repeated command(s) may need batching or narrower gates.` });
if (repeatedBroadCommands.length > 0) {
  findings.push({
    type: "repeated_broad_final",
    message: `${repeatedBroadCommands.length} repeated broad/final command(s) are likely validation waste unless a gate failed or risk changed.`,
  });
}
if (missingContextInputs.length > 0) {
  findings.push({
    type: "missing_context_inputs",
    message: `${missingContextInputs.length} medium/high context record(s) lack context_inputs details.`,
  });
}
if (!closeoutSeen) findings.push({ type: "missing_closeout", message: "No session_closeout event: use closeout.mjs before final reflection." });
const actions = [];
if (waste.length > 0) actions.push("Convert recurring waste/rework reasons into a task, skill rule, validator, or batching rule.");
if (failed.length > 0) actions.push("For failed commands, decide whether the fix is code, environment, narrower validation, or better preflight.");
if (repeatedBroadCommands.length > 0) {
  actions.push(
    "Batch repeated broad/final validation or run `node tools/ai_profile/plan_validation.mjs --change <kind> --risk <risk>` before the next validation loop.",
  );
} else if (repeatedCommands.length > 0) {
  actions.push("Review repeated scoped/preflight commands; keep them only when they guard a fresh edit or failed gate.");
}
if (highContext.length > 0 || missingContextInputs.length > 0) actions.push("Compact source-of-truth docs or log explicit context_inputs for expensive reads.");
if (!closeoutSeen) actions.push("Run `node tools/ai_profile/closeout.mjs` at session end.");
if (actions.length === 0) actions.push("Use this clean profile as baseline; compare against the next real game iteration.");

emit(`# AI Profile Review - ${basename(file)}`);
emit(`\nRecords: ${records.length}`);
emit(`Profiled duration: ${formatMs(totalDuration)}`);
emit(`Closeout event: ${closeoutSeen ? "yes" : "no"}`);

emit("\n## Priority Findings");
if (findings.length === 0) {
  emit("- No obvious waste/rework/failure/context hotspots in this profile.");
} else {
  for (const finding of findings) emit(`- ${finding.message}`);
}

emit("\n## Waste And Rework To Explain");
if (waste.length === 0) {
  emit("- none");
} else {
  for (const record of waste.slice(0, 20)) {
    const reason = record.waste_reason || record.notes || "no reason recorded";
    emit(`- line ${record.__line} [${record.phase}/${record.category}/${record.value}]: ${record.intent} -> ${reason}`);
  }
}

emit("\n## Failed Or Blocked Records");
const failureLike = [...failed, ...blocked.filter((record) => !failed.includes(record))];
if (failureLike.length === 0) {
  emit("- none");
} else {
  for (const record of failureLike.slice(0, 20)) {
    const detail = record.blocked_by || record.command_error || commandText(record) || record.notes || "no detail recorded";
    emit(`- line ${record.__line} [${record.phase}/${record.result}]: ${record.intent} -> ${detail}`);
  }
}

emit("\n## Context Hotspots");
if (topContext.length === 0 && highContext.length === 0) {
  emit("- none");
} else {
  for (const [path, chars] of topContext) emit(`- ${path}: ${chars} chars`);
  for (const record of highContext.slice(0, 10)) {
    emit(`- line ${record.__line} high context: ${record.intent}`);
  }
}

emit("\n## Missing Context Input Details");
if (missingContextInputs.length === 0) {
  emit("- none");
} else {
  for (const record of missingContextInputs.slice(0, 20)) {
    emit(`- line ${record.__line} [${record.context_risk}]: ${record.intent}`);
  }
}

emit("\n## Repeated Commands");
if (repeatedCommands.length === 0) {
  emit("- none");
} else {
  for (const [command, count] of repeatedCommands) {
    emit(`- ${count}x [${commandScopes.get(command) || "unknown"}] ${command}`);
    const variants = [...(commandVariants.get(command) || [])].filter((variant) => variant !== command);
    if (variants.length > 0) {
      emit(`  - variants: ${variants.slice(0, 3).join(" | ")}${variants.length > 3 ? " | ..." : ""}`);
    }
  }
}

emit("\n## Repeated Commands By Scope");
if (repeatedCommands.length === 0) {
  emit("- none");
} else {
  for (const [scope, count] of topEntries(repeatedScopeCounts, 10)) emit(`- ${scope}: ${count}`);
}

emit("\n## Repeated Broad/Final Commands");
if (repeatedBroadCommands.length === 0) {
  emit("- none");
} else {
  for (const [command, count] of repeatedBroadCommands) emit(`- ${count}x ${command}`);
}

emit("\n## Time By Phase");
if (topPhaseDuration.length === 0) {
  emit("- none");
} else {
  for (const [phase, duration] of topPhaseDuration) emit(`- ${phase}: ${formatMs(duration)}`);
}

emit("\n## Suggested Pipeline Actions");
for (const action of actions) emit(`- ${action}`);

const rendered = `${output.join("\n")}\n`;
if (outputFile) {
  const target = resolve(outputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, rendered, "utf8");
}
if (jsonOutputFile) {
  const repeatedCommandObjects = repeatedCommands.map(([command, count]) => ({
    command,
    count,
    scope: commandScopes.get(command) || "unknown",
    variants: [...(commandVariants.get(command) || [])],
  }));
  const reviewJson = {
    schema_version: 1,
    profile: file,
    records: records.length,
    profiled_duration_ms: totalDuration,
    closeout_seen: closeoutSeen,
    findings,
    waste_or_rework: waste.map((record) => ({
      line: record.__line,
      phase: record.phase,
      category: record.category,
      value: record.value,
      intent: record.intent,
      reason: record.waste_reason || record.notes || "",
    })),
    failed_or_blocked: failureLike.map((record) => ({
      line: record.__line,
      phase: record.phase,
      result: record.result,
      intent: record.intent,
      detail: record.blocked_by || record.command_error || commandText(record) || record.notes || "",
    })),
    context_hotspots: topContext.map(([path, chars]) => ({ path, chars })),
    high_context: highContext.map((record) => ({ line: record.__line, intent: record.intent, context_risk: record.context_risk })),
    missing_context_inputs: missingContextInputs.map((record) => ({
      line: record.__line,
      intent: record.intent,
      context_risk: record.context_risk,
    })),
    repeated_commands: repeatedCommandObjects,
    repeated_commands_by_scope: mapEntries(repeatedScopeCounts, 10).map(({ key, value }) => ({ scope: key, count: value })),
    repeated_broad_final_commands: repeatedCommandObjects.filter((item) => item.scope === "broad/final"),
    time_by_phase: topPhaseDuration.map(([phase, duration_ms]) => ({ phase, duration_ms })),
    suggested_pipeline_actions: actions,
  };
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(reviewJson, null, 2)}\n`, "utf8");
}
process.stdout.write(rendered);
