// Per-agent tool-usage rollup (advisory, read-only, cross-harness).
//
// Answers "how many subagents ran, what each was asked, which tools each used,
// how long they took, and whether they finished cleanly" by reading the
// harness's OWN native transcripts (reliably linked by agentId /
// parent_thread_id) - NOT by guessing or counting sessions as proof. Diagnostic
// observability for the lead to inspect/report; NEVER an acceptance gate.
// (Distinct from the removed proof layer, which attributed day-cumulative
// session counts to a TASK as a pass/fail condition.)
//
// Claude: ~/.claude/projects/<proj>/<sessionId>/subagents/agent-<id>.jsonl
//         (+ .meta.json; workflow agents under subagents/workflows/<wf>/).
// Codex:  ~/.codex/sessions/YYYY/MM/DD/rollout-*.jsonl with
//         session_meta.payload.thread_source === "subagent".

import { existsSync, readdirSync, readFileSync } from "node:fs";
import { homedir } from "node:os";
import { basename, dirname, join, sep } from "node:path";

function sortedTools(tools) {
  return Object.entries(tools).sort((a, b) => b[1] - a[1]);
}

function toolTotal(tools) {
  return Object.values(tools).reduce((sum, n) => sum + n, 0);
}

function durationMs(firstTs, lastTs) {
  const a = Date.parse(firstTs || "");
  const b = Date.parse(lastTs || "");
  return Number.isFinite(a) && Number.isFinite(b) && b >= a ? b - a : 0;
}

export function formatDuration(ms) {
  if (!Number.isFinite(ms) || ms <= 0) return "0s";
  const s = Math.round(ms / 1000);
  if (s < 90) return `${s}s`;
  const m = Math.round(s / 60);
  if (m < 90) return `${m}m`;
  return `${(m / 60).toFixed(1)}h`;
}

// --since: absolute ISO time, or relative "<N>m|h|d" from now. Returns epoch ms or null.
export function parseSince(value, now = Date.now()) {
  if (!value || value === true) return null;
  const relative = String(value).match(/^(\d+)\s*([mhd])$/i);
  if (relative) {
    const n = Number(relative[1]);
    const unit = { m: 60_000, h: 3_600_000, d: 86_400_000 }[relative[2].toLowerCase()];
    return now - n * unit;
  }
  const parsed = Date.parse(value);
  return Number.isFinite(parsed) ? parsed : null;
}

/* ---- Claude ---- */

function findClaudeSubagentsDir(sessionId, projectsRoot) {
  if (!sessionId || !existsSync(projectsRoot)) return "";
  for (const project of readdirSync(projectsRoot, { withFileTypes: true })) {
    if (!project.isDirectory()) continue;
    const projectDir = join(projectsRoot, project.name);
    // Exact session-dir match (full id) - fast path.
    const exact = join(projectDir, sessionId, "subagents");
    if (existsSync(exact)) return exact;
    // Short-id / prefix match: `--session e03c764c` should find dir e03c764c-<rest>.
    let sessionDirs;
    try { sessionDirs = readdirSync(projectDir, { withFileTypes: true }); } catch { continue; }
    for (const entry of sessionDirs) {
      if (!entry.isDirectory() || !entry.name.startsWith(sessionId)) continue;
      const dir = join(projectDir, entry.name, "subagents");
      if (existsSync(dir)) return dir;
    }
  }
  return "";
}

function collectAgentFiles(dir) {
  const out = [];
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isFile() && /^agent-.*\.jsonl$/.test(entry.name)) out.push(path);
    else if (entry.isDirectory()) out.push(...collectAgentFiles(path));
  }
  return out;
}

function messageText(content) {
  if (typeof content === "string") return content;
  if (Array.isArray(content)) return content.map((b) => (b && typeof b.text === "string" ? b.text : "")).join(" ");
  return "";
}

function readClaudeAgent(text) {
  const tools = {};
  let messages = 0;
  let firstTs = "";
  let lastTs = "";
  let firstUser = "";
  let toolErrors = 0;
  let completed = false;
  for (const line of text.split(/\r?\n/)) {
    if (!line.trim()) continue;
    let record;
    try { record = JSON.parse(line); } catch { continue; }
    if (record.timestamp) { if (!firstTs) firstTs = record.timestamp; lastTs = record.timestamp; }
    const msg = record.message || {};
    if (record.type === "assistant" && Array.isArray(msg.content)) {
      for (const block of msg.content) {
        if (block && block.type === "tool_use" && block.name) tools[block.name] = (tools[block.name] || 0) + 1;
      }
      completed = msg.content.some((b) => b && b.type === "text" && String(b.text || "").trim());
      messages += 1;
    } else if (record.type === "user") {
      if (!firstUser && msg.content !== undefined) firstUser = messageText(msg.content).trim();
      if (Array.isArray(msg.content)) {
        for (const block of msg.content) if (block && block.type === "tool_result" && block.is_error) toolErrors += 1;
      }
      messages += 1;
    }
  }
  return { tools, messages, firstTs, lastTs, firstUser, toolErrors, completed };
}

export function claudeAgentRollup(sessionId, projectsRoot = join(homedir(), ".claude", "projects")) {
  const dir = findClaudeSubagentsDir(sessionId, projectsRoot);
  if (!dir) return [];
  const agents = [];
  for (const file of collectAgentFiles(dir)) {
    const id = basename(file).replace(/^agent-/, "").replace(/\.jsonl$/, "");
    let meta = {};
    const metaPath = file.replace(/\.jsonl$/, ".meta.json");
    if (existsSync(metaPath)) {
      try { meta = JSON.parse(readFileSync(metaPath, "utf8")); } catch { meta = {}; }
    }
    let text = "";
    try { text = readFileSync(file, "utf8"); } catch { continue; }
    const parsed = readClaudeAgent(text);
    agents.push({
      id: id.slice(0, 12),
      type: meta.agentType || "",
      label: String(meta.description || parsed.firstUser || "").split(/\r?\n/)[0].slice(0, 110),
      tools: parsed.tools,
      tool_total: toolTotal(parsed.tools),
      tool_errors: parsed.toolErrors,
      messages: parsed.messages,
      duration_ms: durationMs(parsed.firstTs, parsed.lastTs),
      status: parsed.completed ? "ok" : "incomplete",
      ts: parsed.lastTs,
      group: file.includes(`${sep}workflows${sep}`) ? "workflow" : "agent",
    });
  }
  return agents.sort((a, b) => String(b.ts || "").localeCompare(String(a.ts || "")));
}

/* ---- Codex ---- */

function firstUserText(records) {
  for (const record of records) {
    const payload = record.payload || record;
    if (payload && payload.type === "message" && Array.isArray(payload.content)) {
      const text = payload.content.map((c) => (c && (c.text || c.input_text)) || "").join(" ").trim();
      if (text) return text.split(/\r?\n/)[0].slice(0, 110);
    }
  }
  return "";
}

export function codexAgentRollup(parentThreadId, dayDir) {
  if (!dayDir || !existsSync(dayDir)) return [];
  const agents = [];
  for (const entry of readdirSync(dayDir, { withFileTypes: true })) {
    if (!entry.isFile() || !/^rollout-.*\.jsonl$/.test(entry.name)) continue;
    let records;
    try {
      records = readFileSync(join(dayDir, entry.name), "utf8")
        .split(/\r?\n/)
        .filter(Boolean)
        .map((line) => { try { return JSON.parse(line); } catch { return null; } })
        .filter(Boolean);
    } catch { continue; }
    const metaRecord = records.find((r) => (r.payload || r).thread_source !== undefined || r.type === "session_meta");
    const meta = metaRecord ? (metaRecord.payload || metaRecord) : null;
    if (!meta || meta.thread_source !== "subagent") continue;
    const spawn = (meta.source && meta.source.subagent && meta.source.subagent.thread_spawn) || {};
    const spawnParent = String(spawn.parent_thread_id || "");
    if (!spawnParent || (parentThreadId && spawnParent !== parentThreadId)) continue;
    const taskName = String(spawn.agent_path || "").split("/").filter(Boolean).at(-1) || "";
    const tools = {};
    let firstTs = meta.timestamp || "";
    let lastTs = meta.timestamp || "";
    let toolErrors = 0;
    let completed = false;
    for (const record of records) {
      const payload = record.payload || record;
      if (record.timestamp) { if (!firstTs) firstTs = record.timestamp; lastTs = record.timestamp; }
      if (payload && payload.type === "function_call" && payload.name) {
        tools[payload.name] = (tools[payload.name] || 0) + 1;
      }
      if (payload && payload.type === "function_call_output") {
        const match = String(payload.output || "").match(/^Exit code:\s*(-?\d+)/m);
        if (match && Number(match[1]) !== 0) toolErrors += 1;
      }
      if (payload && payload.type === "task_complete") completed = true;
    }
    agents.push({
      id: String(meta.id || entry.name).slice(0, 12),
      type: spawn.agent_role || "subagent",
      label: taskName || firstUserText(records) || String(spawn.agent_nickname || "").slice(0, 110),
      tools,
      tool_total: toolTotal(tools),
      tool_errors: toolErrors,
      duration_ms: durationMs(firstTs, lastTs),
      status: completed ? "ok" : "incomplete",
      ts: lastTs,
      group: "agent",
    });
  }
  return agents.sort((a, b) => String(b.ts || "").localeCompare(String(a.ts || "")));
}

/* ---- combined + render ---- */

export function buildAgentToolRollup(values = {}, env = process.env) {
  const projectsRoot = env.AI_AGENT_CLAUDE_PROJECTS || join(homedir(), ".claude", "projects");
  const sessionId = (values.session && typeof values.session === "string" ? values.session : "")
    || env.CLAUDE_CODE_SESSION_ID || "";
  let claude = sessionId ? claudeAgentRollup(sessionId, projectsRoot) : [];

  let codexDayDir = env.AI_AGENT_CODEX_DAY_DIR || "";
  let parentThreadId = env.AI_AGENT_CODEX_PARENT || "";
  if (!codexDayDir && env.CODEX_SESSION_FILE) {
    codexDayDir = dirname(env.CODEX_SESSION_FILE);
    const match = basename(env.CODEX_SESSION_FILE).match(/-([0-9a-f-]{36})\.jsonl$/i);
    if (match) parentThreadId = match[1];
  }
  let codex = codexDayDir ? codexAgentRollup(parentThreadId, codexDayDir) : [];

  const since = parseSince(values.since);
  if (since !== null) {
    const keep = (agent) => {
      const t = Date.parse(agent.ts || "");
      return Number.isFinite(t) ? t >= since : true;
    };
    claude = claude.filter(keep);
    codex = codex.filter(keep);
  }
  return { claude, codex, since };
}

function renderSection(title, agents) {
  const lines = ["", `## ${title}`];
  if (agents.length === 0) {
    lines.push("- none recorded");
    return lines;
  }
  for (const agent of agents) {
    const tools = sortedTools(agent.tools).map(([name, n]) => `${name} ${n}`).join(", ") || "no tool calls";
    const type = agent.type ? `[${agent.type}] ` : "";
    const label = agent.label || "(no label recorded)";
    const meta = [
      formatDuration(agent.duration_ms),
      agent.tool_errors > 0 ? `${agent.tool_errors} tool-err` : null,
      agent.status === "ok" ? "ok" : agent.status,
    ].filter(Boolean).join(", ");
    lines.push(`- ${agent.id} ${type}${label} - ${tools} | ${meta}`);
  }
  const totalCalls = agents.reduce((sum, a) => sum + a.tool_total, 0);
  const totalErr = agents.reduce((sum, a) => sum + a.tool_errors, 0);
  lines.push(`Total: ${agents.length} agent(s), ${totalCalls} tool call(s)${totalErr > 0 ? `, ${totalErr} tool error(s)` : ""}.`);
  return lines;
}

export function renderAgentRollup(rollup) {
  const lines = [];
  const sinceNote = rollup.since !== null && rollup.since !== undefined ? " (filtered by --since)" : "";
  if (rollup.claude.length > 0 || rollup.codex.length > 0) {
    if (rollup.claude.length > 0) lines.push(...renderSection(`Agents - Claude (advisory, from native transcripts)${sinceNote}`, rollup.claude));
    if (rollup.codex.length > 0) lines.push(...renderSection(`Agents - Codex (advisory, from native transcripts)${sinceNote}`, rollup.codex));
  } else {
    lines.push("", "## Agents (advisory)", `- no subagent transcripts found for this session${sinceNote}`);
  }
  return `${lines.join("\n")}\n`;
}
