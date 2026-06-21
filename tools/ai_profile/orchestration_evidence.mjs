#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, readdirSync } from "node:fs";
import { dirname, isAbsolute, join, relative, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { parseArgs, latestSessionProfilePath, stringArg } from "./profile_lib.mjs";
import { todaySessionRoot } from "./orchestration_trace.mjs";
import { findRoot as findTaskboardRoot, inferCurrentDoingOrchestrationTaskId } from "../taskboard/lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/orchestration_evidence.mjs [--current|--task <T####>|--id <T####>|--file <task.md>] [--run] [options]

Options:
  --parent-thread-id <id>    Explicit parent thread id evidence source.
  --trace-session <file>     Explicit parent transcript evidence source.
  --session-root <dir>       Codex session root. Defaults to task command, CODEX_SESSION_FILE dir, then today's session root.
  --agent-cwd <dir>          Subagent cwd filter. Defaults to the taskboard root.
  --profile <file>           Parent profile used by status and optional parent id inference.
  --agent-profile-dir <dir>  Subagent profile directory for debug/tests.
  --min-agents <n>           Required subagent count. Defaults to task command value, then 2.
  --json-output <file>       Evidence artifact path. Defaults to task command value, then tasks/evidence/<task>-status-rollup.json.
  --run                      Execute the generated strict compact status evidence command.
  --json                     Emit wrapper metadata as JSON.`);
  process.exit(2);
}

function firstValue(value) {
  return Array.isArray(value) ? String(value[0] || "") : String(value || "");
}

function readSessionMeta(file) {
  if (!file || !existsSync(file)) return null;
  const firstLine = readFileSync(file, "utf8").split(/\r?\n/, 1)[0] || "";
  if (!firstLine.trim()) return null;
  try {
    const row = JSON.parse(firstLine);
    return row?.type === "session_meta" && row.payload ? row.payload : null;
  } catch {
    return null;
  }
}

function isSubagentMeta(meta) {
  return meta?.thread_source === "subagent" || Boolean(meta?.source?.subagent);
}

function listRollouts(dir) {
  if (!dir || !existsSync(dir)) return [];
  const out = [];
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) out.push(...listRollouts(path));
    else if (entry.isFile() && entry.name.startsWith("rollout-") && entry.name.endsWith(".jsonl")) out.push(path);
  }
  return out.sort();
}

function profileShortId(profilePath) {
  const name = String(profilePath || "").split(/[\\/]/).pop() || "";
  const match = name.match(/__(?:claude|codex)__([0-9a-fA-F]{8})(?:\.jsonl)?$/);
  return match ? match[1].toLowerCase() : "";
}

function shellFlagValue(command, flags) {
  for (const flag of flags) {
    const escaped = flag.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    const match = String(command || "").match(new RegExp(`(?:^|\\s)${escaped}(?:=|\\s+)(?:"([^"]+)"|'([^']+)'|(\\S+))`, "i"));
    if (match) return match[1] || match[2] || match[3] || "";
  }
  return "";
}

function parseMinAgents(raw, fallback = 2) {
  const parsed = Number.parseInt(String(raw || ""), 10);
  return Number.isFinite(parsed) && parsed >= 0 ? parsed : fallback;
}

function findTaskFile(root, taskId) {
  const id = String(taskId || "").toLowerCase();
  if (!id) return "";
  for (const area of ["active", "review", "backlog", "todo"]) {
    const dir = join(root, "tasks", area);
    if (!existsSync(dir)) continue;
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      if (entry.isFile() && entry.name.toLowerCase().startsWith(id) && entry.name.endsWith(".md")) {
        return join(dir, entry.name);
      }
    }
  }
  return "";
}

function taskIdFromFile(file) {
  if (!file || !existsSync(file)) return "";
  const text = readFileSync(file, "utf8");
  const match = text.match(/^id:\s*(T\d+)\s*$/mi);
  return match ? match[1] : "";
}

function evidenceCommandsFromTask(file) {
  if (!file || !existsSync(file)) return [];
  const text = readFileSync(file, "utf8");
  const commands = [];
  for (const line of text.split(/\r?\n/)) {
    const match = line.match(/\bevidence command:\s*(.+)$/i);
    if (match) commands.push(match[1].trim());
  }
  return commands;
}

function declaredStatusEvidence(commands) {
  return commands
    .filter((command) => /\bnode\s+tools[\\/]ai\.mjs\s+status\b/i.test(command.replaceAll("\\", "/")))
    .filter((command) => /\s--agent-rollup\b/i.test(command) && /\s--require-agent-rollup-ok\b/i.test(command))
    .map((command) => ({
      command,
      parentThreadId: shellFlagValue(command, ["--parent-thread-id"]),
      traceSession: shellFlagValue(command, ["--trace-session"]),
      sessionRoot: shellFlagValue(command, ["--session-root"]),
      agentCwd: shellFlagValue(command, ["--agent-cwd", "--cwd"]),
      artifact: shellFlagValue(command, ["--json-output"]),
      minAgents: shellFlagValue(command, ["--min-agents"]),
      profile: shellFlagValue(command, ["--profile"]),
    }));
}

function repoRelativeArtifact(root, raw) {
  const value = String(raw || "").trim();
  if (!value) return { ok: false, problem: "missing artifact path" };
  const absolute = isAbsolute(value) ? resolve(value) : resolve(root, value);
  const rel = relative(root, absolute);
  if (rel === "" || rel.startsWith("..") || isAbsolute(rel)) {
    return { ok: false, problem: `artifact path is outside taskboard root: ${value}` };
  }
  if (!rel.replaceAll("\\", "/").startsWith("tasks/evidence/") || !rel.endsWith(".json")) {
    return { ok: false, problem: `artifact path must be a repo-local tasks/evidence/*.json file: ${value}` };
  }
  return { ok: true, relative: rel };
}

function inferParentThreadId({ explicit, declared, traceSession, sessionRoot, profile, cwd }) {
  if (explicit) return { parentThreadId: explicit, source: "explicit" };
  if (traceSession) return { parentThreadId: "", source: "trace-session" };
  if (declared) return { parentThreadId: declared, source: "task-command" };

  const envFile = process.env.CODEX_SESSION_FILE || "";
  const envMeta = readSessionMeta(envFile);
  if (envMeta?.id && !isSubagentMeta(envMeta)) {
    return { parentThreadId: String(envMeta.id), source: "CODEX_SESSION_FILE", sessionFile: envFile };
  }

  const profilePath = profile || latestSessionProfilePath();
  const shortId = profileShortId(profilePath);
  if (!shortId) {
    return {
      parentThreadId: "",
      source: "missing",
      problem: "could not infer parent thread id from task command, CODEX_SESSION_FILE, or profile name",
    };
  }

  const wantedCwd = cwd ? resolve(cwd).toLowerCase() : "";
  const candidates = [];
  for (const file of listRollouts(sessionRoot)) {
    const meta = readSessionMeta(file);
    if (!meta || isSubagentMeta(meta)) continue;
    const id = String(meta.id || "");
    if (!id.toLowerCase().startsWith(shortId)) continue;
    if (wantedCwd && resolve(String(meta.cwd || "")).toLowerCase() !== wantedCwd) continue;
    candidates.push({ id, file });
  }

  if (candidates.length === 1) {
    return { parentThreadId: candidates[0].id, source: "profile-session-root", sessionFile: candidates[0].file };
  }
  return {
    parentThreadId: "",
    source: "ambiguous",
    problem: candidates.length === 0
      ? `no parent transcript in session root matched profile short id ${shortId}`
      : `multiple parent transcripts matched profile short id ${shortId}`,
    candidates,
  };
}

function quoteArg(value) {
  const text = String(value);
  return /[\s"]/u.test(text) ? `"${text.replaceAll('"', '\\"')}"` : text;
}

function commandString(args) {
  return args.map(quoteArg).join(" ");
}

const statusScript = resolve(dirname(fileURLToPath(import.meta.url)), "status.mjs");

function fail(problem, nextAction, extra = {}) {
  return { ok: false, problem, next_action: nextAction, ...extra };
}

function selectTask(root, values) {
  const explicitFile = stringArg(values, "file", "");
  if (explicitFile) {
    const taskFile = isAbsolute(explicitFile) ? resolve(explicitFile) : resolve(root, explicitFile);
    const id = taskIdFromFile(taskFile);
    return id
      ? { taskId: id, taskFile, selector: "file" }
      : { problem: `could not read task id from ${explicitFile}` };
  }
  const taskId = stringArg(values, "task", "") || stringArg(values, "id", "");
  if (taskId) {
    const taskFile = findTaskFile(root, taskId);
    if (!taskFile) return { problem: `task not found: ${taskId}` };
    return { taskId, taskFile, selector: "task" };
  }
  const current = inferCurrentDoingOrchestrationTaskId(root);
  if (!current) {
    return { problem: "missing current orchestration task" };
  }
  const taskFile = findTaskFile(root, current);
  if (!taskFile) return { problem: `task not found: ${current}` };
  return { taskId: current, taskFile, selector: "current" };
}

function assertNoConflict(name, explicit, declared) {
  if (!explicit || !declared || resolveLike(name, explicit) === resolveLike(name, declared)) return "";
  return `${name} conflicts with task evidence command: ${explicit} != ${declared}`;
}

function resolveLike(name, value) {
  return name.includes("path") || name.includes("root") || name.includes("cwd") || name.includes("session")
    ? resolve(String(value || "")).toLowerCase()
    : String(value || "");
}

function buildResult(values) {
  const root = findTaskboardRoot(process.cwd());
  const selected = selectTask(root, values);
  if (selected.problem) {
    return fail(
      selected.problem,
      "run `node tools/ai.mjs orchestration-check --current --json` or pass `--task T####`",
    );
  }

  const taskCommands = evidenceCommandsFromTask(selected.taskFile);
  const declaredCandidates = declaredStatusEvidence(taskCommands);
  if (declaredCandidates.length > 1) {
    const unique = new Set(declaredCandidates.map((item) => [
      item.parentThreadId,
      item.traceSession,
      item.sessionRoot,
      item.agentCwd,
      item.artifact,
      item.minAgents,
    ].join("\0")));
    if (unique.size > 1) {
      return fail(
        "multiple status evidence commands declare different sources or artifacts",
        "keep one task evidence command or pass explicit --parent-thread-id/--trace-session and --json-output",
        { task_id: selected.taskId, task_file: relative(root, selected.taskFile) },
      );
    }
  }
  const declared = declaredCandidates[0] || {};

  const envSessionFile = process.env.CODEX_SESSION_FILE || "";
  const explicitParent = stringArg(values, "parent-thread-id", "");
  const explicitTrace = stringArg(values, "trace-session", "");
  const explicitSessionRoot = stringArg(values, "session-root", "");
  const explicitAgentCwd = stringArg(values, "agent-cwd", "");
  const explicitArtifact = stringArg(values, "json-output", "");

  const conflicts = [
    assertNoConflict("parent-thread-id", explicitParent, declared.parentThreadId),
    assertNoConflict("trace-session path", explicitTrace, declared.traceSession),
    assertNoConflict("session-root path", explicitSessionRoot, declared.sessionRoot),
    assertNoConflict("agent-cwd path", explicitAgentCwd, declared.agentCwd),
    assertNoConflict("json-output path", explicitArtifact, declared.artifact),
  ].filter(Boolean);
  if (conflicts.length) {
    return fail(conflicts[0], "update the task evidence command or remove the conflicting override", {
      task_id: selected.taskId,
      task_file: relative(root, selected.taskFile),
    });
  }

  const sessionRoot = explicitSessionRoot
    || declared.sessionRoot
    || (envSessionFile ? dirname(envSessionFile) : "")
    || todaySessionRoot();
  const agentCwd = explicitAgentCwd || declared.agentCwd || root;
  const traceSession = explicitTrace || declared.traceSession || "";
  const profile = stringArg(values, "profile", "") || declared.profile || "";
  const parent = inferParentThreadId({
    explicit: explicitParent,
    declared: declared.parentThreadId,
    traceSession,
    sessionRoot,
    profile,
    cwd: agentCwd,
  });
  if (!parent.parentThreadId && !traceSession) {
    return fail(
      parent.problem || "missing evidence source",
      "pass `--parent-thread-id <id>` or `--trace-session <codex-session.jsonl>` explicitly",
      {
        task_id: selected.taskId,
        task_file: selected.taskFile ? relative(root, selected.taskFile) : "",
        inference_source: parent.source,
        candidates: parent.candidates || [],
      },
    );
  }

  const minAgents = parseMinAgents(firstValue(values["min-agents"]) || declared.minAgents, 2);
  const artifactRaw = explicitArtifact || declared.artifact || join("tasks", "evidence", `${selected.taskId}-status-rollup.json`);
  const artifact = repoRelativeArtifact(root, artifactRaw);
  if (!artifact.ok) {
    return fail(artifact.problem, "use a repo-local --json-output under tasks/evidence/", {
      task_id: selected.taskId,
      task_file: selected.taskFile ? relative(root, selected.taskFile) : "",
    });
  }

  const statusOptions = [
    "--agent-rollup",
    "--require-agent-rollup-ok",
    "--min-agents", String(minAgents),
    "--session-root", sessionRoot,
    "--agent-cwd", agentCwd,
    "--agent-rollup-evidence",
    "--json-output", artifact.relative,
    "--no-import-codex-session",
  ];
  if (traceSession) statusOptions.push("--trace-session", traceSession);
  if (parent.parentThreadId) statusOptions.push("--parent-thread-id", parent.parentThreadId);
  if (profile) statusOptions.push("--profile", profile);
  const agentProfileDir = stringArg(values, "agent-profile-dir", "");
  if (agentProfileDir) statusOptions.push("--agent-profile-dir", agentProfileDir);
  const command = commandString(["node", "tools/ai.mjs", "status", ...statusOptions]);

  return {
    ok: true,
    mode: values.run === true ? "run" : "dry-run",
    task_id: selected.taskId,
    task_file: selected.taskFile ? relative(root, selected.taskFile) : "",
    selector: selected.selector,
    artifact: artifact.relative,
    artifact_source: explicitArtifact ? "explicit" : declared.artifact ? "task-command" : "inferred",
    min_agents: minAgents,
    parent_thread_id: parent.parentThreadId,
    trace_session: traceSession,
    source: traceSession ? "trace-session" : "parent-thread",
    inference_source: parent.source,
    session_root: sessionRoot,
    agent_cwd: agentCwd,
    command_args: [process.execPath, statusScript, ...statusOptions],
    command,
    next_action: values.run === true
      ? `append task log evidence after success: - evidence: PASS \`${command}\``
      : "run with `--run` to write strict compact status evidence",
  };
}

function printDryRun(result) {
  console.log(`task: ${result.task_id}${result.task_file ? ` (${result.task_file})` : ""}`);
  console.log(`source: ${result.source} ${result.parent_thread_id || result.trace_session}`);
  console.log(`session root: ${result.session_root}`);
  console.log(`agent cwd: ${result.agent_cwd}`);
  console.log(`min agents: ${result.min_agents}`);
  console.log(`artifact: ${result.artifact} (${result.artifact_source})`);
  console.log("command:");
  console.log(result.command);
  console.log(`next: ${result.next_action}`);
}

function main() {
  const { values } = parseArgs(process.argv.slice(2));
  if (values.help || values.h) usage();

  const result = buildResult(values);
  if (!result.ok) {
    if (values.json === true) console.log(JSON.stringify(result, null, 2));
    else {
      console.error(`error: ${result.problem}`);
      console.error(`next: ${result.next_action}`);
    }
    return 1;
  }

  if (values.run !== true) {
    if (values.json === true) console.log(JSON.stringify(result, null, 2));
    else printDryRun(result);
    return 0;
  }

  mkdirSync(dirname(resolve(findTaskboardRoot(process.cwd()), result.artifact)), { recursive: true });
  const run = spawnSync(result.command_args[0], result.command_args.slice(1), {
    cwd: findTaskboardRoot(process.cwd()),
    env: process.env,
    stdio: values.json === true ? ["ignore", "pipe", "pipe"] : "inherit",
    encoding: values.json === true ? "utf8" : undefined,
    shell: false,
  });
  result.exit_code = run.status ?? 1;
  result.status_ok = result.exit_code === 0;
  if (values.json === true) {
    result.stdout = run.stdout || "";
    result.stderr = run.stderr || "";
    console.log(JSON.stringify(result, null, 2));
  }
  return result.exit_code;
}

process.exit(main());
