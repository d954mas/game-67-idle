#!/usr/bin/env node
import { readFileSync } from "node:fs";
import { homedir } from "node:os";
import { basename, isAbsolute, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const GENERIC_ROLES = new Set(["", "subagent", "default", "general-purpose", "explorer", "worker"]);
const UUID_RE = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

function requiredArg(argv, name) {
  const index = argv.indexOf(name);
  const value = index >= 0 ? argv[index + 1] : "";
  if (!value || value.startsWith("--")) throw new Error(`${name} is required`);
  return value;
}

export function readCodexRoleEvidence(text) {
  const threadSources = new Set();
  const observedRoles = new Set();
  const actualModels = new Set();
  const rolloutIds = new Set();
  const sessionIds = new Set();
  const parentThreadIds = new Set();
  const depths = new Set();
  const agentPaths = new Set();
  let malformedRecords = 0;
  for (const line of String(text).split(/\r?\n/)) {
    if (!line.trim()) continue;
    let record;
    try { record = JSON.parse(line); } catch { malformedRecords += 1; continue; }
    if (record.type === "session_meta") {
      const payload = record.payload || {};
      const spawn = payload.source?.subagent?.thread_spawn || {};
      threadSources.add(payload.thread_source || "");
      rolloutIds.add(payload.id || "");
      sessionIds.add(payload.session_id || "");
      parentThreadIds.add(spawn.parent_thread_id || "");
      depths.add(spawn.depth ?? "");
      agentPaths.add(spawn.agent_path || "");
      observedRoles.add(spawn.agent_role || "");
    }
    if (record.type === "turn_context") actualModels.add(record.payload?.model || "");
  }
  return {
    thread_source: threadSources.size === 1 ? [...threadSources][0] : "",
    thread_sources: [...threadSources],
    observed_agent_role: observedRoles.size === 1 ? [...observedRoles][0] : "",
    actual_model: actualModels.size === 1 ? [...actualModels][0] : "",
    observed_roles: [...observedRoles],
    actual_models: [...actualModels],
    rollout_ids: [...rolloutIds],
    session_ids: [...sessionIds],
    parent_thread_ids: [...parentThreadIds],
    depths: [...depths],
    agent_paths: [...agentPaths],
    malformed_records: malformedRecords,
  };
}

export function verifyCodexRoleEvidence(evidence, requestedRole, expectedModel) {
  const errors = [];
  if (GENERIC_ROLES.has(String(requestedRole).toLowerCase())) errors.push("requested role must be a named stock role");
  if (evidence.thread_sources?.length > 1) errors.push("transcript contains conflicting thread_source values");
  if (evidence.thread_source !== "subagent") errors.push("evidence is not a native subagent transcript");
  if (evidence.malformed_records > 0) errors.push(`transcript contains ${evidence.malformed_records} malformed record(s)`);
  if (evidence.observed_roles?.length > 1) errors.push("transcript contains conflicting agent_role values");
  if (evidence.actual_models?.length > 1) errors.push("transcript contains conflicting model values");
  if (evidence.rollout_ids?.length !== 1 || !UUID_RE.test(evidence.rollout_ids[0])) errors.push("native rollout id is missing or conflicting");
  if (evidence.session_ids?.length !== 1 || !UUID_RE.test(evidence.session_ids[0])) errors.push("native session id is missing or conflicting");
  if (evidence.parent_thread_ids?.length !== 1 || !UUID_RE.test(evidence.parent_thread_ids[0])) errors.push("native parent_thread_id is missing or conflicting");
  if (evidence.depths?.length !== 1 || !Number.isInteger(evidence.depths[0]) || evidence.depths[0] < 1) errors.push("native subagent depth is missing or invalid");
  if (evidence.agent_paths?.length !== 1 || !/^\/root\/[a-z0-9_/-]+$/i.test(evidence.agent_paths[0])) errors.push("native agent_path is missing or invalid");
  if (GENERIC_ROLES.has(String(evidence.observed_agent_role || "").toLowerCase())) errors.push("generic fallback or missing agent_role observed");
  else if (evidence.observed_agent_role !== requestedRole) errors.push(`agent_role mismatch: expected ${requestedRole}, observed ${evidence.observed_agent_role}`);
  if (!evidence.actual_model) errors.push("actual selected model is missing from turn_context");
  else if (evidence.actual_model !== expectedModel) errors.push(`model mismatch: expected ${expectedModel}, observed ${evidence.actual_model}`);
  return errors;
}

export function verifyNativeEvidencePath(evidencePath, evidence, env = process.env, home = homedir()) {
  const sessionsRoot = resolve(env.CODEX_HOME || join(home, ".codex"), "sessions");
  const absolute = resolve(evidencePath);
  const within = relative(sessionsRoot, absolute);
  const rolloutId = evidence.rollout_ids?.length === 1 ? evidence.rollout_ids[0] : "";
  const errors = [];
  if (!within || within.startsWith("..") || isAbsolute(within)) errors.push("evidence must come from the native Codex sessions store");
  if (!rolloutId || !basename(absolute).startsWith("rollout-") || !basename(absolute).endsWith(`${rolloutId}.jsonl`)) {
    errors.push("evidence filename does not match the native rollout id");
  }
  return errors;
}

export function expectedCodexModel(root, requestedRole) {
  if (!/^[a-z0-9-]+$/.test(requestedRole) || GENERIC_ROLES.has(requestedRole)) {
    throw new Error("--requested-role must name a stock Codex role");
  }
  const catalog = readFileSync(resolve(root, ".codex", "agents", `${requestedRole}.toml`), "utf8");
  const match = catalog.match(/^model\s*=\s*"([^"]+)"\s*$/m);
  if (!match) throw new Error(`stock role ${requestedRole} has no explicit model`);
  return match[1];
}

function main(argv) {
  const root = resolve(argv.includes("--root") ? requiredArg(argv, "--root") : ".");
  const evidencePath = resolve(requiredArg(argv, "--evidence"));
  const requestedRole = requiredArg(argv, "--requested-role");
  const expectedModel = expectedCodexModel(root, requestedRole);
  const evidence = readCodexRoleEvidence(readFileSync(evidencePath, "utf8"));
  const errors = [
    ...verifyNativeEvidencePath(evidencePath, evidence),
    ...verifyCodexRoleEvidence(evidence, requestedRole, expectedModel),
  ];
  const result = { requested_role: requestedRole, expected_model: expectedModel, ...evidence, pass: errors.length === 0, errors };
  if (argv.includes("--json")) process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
  else if (errors.length === 0) process.stdout.write(`role/model smoke passed: ${requestedRole} -> ${evidence.actual_model}\n`);
  else process.stderr.write(`${errors.join("\n")}\n`);
  return errors.length === 0 ? 0 : 1;
}

if (process.argv[1] && resolve(process.argv[1]) === resolve(fileURLToPath(import.meta.url))) {
  try { process.exitCode = main(process.argv.slice(2)); }
  catch (error) { process.stderr.write(`${error.message}\n`); process.exitCode = 2; }
}
