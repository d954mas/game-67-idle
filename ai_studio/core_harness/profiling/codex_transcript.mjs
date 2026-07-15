import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { homedir } from "node:os";
import { basename, join, resolve } from "node:path";

function sessionRoot() {
  return process.env.CODEX_SESSION_ROOT || join(homedir(), ".codex", "sessions");
}

function transcriptCandidates(root) {
  if (!existsSync(root)) return [];
  const files = [];
  const pending = [root];
  while (pending.length > 0) {
    const dir = pending.pop();
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) pending.push(path);
      else if (entry.isFile() && entry.name.endsWith(".jsonl")) files.push(path);
    }
  }
  return files;
}

export function resolveCodexTranscript({ transcript = "", sessionId = "" } = {}) {
  const explicit = transcript || process.env.CODEX_SESSION_FILE || "";
  if (explicit && existsSync(explicit)) return resolve(explicit);

  const wanted = sessionId || process.env.CODEX_THREAD_ID || "";
  const files = transcriptCandidates(sessionRoot());
  if (wanted) {
    const exact = files.find((path) => basename(path).includes(wanted));
    if (exact) return exact;
    const short = wanted.slice(0, 8);
    const partial = files.find((path) => basename(path).includes(short));
    if (partial) return partial;
  }
  return files.sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs)[0] || "";
}

function outputText(value) {
  if (value === undefined || value === null) return "";
  if (typeof value === "string") return value;
  if (Array.isArray(value)) return value.map(outputText).filter(Boolean).join("\n");
  if (typeof value === "object") {
    return [value.text, value.output, value.stdout, value.stderr, value.message, value.error]
      .map(outputText)
      .filter(Boolean)
      .join("\n");
  }
  return String(value);
}

function parseArguments(raw) {
  try { return JSON.parse(String(raw || "{}")); } catch { return {}; }
}

function embeddedCommand(input) {
  const source = String(input || "");
  const quoted = source.match(/["']?command["']?\s*:\s*("(?:\\.|[^"\\])*")/s);
  if (quoted) {
    try { return JSON.parse(quoted[1]); } catch { /* fall through */ }
  }
  const single = source.match(/["']?command["']?\s*:\s*'((?:\\.|[^'\\])*)'/s);
  if (single) return single[1].replace(/\\'/g, "'").replace(/\\\\/g, "\\");
  const template = source.match(/["']?command["']?\s*:\s*`([^`$]*)`/s);
  return template ? template[1] : "";
}

function commandFor(payload) {
  const name = String(payload.name || "tool");
  if (payload.type === "function_call") {
    const args = parseArguments(payload.arguments);
    if (/^(?:shell_command|exec_command|bash|shell)$/i.test(name)) {
      return String(args.command || args.cmd || name).trim();
    }
    return name;
  }
  if (name === "exec") return embeddedCommand(payload.input) || "functions.exec";
  return name;
}

function categoryFor(command, tool) {
  const text = `${command} ${tool}`.toLowerCase();
  if (/spawn_agent|followup_task|send_message|wait_agent/.test(text)) return "delegation";
  if (/node --test|pytest|unittest|\bverify\b|\btest\b/.test(text)) return "validation";
  if (/git (?:commit|add|push|status|log|diff)/.test(text)) return "task_status";
  if (/apply_patch|cmake|ninja|\bbuild\b/.test(text)) return "implementation";
  if (/\brg\b|grep|find|select-string|get-content|read_mcp/.test(text)) return "research";
  return "tooling";
}

function outputMetrics(text) {
  if (!text) return {};
  return { output_chars: text.length, output_lines: text.split(/\r?\n/).length };
}

function failedOutput(text, command = "") {
  const exitCodes = [...text.matchAll(/^Exit code:\s*(-?\d+)/gm)].map((match) => Number(match[1]));
  if (/^(?:rg|grep|egrep|fgrep|findstr|ack|select-string)(?:\s|$)/i.test(command.trim())
    && exitCodes.length > 0
    && exitCodes.every((code) => code === 1)) return false;
  if (/^Script failed\b/m.test(text)) return true;
  if (/^Script completed\b/m.test(text)) return false;
  return exitCodes.length > 0 ? exitCodes.some((code) => code !== 0) : false;
}

function tokenTelemetry(payload, measuredRecords) {
  const totals = payload?.info?.total_token_usage;
  if (!totals) return null;
  const currentContext = Number(payload?.info?.last_token_usage?.total_tokens || 0);
  const contextWindow = Number(payload?.info?.model_context_window || 0);
  return {
    available: true,
    measured_records: measuredRecords,
    input_tokens: Number(totals.input_tokens || 0),
    cached_input_tokens: Number(totals.cached_input_tokens || 0),
    output_tokens: Number(totals.output_tokens || 0),
    reasoning_output_tokens: Number(totals.reasoning_output_tokens || 0),
    total_tokens: Number(totals.total_tokens || 0),
    current_context_tokens: currentContext,
    model_context_window: contextWindow,
    context_utilization: contextWindow > 0 ? currentContext / contextWindow : null,
  };
}

export function parseCodexTranscript(file) {
  if (!file || !existsSync(file)) {
    return {
      records: [], errors: [], exists: false, sourceKind: "codex-transcript",
      tokenTelemetry: { available: false, measured_records: 0 },
      durationTelemetry: { available: false, start_records: 0, result_records: 0, measured_records: 0 },
    };
  }

  const records = [];
  const errors = [];
  const pending = new Map();
  const runningCells = new Map();
  let sessionId = "";
  let tokenRecords = 0;
  let tokens = { available: false, measured_records: 0 };

  for (const [index, rawLine] of readFileSync(file, "utf8").split(/\r?\n/).entries()) {
    if (!rawLine.trim()) continue;
    let line;
    try { line = JSON.parse(rawLine); } catch (error) {
      errors.push(`line ${index + 1}: invalid JSON: ${error.message}`);
      continue;
    }
    const payload = line.payload || {};
    if (line.type === "session_meta") {
      sessionId = String(payload.id || sessionId);
      records.push({
        ts: line.timestamp || "", phase: "session", category: "context",
        intent: "session start (codex)", result: "pass", value: "unknown",
        event_type: "session_start", tools: ["codex/session"], commands: [],
        session_id: sessionId, source_session_file: file, __line: index + 1,
      });
      continue;
    }
    if (line.type === "event_msg" && payload.type === "token_count") {
      tokenRecords += 1;
      tokens = tokenTelemetry(payload, tokenRecords) || tokens;
      continue;
    }
    if (line.type !== "response_item") continue;
    if (payload.type === "custom_tool_call" || payload.type === "function_call") {
      const callId = String(payload.call_id || payload.id || "");
      if (!callId) continue;
      pending.set(callId, { ts: line.timestamp || "", payload, line: index + 1 });
      continue;
    }
    if (payload.type !== "custom_tool_call_output" && payload.type !== "function_call_output") continue;
    const callId = String(payload.call_id || "");
    const start = pending.get(callId);
    if (!start) continue;
    pending.delete(callId);
    const endTs = line.timestamp || "";
    const durationMs = Date.parse(endTs) - Date.parse(start.ts);
    const text = outputText(payload.output);
    const tool = String(start.payload.name || "tool");
    const command = commandFor(start.payload).slice(0, 500);
    const failed = failedOutput(text, command);
    const waitCell = tool === "wait"
      ? String(parseArguments(start.payload.arguments).cell_id || "")
      : "";
    if (waitCell && runningCells.has(waitCell)) {
      const original = runningCells.get(waitCell);
      const totalDuration = Date.parse(endTs) - Date.parse(original.__start_ts);
      if (Number.isFinite(totalDuration) && totalDuration >= 0) original.duration_ms = totalDuration;
      const metrics = outputMetrics(text);
      original.output_chars = Number(original.output_chars || 0) + Number(metrics.output_chars || 0);
      original.output_lines = Number(original.output_lines || 0) + Number(metrics.output_lines || 0);
      original.ts = endTs;
      original.__line = index + 1;
      if (!/^Script running with cell ID\b/m.test(text)) {
        original.result = failed ? "fail" : "pass";
        original.value = failed ? "rework" : "unknown";
        runningCells.delete(waitCell);
      }
      continue;
    }
    const record = {
      ts: endTs, phase: "session", category: categoryFor(command, tool),
      intent: `auto:${tool}`, result: failed ? "fail" : "pass",
      value: failed ? "rework" : "unknown", event_type: "tool_call_result",
      tools: [`codex/${tool}`], commands: [command], session_id: sessionId,
      source_call_id: callId, source_session_file: file, __line: index + 1,
      __start_ts: start.ts,
      ...outputMetrics(text),
    };
    if (tool === "spawn_agent") record.subagent_type = String(parseArguments(start.payload.arguments).task_name || "agent");
    if (Number.isFinite(durationMs) && durationMs >= 0) {
      record.duration_ms = durationMs;
    }
    const runningCell = text.match(/^Script running with cell ID\s+(\S+)/m);
    if (runningCell) {
      record.result = "unknown";
      record.value = "necessary_overhead";
      runningCells.set(runningCell[1], record);
    }
    records.push(record);
  }

  const resultRecords = records.filter((record) => record.event_type === "tool_call_result").length;
  const measuredRecords = records.filter((record) => record.event_type === "tool_call_result"
    && Number.isFinite(Number(record.duration_ms))).length;

  return {
    records,
    errors,
    exists: true,
    sourceKind: "codex-transcript",
    tokenTelemetry: tokens,
    durationTelemetry: {
      available: measuredRecords > 0,
      start_records: resultRecords,
      result_records: resultRecords,
      measured_records: measuredRecords,
    },
  };
}
