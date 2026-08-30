#!/usr/bin/env node
// Cross-session iteration report: what a week of agent work cost, and where.
//
// `status.mjs` reviews one session. This reads every Claude and Codex
// transcript in a window and answers the question a session cannot: is
// iteration getting faster or slower, and which category of work is paying for
// it. Output volume is reported next to time because every tool result is
// re-read on each later request, so a noisy command is charged again on every
// turn that follows it.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { homedir } from "node:os";
import { basename, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";

import { parseArgs, stringArg, numberArg } from "./profile_lib.mjs";
import { parseSince } from "./agent_rollup.mjs";
import { categoryFor, parseCodexTranscript } from "./codex_transcript.mjs";

const REPO_ROOT = resolve(fileURLToPath(new URL("../../..", import.meta.url)));
const MAX_TOOL_MS = 20 * 60 * 1000;

function usage() {
  console.error(`usage:
  node ai_studio/core_harness/profiling/iteration_report.mjs [--since <Nh|Nd|ISO>] [--top <n>] [--json]

Reads Claude transcripts (~/.claude/projects) and Codex rollouts
(~/.codex/sessions) whose working directory is this repository, and reports
per-harness cost, per-category cost, the noisiest commands, and test-vs-source
churn from git. Defaults to the last 7 days. Overrides for tests:
AI_ITERATION_CLAUDE_PROJECTS, CODEX_SESSION_ROOT, AI_ITERATION_REPO.`);
}

function emptyBucket() {
  return { calls: 0, ms: 0, chars: 0, truncatedTokens: 0, fails: 0 };
}

function add(bucket, record) {
  bucket.calls += 1;
  bucket.ms += Math.min(Number(record.duration_ms) || 0, MAX_TOOL_MS);
  bucket.chars += Number(record.output_chars) || 0;
  bucket.truncatedTokens += Number(record.output_truncated_tokens) || 0;
  if (record.result === "fail") bucket.fails += 1;
}

function normalizeCommand(command) {
  return String(command || "").replace(/\s+/g, " ").trim().slice(0, 96);
}

// A parallel batch carries several commands under one timing and one output.
// Its category is the one every nested command shares; anything else is mixed,
// because splitting the shared bytes between them would be invention.
function categoryOf(record) {
  const nested = record.nested_commands || [];
  if (nested.length === 0) return record.category || "tooling";
  const found = new Set(nested.map((command) => categoryFor(command, "exec")));
  return found.size === 1 ? [...found][0] : "mixed batch";
}

function median(values) {
  if (values.length === 0) return 0;
  const sorted = [...values].sort((a, b) => a - b);
  return sorted[Math.floor(sorted.length / 2)];
}

// --- Claude native transcripts -------------------------------------------

export function claudeProjectDirs(projectsRoot, repoRoot) {
  if (!existsSync(projectsRoot)) return [];
  // Claude names a project directory after its cwd with separators flattened.
  const slug = repoRoot.replace(/[\\/:]/g, "-").replace(/^-+/, "");
  const wanted = slug.toLowerCase();
  return readdirSync(projectsRoot, { withFileTypes: true })
    .filter((entry) => entry.isDirectory() && entry.name.toLowerCase().startsWith(wanted))
    .map((entry) => join(projectsRoot, entry.name));
}

export function parseClaudeTranscript(file) {
  const records = [];
  const turns = [];
  const pending = new Map();
  let turnStart = 0;
  let turnTools = 0;
  let lastActivity = 0;

  for (const rawLine of readFileSync(file, "utf8").split(/\r?\n/)) {
    if (!rawLine.trim()) continue;
    let line;
    try { line = JSON.parse(rawLine); } catch { continue; }
    const ts = line.timestamp ? Date.parse(line.timestamp) : 0;
    const message = line.message;

    if (line.type === "user" && message && !line.toolUseResult && !line.isMeta && !line.isSidechain) {
      const content = message.content;
      const text = typeof content === "string"
        ? content
        : (Array.isArray(content) ? content.filter((block) => block.type === "text").map((block) => block.text).join(" ") : "");
      if (text && !/^\[Request interrupted/.test(text)) {
        if (turnStart && lastActivity > turnStart) turns.push({ ms: lastActivity - turnStart, tools: turnTools });
        turnStart = ts;
        turnTools = 0;
        lastActivity = ts;
      }
      continue;
    }
    if (line.type === "assistant" && message && Array.isArray(message.content)) {
      lastActivity = ts || lastActivity;
      for (const block of message.content) {
        if (block.type !== "tool_use") continue;
        turnTools += 1;
        pending.set(block.id, { ts, name: block.name, input: block.input || {} });
      }
      continue;
    }
    if (line.toolUseResult && message && Array.isArray(message.content)) {
      lastActivity = ts || lastActivity;
      for (const block of message.content) {
        if (block.type !== "tool_result") continue;
        const start = pending.get(block.tool_use_id);
        if (!start) continue;
        pending.delete(block.tool_use_id);
        const result = line.toolUseResult;
        const text = typeof result === "string"
          ? result
          : `${result.stdout || ""}${result.stderr || ""}`;
        const command = start.name === "Bash" ? String(start.input.command || "") : start.name;
        records.push({
          ts: line.timestamp || "",
          category: categoryFor(command, start.name),
          commands: [command.slice(0, 500)],
          duration_ms: ts && start.ts ? ts - start.ts : 0,
          output_chars: text.length,
          result: block.is_error === true ? "fail" : "pass",
          tools: [`claude/${start.name}`],
        });
      }
    }
  }
  if (turnStart && lastActivity > turnStart) turns.push({ ms: lastActivity - turnStart, tools: turnTools });
  return { records, turns };
}

// --- Codex rollouts -------------------------------------------------------

function codexTranscriptCwd(file) {
  const handle = readFileSync(file, "utf8");
  const firstLine = handle.slice(0, handle.indexOf("\n") + 1 || undefined);
  try {
    const meta = JSON.parse(firstLine);
    return String(meta?.payload?.cwd || "");
  } catch {
    return "";
  }
}

function codexTranscriptFiles(root, since) {
  if (!existsSync(root)) return [];
  const files = [];
  const pending = [root];
  while (pending.length > 0) {
    const dir = pending.pop();
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) pending.push(path);
      else if (entry.isFile() && entry.name.endsWith(".jsonl") && statSync(path).mtimeMs >= since) files.push(path);
    }
  }
  return files;
}

function codexTurns(file) {
  const turns = [];
  let turnStart = 0;
  let turnTools = 0;
  let lastActivity = 0;
  for (const rawLine of readFileSync(file, "utf8").split(/\r?\n/)) {
    if (!rawLine.trim()) continue;
    let line;
    try { line = JSON.parse(rawLine); } catch { continue; }
    if (line.type !== "response_item") continue;
    const ts = line.timestamp ? Date.parse(line.timestamp) : 0;
    const payload = line.payload || {};
    if (payload.type === "message" && payload.role === "user") {
      if (turnStart && lastActivity > turnStart) turns.push({ ms: lastActivity - turnStart, tools: turnTools });
      turnStart = ts;
      turnTools = 0;
      lastActivity = ts;
      continue;
    }
    if (payload.type === "custom_tool_call" || payload.type === "function_call") {
      turnTools += 1;
      lastActivity = ts || lastActivity;
    }
    if (payload.type === "custom_tool_call_output" || payload.type === "function_call_output") {
      lastActivity = ts || lastActivity;
    }
  }
  if (turnStart && lastActivity > turnStart) turns.push({ ms: lastActivity - turnStart, tools: turnTools });
  return turns;
}

// --- git churn ------------------------------------------------------------

function isTestPath(path) {
  const file = path.toLowerCase();
  return /(^|\/)tests?\//.test(file) || /(^|\/)test_[^/]*$/.test(file) || /\.test\.[a-z]+$/.test(file) || /_test\.[a-z]+$/.test(file);
}

function isSourcePath(path) {
  return /\.(c|h|cpp|hpp|mjs|js|ts|py|lua|glsl|cmake)$/i.test(path) || /CMakeLists\.txt$/i.test(path);
}

export function churnFor(dir, sinceIso) {
  const git = spawnSync("git", ["log", `--since=${sinceIso}`, "--numstat", "--pretty=format:"], {
    cwd: dir, encoding: "utf8", stdio: ["ignore", "pipe", "pipe"],
  });
  if (git.status !== 0 || !git.stdout) return null;
  let testAdded = 0;
  let sourceAdded = 0;
  for (const line of git.stdout.split(/\r?\n/)) {
    const parts = line.split("\t");
    if (parts.length < 3) continue;
    const added = Number(parts[0]);
    if (!Number.isFinite(added)) continue;
    const path = parts[2];
    if (!isSourcePath(path)) continue;
    if (isTestPath(path)) testAdded += added;
    else sourceAdded += added;
  }
  if (testAdded + sourceAdded === 0) return null;
  return { testAdded, sourceAdded, testShare: testAdded / (testAdded + sourceAdded) };
}

function churnTargets(repoRoot) {
  const targets = [{ name: basename(repoRoot), dir: repoRoot }];
  const privateGames = join(repoRoot, "games", "private");
  if (existsSync(privateGames)) {
    for (const entry of readdirSync(privateGames, { withFileTypes: true })) {
      if (!entry.isDirectory()) continue;
      const dir = join(privateGames, entry.name);
      if (existsSync(join(dir, ".git"))) targets.push({ name: entry.name, dir });
    }
  }
  return targets;
}

// --- report ---------------------------------------------------------------

export function buildIterationReport(options = {}) {
  const now = options.now ?? Date.now();
  const sinceMs = options.sinceMs ?? (now - 7 * 24 * 60 * 60 * 1000);
  const repoRoot = options.repoRoot || process.env.AI_ITERATION_REPO || REPO_ROOT;
  const projectsRoot = options.claudeProjects || process.env.AI_ITERATION_CLAUDE_PROJECTS
    || join(homedir(), ".claude", "projects");
  const codexRoot = options.codexSessions || process.env.CODEX_SESSION_ROOT
    || join(homedir(), ".codex", "sessions");

  const harness = {
    claude: { sessions: 0, ...emptyBucket(), turns: [] },
    codex: { sessions: 0, ...emptyBucket(), turns: [] },
  };
  const categories = new Map();
  const commands = new Map();

  const nestedCounts = new Map();

  const account = (harnessName, records) => {
    for (const record of records) {
      if (record.event_type === "session_start") continue;
      if (record.ts && Date.parse(record.ts) < sinceMs) continue;
      add(harness[harnessName], record);
      const category = categoryOf(record);
      if (!categories.has(category)) categories.set(category, emptyBucket());
      add(categories.get(category), record);
      for (const nested of record.nested_commands || []) {
        const key = normalizeCommand(nested);
        if (key) nestedCounts.set(key, (nestedCounts.get(key) || 0) + 1);
      }
      const key = normalizeCommand(record.commands && record.commands[0]);
      if (!key) continue;
      if (!commands.has(key)) commands.set(key, emptyBucket());
      add(commands.get(key), record);
    }
  };

  for (const dir of claudeProjectDirs(projectsRoot, repoRoot)) {
    for (const entry of readdirSync(dir)) {
      if (!entry.endsWith(".jsonl")) continue;
      const file = join(dir, entry);
      if (statSync(file).mtimeMs < sinceMs) continue;
      const parsed = parseClaudeTranscript(file);
      if (parsed.records.length === 0 && parsed.turns.length === 0) continue;
      harness.claude.sessions += 1;
      harness.claude.turns.push(...parsed.turns);
      account("claude", parsed.records);
    }
  }

  for (const file of codexTranscriptFiles(codexRoot, sinceMs)) {
    let cwd = "";
    try { cwd = codexTranscriptCwd(file); } catch { continue; }
    if (!cwd || !cwd.replace(/\\/g, "/").toLowerCase().includes(basename(repoRoot).toLowerCase())) continue;
    const parsed = parseCodexTranscript(file);
    if (parsed.records.length === 0) continue;
    harness.codex.sessions += 1;
    harness.codex.turns.push(...codexTurns(file));
    account("codex", parsed.records);
  }

  const churn = [];
  const sinceIso = new Date(sinceMs).toISOString();
  for (const target of churnTargets(repoRoot)) {
    const result = churnFor(target.dir, sinceIso);
    if (result) churn.push({ name: target.name, ...result });
  }

  const topCommands = [...commands.entries()]
    .sort((a, b) => b[1].chars - a[1].chars)
    .slice(0, options.top || 8)
    .map(([command, bucket]) => ({ command, ...bucket }));

  return {
    schema: "ai_studio.profiling.iteration_report.v1",
    window: { since: sinceIso, until: new Date(now).toISOString() },
    repo: repoRoot,
    harness: Object.fromEntries(Object.entries(harness).map(([name, bucket]) => [name, {
      sessions: bucket.sessions,
      calls: bucket.calls,
      hours: bucket.ms / 3.6e6,
      output_mb: bucket.chars / 1e6,
      truncated_tokens: bucket.truncatedTokens,
      fail_rate: bucket.calls > 0 ? bucket.fails / bucket.calls : 0,
      turns: bucket.turns.length,
      median_turn_minutes: median(bucket.turns.map((turn) => turn.ms)) / 6e4,
      median_tools_per_turn: median(bucket.turns.map((turn) => turn.tools)),
    }])),
    categories: [...categories.entries()]
      .sort((a, b) => b[1].chars - a[1].chars)
      .map(([name, bucket]) => ({
        name,
        calls: bucket.calls,
        hours: bucket.ms / 3.6e6,
        output_mb: bucket.chars / 1e6,
        fail_rate: bucket.calls > 0 ? bucket.fails / bucket.calls : 0,
      })),
    noisiest_commands: topCommands.map((entry) => ({
      command: entry.command,
      calls: entry.calls,
      output_mb: entry.chars / 1e6,
      truncated_tokens: entry.truncatedTokens,
    })),
    frequent_nested_commands: [...nestedCounts.entries()]
      .sort((a, b) => b[1] - a[1])
      .slice(0, options.top || 8)
      .map(([command, calls]) => ({ command, calls })),
    churn,
  };
}

export function renderIterationReport(report) {
  const lines = [];
  const pct = (value) => `${Math.round(value * 100)}%`;
  lines.push(`Iteration report  ${report.window.since.slice(0, 10)} .. ${report.window.until.slice(0, 10)}  (${basename(report.repo)})`);
  lines.push("");
  lines.push("harness  sessions   calls   hours   output   fail   turn(med)  tools/turn");
  for (const [name, data] of Object.entries(report.harness)) {
    lines.push([
      name.padEnd(7),
      String(data.sessions).padStart(8),
      String(data.calls).padStart(7),
      data.hours.toFixed(1).padStart(7),
      `${data.output_mb.toFixed(1)}MB`.padStart(8),
      pct(data.fail_rate).padStart(6),
      `${data.median_turn_minutes.toFixed(1)}m`.padStart(10),
      String(data.median_tools_per_turn).padStart(11),
    ].join(" "));
  }
  const truncated = Object.values(report.harness).reduce((sum, data) => sum + data.truncated_tokens, 0);
  if (truncated > 0) lines.push(`Truncated output: ${(truncated / 1e6).toFixed(1)}M tokens produced and cut before the model saw them.`);
  lines.push("");
  lines.push("category         calls   hours   output   fail");
  for (const category of report.categories) {
    lines.push([
      category.name.padEnd(14),
      String(category.calls).padStart(7),
      category.hours.toFixed(1).padStart(7),
      `${category.output_mb.toFixed(1)}MB`.padStart(8),
      pct(category.fail_rate).padStart(6),
    ].join(" "));
  }
  if (report.noisiest_commands.length > 0) {
    lines.push("");
    lines.push("noisiest commands by output");
    for (const entry of report.noisiest_commands) {
      lines.push(`  ${entry.output_mb.toFixed(1)}MB  n=${String(entry.calls).padStart(4)}  ${entry.command}`);
    }
  }
  if (report.frequent_nested_commands.length > 0) {
    lines.push("");
    lines.push("most frequent commands inside parallel batches (counts only; shared output is not attributable)");
    for (const entry of report.frequent_nested_commands) {
      lines.push(`  n=${String(entry.calls).padStart(5)}  ${entry.command}`);
    }
  }
  if (report.churn.length > 0) {
    lines.push("");
    lines.push("written lines in window (git)");
    for (const entry of report.churn) {
      lines.push(`  ${entry.name.padEnd(28)} source +${entry.sourceAdded}  tests +${entry.testAdded}  (${pct(entry.testShare)} of new lines are tests)`);
    }
  }
  return lines.join("\n");
}

async function main() {
  const { values } = parseArgs(process.argv.slice(2));
  if (values.help) {
    usage();
    return 0;
  }
  const since = stringArg(values, "since", "7d");
  const sinceMs = parseSince(since);
  if (!sinceMs) {
    usage();
    return 2;
  }
  const report = buildIterationReport({ sinceMs, top: numberArg(values, "top") || 8 });
  console.log(values.json ? JSON.stringify(report, null, 2) : renderIterationReport(report));
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  process.exitCode = await main();
}
