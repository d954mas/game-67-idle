import { appendFileSync, existsSync, mkdirSync, readFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

export const CATEGORIES = new Set([
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
]);

export const RESULTS = new Set(["pass", "fail", "mixed", "blocked", "skipped", "unknown"]);
export const VALUES = new Set(["productive", "necessary_overhead", "rework", "waste", "unknown"]);
export const CONTEXT_RISKS = new Set(["low", "medium", "high", "unknown"]);

export function localDate() {
  const now = new Date();
  const year = now.getFullYear();
  const month = String(now.getMonth() + 1).padStart(2, "0");
  const day = String(now.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}`;
}

export function defaultProfilePath() {
  return resolve("tmp", "session_profiles", `session_profile_${localDate()}.jsonl`);
}

export function defaultScopePath() {
  return resolve("tmp", "session_profiles", "current_scope.json");
}

export function profileScopePath() {
  return resolve(envString("AI_PROFILE_SCOPE_FILE") || defaultScopePath());
}

export function timestamp() {
  const now = new Date();
  const offsetMinutes = -now.getTimezoneOffset();
  const sign = offsetMinutes >= 0 ? "+" : "-";
  const absolute = Math.abs(offsetMinutes);
  const hours = String(Math.floor(absolute / 60)).padStart(2, "0");
  const minutes = String(absolute % 60).padStart(2, "0");
  const local = new Date(now.getTime() - now.getTimezoneOffset() * 60000).toISOString().replace("Z", "");
  return `${local}${sign}${hours}:${minutes}`;
}

export function parseArgs(argv) {
  const result = { values: {}, positionals: [] };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--") {
      result.positionals.push(...argv.slice(index + 1));
      break;
    }
    if (!arg.startsWith("--")) {
      result.positionals.push(arg);
      continue;
    }
    const key = arg.slice(2);
    const next = argv[index + 1];
    if (next === undefined || next.startsWith("--")) {
      addArgValue(result.values, key, true);
      continue;
    }
    addArgValue(result.values, key, next);
    index += 1;
  }
  return result;
}

function addArgValue(values, key, value) {
  if (values[key] === undefined) {
    values[key] = value;
    return;
  }
  if (!Array.isArray(values[key])) values[key] = [values[key]];
  values[key].push(value);
}

export function listArg(values, key) {
  const value = values[key];
  if (value === undefined || value === true) return [];
  return Array.isArray(value) ? value : [value];
}

export function stringArg(values, key, fallback = "") {
  const value = values[key];
  if (value === undefined || value === true) return fallback;
  return Array.isArray(value) ? String(value[value.length - 1]) : String(value);
}

function envString(name) {
  const value = process.env[name];
  return value === undefined ? "" : String(value).trim();
}

export function readProfileScope(scopePath = profileScopePath()) {
  const target = resolve(scopePath);
  if (!existsSync(target)) return { path: target, exists: false, valid: true, work_item: "", iteration: "", error: "" };
  try {
    const parsed = JSON.parse(readFileSync(target, "utf8"));
    return {
      path: target,
      exists: true,
      valid: true,
      work_item: String(parsed.work_item || "").trim(),
      iteration: String(parsed.iteration || "").trim(),
      updated_at: String(parsed.updated_at || ""),
      error: "",
    };
  } catch (error) {
    return { path: target, exists: true, valid: false, work_item: "", iteration: "", error: error.message };
  }
}

export function numberArg(values, key) {
  const value = stringArg(values, key, "");
  if (!value) return undefined;
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : Number.NaN;
}

export function parseContextInput(raw) {
  const parts = raw.split(":");
  let charsIndex = parts.findIndex((part, index) => index > 0 && Number.isFinite(Number(part)));
  if (charsIndex === -1) charsIndex = 1;
  const path = parts.slice(0, charsIndex).join(":");
  const charsRaw = parts[charsIndex] || "0";
  const reasonParts = parts.slice(charsIndex + 1);
  const chars = Number(charsRaw);
  return {
    path,
    chars: Number.isFinite(chars) ? chars : 0,
    reason: reasonParts.join(":"),
  };
}

export function buildRecord(values, extra = {}) {
  const profileScope = readProfileScope();
  const record = {
    ts: stringArg(values, "ts", timestamp()),
    phase: stringArg(values, "phase"),
    category: stringArg(values, "category"),
    intent: stringArg(values, "intent"),
    result: stringArg(values, "result", "unknown"),
    value: stringArg(values, "value", "unknown"),
    ...extra,
  };

  const durationMs = numberArg(values, "duration-ms");
  if (durationMs !== undefined) record.duration_ms = durationMs;

  const contextRisk = stringArg(values, "context-risk", "");
  if (contextRisk) record.context_risk = contextRisk;

  const workItem = stringArg(values, "work-item", envString("AI_PROFILE_WORK_ITEM") || profileScope.work_item);
  if (workItem) record.work_item = workItem;

  const iteration = stringArg(values, "iteration", envString("AI_PROFILE_ITERATION") || profileScope.iteration);
  if (iteration) record.iteration = iteration;

  const wasteReason = stringArg(values, "waste-reason", "");
  if (wasteReason) record.waste_reason = wasteReason;

  const blockedBy = stringArg(values, "blocked-by", "");
  if (blockedBy) record.blocked_by = blockedBy;

  const notes = stringArg(values, "notes", "");
  if (notes) record.notes = notes;

  const tools = listArg(values, "tool");
  if (tools.length > 0) record.tools = tools;

  const commands = listArg(values, "command");
  if (commands.length > 0) record.commands = commands;

  const filesRead = listArg(values, "file-read");
  if (filesRead.length > 0) record.files_read = filesRead;

  const filesWritten = listArg(values, "file-written");
  if (filesWritten.length > 0) record.files_written = filesWritten;

  const evidence = listArg(values, "evidence");
  if (evidence.length > 0) record.evidence = evidence;

  const contextInputs = listArg(values, "context-input").map(parseContextInput);
  if (contextInputs.length > 0) record.context_inputs = contextInputs;

  return record;
}

export function validateRecord(record) {
  const errors = [];
  for (const field of ["ts", "phase", "category", "intent", "result", "value"]) {
    if (!record[field]) errors.push(`missing required field ${field}`);
  }
  if (record.category && !CATEGORIES.has(record.category)) errors.push(`unknown category ${record.category}`);
  if (record.result && !RESULTS.has(record.result)) errors.push(`unknown result ${record.result}`);
  if (record.value && !VALUES.has(record.value)) errors.push(`unknown value ${record.value}`);
  if (record.context_risk && !CONTEXT_RISKS.has(record.context_risk)) {
    errors.push(`unknown context_risk ${record.context_risk}`);
  }
  if (record.duration_ms !== undefined && (!Number.isFinite(record.duration_ms) || record.duration_ms < 0)) {
    errors.push("duration_ms must be a non-negative number");
  }
  return errors;
}

export function appendRecord(profilePath, record) {
  const errors = validateRecord(record);
  if (errors.length > 0) {
    throw new Error(errors.join("; "));
  }
  const target = resolve(profilePath || defaultProfilePath());
  mkdirSync(dirname(target), { recursive: true });
  appendFileSync(target, `${JSON.stringify(record)}\n`, "utf8");
  return target;
}
