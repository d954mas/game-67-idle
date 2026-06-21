#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

function usage() {
  return `usage:
  node tools/ai_profile/orchestration_trace.mjs [--session <codex-session.jsonl>] [--parent-thread-id <id>] [options]

Options:
  --session <file>     Parent Codex transcript JSONL to scan for spawn/wait/close calls.
  --session-root <dir>  Directory containing rollout-*.jsonl files.
                       Defaults to today's ~/.codex/sessions/YYYY/MM/DD.
  --min-agents <n>     Required subagent count before ok=true. Default: 1.
  --cwd <path>         Only count subagent sessions from this cwd.
  --json               Emit JSON only.
  --json-output <file> Write JSON trace artifact.`;
}

function argValue(args, flag) {
  const index = args.indexOf(flag);
  if (index === -1) return "";
  const value = args[index + 1];
  return value && !value.startsWith("--") ? value : "";
}

function todaySessionRoot() {
  const now = new Date();
  return join(
    homedir(),
    ".codex",
    "sessions",
    String(now.getFullYear()),
    String(now.getMonth() + 1).padStart(2, "0"),
    String(now.getDate()).padStart(2, "0"),
  );
}

function readJsonl(file) {
  if (!file || !existsSync(file)) return [];
  return readFileSync(file, "utf8")
    .split(/\r?\n/)
    .filter((line) => line.trim())
    .map((line, index) => {
      try {
        return { line: index + 1, record: JSON.parse(line) };
      } catch {
        return { line: index + 1, record: null };
      }
    });
}

function listRollouts(dir) {
  if (!existsSync(dir)) return [];
  const out = [];
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) {
      out.push(...listRollouts(path));
    } else if (entry.isFile() && entry.name.startsWith("rollout-") && entry.name.endsWith(".jsonl")) {
      out.push(path);
    }
  }
  return out.sort();
}

function parseJsonObject(raw) {
  try {
    const parsed = JSON.parse(String(raw || "{}"));
    return parsed && typeof parsed === "object" ? parsed : {};
  } catch {
    return {};
  }
}

function parseOutputObject(raw) {
  const text = String(raw || "").trim();
  if (!text) return {};
  return parseJsonObject(text);
}

function operationFromName(name) {
  const n = String(name || "");
  if (/(^|[._-])spawn_agent$/.test(n)) return "spawn";
  if (/(^|[._-])wait_agent$/.test(n)) return "wait";
  if (/(^|[._-])close_agent$/.test(n)) return "close";
  return "";
}

function targetsForCall(operation, args, output) {
  if (operation === "spawn") {
    const id = output.agent_id || output.agentId || args.agent_id || args.agentId || args.target;
    return id ? [String(id)] : [];
  }
  if (operation === "wait") {
    const targets = Array.isArray(args.targets) ? args.targets : [args.target].filter(Boolean);
    if (targets.length === 0 && output.status && typeof output.status === "object") {
      return Object.keys(output.status).map(String);
    }
    return targets.map(String);
  }
  if (operation === "close") {
    const target = args.target || args.agent_id || args.agentId;
    return target ? [String(target)] : [];
  }
  return [];
}

function statusEntryForTarget(output, target) {
  const status = output.status && typeof output.status === "object" ? output.status : {};
  if (status[target] && typeof status[target] === "object") return status[target];
  return null;
}

function previousStatusEntry(output) {
  const status = output.previous_status || output.previousStatus;
  return status && typeof status === "object" ? status : null;
}

function hasCompletedStatus(entry) {
  return Boolean(entry && Object.hasOwn(entry, "completed"));
}

function scanSessionCalls(session) {
  const rows = readJsonl(session);
  const outputs = new Map();
  for (const row of rows) {
    const payload = row.record?.payload;
    if (row.record?.type !== "response_item" || payload?.type !== "function_call_output") continue;
    outputs.set(String(payload.call_id || ""), { line: row.line, output: parseOutputObject(payload.output), raw: String(payload.output || "") });
  }

  const calls = [];
  const unpaired = [];
  for (const row of rows) {
    const payload = row.record?.payload;
    if (row.record?.type !== "response_item" || payload?.type !== "function_call") continue;
    const operation = operationFromName(payload.name);
    if (!operation) continue;
    const callId = String(payload.call_id || "");
    const args = parseJsonObject(payload.arguments);
    const paired = outputs.get(callId);
    const output = paired?.output || {};
    const targets = targetsForCall(operation, args, output);
    const call = {
      operation,
      callId,
      line: row.line,
      outputLine: paired?.line || null,
      targets,
      output,
      name: String(payload.name || ""),
      timestamp: String(row.record.timestamp || ""),
    };
    calls.push(call);
    if (!paired) unpaired.push({ callId, operation, line: row.line });
  }
  return { calls, unpaired };
}

function analyzeCallOrder(calls, unpaired) {
  const spawned = calls.filter((call) => call.operation === "spawn");
  const missing = [];
  const unordered = [];
  const incomplete = [];
  for (const spawn of spawned) {
    for (const target of spawn.targets) {
      const wait = calls.find((call) => call.operation === "wait" && call.targets.includes(target));
      const close = calls.find((call) => call.operation === "close" && call.targets.includes(target));
      if (!wait) missing.push({ target, operation: "wait", afterCallId: spawn.callId });
      if (!close) missing.push({ target, operation: "close", afterCallId: spawn.callId });
      if (wait && wait.line < spawn.line) unordered.push({ target, operation: "wait", before: spawn.callId, callId: wait.callId });
      if (close && close.line < spawn.line) unordered.push({ target, operation: "close", before: spawn.callId, callId: close.callId });
      if (wait && close && close.line < wait.line) unordered.push({ target, operation: "close_before_wait", waitCallId: wait.callId, closeCallId: close.callId });
      if (wait) {
        if (wait.output.timed_out === true || wait.output.timedOut === true) {
          incomplete.push({ target, operation: "wait_timeout", callId: wait.callId });
        } else if (!hasCompletedStatus(statusEntryForTarget(wait.output, target))) {
          incomplete.push({ target, operation: "wait_incomplete", callId: wait.callId });
        }
      }
      if (close && !hasCompletedStatus(previousStatusEntry(close.output))) {
        incomplete.push({ target, operation: "close_incomplete", callId: close.callId });
      }
    }
    if (spawn.targets.length === 0) missing.push({ operation: "spawn_target", callId: spawn.callId });
  }
  return { missing, unordered, incomplete, unpaired };
}

function readSessionMeta(file) {
  let firstLine = "";
  try {
    firstLine = readFileSync(file, "utf8").split(/\r?\n/, 1)[0] || "";
  } catch {
    return null;
  }
  if (!firstLine.trim()) return null;
  try {
    const line = JSON.parse(firstLine);
    if (line.type !== "session_meta" || !line.payload) return null;
    return line.payload;
  } catch {
    return null;
  }
}

function subagentParentId(meta) {
  return (
    meta?.source?.subagent?.thread_spawn?.parent_thread_id
    || meta?.parent_thread_id
    || ""
  );
}

function isSubagentSession(meta) {
  return meta?.thread_source === "subagent" || Boolean(meta?.source?.subagent);
}

export function buildTrace({ sessionRoot, parentThreadId, minAgents = 1, cwd = "" }) {
  const files = listRollouts(sessionRoot);
  const wantedCwd = cwd ? resolve(cwd).toLowerCase() : "";
  const agents = [];

  for (const file of files) {
    const meta = readSessionMeta(file);
    if (!meta || !isSubagentSession(meta)) continue;
    if (subagentParentId(meta) !== parentThreadId) continue;
    if (wantedCwd && resolve(String(meta.cwd || "")).toLowerCase() !== wantedCwd) continue;
    const stats = statSync(file);
    agents.push({
      id: String(meta.id || ""),
      nickname: String(meta.agent_nickname || ""),
      role: String(meta.agent_role || ""),
      cwd: String(meta.cwd || ""),
      threadSource: String(meta.thread_source || ""),
      sessionFile: file,
      timestamp: String(meta.timestamp || ""),
      mtimeMs: stats.mtimeMs,
    });
  }

  agents.sort((a, b) => a.timestamp.localeCompare(b.timestamp) || a.id.localeCompare(b.id));
  const problems = [];
  if (!parentThreadId) problems.push("missing parent thread id");
  if (agents.length < minAgents) {
    problems.push(`expected at least ${minAgents} subagent session(s), found ${agents.length}`);
  }
  return {
    ok: problems.length === 0,
    parentThreadId,
    sessionRoot,
    minAgents,
    count: agents.length,
    agents,
    problems,
  };
}

export function buildOrchestrationTrace({ session = "", sessionRoot = todaySessionRoot(), parentThreadId = "", minAgents = 1, cwd = "" }) {
  const callTrace = session ? scanSessionCalls(session) : { calls: [], unpaired: [] };
  const order = analyzeCallOrder(callTrace.calls, callTrace.unpaired);
  const sessionTrace = parentThreadId ? buildTrace({ sessionRoot, parentThreadId, minAgents, cwd }) : {
    ok: true,
    parentThreadId,
    sessionRoot,
    minAgents,
    count: 0,
    agents: [],
    problems: [],
  };
  const problems = [
    ...order.missing.map((item) => `missing ${item.operation}${item.target ? ` for ${item.target}` : ""}`),
    ...order.unordered.map((item) => `unordered ${item.operation}${item.target ? ` for ${item.target}` : ""}`),
    ...order.incomplete.map((item) => `incomplete ${item.operation}${item.target ? ` for ${item.target}` : ""}`),
    ...order.unpaired.map((item) => `unpaired ${item.operation} call ${item.callId || "(missing call id)"}`),
    ...sessionTrace.problems,
  ];
  if (!session && !parentThreadId) {
    problems.push("missing evidence source: pass --session or --parent-thread-id");
  }
  if (session && callTrace.calls.length === 0) {
    problems.push("no multi-agent orchestration calls found in session");
  }
  return {
    ok: problems.length === 0,
    session,
    parentThreadId,
    calls: callTrace.calls,
    missing: order.missing,
    unordered: order.unordered,
    incomplete: order.incomplete,
    unpaired: order.unpaired,
    subagentSessions: sessionTrace.agents,
    subagentSessionCount: sessionTrace.count,
    problems,
  };
}

function main() {
  const args = process.argv.slice(2);
  if (args.includes("--help") || args.includes("-h")) {
    console.log(usage());
    return 0;
  }
  const session = argValue(args, "--session");
  const parentThreadId = argValue(args, "--parent-thread-id");
  const sessionRoot = argValue(args, "--session-root") || todaySessionRoot();
  const minAgentsRaw = argValue(args, "--min-agents");
  const minAgents = minAgentsRaw ? Math.max(0, Number(minAgentsRaw) || 0) : 1;
  const cwd = argValue(args, "--cwd");
  const trace = buildOrchestrationTrace({ session, sessionRoot, parentThreadId, minAgents, cwd });
  const jsonOutput = argValue(args, "--json-output");
  if (jsonOutput) {
    mkdirSync(dirname(jsonOutput), { recursive: true });
    writeFileSync(jsonOutput, `${JSON.stringify(trace, null, 2)}\n`, "utf8");
  }

  if (args.includes("--json")) {
    console.log(JSON.stringify(trace, null, 2));
  } else if (trace.ok) {
    console.log(`ok: orchestration trace passed`);
    if (trace.calls.length) console.log(`calls: ${trace.calls.length}`);
    if (trace.subagentSessionCount) console.log(`subagent sessions: ${trace.subagentSessionCount}`);
    for (const agent of trace.subagentSessions) {
      console.log(`- ${agent.id} ${agent.nickname || "(unnamed)"} ${agent.role || "(no-role)"} ${agent.sessionFile}`);
    }
  } else {
    console.log(`problem: orchestration trace incomplete`);
    for (const problem of trace.problems) console.log(`hint: ${problem}`);
    console.log(`hint: use --json for machine-readable calls[], subagentSessions[], and problems[]`);
  }
  return trace.ok ? 0 : 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exit(main());
}
