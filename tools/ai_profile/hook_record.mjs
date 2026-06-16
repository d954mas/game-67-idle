#!/usr/bin/env node
// Cross-harness profiling hook handler.
//
// Wired as a PostToolUse / SessionStart hook in BOTH harnesses:
//   - Claude Code:  .claude/settings.json   (hooks.PostToolUse ...)
//   - Codex CLI:    .codex/hooks.json        (hooks.PostToolUse ...)
// The harness runs this automatically on every tool call, so profiling coverage
// is GUARANTEED without the agent remembering to call `ai.mjs start/run`.
//
// It reads the hook JSON payload on stdin (Claude and Codex share the shape:
// hook_event_name / tool_name / tool_input.command / tool_response) and appends
// one passive profile record to the session JSONL that `ai.mjs status` reads.
//
// Hard rule: NEVER break or slow the harness. Any error -> exit 0 silently.
// Pass the harness name via env AI_PROFILE_HARNESS (claude|codex) in the config.

import { appendRecord, buildRecord } from "./profile_lib.mjs";

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

    if (event === "SessionStart") {
      appendRecord("", buildRecord({
        phase: "session", category: "context", result: "pass",
        intent: `session start (${harness})`, tool: [`${harness}/session`],
      }));
      process.exit(0);
    }

    const tool = String(pick(payload, "tool_name", "toolName") || "");
    const input = pick(payload, "tool_input", "toolInput") || {};
    const response = pick(payload, "tool_response", "toolResponse") || {};
    const command = pick(input, "command");
    if (!command) process.exit(0); // only profile command/shell tool calls

    const exit = pick(response, "exit_code", "exitCode", "code", "returncode");
    const isErr = pick(response, "is_error", "isError", "error");
    const result =
      (typeof exit === "number" && exit !== 0) || isErr === true || (typeof isErr === "string" && isErr.length > 0)
        ? "fail"
        : "pass";

    const cmd1 = String(command).split(/\r?\n/)[0].trim().slice(0, 200);
    appendRecord("", buildRecord({
      phase: "session",
      category: inferCategory(cmd1),
      result,
      intent: `auto:${tool || "shell"}`,
      tool: [`${harness}/${tool || "shell"}`],
      command: [cmd1],
    }));
  } catch {
    // swallow — a profiling hook must never disrupt the agent
  }
  process.exit(0);
})();
