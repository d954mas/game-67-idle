// Per-agent tool-usage rollup (advisory, read-only, cross-harness).
//
// Answers "how many subagents ran, what each was asked, and which tools each
// used" by reading the harness's OWN native transcripts (reliably linked by
// agentId / parent_thread_id) — NOT by guessing or counting sessions as proof.
// This is diagnostic observability for the lead to inspect/report; it is NEVER
// an acceptance gate. (Distinct from the proof layer that was removed: that
// attributed day-cumulative session counts to a TASK as a pass/fail condition.)
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

/* ---- Claude ---- */

function findClaudeSubagentsDir(sessionId, projectsRoot) {
  if (!sessionId || !existsSync(projectsRoot)) return "";
  for (const project of readdirSync(projectsRoot, { withFileTypes: true })) {
    if (!project.isDirectory()) continue;
    const dir = join(projectsRoot, project.name, sessionId, "subagents");
    if (existsSync(dir)) return dir;
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

function countClaudeTools(text) {
  const tools = {};
  let messages = 0;
  let lastTs = "";
  let firstUser = "";
  for (const line of text.split(/\r?\n/)) {
    if (!line.trim()) continue;
    let record;
    try { record = JSON.parse(line); } catch { continue; }
    if (record.timestamp) lastTs = record.timestamp;
    if (record.type === "assistant" && record.message && Array.isArray(record.message.content)) {
      for (const block of record.message.content) {
        if (block && block.type === "tool_use" && block.name) {
          tools[block.name] = (tools[block.name] || 0) + 1;
        }
      }
      messages += 1;
    } else if (record.type === "user") {
      if (!firstUser && record.message) firstUser = messageText(record.message.content).trim();
      messages += 1;
    }
  }
  return { tools, messages, lastTs, firstUser };
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
    const { tools, messages, lastTs, firstUser } = countClaudeTools(text);
    agents.push({
      id: id.slice(0, 12),
      type: meta.agentType || "",
      objective: String(meta.description || firstUser || "").split(/\r?\n/)[0].slice(0, 120),
      tools,
      tool_total: toolTotal(tools),
      messages,
      ts: lastTs,
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
      if (text) return text.split(/\r?\n/)[0].slice(0, 120);
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
    const metaRecord = records.find((r) => (r.payload || r).thread_source !== undefined || (r.type === "session_meta"));
    const meta = metaRecord ? (metaRecord.payload || metaRecord) : null;
    if (!meta || meta.thread_source !== "subagent") continue;
    const spawn = (meta.source && meta.source.subagent && meta.source.subagent.thread_spawn) || {};
    if (parentThreadId && spawn.parent_thread_id && spawn.parent_thread_id !== parentThreadId) continue;
    const tools = {};
    let lastTs = meta.timestamp || "";
    for (const record of records) {
      const payload = record.payload || record;
      if (payload && payload.type === "function_call" && payload.name) {
        tools[payload.name] = (tools[payload.name] || 0) + 1;
      }
      if (record.timestamp) lastTs = record.timestamp;
    }
    agents.push({
      id: String(meta.id || entry.name).slice(0, 12),
      type: spawn.agent_role || "subagent",
      objective: firstUserText(records) || String(spawn.agent_nickname || "").slice(0, 120),
      tools,
      tool_total: toolTotal(tools),
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
  const claude = sessionId ? claudeAgentRollup(sessionId, projectsRoot) : [];

  let codexDayDir = env.AI_AGENT_CODEX_DAY_DIR || "";
  let parentThreadId = env.AI_AGENT_CODEX_PARENT || "";
  if (!codexDayDir && env.CODEX_SESSION_FILE) {
    codexDayDir = dirname(env.CODEX_SESSION_FILE);
    const match = basename(env.CODEX_SESSION_FILE).match(/-([0-9a-f-]{36})\.jsonl$/i);
    if (match) parentThreadId = match[1];
  }
  const codex = codexDayDir ? codexAgentRollup(parentThreadId, codexDayDir) : [];
  return { claude, codex };
}

function renderSection(title, agents) {
  const lines = [];
  lines.push("");
  lines.push(`## ${title}`);
  if (agents.length === 0) {
    lines.push("- none recorded");
    return lines;
  }
  for (const agent of agents) {
    const tools = sortedTools(agent.tools).map(([name, n]) => `${name} ${n}`).join(", ") || "no tool calls";
    const type = agent.type ? `[${agent.type}] ` : "";
    const objective = agent.objective || "(no objective recorded)";
    lines.push(`- ${agent.id} ${type}${objective} — ${tools}`);
  }
  const totalCalls = agents.reduce((sum, a) => sum + a.tool_total, 0);
  lines.push(`Total: ${agents.length} agent(s), ${totalCalls} tool call(s).`);
  return lines;
}

export function renderAgentRollup(rollup) {
  const lines = [];
  if (rollup.claude.length > 0 || rollup.codex.length > 0) {
    if (rollup.claude.length > 0) lines.push(...renderSection("Agents — Claude (advisory, from native transcripts)", rollup.claude));
    if (rollup.codex.length > 0) lines.push(...renderSection("Agents — Codex (advisory, from native transcripts)", rollup.codex));
  } else {
    lines.push("");
    lines.push("## Agents (advisory)");
    lines.push("- no subagent transcripts found for this session");
  }
  return `${lines.join("\n")}\n`;
}
