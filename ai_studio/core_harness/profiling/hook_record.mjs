#!/usr/bin/env node
// Cross-harness profiling hook handler.
//
// Fallback/rich handler for the native hook_record_fast hot path.
// Default harness configs use PostToolUse / SessionStart only:
//   - Claude Code:  .claude/settings.json
//   - Codex CLI:    .codex/hooks.json
// The harness runs this automatically on every tool call, so profiling coverage
// is not dependent on the agent remembering to call `ai.mjs start/run`.
//
// It reads the hook JSON payload on stdin (Claude and Codex share the shape:
// hook_event_name / tool_name / tool_input.command / tool_response) and appends
// one passive profile record to the session JSONL that `ai.mjs status` reads.
// Codex currently may skip PostToolUse for failed shell commands, so successful
// Codex hook invocations also recover missed failed shell calls from the local
// Codex session transcript and dedupe by call_id.
//
// Hard rule: NEVER break or slow the harness. Any error -> exit 0 silently.
// Pass the harness name as argv[2] or AI_PROFILE_HARNESS (claude|codex).
// Manual recovery:
//   node ai_studio/core_harness/profiling/hook_record.mjs codex --recover-only [--profile <profile.jsonl>] [--session <codex-session.jsonl>]

import { closeSync, existsSync, openSync, readFileSync, readSync, readdirSync, statSync } from "node:fs";
import { homedir } from "node:os";
import { join, resolve } from "node:path";

let profileLibPromise;

function usage() {
  console.error(`usage:
  node ai_studio/core_harness/profiling/hook_record.mjs [claude|codex|agent]
  node ai_studio/core_harness/profiling/hook_record.mjs codex --recover-only [--profile <profile.jsonl>] [--session <codex-session.jsonl>]

Records harness hook payloads from stdin. --recover-only imports missed failed
Codex shell calls from a Codex session JSONL into the normal profile.`);
}

function parseCliArgs(args) {
  const options = { harness: "", recoverOnly: false, profile: "", session: "", help: false };
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--help" || arg === "-h") {
      options.help = true;
    } else if (arg === "--recover-only") {
      options.recoverOnly = true;
    } else if (arg === "--profile") {
      options.profile = args[index + 1] && !args[index + 1].startsWith("--") ? args[index + 1] : "";
      if (options.profile) index += 1;
    } else if (arg === "--session") {
      options.session = args[index + 1] && !args[index + 1].startsWith("--") ? args[index + 1] : "";
      if (options.session) index += 1;
    } else if (!arg.startsWith("--") && !options.harness) {
      options.harness = arg;
    }
  }
  return options;
}

function loadProfileLib() {
  profileLibPromise ??= import("./profile_lib.mjs");
  return profileLibPromise;
}

function pick(obj, ...keys) {
  if (!obj || typeof obj !== "object") return undefined;
  for (const k of keys) if (obj[k] !== undefined && obj[k] !== null) return obj[k];
  return undefined;
}

function inferCategory(cmd) {
  const c = cmd.toLowerCase();
  if (/\b(node --test|--test |unittest|cmake --build|pipeline_validate|taskboard.*validate|pytest|py -3\.\d+ -m unittest)\b/.test(c)) return "validation";
  if (/\bgit (commit|add|push|status|log|diff)\b/.test(c)) return "task_status";
  if (/\b(cmake|ninja|gcc|clang|build)\b/.test(c)) return "implementation";
  if (/\b(grep|find|ls|cat|rg|glob)\b/.test(c)) return "research";
  return "tooling";
}

function stripLeadingEnvAssignments(cmd) {
  let c = cmd.trim();
  while (/^[A-Za-z_][A-Za-z0-9_]*=("[^"]*"|'[^']*'|\S+)\s+/.test(c)) {
    c = c.replace(/^[A-Za-z_][A-Za-z0-9_]*=("[^"]*"|'[^']*'|\S+)\s+/, "");
  }
  return c;
}

function isReadOnlyPlumbingCommand(cmd) {
  let c = stripLeadingEnvAssignments(String(cmd || ""));
  c = c.split(/\r?\n/)[0].trim();
  if (!c) return false;

  const lower = c.toLowerCase().replace(/\s+/g, " ");
  if (/^git(\.exe)?\s+(?:-[a-z]\s+\S+\s+)*(?:status|diff|log)\b/.test(lower)) return true;
  if (/^git(\.exe)?\s+-c\s+\S+\s+(?:-[a-z]\s+\S+\s+)*(?:status|diff|log)\b/.test(lower)) return true;

  return /^(?:get-content|get-childitem|test-path|select-string|where\.exe|ls|cat)(?:\s|$)/i.test(c);
}

/* Mirror is_search_command in hook_record_fast.c: a search tool exiting 1 = "no
 * match", a normal outcome, not a failure. Keep the C hot path and JS fallback
 * in agreement (the parity test in test.mjs enforces this). */
function isSearchCommand(cmd) {
  const c = stripLeadingEnvAssignments(String(cmd || "")).split(/\r?\n/)[0].trim().toLowerCase();
  return /^(?:rg|grep|egrep|fgrep|findstr|ack|select-string)(?:\s|$)/.test(c);
}

function todaySessionDir() {
  const now = new Date();
  return join(
    process.env.CODEX_SESSION_ROOT || join(homedir(), ".codex", "sessions"),
    String(now.getFullYear()),
    String(now.getMonth() + 1).padStart(2, "0"),
    String(now.getDate()).padStart(2, "0"),
  );
}

function latestCodexSessionFile() {
  if (process.env.CODEX_SESSION_FILE) return process.env.CODEX_SESSION_FILE;
  const dir = todaySessionDir();
  if (!existsSync(dir)) return "";
  let latest = null;
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    if (!entry.isFile() || !entry.name.endsWith(".jsonl")) continue;
    const path = join(dir, entry.name);
    const stats = statSync(path);
    if (!latest || stats.mtimeMs > latest.mtimeMs) latest = { path, mtimeMs: stats.mtimeMs };
  }
  return latest?.path || "";
}

function seenRecoveredCallIds(profilePath, defaultProfilePath) {
  const target = resolve(profilePath || defaultProfilePath());
  if (!existsSync(target)) return new Set();
  const seen = new Set();
  for (const rawLine of readFileSync(target, "utf8").split(/\r?\n/)) {
    if (!rawLine.trim()) continue;
    try {
      const record = JSON.parse(rawLine);
      if (record.source_call_id) seen.add(String(record.source_call_id));
    } catch {
      // Ignore malformed historical profile lines; status.mjs reports them.
    }
  }
  return seen;
}

function parseCodexCommandArguments(raw) {
  try {
    const parsed = JSON.parse(String(raw || "{}"));
    return String(parsed.command || "").trim();
  } catch {
    return "";
  }
}

function responseText(response) {
  if (!response || typeof response !== "object") return String(response || "");
  return [
    response.output,
    response.stdout,
    response.stderr,
    response.message,
    response.error,
  ].filter((value) => value !== undefined && value !== null).map(String).join("\n");
}

function environmentBlockReason(command, output) {
  const text = `${command || ""}\n${output || ""}`;
  if (
    /no working Python runner found/i.test(text)
    || /ModuleNotFoundError:\s*No module named ['"]?(?:PIL|numpy|scipy)['"]?/i.test(text)
    || /AI_PIPELINE_PYTHON/i.test(text) && /tools\/requirements\/ai-pipeline-full\.txt|ai-pipeline-full\.txt/i.test(text)
  ) {
    return "missing full-gate Python modules; install tools/requirements/ai-pipeline-full.txt or set AI_PIPELINE_PYTHON";
  }
  return "";
}

function readSessionTailLines(sessionFile) {
  const maxBytes = Math.max(4096, Number(process.env.CODEX_SESSION_RECOVERY_BYTES || 1024 * 1024));
  const stats = statSync(sessionFile);
  if (stats.size <= maxBytes) return readFileSync(sessionFile, "utf8").split(/\r?\n/);

  const fd = openSync(sessionFile, "r");
  try {
    const start = Math.max(0, stats.size - maxBytes);
    const buffer = Buffer.alloc(stats.size - start);
    readSync(fd, buffer, 0, buffer.length, start);
    const text = buffer.toString("utf8");
    return text.slice(text.indexOf("\n") + 1).split(/\r?\n/);
  } finally {
    closeSync(fd);
  }
}

async function recoverCodexFailedCommands(profilePath, harness, stamp = {}) {
  if (harness !== "codex") return;
  const sessionFile = latestCodexSessionFile();
  if (!sessionFile || !existsSync(sessionFile)) return;

  const { appendRecord, buildRecord, defaultProfilePath } = await loadProfileLib();
  const commandsByCallId = new Map();
  const seen = seenRecoveredCallIds(profilePath, defaultProfilePath);
  for (const rawLine of readSessionTailLines(sessionFile)) {
    if (!rawLine.trim()) continue;
    let line;
    try { line = JSON.parse(rawLine); } catch { continue; }
    const payload = line?.payload;
    if (line?.type !== "response_item" || !payload) continue;

    if (payload.type === "function_call") {
      const command = parseCodexCommandArguments(payload.arguments);
      if (command) commandsByCallId.set(String(payload.call_id || ""), command);
      continue;
    }

    if (payload.type !== "function_call_output") continue;
    const callId = String(payload.call_id || "");
    if (!callId || seen.has(callId)) continue;
    const output = String(payload.output || "");
    const exitMatch = output.match(/^Exit code:\s*(-?\d+)/m);
    if (!exitMatch) continue;
    const exitCode = Number(exitMatch[1]);
    if (!Number.isFinite(exitCode) || exitCode === 0) continue;
    const command = commandsByCallId.get(callId);
    if (!command) continue;
    const cmd1 = command.split(/\r?\n/)[0].trim().slice(0, 200);
    if (exitCode === 1 && isSearchCommand(cmd1)) continue;
    const blockedBy = environmentBlockReason(cmd1, output);
    appendRecord(profilePath, buildRecord({
      phase: "session",
      category: inferCategory(cmd1),
      result: "fail",
      value: blockedBy ? "necessary_overhead" : "rework",
      intent: "auto:codex-session-recovery",
      tool: ["codex/session-jsonl"],
      command: [cmd1],
      ...(blockedBy ? { "blocked-by": blockedBy } : {}),
    }, {
      event_type: "tool_call_result_recovered",
      source_call_id: callId,
      source_session_file: sessionFile,
      exit_code: exitCode,
      ...(blockedBy ? { failure_kind: "environment_blocked" } : {}),
      ...stamp,
    }));
    seen.add(callId);
  }
}

function readStdin() {
  return new Promise((resolve) => {
    let data = "";
    let done = false;
    const finish = () => { if (!done) { done = true; resolve(data); } };
    try {
      process.stdin.setEncoding("utf8");
      process.stdin.on("data", (c) => (data += c));
      process.stdin.on("end", finish);
      process.stdin.on("error", finish);
    } catch { finish(); }
    setTimeout(finish, 400); // never hang the harness
  });
}

(async () => {
  try {
    const options = parseCliArgs(process.argv.slice(2));
    if (options.help) {
      usage();
      process.exit(0);
    }
    if (options.session) process.env.CODEX_SESSION_FILE = options.session;

    const raw = await readStdin();
    let payload = {};
    try { payload = JSON.parse(raw || "{}"); } catch { payload = {}; }

    const harness = options.harness || process.env.AI_PROFILE_HARNESS || "agent";
    const event = String(pick(payload, "hook_event_name", "hookEventName") || "PostToolUse");

    /* Route to the SAME per-session file the native hot path uses, and stamp
     * session_id/harness/cwd, so the fallback + Codex recovery never re-mix
     * parallel work into one daily file. */
    const { sessionProfilePathFor, deriveSessionId } = await loadProfileLib();
    let rawSession = pick(payload, "session_id", "sessionId", "conversation_id") || "";
    if (!rawSession && harness === "codex") rawSession = latestCodexSessionFile();
    const session = deriveSessionId(rawSession);
    const profilePath = options.profile || process.env.AI_PROFILE_FILE || (session.short ? sessionProfilePathFor(harness, session.short) : "");
    const stamp = { harness, cwd: process.cwd() };
    if (session.full) stamp.session_id = session.full;

    if (options.recoverOnly || process.env.AI_PROFILE_RECOVER_ONLY === "1") {
      await recoverCodexFailedCommands(profilePath, harness, stamp);
      process.exit(0);
    }

    if (event === "SessionStart") {
      const { appendRecord, buildRecord } = await loadProfileLib();
      appendRecord(profilePath, buildRecord({
        phase: "session", category: "context", result: "pass",
        intent: `session start (${harness})`, tool: [`${harness}/session`],
      }, { event_type: "session_start", ...stamp }));
      await recoverCodexFailedCommands(profilePath, harness, stamp);
      process.exit(0);
    }

    const tool = String(pick(payload, "tool_name", "toolName") || "");
    const input = pick(payload, "tool_input", "toolInput") || {};
    const response = pick(payload, "tool_response", "toolResponse") || {};

    /* Subagent-spawn telemetry (advisory, cross-harness): record Claude Agent/Task
     * and Codex spawn_agent calls so `status` shows what the lead delegated this
     * session. Diagnostic only — never an acceptance gate. */
    const isSubagentSpawn = event === "PostToolUse"
      && (/^(?:agent|task)$/i.test(tool)
        || /(?:^|[._-])spawn_agent$/i.test(tool)
        || pick(input, "subagent_type", "agentType", "agent_type") !== undefined);
    if (isSubagentSpawn) {
      const objectiveRaw = pick(input, "description", "objective", "task", "prompt", "instructions", "instruction") || "";
      const objective = String(objectiveRaw).split(/\r?\n/)[0].trim().slice(0, 200);
      const subType = String(pick(input, "subagent_type", "agentType", "agent_type") || "").trim();
      const { appendRecord, buildRecord } = await loadProfileLib();
      appendRecord(profilePath, buildRecord({
        phase: "session",
        category: "delegation",
        result: "pass",
        value: "necessary_overhead",
        intent: `subagent:${subType || tool || "agent"}`,
        tool: [`${harness}/${tool || "agent"}`],
        ...(objective ? { command: [objective] } : {}),
      }, { event_type: "subagent_spawn", subagent_type: subType, ...stamp }));
      process.exit(0);
    }

    const command = pick(input, "command");
    if (!command) process.exit(0); // only profile command/shell tool calls

    const cmd1 = String(command).split(/\r?\n/)[0].trim().slice(0, 200);
    const baseValues = {
      phase: "session",
      category: inferCategory(cmd1),
      intent: `auto:${tool || "shell"}`,
      tool: [`${harness}/${tool || "shell"}`],
      command: [cmd1],
    };

    if (event === "PreToolUse") {
      if (isReadOnlyPlumbingCommand(cmd1)) process.exit(0);
      const { appendRecord, buildRecord } = await loadProfileLib();
      appendRecord(profilePath, buildRecord({
        ...baseValues,
        result: "unknown",
        value: "necessary_overhead",
      }, { event_type: "tool_call_start", ...stamp }));
      process.exit(0);
    }

    const exit = pick(response, "exit_code", "exitCode", "code", "returncode");
    const isErr = pick(response, "is_error", "isError", "error");
    const errored = isErr === true || (typeof isErr === "string" && isErr.length > 0);
    const nonZero = typeof exit === "number" && exit !== 0;
    // search exit 1 = "no match" -> pass (mirror hook_record_fast.c); exit 2 / real error still fails.
    const searchNoMatch = exit === 1 && !errored && isSearchCommand(cmd1);
    const result = (nonZero || errored) && !searchNoMatch ? "fail" : "pass";

    if (result !== "fail" && isReadOnlyPlumbingCommand(cmd1)) process.exit(0);

    const { appendRecord, buildRecord } = await loadProfileLib();
    const blockedBy = result === "fail" ? environmentBlockReason(cmd1, responseText(response)) : "";
    appendRecord(profilePath, buildRecord({
      ...baseValues,
      result,
      value: blockedBy ? "necessary_overhead" : (result === "fail" ? "rework" : "unknown"),
      ...(blockedBy ? { "blocked-by": blockedBy } : {}),
    }, { event_type: "tool_call_result", ...(blockedBy ? { failure_kind: "environment_blocked" } : {}), ...stamp }));
    await recoverCodexFailedCommands(profilePath, harness, stamp);
  } catch {
    // swallow — a profiling hook must never disrupt the agent
  }
  process.exit(0);
})();
