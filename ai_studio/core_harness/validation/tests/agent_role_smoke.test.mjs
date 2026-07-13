import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { expectedCodexModel, readCodexRoleEvidence, verifyCodexRoleEvidence, verifyNativeEvidencePath } from "../agent_role_smoke.mjs";

const root = resolve(fileURLToPath(new URL("../../../../", import.meta.url)));
const command = join(root, "ai_studio", "core_harness", "validation", "agent_role_smoke.mjs");
const rolloutId = "019f5001-1509-77f2-8624-c41e352d22e2";
const parentThreadId = "019f4d71-3c4e-7fd2-ad11-257dcc9361b4";

function transcript(role = "fast-worker", model = "gpt-5.6-luna") {
  const records = [
    { type: "session_meta", payload: { id: rolloutId, thread_source: "subagent", source: { subagent: { thread_spawn: { parent_thread_id: parentThreadId, depth: 1, agent_path: "/root/smoke", agent_nickname: "Smoke", agent_role: role } } } } },
    { type: "session_meta", payload: { id: parentThreadId, thread_source: "user" } },
    { type: "turn_context", payload: { model } },
  ];
  return `${records.map((record) => JSON.stringify(record)).join("\n")}\n`;
}

test("role smoke accepts the canonical rollout schema and matching role/model", () => {
  const evidence = readCodexRoleEvidence(transcript());
  assert.deepEqual(verifyCodexRoleEvidence(evidence, "fast-worker", "gpt-5.6-luna"), []);
});

test("role smoke rejects model and role mismatch", () => {
  const errors = verifyCodexRoleEvidence(readCodexRoleEvidence(transcript("researcher", "gpt-5.6-terra")), "fast-worker", "gpt-5.6-luna");
  assert.match(errors.join("\n"), /agent_role mismatch/);
  assert.match(errors.join("\n"), /model mismatch/);
});

test("role smoke rejects generic or missing agent role and missing model", () => {
  const errors = verifyCodexRoleEvidence(readCodexRoleEvidence(transcript(null, "")), "fast-worker", "gpt-5.6-luna");
  assert.match(errors.join("\n"), /generic fallback or missing agent_role/);
  assert.match(errors.join("\n"), /actual selected model is missing/);
});

test("role smoke rejects a top-level role without native spawn evidence", () => {
  const records = [
    { type: "session_meta", payload: { thread_source: "subagent", agent_role: "fast-worker" } },
    { type: "turn_context", payload: { model: "gpt-5.6-luna" } },
  ];
  const evidence = readCodexRoleEvidence(`${records.map((record) => JSON.stringify(record)).join("\n")}\n`);
  const errors = verifyCodexRoleEvidence(evidence, "fast-worker", "gpt-5.6-luna");
  assert.match(errors.join("\n"), /native rollout id/);
});

test("role smoke derives the expected model from the stock role catalog", () => {
  assert.equal(expectedCodexModel(root, "fast-worker"), "gpt-5.6-luna");
  assert.throws(() => expectedCodexModel(root, "worker"), /stock Codex role/);
});

test("role smoke rejects malformed or conflicting native evidence", () => {
  const evidence = readCodexRoleEvidence(`${transcript()}not-json\n${transcript("fast-worker", "gpt-5.6-terra")}`);
  const errors = verifyCodexRoleEvidence(evidence, "fast-worker", "gpt-5.6-luna");
  assert.match(errors.join("\n"), /malformed record/);
  assert.match(errors.join("\n"), /conflicting model/);
});

test("role smoke rejects conflicting explicitly reported agent roles", () => {
  const evidence = readCodexRoleEvidence(`${transcript("fast-worker")}${transcript("researcher")}`);
  const errors = verifyCodexRoleEvidence(evidence, "fast-worker", "gpt-5.6-luna");
  assert.match(errors.join("\n"), /conflicting agent_role/);
});

test("role smoke rejects conflicting native rollout ids", () => {
  const conflictingNative = JSON.stringify({
    type: "session_meta",
    payload: { id: "019f5001-1509-77f2-8624-c41e352d22e3", thread_source: "subagent", source: { subagent: { thread_spawn: { parent_thread_id: parentThreadId, depth: 1, agent_path: "/root/smoke", agent_role: null } } } },
  });
  const evidence = readCodexRoleEvidence(`${transcript()}${conflictingNative}\n`);
  const errors = verifyCodexRoleEvidence(evidence, "fast-worker", "gpt-5.6-luna");
  assert.match(errors.join("\n"), /native rollout id is missing or conflicting/);
});

test("role smoke CLI reports native evidence fields as JSON", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "agent-role-smoke-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const sessions = join(dir, ".codex", "sessions", "2026", "07", "11");
  mkdirSync(sessions, { recursive: true });
  const evidence = join(sessions, `rollout-2026-07-11T12-08-01-${rolloutId}.jsonl`);
  writeFileSync(evidence, transcript(), "utf8");
  const result = spawnSync(process.execPath, [command, "--evidence", evidence, "--requested-role", "fast-worker", "--json"], {
    cwd: root,
    encoding: "utf8",
    env: { ...process.env, CODEX_HOME: join(dir, ".codex") },
  });
  assert.equal(result.status, 0, result.stderr);
  const output = JSON.parse(result.stdout);
  assert.equal(output.observed_agent_role, "fast-worker");
  assert.equal(output.actual_model, "gpt-5.6-luna");
  assert.deepEqual(output.rollout_ids, [rolloutId]);
  assert.deepEqual(output.parent_thread_ids, [parentThreadId]);
  assert.deepEqual(output.depths, [1]);
  assert.deepEqual(output.agent_paths, ["/root/smoke"]);
  assert.equal(Object.hasOwn(output, "session_ids"), false);
  assert.equal(output.pass, true);
});

test("role smoke rejects evidence outside the native sessions store", (t) => {
  const dir = mkdtempSync(join(tmpdir(), "agent-role-path-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const evidence = readCodexRoleEvidence(transcript());
  const errors = verifyNativeEvidencePath(join(dir, `rollout-${rolloutId}.jsonl`), evidence, { CODEX_HOME: join(dir, ".codex") }, dir);
  assert.match(errors.join("\n"), /native Codex sessions store/);
});
