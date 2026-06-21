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
  stringArg,
  todaySessionProfiles,
} from "./profile_lib.mjs";
import { buildOrchestrationTrace, buildTrace, todaySessionRoot } from "./orchestration_trace.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/status.mjs [--profile <p>] [--session <id>] [--harness claude|codex] [--all] [--json-output <status.json>] [--verbose] [--agent-rollup]

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
Use --trace-session <codex-session.jsonl> when checking parent transcript calls.
If CODEX_SESSION_FILE points at the parent session, --agent-rollup can infer its
id.`);
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
    for (const error of parsed.errors) errors.push(`${file}: ${error}`);
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
  return String(command || "").replaceAll("\\", "/").replace(/\s+/g, " ").trim();
}

function commandKeys(record) {
  const keys = (record.commands || []).map(normalizeCommand).filter(Boolean);
  if (record.validation_check_id) keys.push(`validation_check:${record.validation_check_id}`);
  return keys;
}

function isEnvironmentBlocked(record) {
  return record.failure_kind === "environment_blocked";
}

/* A failed record is "recovered" if the same command passed on a later line, and
 * "unresolved" otherwise -- so a retried-then-fixed command is not an alarm. */
function classifyFailedRecords(records) {
  const passedLater = new Map();
  let resolvedLater = 0;
  let environmentBlocked = 0;
  let unresolved = 0;
  const environmentBlockedReasons = new Map();
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
    else unresolved += 1;
  }
  return {
    resolvedLater,
    recovered: resolvedLater,
    environmentBlocked,
    unresolved,
    environmentBlockedReasons: [...environmentBlockedReasons.entries()].map(([reason, count]) => ({ reason, count })),
  };
}

/* Normalize a command to a tool-level key: strip leading env assignments, take
 * the first token's basename, and for interpreters append the script basename
 * (so "node tools/ai.mjs status" -> "node ai.mjs"). */
function commandKey(cmd) {
  let text = String(cmd || "").trim();
  while (/^[A-Za-z_][A-Za-z0-9_]*=(?:"[^"]*"|'[^']*'|\S+)\s+/.test(text)) {
    text = text.replace(/^[A-Za-z_][A-Za-z0-9_]*=(?:"[^"]*"|'[^']*'|\S+)\s+/, "");
  }
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

function buildAgentRollup(values) {
  if (values["agent-rollup"] !== true && values.agents !== true) return { enabled: false };
  const traceSession = stringArg(values, "trace-session", "");
  const parentThreadId = stringArg(values, "parent-thread-id", "") || readSessionMetaId(process.env.CODEX_SESSION_FILE || "");
  const sessionRoot = stringArg(values, "session-root", "") || todaySessionRoot();
  const cwd = stringArg(values, "agent-cwd", stringArg(values, "cwd", process.cwd()));
  const rawMinAgents = numberArg(values, "min-agents");
  const minAgents = Number.isFinite(rawMinAgents) ? Math.max(0, rawMinAgents) : 0;
  if (!parentThreadId && !traceSession) {
    return {
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
      problems: ["missing parent thread id for agent rollup"],
    };
  }

  const trace = traceSession
    ? buildOrchestrationTrace({ session: traceSession, sessionRoot, parentThreadId, minAgents, cwd })
    : buildTrace({ sessionRoot, parentThreadId, minAgents, cwd });
  const agents = trace.agents || trace.subagentSessions || [];
  return {
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
    problems: trace.problems,
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
  const agentRollup = buildAgentRollup(values);
  const agentRollupHint = buildAgentRollupHint(values);

  let nextAction;
  if (!parsed.exists) {
    nextAction = "No session profile yet; the hook records every tool call automatically once commands run.";
  } else if (parsed.errors.length > 0) {
    nextAction = "Fix the invalid JSONL lines before trusting this profile.";
  } else if (records.length === 0) {
    nextAction = "No tool calls recorded yet in this session.";
  } else if (failedClassification.unresolved > 0) {
    nextAction = "Inspect the unresolved failed commands before drawing conclusions.";
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
  writeFileSync(target, `${JSON.stringify(status, null, 2)}\n`, "utf8");
}
process.stdout.write(rendered);
