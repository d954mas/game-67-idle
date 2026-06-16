#!/usr/bin/env node
// Cross-harness profiling hook handler.
//
// Wired as a PreToolUse / PostToolUse / SessionStart hook in BOTH harnesses:
//   - Claude Code:  .claude/settings.json   (hooks.PreToolUse/PostToolUse ...)
//   - Codex CLI:    .codex/hooks.json        (hooks.PreToolUse/PostToolUse ...)
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

import { closeSync, existsSync, openSync, readFileSync, readSync, readdirSync, statSync } from "node:fs";
import { homedir } from "node:os";
import { join, resolve } from "node:path";
import { appendRecord, buildRecord, defaultProfilePath } from "./profile_lib.mjs";

function pick(obj, ...keys) {
  if (!obj || typeof obj !== "object") return undefined;
  for (const k of keys) if (obj[k] !== undefined && obj[k] !== null) return obj[k];
  return undefined;
}

function inferCategory(cmd) {
  const c = cmd.toLowerCase();
  if (/\b(node --test|--test |unittest|cmake --build|pipeline_validate|skills_eval|taskboard.*validate|pytest|py -3\.\d+ -m unittest)\b/.test(c)) return "validation";
  if (/\bgit (commit|add|push|status|log|diff)\b/.test(c)) return "task_status";
  if (/\b(cmake|ninja|gcc|clang|build)\b/.test(c)) return "implementation";
  if (/\b(grep|find|ls|cat|rg|glob)\b/.test(c)) return "research";
  return "tooling";
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

function seenRecoveredCallIds(profilePath) {
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

function recoverCodexFailedCommands(profilePath, harness) {
  if (harness !== "codex") return;
  const sessionFile = latestCodexSessionFile();
  if (!sessionFile || !existsSync(sessionFile)) return;

  const commandsByCallId = new Map();
  const seen = seenRecoveredCallIds(profilePath);
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
    appendRecord(profilePath, buildRecord({
      phase: "session",
      category: inferCategory(cmd1),
      result: "fail",
      value: "rework",
      intent: "auto:codex-session-recovery",
      tool: ["codex/session-jsonl"],
      command: [cmd1],
    }, {
      event_type: "tool_call_result_recovered",
      source_call_id: callId,
      source_session_file: sessionFile,
      exit_code: exitCode,
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
    const raw = await readStdin();
    let payload = {};
    try { payload = JSON.parse(raw || "{}"); } catch { payload = {}; }

    const harness = process.argv[2] || process.env.AI_PROFILE_HARNESS || "agent";
    const event = String(pick(payload, "hook_event_name", "hookEventName") || "PostToolUse");

    const profilePath = process.env.AI_PROFILE_FILE || "";

    if (event === "SessionStart") {
      appendRecord(profilePath, buildRecord({
        phase: "session", category: "context", result: "pass",
        intent: `session start (${harness})`, tool: [`${harness}/session`],
      }, { event_type: "session_start" }));
      recoverCodexFailedCommands(profilePath, harness);
      process.exit(0);
    }

    const tool = String(pick(payload, "tool_name", "toolName") || "");
    const input = pick(payload, "tool_input", "toolInput") || {};
    const response = pick(payload, "tool_response", "toolResponse") || {};
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
      appendRecord(profilePath, buildRecord({
        ...baseValues,
        result: "unknown",
        value: "necessary_overhead",
      }, { event_type: "tool_call_start" }));
      process.exit(0);
    }

    const exit = pick(response, "exit_code", "exitCode", "code", "returncode");
    const isErr = pick(response, "is_error", "isError", "error");
    const result =
      (typeof exit === "number" && exit !== 0) || isErr === true || (typeof isErr === "string" && isErr.length > 0)
        ? "fail"
        : "pass";

    appendRecord(profilePath, buildRecord({
      ...baseValues,
      result,
      value: result === "fail" ? "rework" : "unknown",
    }, { event_type: "tool_call_result" }));
    recoverCodexFailedCommands(profilePath, harness);
  } catch {
    // swallow — a profiling hook must never disrupt the agent
  }
  process.exit(0);
})();
