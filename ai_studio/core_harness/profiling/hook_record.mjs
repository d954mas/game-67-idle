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
// Hard rule: NEVER break or slow the harness. Any error -> exit 0 silently.
// Pass the harness name as argv[2] or AI_PROFILE_HARNESS (claude|codex).

let profileLibPromise;

function usage() {
  console.error(`usage:
  node ai_studio/core_harness/profiling/hook_record.mjs [claude|codex|agent]

Records harness hook payloads from stdin. Use status.mjs --complete for a full
Codex report sourced from the canonical transcript.`);
}

function parseCliArgs(args) {
  const options = { harness: "", profile: "", help: false };
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--help" || arg === "-h") {
      options.help = true;
    } else if (arg === "--profile") {
      options.profile = args[index + 1] && !args[index + 1].startsWith("--") ? args[index + 1] : "";
      if (options.profile) index += 1;
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

function outputLineCount(text) {
  if (!text) return 0;
  let lines = 0;
  let previousWasCr = false;
  let endedWithNewline = false;
  for (const char of text) {
    if (char === "\r") {
      lines += 1;
      previousWasCr = true;
      endedWithNewline = true;
    } else if (char === "\n") {
      if (!previousWasCr) lines += 1;
      previousWasCr = false;
      endedWithNewline = true;
    } else {
      previousWasCr = false;
      endedWithNewline = false;
    }
  }
  return lines + (endedWithNewline ? 0 : 1);
}

function outputSizeMetrics(text) {
  const output = String(text || "");
  if (!output) return {};
  return {
    output_chars: output.length,
    output_lines: outputLineCount(output),
  };
}

function environmentBlockReason(command, output) {
  const text = `${command || ""}\n${output || ""}`;
  if (
    /no working Python runner found/i.test(text)
    || /ModuleNotFoundError:\s*No module named ['"]?(?:PIL|numpy|scipy)['"]?/i.test(text)
  ) {
    return "missing Studio Python modules; repair the root .venv with ai_studio/dev_environment/python_setup.mjs and verify it with python_check.mjs";
  }
  return "";
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
    if (!rawSession && harness === "codex") rawSession = process.env.CODEX_THREAD_ID || process.env.CODEX_SESSION_FILE || "";
    const session = deriveSessionId(rawSession);
    const profilePath = options.profile || process.env.AI_PROFILE_FILE || (session.short ? sessionProfilePathFor(harness, session.short) : "");
    const stamp = { harness, cwd: process.cwd() };
    if (session.full) stamp.session_id = session.full;

    if (event === "SessionStart") {
      const { appendRecord, buildRecord } = await loadProfileLib();
      appendRecord(profilePath, buildRecord({
        phase: "session", category: "context", result: "pass",
        intent: `session start (${harness})`, tool: [`${harness}/session`],
      }, { event_type: "session_start", ...stamp }));
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
    const output = responseText(response);
    const blockedBy = result === "fail" ? environmentBlockReason(cmd1, output) : "";
    appendRecord(profilePath, buildRecord({
      ...baseValues,
      result,
      value: blockedBy ? "necessary_overhead" : (result === "fail" ? "rework" : "unknown"),
      ...(blockedBy ? { "blocked-by": blockedBy } : {}),
    }, { event_type: "tool_call_result", ...outputSizeMetrics(output), ...(blockedBy ? { failure_kind: "environment_blocked" } : {}), ...stamp }));
  } catch {
    // swallow — a profiling hook must never disrupt the agent
  }
  process.exit(0);
})();
