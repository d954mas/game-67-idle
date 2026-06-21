#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import {
  defaultProfilePath,
  deriveSessionId,
  latestSessionProfilePath,
  listSessionProfiles,
  numberArg,
  parseArgs,
  sessionsDir,
  stringArg,
  todaySessionProfiles,
} from "./profile_lib.mjs";
import { buildOrchestrationTrace, buildTrace, todaySessionRoot } from "./orchestration_trace.mjs";
import { currentDoingOrchestrationTaskIds, DEFAULT_ORCHESTRATION_TOOL_USE_GUARD, findRoot as findTaskboardRoot } from "../taskboard/lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/status.mjs [--profile <p>] [--session <id>] [--harness claude|codex] [--all] [--json-output <status.json>] [--agent-rollup-evidence] [--verbose] [--agent-rollup] [--require-agent-rollup-ok]

Profiling is fully passive: the PostToolUse hook records every tool call to a
per-session log automatically. This command READS that log and reports the
session's commands, durations, slowest work, repeats (friction), failures, and
wall-clock coverage. It never appends records.

Default reads the ACTIVE session log (the current session, self-identified from
the harness env; newest tmp/session_profiles/sessions/*.jsonl as a fallback).
--harness <name> picks the newest log for that harness; --all aggregates today's
session logs; --session <id> picks one session; --profile <p> reads an explicit
file. --verbose adds coverage gaps and parse errors.

Agent rollup is analysis-time only: pass --agent-rollup with --parent-thread-id
<id> and optional --session-root <dir>, --agent-cwd <path>, --min-agents <n>.
It also reads matching subagent profile logs from --agent-profile-dir
(default: tmp/session_profiles/sessions) when they exist.
Use --trace-session <codex-session.jsonl> when checking parent transcript calls.
If CODEX_SESSION_FILE points at the parent session, --agent-rollup can infer its
id. Add --require-agent-rollup-ok only for strict task evidence; it exits
nonzero when the rollup is missing or incomplete.

Use --agent-rollup-evidence with --json-output for a compact, sanitized
taskboard evidence artifact instead of a full local diagnostic status dump.`);
  process.exit(2);
}

function parseProfile(file) {
  if (!existsSync(file)) return { records: [], errors: [], exists: false };
  const text = readFileSync(file, "utf8");
  const records = [];
  const errors = [];
  for (const [index, rawLine] of text.split(/\r?\n/).entries()) {
    const line = rawLine.trim();
    if (!line) continue;
    try {
      records.push({ ...JSON.parse(line), __line: index + 1 });
    } catch (error) {
      errors.push(`line ${index + 1}: invalid JSON: ${error.message}`);
    }
  }
  return { records, errors, exists: true };
}

/* Pair each tool_call_start with its following tool_call_result (sequential per
 * session) to derive the command's own duration_ms, then DROP the start records
 * so counts/coverage stay clean. Lets "Slowest Recorded Work" surface the tools
 * that actually take time. Backward-compatible: files with no start events
 * (PreToolUse not wired) are unchanged. */
function attachDurations(records) {
  const out = [];
  let pending = null;
  for (const record of records) {
    if (record.event_type === "tool_call_start") {
      pending = record;
      continue;
    }
    if (record.event_type === "tool_call_result" && pending) {
      const resultCmd = (record.commands && record.commands[0]) || "";
      const startCmd = (pending.commands && pending.commands[0]) || "";
      const sameSession = (record.session_id || "") === (pending.session_id || "");
      if (resultCmd === startCmd && sameSession) {
        const delta = Date.parse(record.ts) - Date.parse(pending.ts);
        if (Number.isFinite(delta) && delta >= 0) record.duration_ms = delta;
      }
      pending = null;
    }
    out.push(record);
  }
  return out;
}

/* Merge several per-session logs into one time-ordered record set. */
function parseProfiles(files) {
  const records = [];
  const errors = [];
  let exists = false;
  for (const file of files) {
    const parsed = parseProfile(file);
    if (parsed.exists) exists = true;
    records.push(...parsed.records);
    for (const error of parsed.errors) errors.push(error);
  }
  records.sort((a, b) => String(a.ts).localeCompare(String(b.ts)));
  return { records: attachDurations(records), errors, exists };
}

/* Resolve which log file(s) `status` reads:
 *   --profile <p>  -> that file
 *   --session <id> -> the matching per-session log (any day)
 *   --all          -> all of today's per-session logs + the legacy daily file
 *   (default)      -> the active session (self-identified from harness env);
 *                     newest per-session log, then legacy daily file, as fallback. */
function resolveProfilePaths(values) {
  const explicit = stringArg(values, "profile", "");
  if (explicit) return [resolve(explicit)];

  const sessionId = stringArg(values, "session", "");
  if (sessionId) {
    const matches = listSessionProfiles().filter((path) => basename(path).includes(sessionId));
    if (matches.length > 0) return matches;
  }

  if (values.all === true) {
    const all = [...todaySessionProfiles()];
    const daily = defaultProfilePath();
    if (existsSync(daily)) all.push(daily);
    return all.length > 0 ? all : [daily];
  }

  /* --harness <name>: the latest session for that harness -- reliably picks YOUR
   * session even while another harness (e.g. Codex) writes concurrently. */
  const harness = stringArg(values, "harness", "");
  if (harness) {
    const matches = todaySessionProfiles().filter((path) => basename(path).includes(`__${harness}__`));
    if (matches.length > 0) {
      return [matches.sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs)[0]];
    }
  }

  /* Default: the CURRENT session, self-identified from the harness env, so it is
   * correct under ANY number of parallel sessions. Fall back to the newest log. */
  const envSources = [
    ["claude", process.env.CLAUDE_CODE_SESSION_ID || ""],
    ["codex", process.env.CODEX_SESSION_FILE || ""],
  ];
  for (const [name, raw] of envSources) {
    if (!raw) continue;
    const { short } = deriveSessionId(name === "codex" ? basename(raw) : raw);
    if (!short) continue;
    const match = todaySessionProfiles().find((path) => basename(path).includes(`__${name}__${short}`));
    if (match) return [match];
  }

  const latest = latestSessionProfilePath();
  return [latest || defaultProfilePath()];
}

function eventTime(record) {
  const parsed = Date.parse(record.ts || "");
  return Number.isFinite(parsed) ? parsed : undefined;
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

/* Wall-clock coverage: long idle gaps (>= idleGapMs, e.g. overnight) are
 * excluded so a fully-recorded session does not look "low coverage" after a
 * pause; short gaps between events count as active hands-on work. */
function coverageStats(records, gapThresholdMs = 5 * 60 * 1000, idleGapMs = 60 * 60 * 1000) {
  const intervals = [];
  for (const record of records) {
    const endMs = eventTime(record);
    if (endMs === undefined) continue;
    const durationMs = Math.max(0, Number(record.duration_ms || 0));
    intervals.push({ start_ms: endMs - durationMs, end_ms: endMs, line: record.__line, intent: record.intent || "" });
  }
  intervals.sort((a, b) => a.start_ms - b.start_ms || a.end_ms - b.end_ms);
  if (intervals.length === 0) {
    return { wall_clock_span_ms: 0, effective_span_ms: 0, active_ms: 0, idle_ms: 0, coverage_ratio: undefined, largest_gaps: [] };
  }
  const merged = [];
  for (const interval of intervals) {
    const last = merged[merged.length - 1];
    if (!last || interval.start_ms > last.end_ms) merged.push({ ...interval });
    else if (interval.end_ms > last.end_ms) last.end_ms = interval.end_ms;
  }
  const firstMs = intervals[0].start_ms;
  const lastMs = intervals.reduce((max, interval) => Math.max(max, interval.end_ms), intervals[0].end_ms);
  const wallClockSpanMs = Math.max(0, lastMs - firstMs);
  let activeMs = 0;
  let idleMs = 0;
  for (let index = 1; index < merged.length; index += 1) {
    const gap = merged[index].start_ms - merged[index - 1].end_ms;
    if (gap <= 0) continue;
    if (gap < gapThresholdMs) activeMs += gap;
    else if (gap >= idleGapMs) idleMs += gap;
  }
  activeMs += merged.reduce((sum, interval) => sum + Math.max(0, interval.end_ms - interval.start_ms), 0);
  const effectiveSpanMs = Math.max(0, wallClockSpanMs - idleMs);
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
        previous_line: previous.line,
        next_line: current.line,
      });
    }
  }
  largestGaps.sort((a, b) => b.duration_ms - a.duration_ms);
  return {
    wall_clock_span_ms: wallClockSpanMs,
    effective_span_ms: effectiveSpanMs,
    active_ms: activeMs,
    idle_ms: idleMs,
    coverage_ratio: effectiveSpanMs > 0 ? Math.min(1, activeMs / effectiveSpanMs) : undefined,
    largest_gaps: largestGaps.slice(0, 10),
  };
}

function isLowCoverage(coverage) {
  return coverage.effective_span_ms >= 30 * 60 * 1000 && Number.isFinite(coverage.coverage_ratio) && coverage.coverage_ratio < 0.25;
}

function latestRecord(records) {
  return [...records].sort((a, b) => (eventTime(b) || 0) - (eventTime(a) || 0))[0];
}

function normalizeCommand(command) {
  return stripLeadingCommandAssignments(String(command || "")).replaceAll("\\", "/").replace(/\s+/g, " ").trim();
}

function commandTokens(command) {
  const tokens = [];
  const text = stripLeadingCommandAssignments(command);
  const pattern = /"([^"]*)"|'([^']*)'|(\S+)/g;
  let match;
  while ((match = pattern.exec(text)) !== null) tokens.push(match[1] ?? match[2] ?? match[3] ?? "");
  return tokens;
}

function nodeTestFileRecoveryKeys(command, { failedRecord = false } = {}) {
  const tokens = commandTokens(command);
  const first = (tokens[0] || "").split(/[\\/]/).pop()?.replace(/\.(exe|cmd)$/i, "").toLowerCase();
  if (first !== "node") return [];
  const testIndex = tokens.findIndex((token) => token === "--test");
  if (testIndex < 0) return [];
  const unsafeFlags = new Set(["--test-name-pattern", "--test-skip-pattern", "--test-only", "--watch", "--watch-path"]);
  if (tokens.some((token) => unsafeFlags.has(token) || [...unsafeFlags].some((flag) => token.startsWith(`${flag}=`)))) return [];
  if (tokens.slice(0, testIndex).some((token) => token.includes("&&") || token.includes(";") || token.includes("|"))) return [];
  const files = [];
  for (const token of tokens.slice(testIndex + 1)) {
    if (!token || token.startsWith("-")) continue;
    const normalized = normalizeCommand(token);
    if (!/\.(?:mjs|cjs|js|ts|mts|cts)$/i.test(normalized)) continue;
    if (/[*?[\]{}]/.test(normalized)) continue;
    files.push(normalized);
  }
  if (failedRecord && files.length !== 1) return [];
  return [...new Set(files)].map((file) => `node-test-file:${file}`);
}

function commandKeys(record) {
  const keys = (record.commands || []).map(normalizeCommand).filter(Boolean);
  for (const command of record.commands || []) {
    keys.push(...nodeTestFileRecoveryKeys(command, { failedRecord: record.result === "fail" }));
  }
  if (record.validation_check_id) keys.push(`validation_check:${record.validation_check_id}`);
  return keys;
}

function isEnvironmentBlocked(record) {
  return record.failure_kind === "environment_blocked";
}

function isAgentToolUsage(record) {
  return record.failure_kind === "agent_tool_usage";
}

function isAgentEvidenceProbe(record) {
  return record.failure_kind === "agent_evidence_probe";
}

function isStrictAgentRollupCommand(command) {
  const text = normalizeCommand(command);
  return /\bnode(?:\.exe|\.cmd)?\s+tools\/ai\.mjs\s+status\b/i.test(text)
    && /\s--agent-rollup\b/i.test(text)
    && /\s--require-agent-rollup-ok\b/i.test(text);
}

function isAiHelpProbeCommand(command) {
  return /\bnode(?:\.exe|\.cmd)?\s+tools\/ai\.mjs\s+(?:--help|-h|help)\b/i.test(normalizeCommand(command));
}

function isFailedReadOnlyDiscoveryCommand(command) {
  const text = normalizeCommand(command);
  return /\bGet-ChildItem\b/i.test(text) || /\brg(?:\.exe)?\s+--files\b/i.test(text);
}

function isAgentEvidenceProbeRecord(record) {
  return isAgentEvidenceProbe(record)
    || (record.commands || []).some((command) => isStrictAgentRollupCommand(command) || isAiHelpProbeCommand(command));
}

function isAgentToolUsageRecord(record) {
  return isAgentToolUsage(record)
    || (record.commands || []).some((command) => isFailedReadOnlyDiscoveryCommand(command));
}

function inferredFailureReason(record, fallback) {
  if (record.blocked_by) return String(record.blocked_by).trim();
  if ((record.commands || []).some(isStrictAgentRollupCommand)) return "failed strict agent rollup probe";
  if ((record.commands || []).some(isAiHelpProbeCommand)) return "failed help/usage probe";
  if ((record.commands || []).some(isFailedReadOnlyDiscoveryCommand)) return "failed read-only discovery command";
  return fallback;
}

function transcriptFailureDetails(command, output) {
  const text = String(output || "");
  if (isStrictAgentRollupCommand(command) && /strict problem:|Fix the agent rollup evidence|Inspect unresolved agent failure samples/i.test(text)) {
    return { failure_kind: "agent_evidence_probe", blocked_by: "failed strict agent rollup probe" };
  }
  if (/ItemNotFoundException|ObjectNotFound/.test(text)) {
    return { failure_kind: "agent_tool_usage", blocked_by: "missing local file/path" };
  }
  if (/NamedParameterNotFound|ParameterBindingException|CannotConvertArgumentNoMessage/.test(text)) {
    return { failure_kind: "agent_tool_usage", blocked_by: "invalid shell command/parameter" };
  }
  if (/orchestration-trace\b/.test(String(command || "")) && /missing evidence source/.test(text)) {
    return { failure_kind: "agent_tool_usage", blocked_by: "missing orchestration evidence source" };
  }
  return {};
}

function buildRecoveryPassesByKey(records) {
  const passes = new Map();
  for (const record of records) {
    if (record.result !== "pass") continue;
    const ts = eventTime(record);
    if (ts === undefined) continue;
    for (const key of commandKeys(record)) {
      const times = passes.get(key) || [];
      times.push(ts);
      passes.set(key, times);
    }
  }
  for (const times of passes.values()) times.sort((a, b) => a - b);
  return passes;
}

function recoveryKeyKind(key) {
  return String(key || "").startsWith("node-test-file:") ? "node-test-file" : "exact";
}

function hasExternalRecoveryPass(keys, failedAt, recoveryPassesByKey) {
  if (failedAt === undefined || recoveryPassesByKey.size === 0) return false;
  return keys.some((key) => (recoveryPassesByKey.get(key) || []).some((passAt) => passAt > failedAt));
}

function externalRecoveryKind(keys, failedAt, recoveryPassesByKey) {
  if (failedAt === undefined || recoveryPassesByKey.size === 0) return "";
  for (const key of keys) {
    if ((recoveryPassesByKey.get(key) || []).some((passAt) => passAt > failedAt)) return recoveryKeyKind(key);
  }
  return "";
}

/* A failed record is "recovered" if the same command passed on a later line, or
 * by a later external/orchestrator pass when provided. */
function classifyFailedRecords(records, { recoveryPassesByKey = new Map() } = {}) {
  const passedLater = new Map();
  let resolvedLater = 0;
  let externallyRecovered = 0;
  let parentExactRecovered = 0;
  let parentNodeTestFileRecovered = 0;
  let environmentBlocked = 0;
  let agentToolUsage = 0;
  let agentEvidenceProbe = 0;
  let unresolved = 0;
  const environmentBlockedReasons = new Map();
  const agentToolUsageReasons = new Map();
  const agentEvidenceProbeReasons = new Map();
  const unresolvedRecords = [];
  const agentToolUsageRecords = [];
  const agentEvidenceProbeRecords = [];
  for (const record of [...records].sort((a, b) => b.__line - a.__line)) {
    const keys = commandKeys(record);
    if (record.result === "pass") {
      for (const key of keys) passedLater.set(key, record);
      continue;
    }
    if (record.result !== "fail") continue;
    if (keys.some((key) => passedLater.has(key))) resolvedLater += 1;
    else if (isEnvironmentBlocked(record)) {
      environmentBlocked += 1;
      const reason = String(record.blocked_by || "environment blocker").trim();
      environmentBlockedReasons.set(reason, (environmentBlockedReasons.get(reason) || 0) + 1);
    }
    else if (hasExternalRecoveryPass(keys, eventTime(record), recoveryPassesByKey)) {
      externallyRecovered += 1;
      if (externalRecoveryKind(keys, eventTime(record), recoveryPassesByKey) === "node-test-file") parentNodeTestFileRecovered += 1;
      else parentExactRecovered += 1;
    }
    else if (isAgentToolUsageRecord(record)) {
      agentToolUsage += 1;
      const reason = inferredFailureReason(record, "agent tool-usage failure");
      agentToolUsageReasons.set(reason, (agentToolUsageReasons.get(reason) || 0) + 1);
      agentToolUsageRecords.push({ ...record, blocked_by: reason });
    }
    else if (isAgentEvidenceProbeRecord(record)) {
      agentEvidenceProbe += 1;
      const reason = inferredFailureReason(record, "agent evidence-probe failure");
      agentEvidenceProbeReasons.set(reason, (agentEvidenceProbeReasons.get(reason) || 0) + 1);
      agentEvidenceProbeRecords.push({ ...record, blocked_by: reason });
    }
    else {
      unresolved += 1;
      unresolvedRecords.push(record);
    }
  }
  return {
    resolvedLater,
    externallyRecovered,
    parentExactRecovered,
    parentNodeTestFileRecovered,
    recovered: resolvedLater + externallyRecovered,
    environmentBlocked,
    agentToolUsage,
    agentEvidenceProbe,
    unresolved,
    unresolvedRecords,
    agentToolUsageRecords,
    agentEvidenceProbeRecords,
    environmentBlockedReasons: [...environmentBlockedReasons.entries()].map(([reason, count]) => ({ reason, count })),
    agentToolUsageReasons: [...agentToolUsageReasons.entries()].map(([reason, count]) => ({ reason, count })),
    agentEvidenceProbeReasons: [...agentEvidenceProbeReasons.entries()].map(([reason, count]) => ({ reason, count })),
  };
}

/* Normalize a command to a tool-level key: strip leading shell assignment
 * wrappers, take the first token's basename, and for interpreters append the
 * script basename (so "node tools/ai.mjs status" -> "node ai.mjs"). */
function commandKey(cmd) {
  const text = stripLeadingCommandAssignments(cmd);
  const tokens = text.split(/\s+/).filter(Boolean);
  if (tokens.length === 0) return "shell";
  const base = (segment) => segment.split(/[\\/]/).pop() || segment;
  let key = base(tokens[0]).replace(/\.(exe|cmd)$/i, "");
  if (/^(node|py|python|python3|bash|sh|npx|deno|pwsh|powershell)$/i.test(key)) {
    const script = tokens.slice(1).find((token) => !token.startsWith("-"));
    if (script) key += ` ${base(script)}`;
  }
  return key;
}

function stripLeadingCommandAssignments(cmd) {
  let text = String(cmd || "").trim();
  while (/^\$env:[A-Za-z_][A-Za-z0-9_]*\s*=\s*(?:"[^"]*"|'[^']*'|[^;]+)\s*;\s*/i.test(text)) {
    text = text.replace(/^\$env:[A-Za-z_][A-Za-z0-9_]*\s*=\s*(?:"[^"]*"|'[^']*'|[^;]+)\s*;\s*/i, "");
  }
  while (/^\$[A-Za-z_][A-Za-z0-9_]*\s*=\s*(?:"[^"]*"|'[^']*'|-?\d+(?:\.\d+)?|\$true|\$false)\s*;\s*/i.test(text)) {
    text = text.replace(/^\$[A-Za-z_][A-Za-z0-9_]*\s*=\s*(?:"[^"]*"|'[^']*'|-?\d+(?:\.\d+)?|\$true|\$false)\s*;\s*/i, "");
  }
  while (/^[A-Za-z_][A-Za-z0-9_]*=(?:"[^"]*"|'[^']*'|\S+)\s+/.test(text)) {
    text = text.replace(/^[A-Za-z_][A-Za-z0-9_]*=(?:"[^"]*"|'[^']*'|\S+)\s+/, "");
  }
  return text.trim();
}

function isSearchCommand(cmd) {
  const text = stripLeadingCommandAssignments(String(cmd || "")).split(/\r?\n/)[0].trim().toLowerCase();
  return /^(?:rg|grep|egrep|fgrep|findstr|ack|select-string)(?:\s|$)/.test(text);
}

function transcriptResult(command, exitCode) {
  if (exitCode === undefined) return "unknown";
  if (exitCode === 0) return "pass";
  if (exitCode === 1 && isSearchCommand(command)) return "pass";
  return "fail";
}

/* Aggregate commands by tool key: which tools cost the most total time (what to
 * speed up) and which run most often (repeats / retries = friction). */
function commandRollup(records) {
  const map = new Map();
  for (const record of records) {
    if (record.event_type === "session_start") continue;
    const cmd = (record.commands && record.commands[0]) || "";
    if (!cmd) continue;
    const key = commandKey(cmd);
    const entry = map.get(key) || { key, count: 0, total_ms: 0, max_ms: 0, fails: 0 };
    entry.count += 1;
    const durationMs = Number(record.duration_ms || 0);
    entry.total_ms += durationMs;
    if (durationMs > entry.max_ms) entry.max_ms = durationMs;
    if (record.result === "fail") entry.fails += 1;
    map.set(key, entry);
  }
  const all = [...map.values()];
  return {
    by_time: all.filter((entry) => entry.total_ms > 0).sort((a, b) => b.total_ms - a.total_ms).slice(0, 6),
    by_count: all.sort((a, b) => b.count - a.count).slice(0, 6),
  };
}

function readSessionMetaId(file) {
  if (!file || !existsSync(file)) return "";
  try {
    const firstLine = readFileSync(file, "utf8").split(/\r?\n/, 1)[0] || "";
    const line = JSON.parse(firstLine);
    if (line.type === "session_meta" && line.payload?.id) return String(line.payload.id);
  } catch {
    return "";
  }
  return "";
}

function roleRollup(agents) {
  const roles = new Map();
  for (const agent of agents) {
    const role = agent.role || "(no-role)";
    roles.set(role, (roles.get(role) || 0) + 1);
  }
  return [...roles.entries()]
    .map(([role, count]) => ({ role, count }))
    .sort((a, b) => b.count - a.count || a.role.localeCompare(b.role));
}

function traceSource(traceSession, parentThreadId) {
  if (traceSession && parentThreadId) return "trace-session+parent-thread";
  if (traceSession) return "trace-session";
  if (parentThreadId) return "parent-thread";
  return "none";
}

function profileKey(file) {
  const name = basename(file).replace(/\.jsonl$/i, "");
  const parts = name.split("__");
  return (parts[parts.length - 1] || name).toLowerCase();
}

function agentProfileKeys(agent) {
  const fromId = deriveSessionId(agent.id || "");
  const fromSessionFile = deriveSessionId(basename(agent.sessionFile || ""));
  return [fromId.full, fromId.short, fromSessionFile.full, fromSessionFile.short]
    .map((value) => String(value || "").toLowerCase())
    .filter(Boolean);
}

function findAgentProfileFile(agent, files) {
  const keys = new Set(agentProfileKeys(agent));
  return files.find((file) => keys.has(profileKey(file))) || "";
}

function parseJsonObject(raw) {
  try {
    const parsed = JSON.parse(String(raw || "{}"));
    return parsed && typeof parsed === "object" ? parsed : {};
  } catch {
    return {};
  }
}

function parseTranscriptDurationMs(output) {
  const match = String(output || "").match(/^Wall time:\s*([0-9.]+)\s*seconds\b/im);
  if (!match) return 0;
  const seconds = Number(match[1]);
  return Number.isFinite(seconds) && seconds >= 0 ? Math.round(seconds * 1000) : 0;
}

function parseTranscriptExitCode(output) {
  const match = String(output || "").match(/^Exit code:\s*(-?\d+)/im);
  if (!match) return undefined;
  const code = Number(match[1]);
  return Number.isInteger(code) ? code : undefined;
}

function readTranscriptProfileRecords(file, agentId) {
  if (!file || !existsSync(file)) return { records: [], errors: [] };
  const calls = new Map();
  const records = [];
  const errors = [];
  const text = readFileSync(file, "utf8");
  for (const [index, rawLine] of text.split(/\r?\n/).entries()) {
    const lineText = rawLine.trim();
    if (!lineText) continue;
    let line;
    try {
      line = JSON.parse(lineText);
    } catch (error) {
      errors.push(`${file}: line ${index + 1}: invalid JSON: ${error.message}`);
      continue;
    }
    const payload = line?.payload;
    if (line?.type !== "response_item" || !payload) continue;
    if (payload.type === "function_call") {
      const name = String(payload.name || "");
      if (!/(^|[._-])shell_command$/.test(name)) continue;
      const args = parseJsonObject(payload.arguments);
      const command = String(args.command || "").trim();
      if (command) calls.set(String(payload.call_id || ""), { command, ts: line.timestamp || "" });
      continue;
    }
    if (payload.type !== "function_call_output") continue;
    const callId = String(payload.call_id || "");
    const call = calls.get(callId);
    if (!call) continue;
    const output = String(payload.output || "");
    const exitCode = parseTranscriptExitCode(output);
    const result = transcriptResult(call.command, exitCode);
    const failureDetails = result === "fail" ? transcriptFailureDetails(call.command, output) : {};
    records.push({
      __line: index + 1,
      ts: line.timestamp || call.ts || "",
      phase: "session",
      category: "tooling",
      intent: "auto:codex-transcript",
      result,
      value: "unknown",
      event_type: "tool_call_result_transcript",
      duration_ms: parseTranscriptDurationMs(output),
      commands: [call.command],
      session_id: agentId,
      source_call_id: callId,
      source_session_file: file,
      ...(exitCode !== undefined ? { exit_code: exitCode } : {}),
      ...failureDetails,
    });
  }
  return { records, errors };
}

function addFailureStats(total, stats) {
  total.unresolved += stats.unresolved;
  total.resolvedLater += stats.resolvedLater;
  total.externallyRecovered += stats.externallyRecovered;
  total.parentExactRecovered += stats.parentExactRecovered;
  total.parentNodeTestFileRecovered += stats.parentNodeTestFileRecovered;
  total.environmentBlocked += stats.environmentBlocked;
  total.agentToolUsage += stats.agentToolUsage;
  total.agentEvidenceProbe += stats.agentEvidenceProbe;
  return total;
}

function failureSample(agent, source, record) {
  const command = String(record.commands?.[0] || "");
  return {
    agent_id: agent.id || "",
    nickname: agent.nickname || "",
    role: agent.role || "",
    source,
    command_key: commandKey(command),
    command: command.slice(0, 240),
    exit_code: record.exit_code,
    line: record.__line,
    source_call_id: record.source_call_id || "",
    source_session_file: record.source_session_file || "",
    blocked_by: record.blocked_by || "",
  };
}

function renderFailureSample(sample) {
  const agent = sample.nickname || sample.agent_id || "(unknown)";
  const role = sample.role ? ` [${sample.role}]` : "";
  const source = sample.source || "unknown";
  const line = sample.line !== undefined ? `:${sample.line}` : "";
  const exit = sample.exit_code !== undefined ? ` exit ${sample.exit_code}` : "";
  return `- unresolved: ${agent}${role} ${source}${line} ${sample.command_key}${exit} - ${sample.command}`;
}

function renderAgentToolUsageSample(sample) {
  const agent = sample.nickname || sample.agent_id || "(unknown)";
  const role = sample.role ? ` [${sample.role}]` : "";
  const source = sample.source || "unknown";
  const line = sample.line !== undefined ? `:${sample.line}` : "";
  const reason = sample.blocked_by ? ` (${sample.blocked_by})` : "";
  return `- tool-usage: ${agent}${role} ${source}${line} ${sample.command_key}${reason} - ${sample.command}`;
}

function renderAgentEvidenceProbeSample(sample) {
  const agent = sample.nickname || sample.agent_id || "(unknown)";
  const role = sample.role ? ` [${sample.role}]` : "";
  const source = sample.source || "unknown";
  const line = sample.line !== undefined ? `:${sample.line}` : "";
  const reason = sample.blocked_by ? ` (${sample.blocked_by})` : "";
  const exit = sample.exit_code !== undefined ? ` exit ${sample.exit_code}` : "";
  return `- evidence-probe: ${agent}${role} ${source}${line} ${sample.command_key}${reason}${exit} - ${sample.command}`;
}

function agentToolUsagePreventionHints(profileRollup, rollupContext = {}) {
  const reasons = new Set((profileRollup.agent_tool_usage_reasons || []).map((item) => item.reason));
  const hints = [];
  if (reasons.has("missing local file/path")) {
    hints.push({
      reason: "missing local file/path",
      hint: "Verify paths with `rg --files <scope>` or `Test-Path -LiteralPath <path>` before reads.",
    });
  }
  if (reasons.has("invalid shell command/parameter")) {
    hints.push({
      reason: "invalid shell command/parameter",
      hint: "Avoid unsupported PowerShell shapes such as `Format-Hex -Count` and `Select-Object -Index 96..114`; use `Select-Object -Skip <n> -First <n>` for line windows.",
    });
  }
  if (reasons.has("missing orchestration evidence source")) {
    const command = [
      "node tools/ai.mjs orchestration-trace",
      rollupContext.parent_thread_id ? `--parent-thread-id ${formatCommandArg(rollupContext.parent_thread_id)}` : "",
      rollupContext.session_root ? `--session-root ${formatCommandArg(rollupContext.session_root)}` : "",
      rollupContext.cwd ? `--cwd ${formatCommandArg(rollupContext.cwd)}` : "",
      "--json-output tmp/orchestration-trace.json --json",
    ].filter(Boolean).join(" ");
    hints.push({
      reason: "missing orchestration evidence source",
      hint: `Prefer task-scoped evidence with \`node tools/ai.mjs orchestration-evidence --current --run --json\`; for raw trace fallback use an evidence source: \`${command}\`.`,
    });
  }
  return hints;
}

function buildAgentProfileRollup(agents, values, parentRecords = []) {
  const profileDir = stringArg(values, "agent-profile-dir", "") || sessionsDir();
  const files = listSessionProfiles(profileDir);
  const profiles = [];
  const missing = [];
  const errors = [];
  const allRecords = [];
  const failed = {
    unresolved: 0,
    resolvedLater: 0,
    externallyRecovered: 0,
    parentExactRecovered: 0,
    parentNodeTestFileRecovered: 0,
    environmentBlocked: 0,
    agentToolUsage: 0,
    agentEvidenceProbe: 0,
  };
  const agentToolUsageReasons = new Map();
  const agentEvidenceProbeReasons = new Map();
  const parentRecoveryPasses = buildRecoveryPassesByKey(parentRecords);
  const unresolvedFailureSamples = [];
  const agentToolUsageFailureSamples = [];
  const agentEvidenceProbeFailureSamples = [];
  let agentToolUsageCleanTailAgents = 0;

  for (const agent of agents) {
    const file = findAgentProfileFile(agent, files);
    const source = file ? "profile" : "transcript";
    const parsed = file ? parseProfiles([file]) : readTranscriptProfileRecords(agent.sessionFile, agent.id);
    const records = parsed.records;
    if (records.length === 0) {
      missing.push({ id: agent.id, nickname: agent.nickname || "", role: agent.role || "" });
      continue;
    }
    const recordedMs = records.reduce((sum, record) => sum + Math.max(0, Number(record.duration_ms || 0)), 0);
    profiles.push({
      id: agent.id,
      nickname: agent.nickname || "",
      role: agent.role || "",
      source,
      profile: file || "",
      session_file: agent.sessionFile || "",
      records: records.length,
      recorded_ms: recordedMs,
    });
    for (const error of parsed.errors) errors.push(file ? `${file}: ${error}` : error);
    const failureStats = classifyFailedRecords(records, { recoveryPassesByKey: parentRecoveryPasses });
    addFailureStats(failed, failureStats);
    if (failureStats.agentToolUsage > 0) agentToolUsageCleanTailAgents = 0;
    else agentToolUsageCleanTailAgents += 1;
    for (const item of failureStats.agentToolUsageReasons) {
      agentToolUsageReasons.set(item.reason, (agentToolUsageReasons.get(item.reason) || 0) + item.count);
    }
    for (const item of failureStats.agentEvidenceProbeReasons) {
      agentEvidenceProbeReasons.set(item.reason, (agentEvidenceProbeReasons.get(item.reason) || 0) + item.count);
    }
    for (const record of failureStats.unresolvedRecords) {
      if (unresolvedFailureSamples.length >= 10) break;
      unresolvedFailureSamples.push(failureSample(agent, source, record));
    }
    for (const record of failureStats.agentToolUsageRecords) {
      if (agentToolUsageFailureSamples.length >= 10) break;
      agentToolUsageFailureSamples.push(failureSample(agent, source, record));
    }
    for (const record of failureStats.agentEvidenceProbeRecords) {
      if (agentEvidenceProbeFailureSamples.length >= 10) break;
      agentEvidenceProbeFailureSamples.push(failureSample(agent, source, record));
    }
    allRecords.push(...records.map((record) => ({ ...record, agent_id: agent.id })));
  }

  const profileAgentCount = profiles.filter((profile) => profile.source === "profile").length;
  const transcriptAgentCount = profiles.filter((profile) => profile.source === "transcript").length;
  return {
    profile_dir: profileDir,
    agent_count: agents.length,
    telemetry_agent_count: profiles.length,
    profiled_agent_count: profileAgentCount,
    profile_agent_count: profileAgentCount,
    transcript_agent_count: transcriptAgentCount,
    missing_agent_profile_count: agents.length - profileAgentCount,
    missing_agent_telemetry_count: missing.length,
    records: allRecords.length,
    recorded_ms: allRecords.reduce((sum, record) => sum + Math.max(0, Number(record.duration_ms || 0)), 0),
    command_rollup: commandRollup(allRecords),
    unresolved_failed_records: failed.unresolved,
    unresolved_failure_samples: unresolvedFailureSamples,
    same_agent_recovered_failed_records: failed.resolvedLater,
    recovered_failed_records: failed.resolvedLater,
    total_recovered_failed_records: failed.resolvedLater + failed.externallyRecovered,
    parent_recovered_failed_records: failed.externallyRecovered,
    parent_exact_recovered_failed_records: failed.parentExactRecovered,
    parent_node_test_file_recovered_failed_records: failed.parentNodeTestFileRecovered,
    environment_blocked_failed_records: failed.environmentBlocked,
    agent_tool_usage_failed_records: failed.agentToolUsage,
    agent_evidence_probe_failed_records: failed.agentEvidenceProbe,
    agent_tool_usage_reasons: [...agentToolUsageReasons.entries()].map(([reason, count]) => ({ reason, count })),
    agent_evidence_probe_reasons: [...agentEvidenceProbeReasons.entries()].map(([reason, count]) => ({ reason, count })),
    agent_tool_usage_failure_samples: agentToolUsageFailureSamples,
    agent_evidence_probe_failure_samples: agentEvidenceProbeFailureSamples,
    agent_tool_usage_clean_tail_agents: agentToolUsageCleanTailAgents,
    agent_tool_usage_prevention_hints: [],
    errors,
    profiles,
    missing,
  };
}

function strictAgentRollupProblems(rollup) {
  if (!rollup?.enabled) return [];
  const problems = [];
  if (rollup.ok !== true) {
    problems.push(...(rollup.problems?.length ? rollup.problems : ["agent rollup trace/count check failed"]));
  }
  const profileRollup = rollup.profile_rollup;
  if (profileRollup) {
    if (Number(profileRollup.unresolved_failed_records || 0) > 0) {
      problems.push(`unresolved agent failures: ${profileRollup.unresolved_failed_records}`);
    }
    if (Number(profileRollup.missing_agent_telemetry_count || 0) > 0) {
      problems.push(`missing telemetry for ${profileRollup.missing_agent_telemetry_count} subagent session(s)`);
    }
    if ((profileRollup.errors || []).length > 0) {
      problems.push(`agent telemetry parse errors: ${profileRollup.errors.length}`);
    }
  }
  return problems;
}

function buildAgentRollup(values, parentRecords = []) {
  if (values["agent-rollup"] !== true && values.agents !== true) return { enabled: false };
  const traceSession = stringArg(values, "trace-session", "");
  const parentThreadId = stringArg(values, "parent-thread-id", "") || readSessionMetaId(process.env.CODEX_SESSION_FILE || "");
  const sessionRoot = stringArg(values, "session-root", "") || todaySessionRoot();
  const cwd = stringArg(values, "agent-cwd", stringArg(values, "cwd", process.cwd()));
  const rawMinAgents = numberArg(values, "min-agents");
  const minAgents = Number.isFinite(rawMinAgents)
    ? Math.max(0, rawMinAgents)
    : (values["require-agent-rollup-ok"] === true ? 1 : 0);
  if (!parentThreadId && !traceSession) {
    const rollup = {
      enabled: true,
      source: "none",
      ok: false,
      parent_thread_id: "",
      trace_session: "",
      session_root: sessionRoot,
      cwd,
      min_agents: minAgents,
      calls_count: 0,
      subagent_session_count: 0,
      count: 0,
      roles: [],
      first_agent_ts: "",
      latest_agent_ts: "",
      agents: [],
      profile_rollup: buildAgentProfileRollup([], values, parentRecords),
      problems: ["missing parent thread id for agent rollup"],
    };
    const strictProblems = strictAgentRollupProblems(rollup);
    return {
      ...rollup,
      strict_ok: strictProblems.length === 0,
      strict_problems: strictProblems,
    };
  }

  const trace = traceSession
    ? buildOrchestrationTrace({ session: traceSession, sessionRoot, parentThreadId, minAgents, cwd })
    : buildTrace({ sessionRoot, parentThreadId, minAgents, cwd });
  const agents = trace.agents || trace.subagentSessions || [];
  const profileRollup = buildAgentProfileRollup(agents, values, parentRecords);
  profileRollup.agent_tool_usage_prevention_hints = agentToolUsagePreventionHints(profileRollup, {
    parent_thread_id: parentThreadId,
    session_root: sessionRoot,
    cwd,
  });
  const rollup = {
    enabled: true,
    ok: trace.ok,
    source: traceSource(traceSession, parentThreadId),
    parent_thread_id: parentThreadId,
    trace_session: traceSession,
    session_root: sessionRoot,
    cwd,
    min_agents: minAgents,
    calls_count: trace.calls?.length || 0,
    subagent_session_count: trace.count ?? trace.subagentSessionCount ?? agents.length,
    count: agents.length,
    roles: roleRollup(agents),
    first_agent_ts: agents[0]?.timestamp || "",
    latest_agent_ts: agents[agents.length - 1]?.timestamp || "",
    agents,
    profile_rollup: profileRollup,
    problems: trace.problems,
  };
  const strictProblems = strictAgentRollupProblems(rollup);
  return {
    ...rollup,
    strict_ok: strictProblems.length === 0,
    strict_problems: strictProblems,
  };
}

function buildAgentRollupHint(values) {
  if (values["agent-rollup"] === true || values.agents === true) return null;
  const parentThreadId = readSessionMetaId(process.env.CODEX_SESSION_FILE || "");
  if (!parentThreadId) return null;
  const sessionRoot = stringArg(values, "session-root", "") || todaySessionRoot();
  const cwd = stringArg(values, "agent-cwd", stringArg(values, "cwd", process.cwd()));
  const trace = buildTrace({ sessionRoot, parentThreadId, minAgents: 0, cwd });
  if ((trace.count || 0) === 0) return null;
  const command = [
    "node", "tools/ai.mjs", "status",
    ...statusSelectionArgs(values),
    "--agent-rollup",
    "--parent-thread-id", parentThreadId,
    "--session-root", sessionRoot,
    "--agent-cwd", cwd,
    ...(values["no-import-codex-session"] === true ? ["--no-import-codex-session"] : []),
  ].map(formatCommandArg).join(" ");
  return {
    parent_thread_id: parentThreadId,
    session_root: sessionRoot,
    cwd,
    subagent_session_count: trace.count || 0,
    command,
  };
}

function statusSelectionArgs(values) {
  const out = [];
  const profile = stringArg(values, "profile", "");
  const session = stringArg(values, "session", "");
  const harness = stringArg(values, "harness", "");
  if (profile) out.push("--profile", profile);
  if (!profile && session) out.push("--session", session);
  if (harness) out.push("--harness", harness);
  if (values.all === true) out.push("--all");
  if (values.verbose === true) out.push("--verbose");
  return out;
}

function formatCommandArg(value) {
  const text = String(value);
  if (!/\s/.test(text)) return text;
  return `"${text.replaceAll('"', '\\"')}"`;
}

function currentOrchestrationPreflightGuidance() {
  try {
    const taskRoot = findTaskboardRoot(process.cwd());
    const taskIds = currentDoingOrchestrationTaskIds(taskRoot);
    if (taskIds.length === 1) return "preflight the current task with `node tools/ai.mjs orchestration-check --current --json`";
    if (taskIds.length === 0) return "create one current orchestration task with `node tools/ai.mjs orchestration-bootstrap` using bounded packet fields, then run `node tools/ai.mjs orchestration-check --current --json`";
    return "resolve multiple current `doing` pipeline/orchestration tasks to exactly one, then run `node tools/ai.mjs orchestration-check --current --json`";
  } catch {
    // Status should stay diagnostic even if a taskboard checkout is incomplete.
  }
  return "preflight the current task with `node tools/ai.mjs orchestration-check --current --json`";
}

function buildStatus(profilePaths, values = {}) {
  const files = Array.isArray(profilePaths) ? profilePaths : [profilePaths];
  const parsed = parseProfiles(files);
  const records = parsed.records;
  const failedRecords = records.filter((record) => record.result === "fail").length;
  const failedClassification = classifyFailedRecords(records);
  const coverage = coverageStats(records);
  const latest = latestRecord(records);
  const slowest = records
    .filter((record) => Number(record.duration_ms || 0) > 0)
    .sort((a, b) => Number(b.duration_ms || 0) - Number(a.duration_ms || 0))[0] || null;
  const lowCoverage = isLowCoverage(coverage);
  const agentRollup = buildAgentRollup(values, records);
  const agentRollupHint = buildAgentRollupHint(values);
  const unresolvedAgentFailures = Number(agentRollup?.profile_rollup?.unresolved_failed_records || 0);
  const missingAgentTelemetry = Number(agentRollup?.profile_rollup?.missing_agent_telemetry_count || 0);
  const agentToolUsageFailures = Number(agentRollup?.profile_rollup?.agent_tool_usage_failed_records || 0);
  const agentToolUsagePreventionHints = agentRollup?.profile_rollup?.agent_tool_usage_prevention_hints || [];
  const agentToolUsageCleanTailAgents = Number(agentRollup?.profile_rollup?.agent_tool_usage_clean_tail_agents || 0);

  let nextAction;
  if (!parsed.exists) {
    nextAction = "No session profile yet; the hook records every tool call automatically once commands run.";
  } else if (parsed.errors.length > 0) {
    nextAction = "Fix the invalid JSONL lines before trusting this profile.";
  } else if (records.length === 0) {
    nextAction = "No tool calls recorded yet in this session.";
  } else if (failedClassification.unresolved > 0) {
    nextAction = "Inspect the unresolved failed commands before drawing conclusions.";
  } else if (agentRollup?.enabled && agentRollup.ok !== true) {
    nextAction = "Fix the agent rollup evidence source or required agent count before trusting this orchestration evidence.";
  } else if (unresolvedAgentFailures > 0) {
    nextAction = "Inspect unresolved agent failure samples before trusting the orchestration rollup.";
  } else if (missingAgentTelemetry > 0) {
    nextAction = "Inspect missing subagent telemetry before trusting the orchestration rollup.";
  } else if (agentToolUsageFailures > 0 && agentToolUsagePreventionHints.length > 0 && agentToolUsageCleanTailAgents >= 3) {
    nextAction = `Recent subagents are clean of classified tool-use failures; keep the printed prevention hints in packets, ${currentOrchestrationPreflightGuidance()} before launching delegated work.`;
  } else if (agentToolUsageFailures > 0 && agentToolUsagePreventionHints.length > 0) {
    nextAction = "Apply the printed agent tool-use prevention hints to subagent packets, prompts, or templates before the next delegated run.";
  } else if (agentToolUsageFailures > 0) {
    nextAction = "Inspect agent tool-usage failure samples to improve subagent prompts, paths, or command patterns.";
  } else if (failedClassification.environmentBlocked > 0) {
    nextAction = "Environment blockers remain; prepare the required local dependencies before repeating those gates.";
  } else if (lowCoverage) {
    nextAction = "Wall-clock coverage is low; long manual/research stretches are uncaptured, so treat time-spend claims as partial.";
  } else {
    nextAction = "No profiling action needed; use the command rollup to spot slow or repeated commands.";
  }

  return {
    schema_version: 2,
    profile: files.length === 1 ? files[0] : `${files.length} session logs`,
    profile_files: files,
    exists: parsed.exists,
    valid: parsed.errors.length === 0,
    errors: parsed.errors,
    records: records.length,
    latest_record: latest ? {
      ts: latest.ts || "",
      line: latest.__line,
      phase: latest.phase || "",
      category: latest.category || "",
      intent: latest.intent || "",
    } : null,
    failed_records: failedRecords,
    resolved_later_failed_records: failedClassification.resolvedLater,
    recovered_failed_records: failedClassification.resolvedLater,
    environment_blocked_failed_records: failedClassification.environmentBlocked,
    environment_blocked_reasons: failedClassification.environmentBlockedReasons,
    unresolved_failed_records: failedClassification.unresolved,
    wall_clock_coverage: coverage,
    low_profile_coverage: lowCoverage,
    command_rollup: commandRollup(records),
    agent_rollup: agentRollup,
    agent_rollup_hint: agentRollupHint,
    slowest_record: slowest ? {
      line: slowest.__line,
      duration_ms: Number(slowest.duration_ms || 0),
      phase: slowest.phase || "",
      category: slowest.category || "",
      intent: slowest.intent || "",
      result: slowest.result || "",
      commands: slowest.commands || [],
    } : null,
    next_action: nextAction,
  };
}

function compactAgentRollupEvidence(status) {
  const rollup = status.agent_rollup || {};
  const profileRollup = rollup.profile_rollup || {};
  return {
    schema_version: 2,
    kind: "status-agent-rollup-evidence",
    generated_at: new Date().toISOString(),
    valid: status.valid === true,
    errors: Array.isArray(status.errors) ? status.errors : [],
    agent_rollup: {
      enabled: rollup.enabled === true,
      ok: rollup.ok === true,
      strict_ok: rollup.strict_ok === true,
      source: rollup.source || "",
      parent_thread_id: rollup.parent_thread_id || "",
      trace_session: rollup.trace_session || "",
      min_agents: Number(rollup.min_agents ?? 0),
      subagent_session_count: Number(rollup.subagent_session_count ?? 0),
      count: Number(rollup.count ?? 0),
      roles: Array.isArray(rollup.roles)
        ? rollup.roles.map((item) => ({
          role: String(item.role || ""),
          count: Number(item.count || 0),
        }))
        : [],
      problems: Array.isArray(rollup.problems) ? rollup.problems : [],
      strict_problems: Array.isArray(rollup.strict_problems) ? rollup.strict_problems : [],
      profile_rollup: {
        missing_agent_telemetry_count: Number(profileRollup.missing_agent_telemetry_count ?? 0),
        unresolved_failed_records: Number(profileRollup.unresolved_failed_records ?? 0),
        errors: Array.isArray(profileRollup.errors) ? profileRollup.errors : [],
      },
    },
  };
}

function renderStatus(status, { verbose }) {
  const lines = [];
  lines.push(`# AI Profile - ${basename(status.profile)}`);
  lines.push("");
  lines.push(`Profile: ${status.profile}`);
  if (status.profile_files.length > 1) lines.push(`Profile files: ${status.profile_files.join(", ")}`);
  lines.push(`Records: ${status.records}`);
  lines.push(`Unresolved failures: ${status.unresolved_failed_records}`);
  lines.push(`Resolved later failures: ${status.resolved_later_failed_records}`);
  lines.push(`Environment-blocked failures: ${status.environment_blocked_failed_records}`);
  const coverage = status.wall_clock_coverage;
  lines.push(`Active work: ${formatMs(coverage.active_ms)} of ${formatMs(coverage.effective_span_ms)} effective (${formatPercent(coverage.coverage_ratio)})${coverage.idle_ms > 0 ? `; ${formatMs(coverage.idle_ms)} idle excluded` : ""}`);
  if (status.latest_record) {
    lines.push(`Latest: line ${status.latest_record.line} ${status.latest_record.ts} [${status.latest_record.phase}/${status.latest_record.category}] ${status.latest_record.intent}`);
  }

  lines.push("");
  lines.push("## Slowest Recorded Work");
  const slowest = status.slowest_record;
  if (slowest) {
    lines.push(`- line ${slowest.line}: ${formatMs(slowest.duration_ms)} [${slowest.phase}/${slowest.category}] ${slowest.intent}`);
    if (slowest.commands.length > 0) lines.push(`- command: ${slowest.commands[0]}`);
  } else {
    lines.push("- none recorded");
  }

  const rollup = status.command_rollup;
  if (rollup.by_time.length > 0) {
    lines.push("");
    lines.push("## Top Time-Sinks (by total duration)");
    for (const entry of rollup.by_time) {
      lines.push(`- ${entry.key}: ${formatMs(entry.total_ms)} total over ${entry.count} run(s), max ${formatMs(entry.max_ms)}${entry.fails > 0 ? `, ${entry.fails} failed` : ""}`);
    }
  }
  if (rollup.by_count.length > 0 && rollup.by_count[0].count > 1) {
    lines.push("");
    lines.push("## Most-Run Commands (repeats/retries = friction)");
    for (const entry of rollup.by_count) {
      if (entry.count < 2) continue;
      lines.push(`- ${entry.key}: ${entry.count} run(s)${entry.total_ms > 0 ? `, ${formatMs(entry.total_ms)} total` : ""}${entry.fails > 0 ? `, ${entry.fails} failed` : ""}`);
    }
  }

  const agentRollup = status.agent_rollup;
  if (agentRollup?.enabled) {
    lines.push("");
    lines.push("## Agent Rollup");
    if (!agentRollup.ok) {
      for (const problem of agentRollup.problems) lines.push(`- problem: ${problem}`);
    }
    if (agentRollup.strict_ok === false) {
      for (const problem of agentRollup.strict_problems || []) lines.push(`- strict problem: ${problem}`);
    }
    lines.push(`- source: ${agentRollup.source}`);
    lines.push(`- transcript calls: ${agentRollup.calls_count}`);
    lines.push(`- subagent sessions: ${agentRollup.subagent_session_count}`);
    if (agentRollup.parent_thread_id) lines.push(`- parent thread: ${agentRollup.parent_thread_id}`);
    if (agentRollup.roles.length > 0) {
      lines.push(`- roles: ${agentRollup.roles.map((entry) => `${entry.role}=${entry.count}`).join(", ")}`);
    }
    if (agentRollup.first_agent_ts || agentRollup.latest_agent_ts) {
      lines.push(`- span: ${agentRollup.first_agent_ts || "unknown"} -> ${agentRollup.latest_agent_ts || "unknown"}`);
    }
    const profileRollup = agentRollup.profile_rollup;
    if (profileRollup && profileRollup.agent_count > 0) {
      lines.push("");
      lines.push("## Agent Profile Rollup");
      lines.push(`- telemetry agents: ${profileRollup.telemetry_agent_count}/${profileRollup.agent_count}`);
      lines.push(`- sources: profiles=${profileRollup.profile_agent_count}, transcripts=${profileRollup.transcript_agent_count}`);
      lines.push(`- telemetry records: ${profileRollup.records}`);
      lines.push(`- recorded command time: ${formatMs(profileRollup.recorded_ms)}`);
      if (profileRollup.unresolved_failed_records > 0) {
        lines.push(`- unresolved agent failures: ${profileRollup.unresolved_failed_records}`);
        const sampleLimit = verbose ? 10 : 3;
        const samples = profileRollup.unresolved_failure_samples.slice(0, sampleLimit);
        for (const sample of samples) {
          lines.push(renderFailureSample(sample));
        }
        const hiddenSamples = Math.max(0, profileRollup.unresolved_failed_records - samples.length);
        if (hiddenSamples > 0) lines.push(`- ... ${hiddenSamples} more unresolved agent failure(s) not shown`);
      }
      if (profileRollup.parent_recovered_failed_records > 0) {
        lines.push(`- parent-recovered agent failures: ${profileRollup.parent_recovered_failed_records}`);
      }
      if (profileRollup.agent_tool_usage_failed_records > 0) {
        lines.push(`- agent tool-usage failures: ${profileRollup.agent_tool_usage_failed_records}`);
        if (profileRollup.agent_tool_usage_clean_tail_agents > 0) {
          lines.push(`- agent tool-usage clean tail: ${profileRollup.agent_tool_usage_clean_tail_agents} agent(s)`);
        }
        const sampleLimit = verbose ? 10 : 3;
        const samples = profileRollup.agent_tool_usage_failure_samples.slice(0, sampleLimit);
        for (const sample of samples) {
          lines.push(renderAgentToolUsageSample(sample));
        }
        const hiddenSamples = Math.max(0, profileRollup.agent_tool_usage_failed_records - samples.length);
        if (hiddenSamples > 0) lines.push(`- ... ${hiddenSamples} more agent tool-usage failure(s) not shown`);
        for (const item of profileRollup.agent_tool_usage_prevention_hints || []) {
          lines.push(`- prevent ${item.reason}: ${item.hint}`);
        }
      }
      if (profileRollup.agent_evidence_probe_failed_records > 0) {
        lines.push(`- agent evidence-probe failures: ${profileRollup.agent_evidence_probe_failed_records}`);
        const sampleLimit = verbose ? 10 : 3;
        const samples = profileRollup.agent_evidence_probe_failure_samples.slice(0, sampleLimit);
        for (const sample of samples) {
          lines.push(renderAgentEvidenceProbeSample(sample));
        }
        const hiddenSamples = Math.max(0, profileRollup.agent_evidence_probe_failed_records - samples.length);
        if (hiddenSamples > 0) lines.push(`- ... ${hiddenSamples} more agent evidence-probe failure(s) not shown`);
      }
      const agentTimeSinks = profileRollup.command_rollup.by_time.slice(0, 3);
      if (agentTimeSinks.length > 0) {
        for (const entry of agentTimeSinks) {
          lines.push(`- ${entry.key}: ${formatMs(entry.total_ms)} total over ${entry.count} run(s), max ${formatMs(entry.max_ms)}${entry.fails > 0 ? `, ${entry.fails} failed` : ""}`);
        }
      }
      if (verbose && profileRollup.missing.length > 0) {
        lines.push(`- missing telemetry: ${profileRollup.missing.length}`);
        for (const agent of profileRollup.missing.slice(0, 10)) {
          lines.push(`- missing: ${agent.id} ${agent.nickname || "(unnamed)"} [${agent.role || "(no-role)"}]`);
        }
      }
      if (verbose && profileRollup.errors.length > 0) {
        for (const error of profileRollup.errors) lines.push(`- profile error: ${error}`);
      }
    }
    if (verbose && agentRollup.agents.length > 0) {
      for (const agent of agentRollup.agents.slice(0, 10)) {
        lines.push(`- ${agent.id} ${agent.nickname || "(unnamed)"} [${agent.role || "(no-role)"}]`);
      }
      if (agentRollup.agents.length > 10) lines.push(`- ... ${agentRollup.agents.length - 10} more`);
    }
  }
  if (!agentRollup?.enabled && status.agent_rollup_hint) {
    lines.push("");
    lines.push("## Agent Rollup");
    lines.push("- not included in this status run");
    lines.push(`- run: \`${status.agent_rollup_hint.command}\``);
  }

  if (verbose && coverage.largest_gaps.length > 0) {
    lines.push("");
    lines.push("## Largest Coverage Gaps");
    for (const gap of coverage.largest_gaps.slice(0, 5)) {
      lines.push(`- ${formatMs(gap.duration_ms)} from ${gap.start_ts} to ${gap.end_ts} (lines ${gap.previous_line}-${gap.next_line})`);
    }
  }
  if (verbose && status.errors.length > 0) {
    lines.push("");
    lines.push("## Errors");
    for (const error of status.errors) lines.push(`- ${error}`);
  }
  if (verbose && status.environment_blocked_reasons.length > 0) {
    lines.push("");
    lines.push("## Environment Blockers");
    for (const item of status.environment_blocked_reasons) {
      lines.push(`- ${item.count}x ${item.reason}`);
    }
  }

  lines.push("");
  lines.push("## Next Action");
  lines.push(`- ${status.next_action}`);
  if (!verbose) lines.push("- Use `node tools/ai.mjs status --verbose` for coverage gaps and parse errors.");
  return `${lines.join("\n")}\n`;
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

const profilePaths = resolveProfilePaths(values);
const jsonOutputFile = stringArg(values, "json-output", "");
const status = buildStatus(profilePaths, values);
const rendered = renderStatus(status, { verbose: values.verbose === true });

if (jsonOutputFile) {
  const target = resolve(jsonOutputFile);
  mkdirSync(dirname(target), { recursive: true });
  const payload = values["agent-rollup-evidence"] === true
    ? compactAgentRollupEvidence(status)
    : status;
  writeFileSync(target, `${JSON.stringify(payload, null, 2)}\n`, "utf8");
}
process.stdout.write(rendered);
if (values["require-agent-rollup-ok"] === true && status.agent_rollup?.ok !== true) {
  process.exit(1);
}
if (values["require-agent-rollup-ok"] === true && status.agent_rollup?.strict_ok === false) {
  process.exit(1);
}
