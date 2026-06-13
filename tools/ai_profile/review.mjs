#!/usr/bin/env node
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { CATEGORIES, CONTEXT_RISKS, RESULTS, VALUES, parseArgs, readProfileScope, stringArg } from "./profile_lib.mjs";

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

function formatPercent(value) {
  if (!Number.isFinite(value)) return "unknown";
  return `${(value * 100).toFixed(1)}%`;
}

function topEntries(map, limit = 10) {
  return [...map.entries()].sort((a, b) => b[1] - a[1]).slice(0, limit);
}

function mapEntries(map, limit = 10) {
  return topEntries(map, limit).map(([key, value]) => ({ key, value }));
}

function segmentCommandKey(segment, command) {
  return `${segment}\u0000${command}`;
}

function splitSegmentCommandKey(key) {
  const separator = key.indexOf("\u0000");
  if (separator === -1) return { segment: key, command: "" };
  return { segment: key.slice(0, separator), command: key.slice(separator + 1) };
}

function repeatedSegmentCommands(map, limit = 20) {
  return topEntries(map, limit)
    .filter(([, count]) => count > 1)
    .map(([key, count]) => ({ ...splitSegmentCommandKey(key), count }));
}

function eventTime(record) {
  const parsed = Date.parse(record.ts || "");
  return Number.isFinite(parsed) ? parsed : undefined;
}

function profileIntervals(records) {
  const intervals = [];
  for (const record of records) {
    const endMs = eventTime(record);
    if (endMs === undefined) continue;
    const durationMs = Math.max(0, Number(record.duration_ms || 0));
    const startMs = endMs - durationMs;
    intervals.push({ start_ms: startMs, end_ms: endMs, duration_ms: durationMs, line: record.__line, intent: record.intent });
  }
  intervals.sort((a, b) => a.start_ms - b.start_ms || a.end_ms - b.end_ms);
  return intervals;
}

function coverageStats(records, gapThresholdMs = 5 * 60 * 1000) {
  const intervals = profileIntervals(records);
  if (intervals.length === 0) {
    return {
      wall_clock_span_ms: 0,
      merged_profiled_ms: 0,
      coverage_ratio: undefined,
      largest_gaps: [],
      first_record_ts: "",
      last_record_ts: "",
    };
  }

  const merged = [];
  for (const interval of intervals) {
    const last = merged[merged.length - 1];
    if (!last || interval.start_ms > last.end_ms) {
      merged.push({ ...interval });
      continue;
    }
    if (interval.end_ms > last.end_ms) last.end_ms = interval.end_ms;
  }

  const firstMs = intervals[0].start_ms;
  const lastMs = intervals.reduce((max, interval) => Math.max(max, interval.end_ms), intervals[0].end_ms);
  const wallClockSpanMs = Math.max(0, lastMs - firstMs);
  const mergedProfiledMs = merged.reduce((sum, interval) => sum + Math.max(0, interval.end_ms - interval.start_ms), 0);
  const largestGaps = [];
  for (let index = 1; index < merged.length; index += 1) {
    const previous = merged[index - 1];
    const current = merged[index];
    const durationMs = current.start_ms - previous.end_ms;
    if (durationMs >= gapThresholdMs) {
      largestGaps.push({
        start_ts: new Date(previous.end_ms).toISOString(),
        end_ts: new Date(current.start_ms).toISOString(),
        duration_ms: durationMs,
      });
    }
  }
  largestGaps.sort((a, b) => b.duration_ms - a.duration_ms);

  return {
    wall_clock_span_ms: wallClockSpanMs,
    merged_profiled_ms: mergedProfiledMs,
    coverage_ratio: wallClockSpanMs > 0 ? Math.min(1, mergedProfiledMs / wallClockSpanMs) : undefined,
    largest_gaps: largestGaps.slice(0, 10),
    first_record_ts: new Date(firstMs).toISOString(),
    last_record_ts: new Date(lastMs).toISOString(),
  };
}

function currentScopeRecords(records, scope) {
  const updatedMs = Date.parse(scope.updated_at || "");
  if (!Number.isFinite(updatedMs)) return [];
  return records.filter((record) => {
    const ts = eventTime(record);
    return ts !== undefined && ts >= updatedMs;
  });
}

function isLowCoverage(coverage) {
  return coverage.wall_clock_span_ms >= 30 * 60 * 1000 && Number.isFinite(coverage.coverage_ratio) && coverage.coverage_ratio < 0.25;
}

function contextUseSummary(records) {
  const chars = new Map();
  const high = [];
  const missing = [];
  for (const record of records) {
    for (const input of record.context_inputs || []) addCount(chars, input.path || "(inline)", Number(input.chars || 0));
    if (record.context_risk === "high") high.push(record);
    if (record.context_risk && record.context_risk !== "low" && !(record.context_inputs || []).length) missing.push(record);
  }
  return {
    hotspots: topEntries(chars, 8).filter(([, count]) => count > 0).map(([path, chars]) => ({ path, chars })),
    high_context: high.slice(0, 8).map((record) => ({
      line: record.__line,
      intent: record.intent || "",
      context_risk: record.context_risk || "",
    })),
    missing_inputs: missing.slice(0, 8).map((record) => ({
      line: record.__line,
      intent: record.intent || "",
      context_risk: record.context_risk || "",
    })),
  };
}

function scopedSummary(records) {
  const broadCommandCounts = new Map();
  const unbatchedBroadCommandCounts = new Map();
  for (const record of records) {
    for (const command of record.commands || []) {
      const normalized = normalizeCommand(command);
      if (commandScope(normalized) === "broad/final") {
        addCount(broadCommandCounts, normalized);
        if (!record.validation_batch_id) addCount(unbatchedBroadCommandCounts, normalized);
      }
    }
  }
  const repeatedBroadFinalCommands = topEntries(broadCommandCounts, 20)
    .filter(([, count]) => count > 1)
    .map(([command, count]) => ({ command, count, scope: "broad/final" }));
  const repeatedUnbatchedBroadFinalCommands = topEntries(unbatchedBroadCommandCounts, 20)
    .filter(([, count]) => count > 1)
    .map(([command, count]) => ({ command, count, scope: "broad/final" }));
  const coverage = coverageStats(records);
  const failedClassification = classifyFailedRecords(records);
  const recoveredFailureClassification = classifyRecoveredFailures(failedClassification.recovered);
  const currentContext = contextUseSummary(records);
  return {
    records: records.length,
    missing_context_inputs: records.filter((record) => record.context_risk && record.context_risk !== "low" && !(record.context_inputs || []).length).length,
    missing_work_item_records: records.filter((record) => !record.work_item).length,
    missing_tool_records: records.filter((record) => !Array.isArray(record.tools) || record.tools.length === 0).length,
    tool_use_summary: toolUseSummary(records).slice(0, 8),
    context_use_summary: currentContext,
    repeated_broad_final_commands: repeatedBroadFinalCommands,
    repeated_unbatched_broad_final_commands: repeatedUnbatchedBroadFinalCommands,
    recovered_failed_records: failedClassification.recovered.map((item) => ({
      line: item.record.__line,
      phase: item.record.phase,
      intent: item.record.intent,
      detail: commandText(item.record) || item.record.command_error || item.record.notes || "",
      recovered_by_line: item.recovered_by_line,
      recovered_by_intent: item.recovered_by_intent,
    })),
    recovered_failure_classification: recoveredFailureClassification,
    unresolved_failed_records: failedClassification.unresolved.map((record) => ({
      line: record.__line,
      phase: record.phase,
      intent: record.intent,
      detail: commandText(record) || record.command_error || record.notes || "",
    })),
    wall_clock_coverage: coverage,
    low_profile_coverage: isLowCoverage(coverage),
  };
}

function validationBatches(records) {
  const batches = new Map();
  for (const record of records) {
    if (!record.validation_batch_id) continue;
    const id = String(record.validation_batch_id);
    if (!batches.has(id)) {
      batches.set(id, {
        batch_id: id,
        work_item: record.work_item || "",
        iteration: record.iteration || "",
        risk: record.validation_plan_risk || "",
        changes: Array.isArray(record.validation_plan_changes) ? record.validation_plan_changes : [],
        records: 0,
        duration_ms: 0,
        failed: 0,
        commands: 0,
        broad_final_commands: 0,
        tiers: new Map(),
      });
    }
    const batch = batches.get(id);
    batch.records += 1;
    batch.duration_ms += Number(record.duration_ms || 0);
    if (record.result === "fail") batch.failed += 1;
    const tier = record.validation_tier || "unknown";
    addCount(batch.tiers, tier);
    for (const command of record.commands || []) {
      batch.commands += 1;
      if (commandScope(command) === "broad/final") batch.broad_final_commands += 1;
    }
  }
  return [...batches.values()]
    .sort((a, b) => b.records - a.records || b.duration_ms - a.duration_ms)
    .map((batch) => ({
      ...batch,
      tiers: mapEntries(batch.tiers, 10).map(({ key, value }) => ({ tier: key, count: value })),
    }));
}

function toolUseSummary(records) {
  const tools = new Map();
  for (const record of records) {
    const duration = Number(record.duration_ms || 0);
    const listedTools = Array.isArray(record.tools) && record.tools.length > 0 ? record.tools : ["(unrecorded)"];
    for (const tool of listedTools) {
      const name = String(tool || "(empty)");
      if (!tools.has(name)) {
        tools.set(name, {
          tool: name,
          records: 0,
          duration_ms: 0,
          failed: 0,
          blocked: 0,
          waste_or_rework: 0,
          contexts: 0,
          commands: 0,
        });
      }
      const item = tools.get(name);
      item.records += 1;
      item.duration_ms += duration;
      if (record.result === "fail") item.failed += 1;
      if (record.result === "blocked" || record.blocked_by) item.blocked += 1;
      if (record.value === "waste" || record.value === "rework" || record.waste_reason) item.waste_or_rework += 1;
      if (record.category === "context" || (record.context_inputs || []).length > 0) item.contexts += 1;
      item.commands += (record.commands || []).length;
    }
  }
  return [...tools.values()].sort((a, b) => b.duration_ms - a.duration_ms || b.records - a.records).slice(0, 20);
}

function classifyRepeatedCommands(records, repeatedCommands, commandScopes) {
  const stats = new Map();
  for (const record of records) {
    for (const command of record.commands || []) {
      const normalized = normalizeCommand(command);
      if (!normalized) continue;
      if (!stats.has(normalized)) {
        stats.set(normalized, {
          command: normalized,
          count: 0,
          scope: commandScopes.get(normalized) || commandScope(normalized),
          batched: 0,
          unbatched: 0,
          failed: 0,
          passed: 0,
          work_items: new Map(),
          iterations: new Map(),
          first_line: record.__line,
          last_line: record.__line,
        });
      }
      const item = stats.get(normalized);
      item.count += 1;
      if (record.validation_batch_id) item.batched += 1;
      else item.unbatched += 1;
      if (record.result === "fail") item.failed += 1;
      if (record.result === "pass") item.passed += 1;
      addCount(item.work_items, record.work_item || "(none)");
      addCount(item.iterations, record.iteration || "(none)");
      item.first_line = Math.min(item.first_line, record.__line);
      item.last_line = Math.max(item.last_line, record.__line);
    }
  }

  return repeatedCommands.map(([command, count]) => {
    const item = stats.get(command) || { command, count, scope: commandScopes.get(command) || "unknown", batched: 0, unbatched: count, failed: 0, passed: 0, work_items: new Map(), iterations: new Map(), first_line: 0, last_line: 0 };
    let classification = "needs_manual_classification";
    let reason = "Command scope is unknown or lacks enough metadata to classify safely.";
    let next_action = "Inspect surrounding profile records before turning this repeat into a process task.";
    if (item.scope === "broad/final" && item.unbatched > 1) {
      classification = "validation_waste_risk";
      reason = "Broad/final command repeated outside a validation batch.";
      next_action = "Use `node tools/ai.mjs validate --change <kind> --risk <risk>` and rerun broad/final gates only after a failed gate, changed risk, or final handoff.";
    } else if (item.batched > 0 && item.unbatched === 0) {
      classification = "planned_validation";
      reason = "All repeats are inside profiled validation batches.";
      next_action = "Keep as planned validation evidence; do not promote as waste.";
    } else if (item.scope === "broad/final" && item.batched > 0) {
      classification = "mostly_planned_validation";
      reason = "Broad/final repeats are partly batched, with at most one unbatched occurrence.";
      next_action = "Keep batched runs as evidence and inspect the unbatched occurrence only if it recurs.";
    } else if (item.failed > 0) {
      classification = "failure_recovery_or_rework";
      reason = "Repeated command includes failed attempts.";
      next_action = "Classify the failure as useful negative feedback, avoidable rework, or tool noise.";
    } else if (item.scope === "preflight" || item.scope === "scoped") {
      classification = "guardrail_rerun_review";
      reason = "Scoped/preflight repeats can be valid guardrails when they follow fresh edits or failed gates.";
      next_action = "Keep the repeat only when it guards a fresh edit or failed gate; otherwise batch it with nearby checks.";
    }
    return {
      command: item.command,
      count,
      scope: item.scope,
      classification,
      reason,
      next_action,
      batched: item.batched,
      unbatched: item.unbatched,
      failed: item.failed,
      passed: item.passed,
      work_items: mapEntries(item.work_items, 5).map(({ key, value }) => ({ work_item: key, count: value })),
      iterations: mapEntries(item.iterations, 5).map(({ key, value }) => ({ iteration: key, count: value })),
      first_line: item.first_line,
      last_line: item.last_line,
    };
  });
}

function currentScopeFindingsAndActions(currentScope) {
  const findings = [];
  const actions = [];
  if (!currentScope.enabled) {
    findings.push({ type: "missing_current_scope", message: "No current profile scope is active; review findings are whole-profile history." });
    actions.push("Start a focused scope with `node tools/ai.mjs start <work-item> <iteration>` before using review findings as current tasks.");
    return { findings, actions };
  }
  if (currentScope.records === 0) {
    findings.push({ type: "empty_current_scope", message: "Current scope has no records yet." });
    actions.push("Append a current-scope checkpoint, context record, or profiled command before relying on review for current work.");
    return { findings, actions };
  }
  if (currentScope.unresolved_failed_records.length > 0) {
    findings.push({ type: "current_unresolved_failed_records", message: `${currentScope.unresolved_failed_records.length} current-scope failed record(s) are unresolved.` });
    actions.push("Resolve or explain current-scope failed records before treating the profile as clean.");
  }
  if (currentScope.recovered_failed_records.length > 0) {
    findings.push({ type: "current_recovered_failed_records", message: `${currentScope.recovered_failed_records.length} current-scope failed record(s) later passed.` });
    actions.push("Classify current-scope recovered failures as useful negative feedback, avoidable rework, or tool noise.");
  }
  if (currentScope.repeated_unbatched_broad_final_commands.length > 0) {
    findings.push({ type: "current_repeated_broad_final", message: `${currentScope.repeated_unbatched_broad_final_commands.length} current-scope unbatched broad/final command(s) repeated.` });
    actions.push("Use `node tools/ai.mjs validate --change <kind> --risk <risk>` for current-scope broad/final gates.");
  }
  if (currentScope.missing_context_inputs > 0) {
    findings.push({ type: "current_missing_context_inputs", message: `${currentScope.missing_context_inputs} current-scope medium/high context record(s) lack context_inputs.` });
    actions.push("Use `node tools/ai.mjs context --path <file>` or `node tools/ai.mjs context -- <command>` for current-scope medium/high context reads.");
  }
  if (currentScope.missing_work_item_records > 0) {
    findings.push({ type: "current_missing_work_item_metadata", message: `${currentScope.missing_work_item_records} current-scope record(s) lack work_item metadata.` });
    actions.push("Use `node tools/ai.mjs focus <iteration>` for a new slice inside the current work item, or `node tools/ai.mjs start <work-item> <iteration>` for a new work item.");
  }
  if (currentScope.missing_tool_records > 0) {
    findings.push({ type: "current_missing_tool_metadata", message: `${currentScope.missing_tool_records} current-scope record(s) lack tools metadata.` });
    actions.push("Use `node tools/ai.mjs run/context/checkpoint/validate` or profiler wrappers that fill `tools` automatically; avoid manual profile events without `tools`.");
  }
  if (currentScope.low_profile_coverage) {
    findings.push({ type: "current_low_profile_coverage", message: `Current scope covers ${formatPercent(currentScope.wall_clock_coverage.coverage_ratio)} of a ${formatMs(currentScope.wall_clock_coverage.wall_clock_span_ms)} span.` });
    actions.push("Add node tools/ai.mjs checkpoint records during long current-scope manual/research/design stretches.");
  }
  if (findings.length === 0) {
    actions.push("Use current scope as clean baseline; treat whole-profile findings as historical retrospective context.");
  }
  return { findings, actions };
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

function commandKeys(record) {
  return (record.commands || []).map(normalizeCommand).filter(Boolean);
}

function classifyFailedRecords(records) {
  const passedLater = new Map();
  const recovered = [];
  const unresolved = [];
  for (const record of [...records].sort((a, b) => b.__line - a.__line)) {
    const keys = commandKeys(record);
    if (record.result === "pass") {
      for (const key of keys) passedLater.set(key, record);
      continue;
    }
    if (record.result !== "fail") continue;
    const recoveredBy = keys.map((key) => passedLater.get(key)).find(Boolean);
    if (recoveredBy) {
      recovered.push({ record, recovered_by_line: recoveredBy.__line, recovered_by_intent: recoveredBy.intent || "" });
    } else {
      unresolved.push(record);
    }
  }
  recovered.sort((a, b) => a.record.__line - b.record.__line);
  unresolved.sort((a, b) => a.__line - b.__line);
  return { recovered, unresolved };
}

function classifyRecoveredFailures(recoveredFailures) {
  return recoveredFailures.map((item) => {
    const record = item.record;
    const detail = [
      commandText(record),
      record.command_error || "",
      record.waste_reason || "",
      record.blocked_by || "",
      record.notes || "",
    ].filter(Boolean).join(" ").toLowerCase();
    const scope = commandScope(commandText(record));
    let classification = "useful_negative_feedback";
    let reason = "A failed check later passed and has no explicit waste/rework marker, so treat it as validation feedback unless review proves otherwise.";
    let next_action = "Keep as learning evidence; only create a rule or preflight if the same class of failure recurs.";
    if (record.value === "waste" || record.value === "rework" || record.waste_reason) {
      classification = "avoidable_rework";
      reason = "The failed record was marked as waste/rework or includes a waste reason.";
      next_action = "Convert the cause into a rule, preflight, narrower validation gate, or implementation checklist.";
    } else if (/(timeout|timed out|flaky|econn|network|dns|eacces|eperm|permission|locked|busy|spawn|enoent)/i.test(detail)) {
      classification = "tool_or_environment_noise";
      reason = "The failure detail looks like tool, filesystem, environment, or transient infrastructure noise.";
      next_action = "Stabilize the tool/environment or mark as noise; do not turn it into a product-process rule without recurrence.";
    } else if (record.validation_batch_id) {
      classification = "planned_validation_feedback";
      reason = "The failure happened inside a profiled validation batch and later passed.";
      next_action = "Keep with validation batch evidence; promote only recurring failures to process changes.";
    }
    return {
      line: record.__line,
      recovered_by_line: item.recovered_by_line,
      intent: record.intent || "",
      recovered_by_intent: item.recovered_by_intent || "",
      command: commandText(record),
      scope,
      classification,
      reason,
      next_action,
      value: record.value || "",
      waste_reason: record.waste_reason || "",
    };
  });
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
    normalized.includes("tools/project_67_world/release_candidate_audit.py") ||
    normalized.includes("tools/project_67_world/package_native_release.mjs")
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
    normalized.includes("tools/project_67_world/devapi_scenarios/package_release_smoke.py")
  ) {
    return "broad/final";
  }
  if (
    normalized.includes("tools/project_67_world/devapi_scenarios/") ||
    normalized.includes("tools/devapi/smoke_test.py") ||
    normalized.includes("tools/devapi/full_probe.py") ||
    normalized.includes("tools/project_67_world/balance/")
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
const workItemCounts = new Map();
const workItemDuration = new Map();
const iterationCounts = new Map();
const iterationDuration = new Map();
const workItemBroadCommandCounts = new Map();
const unbatchedBroadCommandCounts = new Map();
const batchedBroadCommandCounts = new Map();
const contextChars = new Map();
const waste = [];
const failed = [];
const blocked = [];
const highContext = [];
const missingContextInputs = [];
const missingToolRecords = [];
let closeoutSeen = false;

for (const record of records) {
  const duration = Number(record.duration_ms || 0);
  const workItem = record.work_item || "(none)";
  const iteration = record.iteration || "(none)";
  addCount(phaseDuration, record.phase, duration);
  addCount(workItemCounts, workItem);
  addCount(workItemDuration, workItem, duration);
  addCount(iterationCounts, iteration);
  addCount(iterationDuration, iteration, duration);
  for (const command of record.commands || []) {
    const normalized = normalizeCommand(command);
    addCount(commandCounts, normalized);
    if (!commandVariants.has(normalized)) commandVariants.set(normalized, new Set());
    commandVariants.get(normalized).add(command);
    const scope = commandScope(normalized);
    commandScopes.set(normalized, scope);
    if (scope === "broad/final") {
      addCount(workItemBroadCommandCounts, segmentCommandKey(workItem, normalized));
      if (record.validation_batch_id) {
        addCount(batchedBroadCommandCounts, normalized);
      } else {
        addCount(unbatchedBroadCommandCounts, normalized);
      }
    }
  }
  for (const input of record.context_inputs || []) addCount(contextChars, input.path || "(inline)", Number(input.chars || 0));
  if (record.value === "waste" || record.value === "rework" || record.waste_reason) waste.push(record);
  if (record.result === "fail") failed.push(record);
  if (record.result === "blocked" || record.blocked_by) blocked.push(record);
  if (record.context_risk === "high") highContext.push(record);
  if (record.context_risk && record.context_risk !== "low" && !(record.context_inputs || []).length) missingContextInputs.push(record);
  if (!Array.isArray(record.tools) || record.tools.length === 0) missingToolRecords.push(record);
  if (record.phase === "session_closeout") closeoutSeen = true;
}

const repeatedCommands = topEntries(commandCounts, 20).filter(([, count]) => count > 1);
const repeatedScopeCounts = new Map();
for (const [command, count] of repeatedCommands) addCount(repeatedScopeCounts, commandScopes.get(command) || "unknown", count);
const repeatedBroadCommands = repeatedCommands.filter(([command]) => commandScopes.get(command) === "broad/final");
const repeatedUnbatchedBroadCommands = topEntries(unbatchedBroadCommandCounts, 20).filter(([, count]) => count > 1);
const repeatedUnbatchedBroadOccurrences = repeatedUnbatchedBroadCommands.reduce((sum, [, count]) => sum + count, 0);
const batchedBroadCommands = topEntries(batchedBroadCommandCounts, 20);
const repeatedBroadByWorkItem = repeatedSegmentCommands(workItemBroadCommandCounts, 20);
const validationBatchSummaries = validationBatches(records);
const repeatedCommandClassifications = classifyRepeatedCommands(records, repeatedCommands, commandScopes);
const toolSummary = toolUseSummary(records);
const topContext = topEntries(contextChars, 10).filter(([, chars]) => chars > 0);
const topPhaseDuration = topEntries(phaseDuration, 10);
const coverage = coverageStats(records);
const topWorkItems = topEntries(workItemCounts, 20).map(([work_item, count]) => ({
  work_item,
  records: count,
  duration_ms: workItemDuration.get(work_item) || 0,
}));
const topIterations = topEntries(iterationCounts, 20).map(([iteration, count]) => ({
  iteration,
  records: count,
  duration_ms: iterationDuration.get(iteration) || 0,
}));
const missingWorkItemCount = records.filter((record) => !record.work_item).length;
const scope = readProfileScope();
const scopeReady = scope.valid && Boolean(scope.work_item) && Boolean(scope.updated_at);
const scopedRecords = scopeReady ? currentScopeRecords(records, scope) : [];
const currentScope = {
  enabled: scopeReady,
  work_item: scope.work_item || "",
  iteration: scope.iteration || "",
  since: scope.updated_at || "",
  ...scopedSummary(scopedRecords),
};
const currentScopeReview = currentScopeFindingsAndActions(currentScope);
currentScope.findings = currentScopeReview.findings;
currentScope.suggested_actions = currentScopeReview.actions;
const findings = [];
const failedClassification = classifyFailedRecords(records);
const recoveredFailed = failedClassification.recovered;
const unresolvedFailed = failedClassification.unresolved;
const recoveredFailureClassification = classifyRecoveredFailures(recoveredFailed);
if (waste.length > 0) findings.push({ type: "waste_or_rework", message: `${waste.length} waste/rework record(s) need process fixes.` });
if (unresolvedFailed.length > 0) findings.push({ type: "failed_records", message: `${unresolvedFailed.length} unresolved failed command/event record(s) need failure analysis.` });
if (recoveredFailed.length > 0) findings.push({ type: "recovered_failed_records", message: `${recoveredFailed.length} failed record(s) later passed and should be classified as recovered rework.` });
if (blocked.length > 0) findings.push({ type: "blocked_records", message: `${blocked.length} blocker record(s) need owner or decision.` });
if (highContext.length > 0) findings.push({ type: "high_context", message: `${highContext.length} high-context record(s) indicate context pressure.` });
if (repeatedCommands.length > 0) findings.push({ type: "repeated_commands", message: `${repeatedCommands.length} repeated command(s) may need batching or narrower gates.` });
if (repeatedUnbatchedBroadCommands.length > 0) {
  findings.push({
    type: "repeated_broad_final",
    message: `${repeatedUnbatchedBroadOccurrences} unbatched broad/final occurrence(s) across ${repeatedUnbatchedBroadCommands.length} repeated command(s) are likely validation waste unless a gate failed or risk changed.`,
  });
}
if (missingContextInputs.length > 0) {
  findings.push({
    type: "missing_context_inputs",
    message: `${missingContextInputs.length} medium/high context record(s) lack context_inputs details.`,
  });
}
if (records.length >= 20 && missingWorkItemCount > 0) {
  findings.push({
    type: "missing_work_item_metadata",
    message: `${missingWorkItemCount} record(s) lack work_item metadata; multi-task profiles are harder to analyze.`,
  });
}
if (records.length >= 20 && missingToolRecords.length > 0) {
  findings.push({
    type: "missing_tool_metadata",
    message: `${missingToolRecords.length} record(s) lack tools metadata; tool-use analysis is incomplete.`,
  });
}
const lowCoverage = coverage.wall_clock_span_ms >= 30 * 60 * 1000 && Number.isFinite(coverage.coverage_ratio) && coverage.coverage_ratio < 0.25;
if (lowCoverage) {
  findings.push({
    type: "low_profile_coverage",
    message: `Profile covers ${formatPercent(coverage.coverage_ratio)} of a ${formatMs(coverage.wall_clock_span_ms)} wall-clock span; large unprofiled gaps need explanation.`,
  });
}
if (!closeoutSeen) findings.push({ type: "missing_closeout", message: "No session_closeout event: use closeout.mjs before final reflection." });
const actions = [];
if (waste.length > 0) actions.push("Convert recurring waste/rework reasons into a task, skill rule, validator, or batching rule.");
if (unresolvedFailed.length > 0) actions.push("For unresolved failed commands, decide whether the fix is code, environment, narrower validation, or better preflight.");
if (recoveredFailed.length > 0) actions.push("Use recovered_failure_classification to separate useful validation feedback, avoidable rework, and tool/environment noise.");
if (repeatedUnbatchedBroadCommands.length > 0) {
  actions.push(
    "Batch repeated broad/final validation with `node tools/ai.mjs validate --change <kind> --risk <risk>` before the next validation loop.",
  );
} else if (repeatedCommands.length > 0) {
  actions.push("Review repeated scoped/preflight commands; keep them only when they guard a fresh edit or failed gate.");
}
if (highContext.length > 0 || missingContextInputs.length > 0) actions.push("Compact source-of-truth docs or log explicit context_inputs for expensive reads.");
if (records.length >= 20 && missingWorkItemCount > 0) actions.push("For long or multi-task profiles, start with `node tools/ai.mjs start <work-item> <iteration>` and use `node tools/ai.mjs focus <iteration>` for later slices.");
if (records.length >= 20 && missingToolRecords.length > 0) actions.push("Use `node tools/ai.mjs run/context/checkpoint/validate` or profiler wrappers that populate `tools`; avoid manual profile records without tool metadata.");
if (lowCoverage) actions.push("Explain low wall-clock profile coverage in the retrospective, or add `node tools/ai.mjs checkpoint \"<intent>\"` records during long manual/research/design stretches.");
if (coverage.largest_gaps.length > 0) actions.push("Explain the largest profile gaps in the retrospective, or add `node tools/ai.mjs checkpoint \"<intent>\"` records during long idle/manual stretches.");
if (!closeoutSeen) actions.push("Run `node tools/ai_profile/closeout.mjs` at session end.");
if (actions.length === 0) actions.push("Use this clean profile as baseline; compare against the next real game iteration.");

emit(`# AI Profile Review - ${basename(file)}`);
emit(`\nRecords: ${records.length}`);
emit(`Profiled duration: ${formatMs(totalDuration)}`);
emit(`Closeout event: ${closeoutSeen ? "yes" : "no"}`);

emit("\n## Current Scope Snapshot");
if (!currentScope.enabled) {
  emit("- none");
} else {
  const recoveredCount = currentScope.recovered_failed_records.length;
  const unresolvedCount = currentScope.unresolved_failed_records.length;
  emit(`- scope: ${currentScope.work_item}${currentScope.iteration ? `/${currentScope.iteration}` : ""}`);
  emit(`- records: ${currentScope.records}`);
  emit(`- profiled/wall-clock: ${formatMs(currentScope.wall_clock_coverage.merged_profiled_ms)} / ${formatMs(currentScope.wall_clock_coverage.wall_clock_span_ms)} (${formatPercent(currentScope.wall_clock_coverage.coverage_ratio)})`);
  emit(`- telemetry gaps: context=${currentScope.missing_context_inputs}, work_item=${currentScope.missing_work_item_records}, tools=${currentScope.missing_tool_records}`);
  emit(`- failures: unresolved=${unresolvedCount}, recovered=${recoveredCount}`);
}

emit("\n## Current Scope Findings");
if (currentScope.findings.length === 0) {
  emit("- Current scope has no urgent review findings.");
} else {
  for (const finding of currentScope.findings) emit(`- ${finding.message}`);
}

emit("\n## Current Scope Actions");
for (const action of currentScope.suggested_actions) emit(`- ${action}`);

emit("\n## Current Scope Tool Use");
if (!currentScope.enabled || currentScope.records === 0 || currentScope.tool_use_summary.length === 0) {
  emit("- none");
} else {
  for (const item of currentScope.tool_use_summary) {
    emit(`- ${item.tool}: ${item.records} record(s), ${formatMs(item.duration_ms)}, failed=${item.failed}, waste/rework=${item.waste_or_rework}, commands=${item.commands}, context=${item.contexts}`);
  }
}

emit("\n## Current Scope Context Use");
if (!currentScope.enabled || currentScope.records === 0) {
  emit("- none");
} else {
  const currentContext = currentScope.context_use_summary || { hotspots: [], high_context: [], missing_inputs: [] };
  if (currentContext.hotspots.length === 0 && currentContext.high_context.length === 0 && currentContext.missing_inputs.length === 0) {
    emit("- none");
  } else {
    if (currentContext.hotspots.length > 0) {
      emit("- hotspots:");
      for (const item of currentContext.hotspots) emit(`  - ${item.path}: ${item.chars} chars`);
    }
    if (currentContext.high_context.length > 0) {
      emit("- high context:");
      for (const item of currentContext.high_context) emit(`  - line ${item.line}: ${item.intent}`);
    }
    if (currentContext.missing_inputs.length > 0) {
      emit("- missing inputs:");
      for (const item of currentContext.missing_inputs) emit(`  - line ${item.line} [${item.context_risk || "unknown"}]: ${item.intent}`);
    }
  }
}

emit("\n## Historical Whole-Profile Findings");
if (findings.length === 0) {
  emit("- No obvious whole-profile waste/rework/failure/context hotspots in this profile.");
} else {
  for (const finding of findings) emit(`- ${finding.message}`);
}

emit("\n## Tool Use Summary");
if (toolSummary.length === 0) {
  emit("- none");
} else {
  for (const item of toolSummary) {
    emit(`- ${item.tool}: ${item.records} record(s), ${formatMs(item.duration_ms)}, failed=${item.failed}, waste/rework=${item.waste_or_rework}, commands=${item.commands}, context=${item.contexts}`);
  }
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

emit("\n## Recovered Failed Records");
if (recoveredFailed.length === 0) {
  emit("- none");
} else {
  for (const item of recoveredFailed.slice(0, 20)) {
    emit(`- line ${item.record.__line} recovered by line ${item.recovered_by_line}: ${item.record.intent}`);
  }
}

emit("\n## Recovered Failure Classification");
if (recoveredFailureClassification.length === 0) {
  emit("- none");
} else {
  for (const item of recoveredFailureClassification.slice(0, 20)) {
    emit(`- line ${item.line} -> ${item.classification}: ${item.reason}`);
    emit(`  - next: ${item.next_action}`);
  }
}

emit("\n## Unresolved Failed Records");
if (unresolvedFailed.length === 0) {
  emit("- none");
} else {
  for (const record of unresolvedFailed.slice(0, 20)) {
    emit(`- line ${record.__line}: ${record.intent} -> ${commandText(record) || record.command_error || record.notes || "no detail recorded"}`);
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

emit("\n## Missing Tool Metadata");
if (missingToolRecords.length === 0) {
  emit("- none");
} else {
  for (const record of missingToolRecords.slice(0, 20)) {
    emit(`- line ${record.__line}: ${record.intent}`);
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

emit("\n## Repeated Command Classification");
if (repeatedCommandClassifications.length === 0) {
  emit("- none");
} else {
  for (const item of repeatedCommandClassifications.slice(0, 20)) {
    emit(`- ${item.count}x [${item.scope}] ${item.classification}: ${item.command}`);
    emit(`  - reason: ${item.reason}`);
    emit(`  - next: ${item.next_action}`);
    emit(`  - evidence: batched=${item.batched}, unbatched=${item.unbatched}, failed=${item.failed}, lines=${item.first_line}-${item.last_line}`);
  }
}

emit("\n## Repeated Broad/Final Commands");
if (repeatedBroadCommands.length === 0) {
  emit("- none");
} else {
  for (const [command, count] of repeatedBroadCommands) emit(`- ${count}x ${command}`);
}

emit("\n## Broad/Final Validation Classification");
if (repeatedBroadCommands.length === 0 && batchedBroadCommands.length === 0) {
  emit("- none");
} else {
  if (batchedBroadCommands.length > 0) {
    emit("- batched:");
    for (const [command, count] of batchedBroadCommands) emit(`  - ${count}x ${command}`);
  }
  if (repeatedUnbatchedBroadCommands.length > 0) {
    emit("- unbatched repeated:");
    for (const [command, count] of repeatedUnbatchedBroadCommands) emit(`  - ${count}x ${command}`);
  } else {
    emit("- unbatched repeated: none");
  }
}

emit("\n## Repeated Broad/Final Commands By Work Item");
if (repeatedBroadByWorkItem.length === 0) {
  emit("- none");
} else {
  for (const item of repeatedBroadByWorkItem) emit(`- ${item.work_item || item.segment}: ${item.count}x ${item.command}`);
}

emit("\n## Validation Batches");
if (validationBatchSummaries.length === 0) {
  emit("- none");
} else {
  for (const batch of validationBatchSummaries.slice(0, 10)) {
    const result = batch.failed > 0 ? `${batch.failed} failed` : "pass";
    const changes = batch.changes.length > 0 ? batch.changes.join(", ") : "unknown";
    emit(`- ${batch.batch_id}: ${batch.records} record(s), ${formatMs(batch.duration_ms)}, ${result}, risk=${batch.risk || "unknown"}, changes=${changes}, broad/final=${batch.broad_final_commands}`);
  }
}

emit("\n## Time By Phase");
if (topPhaseDuration.length === 0) {
  emit("- none");
} else {
  for (const [phase, duration] of topPhaseDuration) emit(`- ${phase}: ${formatMs(duration)}`);
}

emit("\n## Wall-Clock Coverage");
emit(`- span: ${formatMs(coverage.wall_clock_span_ms)}`);
emit(`- merged profiled time: ${formatMs(coverage.merged_profiled_ms)}`);
emit(`- coverage: ${formatPercent(coverage.coverage_ratio)}`);
if (coverage.largest_gaps.length === 0) {
  emit("- largest gaps: none above 5.0m");
} else {
  emit("- largest gaps:");
  for (const gap of coverage.largest_gaps.slice(0, 5)) {
    emit(`  - ${formatMs(gap.duration_ms)} from ${gap.start_ts} to ${gap.end_ts}`);
  }
}

emit("\n## Current Scope");
if (!currentScope.enabled) {
  emit("- none");
} else {
  emit(`- scope: ${currentScope.work_item}${currentScope.iteration ? `/${currentScope.iteration}` : ""}`);
  emit(`- since: ${currentScope.since}`);
  emit(`- records: ${currentScope.records}`);
  emit(`- missing context inputs: ${currentScope.missing_context_inputs}`);
  emit(`- missing work-item records: ${currentScope.missing_work_item_records}`);
  emit(`- missing tool records: ${currentScope.missing_tool_records}`);
  emit(`- repeated broad/final commands: ${currentScope.repeated_broad_final_commands.length}`);
  emit(`- repeated unbatched broad/final commands: ${currentScope.repeated_unbatched_broad_final_commands.length}`);
  emit(`- recovered failed records: ${currentScope.recovered_failed_records.length}`);
  emit(`- unresolved failed records: ${currentScope.unresolved_failed_records.length}`);
  emit(`- wall-clock coverage: ${formatPercent(currentScope.wall_clock_coverage.coverage_ratio)} (${formatMs(currentScope.wall_clock_coverage.merged_profiled_ms)} / ${formatMs(currentScope.wall_clock_coverage.wall_clock_span_ms)})`);
}

emit("\n## Work Items");
if (topWorkItems.length === 0) {
  emit("- none");
} else {
  for (const item of topWorkItems) emit(`- ${item.work_item}: ${item.records} record(s), ${formatMs(item.duration_ms)}`);
}

emit("\n## Iterations");
if (topIterations.length === 0) {
  emit("- none");
} else {
  for (const item of topIterations) emit(`- ${item.iteration}: ${item.records} record(s), ${formatMs(item.duration_ms)}`);
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
    recovered_failed_records: recoveredFailed.map((item) => ({
      line: item.record.__line,
      phase: item.record.phase,
      intent: item.record.intent,
      detail: commandText(item.record) || item.record.command_error || item.record.notes || "",
      recovered_by_line: item.recovered_by_line,
      recovered_by_intent: item.recovered_by_intent,
    })),
    recovered_failure_classification: recoveredFailureClassification,
    unresolved_failed_records: unresolvedFailed.map((record) => ({
      line: record.__line,
      phase: record.phase,
      intent: record.intent,
      detail: commandText(record) || record.command_error || record.notes || "",
    })),
    context_hotspots: topContext.map(([path, chars]) => ({ path, chars })),
    high_context: highContext.map((record) => ({ line: record.__line, intent: record.intent, context_risk: record.context_risk })),
    missing_context_inputs: missingContextInputs.map((record) => ({
      line: record.__line,
      intent: record.intent,
      context_risk: record.context_risk,
    })),
    missing_tool_records: missingToolRecords.map((record) => ({
      line: record.__line,
      intent: record.intent,
      phase: record.phase,
      category: record.category,
    })),
    repeated_commands: repeatedCommandObjects,
    repeated_commands_by_scope: mapEntries(repeatedScopeCounts, 10).map(({ key, value }) => ({ scope: key, count: value })),
    repeated_command_classification: repeatedCommandClassifications,
    tool_use_summary: toolSummary,
    repeated_broad_final_commands: repeatedCommandObjects.filter((item) => item.scope === "broad/final"),
    repeated_unbatched_broad_final_commands: repeatedUnbatchedBroadCommands.map(([command, count]) => ({
      command,
      count,
      scope: "broad/final",
    })),
    repeated_unbatched_broad_final_occurrences: repeatedUnbatchedBroadOccurrences,
    batched_broad_final_commands: batchedBroadCommands.map(([command, count]) => ({
      command,
      count,
      scope: "broad/final",
    })),
    repeated_broad_final_by_work_item: repeatedBroadByWorkItem.map((item) => ({
      work_item: item.segment,
      command: item.command,
      count: item.count,
    })),
    validation_batches: validationBatchSummaries,
    current_scope: currentScope,
    time_by_phase: topPhaseDuration.map(([phase, duration_ms]) => ({ phase, duration_ms })),
    wall_clock_coverage: coverage,
    work_items: topWorkItems,
    iterations: topIterations,
    suggested_pipeline_actions: actions,
  };
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(reviewJson, null, 2)}\n`, "utf8");
}
process.stdout.write(rendered);
