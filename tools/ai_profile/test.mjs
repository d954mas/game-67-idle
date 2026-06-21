import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawn, spawnSync } from "node:child_process";
import { isMachineEvidenceCommand } from "../taskboard/lib.mjs";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function tempDir() {
  return mkdtempSync(join(tmpdir(), "ai-profile-test-"));
}

function cleanup(dir) {
  rmSync(dir, { recursive: true, force: true });
}

function run(args, options = {}) {
  const result = spawnSync(process.execPath, args, {
    cwd: root,
    env: { ...process.env, ...(options.env || {}) },
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  assert.equal(result.status, 0, `${args.join(" ")}\nstdout:\n${result.stdout}\nstderr:\n${result.stderr}`);
  return result;
}

function readJson(file) {
  return JSON.parse(readFileSync(file, "utf8"));
}

function readJsonl(file) {
  return readFileSync(file, "utf8")
    .trim()
    .split(/\r?\n/)
    .filter(Boolean)
    .map((line) => JSON.parse(line));
}

function writeJsonl(file, records) {
  writeFileSync(file, records.map((record) => JSON.stringify(record)).join("\n") + "\n", "utf8");
}

function writeTaskboardTask(rootDir, { id = "T0001", status = "doing", tags = ["pipeline", "orchestration"], evidenceCommand = "" } = {}) {
  const activeDir = join(rootDir, "tasks", "active");
  mkdirSync(activeDir, { recursive: true });
  const file = join(activeDir, `${id.toLowerCase()}-status-test.md`);
  writeFileSync(file, `---
id: ${id}
title: Status preflight inference
status: ${status}
epic: ""
priority: P2
tags: [${tags.join(", ")}]
created: 2026-06-21
updated: 2026-06-21
---

## What

Test task.

## Done when

- [ ] checked

## Open questions

## Log
${evidenceCommand ? `
- orchestration: used
  objective: test orchestration wrapper
  allowed files: tools/ai_profile/**
  tool-use guard: exact paths/discovery before reads; trace/status commands include evidence source and --json-output where applicable
  expected output: wrapper evidence works
  evidence command: ${evidenceCommand}
  stop condition: wrapper exits successfully
  independent reviewer: test harness
` : ""}
`, "utf8");
  return file;
}

function multiAgentCall(callId, name, args = {}, timestamp = "2026-06-21T10:00:00.000Z") {
  return {
    timestamp,
    type: "response_item",
    payload: {
      type: "function_call",
      name,
      call_id: callId,
      arguments: JSON.stringify(args),
    },
  };
}

function multiAgentOutput(callId, output = {}) {
  return {
    type: "response_item",
    payload: {
      type: "function_call_output",
      call_id: callId,
      output: JSON.stringify(output),
    },
  };
}

function shellCall(callId, command, timestamp = "2026-06-21T10:00:00.000Z") {
  return {
    timestamp,
    type: "response_item",
    payload: {
      type: "function_call",
      name: "shell_command",
      call_id: callId,
      arguments: JSON.stringify({ command }),
    },
  };
}

function shellOutput(callId, output, timestamp = "2026-06-21T10:00:01.000Z") {
  return {
    timestamp,
    type: "response_item",
    payload: {
      type: "function_call_output",
      call_id: callId,
      output,
    },
  };
}

function subagentSessionMeta(id, parentThreadId, cwd = root, timestamp = "2026-06-21T10:00:00.000Z") {
  return {
    type: "session_meta",
    payload: {
      id,
      timestamp,
      cwd,
      thread_source: "subagent",
      agent_nickname: `agent-${id}`,
      agent_role: "test verifier",
      source: {
        subagent: {
          thread_spawn: {
            parent_thread_id: parentThreadId,
          },
        },
      },
    },
  };
}

function writeToolUsageAgentRollout(dir, { file, agent, parent, timestamp = "2026-06-21T10:00:00.000Z", callId = "call_missing_path" }) {
  writeJsonl(join(dir, file), [
    subagentSessionMeta(agent, parent, root, timestamp),
    shellCall(callId, "Get-Content C:\\projects\\game-67-idle\\src\\missing_state.c", "2026-06-21T10:00:01.000Z"),
    shellOutput(callId, "Exit code: 1\nWall time: 0.3 seconds\nOutput:\nGet-Content : Cannot find path 'C:\\projects\\game-67-idle\\src\\missing_state.c' because it does not exist.\nFullyQualifiedErrorId : PathNotFound,Microsoft.PowerShell.Commands.GetContentCommand\nItemNotFoundException\n", "2026-06-21T10:00:02.000Z"),
  ]);
}

function writeEvidenceProbeAgentRollout(dir, { file, agent, parent, timestamp = "2026-06-21T10:00:00.000Z", callId = "call_strict_probe" }) {
  writeJsonl(join(dir, file), [
    subagentSessionMeta(agent, parent, root, timestamp),
    shellCall(callId, `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id ${parent}`, "2026-06-21T10:00:01.000Z"),
    shellOutput(callId, "Exit code: 1\nWall time: 0.4 seconds\nOutput:\n## Agent Rollup\n- strict problem: unresolved agent failures: 1\n", "2026-06-21T10:00:02.000Z"),
  ]);
}

function writeCleanAgentRollout(dir, { file, agent, parent, timestamp = "2026-06-21T10:01:00.000Z", callId = "clean_call" }) {
  writeJsonl(join(dir, file), [
    subagentSessionMeta(agent, parent, root, timestamp),
    shellCall(callId, "rg --files tools/ai_profile", "2026-06-21T10:01:01.000Z"),
    shellOutput(callId, "Exit code: 0\nWall time: 0.1 seconds\nOutput:\ntools/ai_profile/status.mjs\n", "2026-06-21T10:01:02.000Z"),
  ]);
}

function writeUnresolvedAgentRollout(dir, { file, agent, parent, timestamp = "2026-06-21T10:03:00.000Z", callId = "call_test_fail" }) {
  writeJsonl(join(dir, file), [
    subagentSessionMeta(agent, parent, root, timestamp),
    shellCall(callId, "node --test tools/agent.test.mjs", "2026-06-21T10:03:01.000Z"),
    shellOutput(callId, "Exit code: 1\nWall time: 0.1 seconds\nOutput:\nnot ok 1 real validation failure\n", "2026-06-21T10:03:02.000Z"),
  ]);
}

function writeCleanTailRollouts(dir, parent, prefix = "ffffffff-ffff-4fff-8fff-fffffffffff") {
  writeToolUsageAgentRollout(dir, { file: "rollout-bad.jsonl", agent: `${prefix}1`, parent, timestamp: "2026-06-21T10:00:00.000Z" });
  writeCleanAgentRollout(dir, { file: "rollout-clean-1.jsonl", agent: `${prefix}2`, parent, timestamp: "2026-06-21T10:01:00.000Z", callId: "clean_one" });
  writeCleanAgentRollout(dir, { file: "rollout-clean-2.jsonl", agent: `${prefix}3`, parent, timestamp: "2026-06-21T10:02:00.000Z", callId: "clean_two" });
  writeCleanAgentRollout(dir, { file: "rollout-clean-3.jsonl", agent: `${prefix}4`, parent, timestamp: "2026-06-21T10:03:00.000Z", callId: "clean_three" });
}

function runHook(payload, profile, harness = "codex", env = {}) {
  const result = spawnSync(process.execPath, ["tools/ai_profile/hook_record.mjs", harness], {
    cwd: root,
    env: {
      ...process.env,
      AI_PROFILE_FILE: profile,
      CODEX_SESSION_FILE: join(dirname(profile), "missing-codex-session.jsonl"),
      ...env,
    },
    input: JSON.stringify(payload),
    encoding: "utf8",
    stdio: ["pipe", "pipe", "pipe"],
  });
  assert.equal(
    result.status,
    0,
    `hook_record ${payload.hook_event_name || ""}\nstdout:\n${result.stdout}\nstderr:\n${result.stderr}`,
  );
  return result;
}

function runFastHook(payload, profile, harness = "codex") {
  const exe = join(root, "tools", "ai_profile", process.platform === "win32" ? "hook_record_fast.exe" : "hook_record_fast");
  const result = spawnSync(exe, [harness], {
    cwd: root,
    env: { ...process.env, AI_PROFILE_FILE: profile },
    input: JSON.stringify(payload),
    encoding: "utf8",
    stdio: ["pipe", "pipe", "pipe"],
  });
  assert.equal(result.status, 0, `hook_record_fast\nstdout:\n${result.stdout}\nstderr:\n${result.stderr}`);
}

function runFastHookAsync(payload, profile, harness = "codex") {
  const exe = join(root, "tools", "ai_profile", process.platform === "win32" ? "hook_record_fast.exe" : "hook_record_fast");
  return new Promise((resolvePromise, reject) => {
    const child = spawn(exe, [harness], {
      cwd: root,
      env: { ...process.env, AI_PROFILE_FILE: profile },
      stdio: ["pipe", "pipe", "pipe"],
    });
    let stdout = "";
    let stderr = "";
    child.stdout.on("data", (chunk) => { stdout += chunk; });
    child.stderr.on("data", (chunk) => { stderr += chunk; });
    child.on("error", reject);
    child.on("close", (code) => {
      if (code !== 0) {
        reject(new Error(`hook_record_fast exited ${code}\nstdout:\n${stdout}\nstderr:\n${stderr}`));
        return;
      }
      resolvePromise();
    });
    child.stdin.end(JSON.stringify(payload));
  });
}

test("hook_record logs session start and command start records", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "hook.jsonl");
    runHook({ hook_event_name: "SessionStart" }, profile);
    runHook({
      hook_event_name: "PreToolUse",
      tool_name: "Bash",
      tool_input: { command: "node --test tools/ai_profile/test.mjs" },
    }, profile);

    const records = readJsonl(profile);
    assert.equal(records.length, 2);
    assert.equal(records[0].event_type, "session_start");
    assert.equal(records[0].intent, "session start (codex)");
    assert.deepEqual(records[0].tools, ["codex/session"]);

    assert.equal(records[1].event_type, "tool_call_start");
    assert.equal(records[1].category, "validation");
    assert.equal(records[1].result, "unknown");
    assert.equal(records[1].value, "necessary_overhead");
    assert.deepEqual(records[1].tools, ["codex/Bash"]);
    assert.deepEqual(records[1].commands, ["node --test tools/ai_profile/test.mjs"]);
  } finally {
    cleanup(dir);
  }
});

test("hook_record logs failed command result records", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "hook-fail.jsonl");
    runHook({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: { command: "git status --short" },
      tool_response: { exit_code: 7 },
    }, profile);

    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].event_type, "tool_call_result");
    assert.equal(records[0].category, "task_status");
    assert.equal(records[0].result, "fail");
    assert.equal(records[0].value, "rework");
    assert.deepEqual(records[0].commands, ["git status --short"]);
  } finally {
    cleanup(dir);
  }
});

test("hook_record marks full Python dependency failures as environment blocked", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "hook-env-blocked.jsonl");
    runHook({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: { command: "node tools/ai.mjs validate --full" },
      tool_response: {
        exit_code: 1,
        output: "error: no working Python runner found with required modules: PIL, numpy, scipy\nhint: install full-gate modules into the selected runner: py -3.12 -m pip install -r tools/requirements/ai-pipeline-full.txt",
      },
    }, profile);

    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].result, "fail");
    assert.equal(records[0].value, "necessary_overhead");
    assert.equal(records[0].failure_kind, "environment_blocked");
    assert.match(records[0].blocked_by, /missing full-gate Python modules/);
  } finally {
    cleanup(dir);
  }
});

test("hook_record skips successful read-only plumbing commands", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "hook-plumbing.jsonl");
    runHook({
      hook_event_name: "PreToolUse",
      tool_name: "Bash",
      tool_input: { command: "git -c core.excludesFile= status --short" },
    }, profile);
    runHook({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: { command: "Get-Content tools\\ai_profile\\hook_record.mjs" },
      tool_response: { exit_code: 0 },
    }, profile);

    assert.throws(() => readJsonl(profile), /ENOENT/);
  } finally {
    cleanup(dir);
  }
});

test("hook_record keeps failed read-only plumbing commands", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "hook-plumbing-fail.jsonl");
    runHook({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: { command: "git status --short" },
      tool_response: { exit_code: 1 },
    }, profile);

    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].event_type, "tool_call_result");
    assert.equal(records[0].result, "fail");
    assert.deepEqual(records[0].commands, ["git status --short"]);
  } finally {
    cleanup(dir);
  }
});

test("hook_record_fast records work and skips successful plumbing when built", {
  skip: !existsSync(join(root, "tools", "ai_profile", process.platform === "win32" ? "hook_record_fast.exe" : "hook_record_fast")),
}, () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "fast-hook.jsonl");
    runFastHook({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: { command: "git status --short" },
      tool_response: { exit_code: 0 },
    }, profile);
    assert.equal(existsSync(profile), false);

    runFastHook({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: { command: "node --test tools/ai_profile/test.mjs" },
      tool_response: { exit_code: 0 },
    }, profile);

    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].event_type, "tool_call_result");
    assert.equal(records[0].category, "validation");
    assert.equal(records[0].result, "pass");
    assert.deepEqual(records[0].commands, ["node --test tools/ai_profile/test.mjs"]);
  } finally {
    cleanup(dir);
  }
});

test("hook_record_fast keeps parallel JSONL appends valid", {
  skip: !existsSync(join(root, "tools", "ai_profile", process.platform === "win32" ? "hook_record_fast.exe" : "hook_record_fast")),
}, async () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "fast-hook-parallel.jsonl");
    const events = Array.from({ length: 32 }, (_, index) => ({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: {
        command: `node tools/pipeline_validate.mjs --parallel-case ${index} ${"x".repeat(180)}`,
      },
      tool_response: { exit_code: 0 },
    }));

    await Promise.all(events.map((payload) => runFastHookAsync(payload, profile)));

    const records = readJsonl(profile);
    assert.equal(records.length, events.length);
    assert.equal(new Set(records.map((record) => record.commands[0])).size, events.length);
  } finally {
    cleanup(dir);
  }
});

test("hook_record recovers missed Codex failed shell commands from session transcript", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "hook-recovered.jsonl");
    const session = join(dir, "codex-session.jsonl");
    writeFileSync(session, [
      {
        timestamp: "2026-06-16T03:24:19.271Z",
        type: "response_item",
        payload: {
          type: "function_call",
          name: "shell_command",
          call_id: "call_failed_probe",
          arguments: JSON.stringify({
            command: "Write-Error 'HOOK_FAIL'; exit 9",
            workdir: root,
          }),
        },
      },
      {
        timestamp: "2026-06-16T03:24:20.318Z",
        type: "response_item",
        payload: {
          type: "function_call_output",
          call_id: "call_failed_probe",
          output: "Exit code: 9\nWall time: 0.7 seconds\nOutput:\nHOOK_FAIL\n",
        },
      },
    ].map((record) => JSON.stringify(record)).join("\n") + "\n", "utf8");

    runHook({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: { command: "node --test tools/ai_profile/test.mjs" },
      tool_response: { exit_code: 0 },
    }, profile, "codex", { CODEX_SESSION_FILE: session });
    runHook({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: { command: "node --test tools/ai_profile/test.mjs" },
      tool_response: { exit_code: 0 },
    }, profile, "codex", { CODEX_SESSION_FILE: session });

    const recovered = readJsonl(profile).filter((record) => record.event_type === "tool_call_result_recovered");
    assert.equal(recovered.length, 1);
    assert.equal(recovered[0].result, "fail");
    assert.equal(recovered[0].value, "rework");
    assert.equal(recovered[0].source_call_id, "call_failed_probe");
    assert.equal(recovered[0].exit_code, 9);
    assert.deepEqual(recovered[0].commands, ["Write-Error 'HOOK_FAIL'; exit 9"]);
  } finally {
    cleanup(dir);
  }
});

test("hook_record recover-only imports Codex failed shell commands", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "hook-recover-only.jsonl");
    const session = join(dir, "codex-session.jsonl");
    writeFileSync(session, [
      JSON.stringify({
        type: "response_item",
        payload: {
          type: "function_call",
          call_id: "call_recover_only",
          arguments: JSON.stringify({ command: "node --test tools/ai_profile/test.mjs" }),
        },
      }),
      JSON.stringify({
        type: "response_item",
        payload: {
          type: "function_call_output",
          call_id: "call_recover_only",
          output: "Exit code: 1\n",
        },
      }),
    ].join("\n") + "\n", "utf8");

    runHook({}, profile, "codex", {
      AI_PROFILE_RECOVER_ONLY: "1",
      CODEX_SESSION_FILE: session,
    });

    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].event_type, "tool_call_result_recovered");
    assert.equal(records[0].source_call_id, "call_recover_only");
    assert.deepEqual(records[0].commands, ["node --test tools/ai_profile/test.mjs"]);
  } finally {
    cleanup(dir);
  }
});

test("hook_record recover-only ignores search no-match transcript exits", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "hook-recover-search.jsonl");
    const session = join(dir, "codex-session.jsonl");
    writeFileSync(session, [
      JSON.stringify({
        type: "response_item",
        payload: {
          type: "function_call",
          call_id: "call_rg_nomatch",
          arguments: JSON.stringify({ command: "rg definitely-not-present" }),
        },
      }),
      JSON.stringify({
        type: "response_item",
        payload: {
          type: "function_call_output",
          call_id: "call_rg_nomatch",
          output: "Exit code: 1\nWall time: 0.1 seconds\nOutput:\n",
        },
      }),
      JSON.stringify({
        type: "response_item",
        payload: {
          type: "function_call",
          call_id: "call_real_fail",
          arguments: JSON.stringify({ command: "node --test tools/missing.test.mjs" }),
        },
      }),
      JSON.stringify({
        type: "response_item",
        payload: {
          type: "function_call_output",
          call_id: "call_real_fail",
          output: "Exit code: 1\nWall time: 0.2 seconds\nOutput:\nnot ok\n",
        },
      }),
    ].join("\n") + "\n", "utf8");

    runHook({}, profile, "codex", {
      AI_PROFILE_RECOVER_ONLY: "1",
      CODEX_SESSION_FILE: session,
    });

    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].source_call_id, "call_real_fail");
    assert.equal(records[0].result, "fail");
    assert.deepEqual(records[0].commands, ["node --test tools/missing.test.mjs"]);
  } finally {
    cleanup(dir);
  }
});

test("orchestration trace passes spawn wait close transcript", () => {
  const dir = tempDir();
  try {
    const session = join(dir, "codex-session.jsonl");
    const trace = join(dir, "trace.json");
    writeJsonl(session, [
      multiAgentCall("call_spawn", "multi_agent_v1.spawn_agent", { agent_type: "explorer" }),
      multiAgentOutput("call_spawn", { agent_id: "agent-1", nickname: "Ada" }),
      multiAgentCall("call_wait", "multi_agent_v1.wait_agent", { targets: ["agent-1"] }),
      multiAgentOutput("call_wait", { status: { "agent-1": { completed: "done" } } }),
      multiAgentCall("call_close", "multi_agent_v1.close_agent", { target: "agent-1" }),
      multiAgentOutput("call_close", { previous_status: { completed: "done" } }),
    ]);

    const result = run(["tools/ai_profile/orchestration_trace.mjs", "--session", session, "--json-output", trace, "--json"]);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, true);
    assert.equal(parsed.calls.length, 3);
    assert.deepEqual(parsed.problems, []);
    assert.equal(readJson(trace).ok, true);
  } finally {
    cleanup(dir);
  }
});

test("orchestration trace fails missing wait and close", () => {
  const dir = tempDir();
  try {
    const session = join(dir, "codex-session.jsonl");
    writeJsonl(session, [
      multiAgentCall("call_spawn", "multi_agent_v1.spawn_agent", { agent_type: "explorer" }),
      multiAgentOutput("call_spawn", { agent_id: "agent-1" }),
    ]);

    const result = spawnSync(process.execPath, ["tools/ai_profile/orchestration_trace.mjs", "--session", session, "--json"], {
      cwd: root,
      encoding: "utf8",
      stdio: "pipe",
    });
    assert.notEqual(result.status, 0);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, false);
    assert.deepEqual(parsed.missing.map((item) => item.operation), ["wait", "close"]);
  } finally {
    cleanup(dir);
  }
});

test("orchestration trace fails close before wait", () => {
  const dir = tempDir();
  try {
    const session = join(dir, "codex-session.jsonl");
    writeJsonl(session, [
      multiAgentCall("call_spawn", "spawn_agent", {}),
      multiAgentOutput("call_spawn", { agent_id: "agent-1" }),
      multiAgentCall("call_close", "close_agent", { target: "agent-1" }),
      multiAgentOutput("call_close", { previous_status: { completed: "done" } }),
      multiAgentCall("call_wait", "wait_agent", { targets: ["agent-1"] }),
      multiAgentOutput("call_wait", { status: { "agent-1": { completed: "done" } } }),
    ]);

    const result = spawnSync(process.execPath, ["tools/ai_profile/orchestration_trace.mjs", "--session", session, "--json"], {
      cwd: root,
      encoding: "utf8",
      stdio: "pipe",
    });
    assert.notEqual(result.status, 0);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, false);
    assert.equal(parsed.unordered[0].operation, "close_before_wait");
  } finally {
    cleanup(dir);
  }
});

test("orchestration trace ignores unrelated transcript calls", () => {
  const dir = tempDir();
  try {
    const session = join(dir, "codex-session.jsonl");
    writeJsonl(session, [
      multiAgentCall("call_shell", "shell_command", { command: "git status --short" }),
      multiAgentOutput("call_shell", { output: "clean" }),
    ]);

    const result = spawnSync(process.execPath, ["tools/ai_profile/orchestration_trace.mjs", "--session", session, "--json"], {
      cwd: root,
      encoding: "utf8",
      stdio: "pipe",
    });
    assert.notEqual(result.status, 0);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, false);
    assert.deepEqual(parsed.calls, []);
    assert.ok(parsed.problems.includes("no multi-agent orchestration calls found in session"));
  } finally {
    cleanup(dir);
  }
});

test("orchestration trace fails without an evidence source", () => {
  const result = spawnSync(process.execPath, ["tools/ai_profile/orchestration_trace.mjs", "--json"], {
    cwd: root,
    encoding: "utf8",
    stdio: "pipe",
  });
  assert.notEqual(result.status, 0);
  const parsed = JSON.parse(result.stdout);
  assert.equal(parsed.ok, false);
  assert.ok(parsed.problems.includes("missing evidence source: pass --session or --parent-thread-id"));
});

test("orchestration trace fails timed out or incomplete agent outputs", () => {
  const dir = tempDir();
  try {
    const session = join(dir, "codex-session.jsonl");
    writeJsonl(session, [
      multiAgentCall("call_spawn", "spawn_agent", {}),
      multiAgentOutput("call_spawn", { agent_id: "agent-1" }),
      multiAgentCall("call_wait", "wait_agent", { targets: ["agent-1"] }),
      multiAgentOutput("call_wait", { timed_out: true, status: { "agent-1": {} } }),
      multiAgentCall("call_close", "close_agent", { target: "agent-1" }),
      multiAgentOutput("call_close", { previous_status: { running: true } }),
    ]);

    const result = spawnSync(process.execPath, ["tools/ai_profile/orchestration_trace.mjs", "--session", session, "--json"], {
      cwd: root,
      encoding: "utf8",
      stdio: "pipe",
    });
    assert.notEqual(result.status, 0);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, false);
    assert.deepEqual(parsed.incomplete.map((item) => item.operation), ["wait_timeout", "close_incomplete"]);
  } finally {
    cleanup(dir);
  }
});

test("orchestration trace accepts targetless wait when output status names the agent", () => {
  const dir = tempDir();
  try {
    const session = join(dir, "codex-session.jsonl");
    writeJsonl(session, [
      multiAgentCall("call_spawn", "spawn_agent", {}),
      multiAgentOutput("call_spawn", { agent_id: "agent-1" }),
      multiAgentCall("call_wait", "wait_agent", {}),
      multiAgentOutput("call_wait", { status: { "agent-1": { completed: "done" } } }),
      multiAgentCall("call_close", "close_agent", { target: "agent-1" }),
      multiAgentOutput("call_close", { previous_status: { completed: "done" } }),
    ]);

    const result = run(["tools/ai_profile/orchestration_trace.mjs", "--session", session, "--json"]);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, true);
    assert.equal(parsed.calls.find((call) => call.operation === "wait").targets[0], "agent-1");
  } finally {
    cleanup(dir);
  }
});

test("orchestration trace counts subagent sessions by parent thread", () => {
  const dir = tempDir();
  try {
    const parent = "parent-thread-1";
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta("subagent-a", parent)]);
    writeJsonl(join(dir, "rollout-b.jsonl"), [subagentSessionMeta("subagent-b", parent)]);
    writeJsonl(join(dir, "rollout-other.jsonl"), [subagentSessionMeta("subagent-other", "other-parent")]);

    const result = run([
      "tools/ai_profile/orchestration_trace.mjs",
      "--session-root", dir,
      "--parent-thread-id", parent,
      "--min-agents", "2",
      "--cwd", root,
      "--json",
    ]);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, true);
    assert.equal(parsed.subagentSessionCount, 2);
    assert.deepEqual(parsed.subagentSessions.map((agent) => agent.id), ["subagent-a", "subagent-b"]);
  } finally {
    cleanup(dir);
  }
});

test("status reads a session log and reports records, slowest, and rollup", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    // A repeated command, paired start/result so a duration is derived. The
    // first run is slow (3s); the start records are dropped after pairing.
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "unknown", value: "necessary_overhead", event_type: "tool_call_start", commands: ["node --test tools/x.test.mjs"], session_id: "s1" },
      { ts: "2026-06-13T10:00:03+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/x.test.mjs"], session_id: "s1" },
      { ts: "2026-06-13T10:00:05+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "unknown", value: "necessary_overhead", event_type: "tool_call_start", commands: ["node --test tools/x.test.mjs"], session_id: "s1" },
      { ts: "2026-06-13T10:00:06+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/x.test.mjs"], session_id: "s1" },
    ]);

    const result = run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson]);
    const status = readJson(statusJson);
    assert.equal(status.schema_version, 2);
    assert.equal(status.records, 2); // start records dropped after pairing
    assert.ok(status.slowest_record);
    assert.equal(status.slowest_record.duration_ms, 3000);
    assert.equal(status.command_rollup.by_count[0].key, "node x.test.mjs");
    assert.equal(status.command_rollup.by_count[0].count, 2);
    assert.match(result.stdout, /Records: 2/);
    assert.match(result.stdout, /Most-Run Commands/);
  } finally {
    cleanup(dir);
  }
});

test("status command rollup strips shell assignment wrappers", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    writeJsonl(profile, [
      {
        ts: "2026-06-13T10:00:00+05:00",
        phase: "session",
        category: "validation",
        intent: "auto:Bash",
        result: "pass",
        value: "unknown",
        event_type: "tool_call_result",
        commands: ["$env:AI_PIPELINE_PYTHON='C:\\Users\\ROG\\.cache\\codex-runtimes\\python\\python.exe'; node tools/ai.mjs validate --full"],
        session_id: "s1",
      },
      {
        ts: "2026-06-13T10:00:01+05:00",
        phase: "session",
        category: "validation",
        intent: "auto:Bash",
        result: "pass",
        value: "unknown",
        event_type: "tool_call_result",
        commands: ["AI_PIPELINE_PYTHON=/tmp/python node tools/ai.mjs validate --review"],
        session_id: "s1",
      },
      {
        ts: "2026-06-13T10:00:02+05:00",
        phase: "session",
        category: "tooling",
        intent: "auto:Bash",
        result: "pass",
        value: "unknown",
        event_type: "tool_call_result",
        commands: ["$i=0; Get-Content tools/ai_profile/status.mjs | ForEach-Object { $i++; $_ }"],
        session_id: "s1",
      },
      {
        ts: "2026-06-13T10:00:03+05:00",
        phase: "session",
        category: "tooling",
        intent: "auto:Bash",
        result: "pass",
        value: "unknown",
        event_type: "tool_call_result",
        commands: ["Get-Content tools/ai_profile/test.mjs"],
        session_id: "s1",
      },
    ]);

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson]);
    const status = readJson(statusJson);
    const keys = status.command_rollup.by_count.map((entry) => entry.key);
    assert.deepEqual(keys, ["node ai.mjs", "Get-Content"]);
    assert.equal(status.command_rollup.by_count.find((entry) => entry.key === "Get-Content")?.count, 2);
    assert.ok(!keys.includes("$i=0;"));
  } finally {
    cleanup(dir);
  }
});

test("status reports agent rollup when requested", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const agentProfileDir = join(dir, "profiles");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agentA = "11111111-1111-4111-8111-111111111111";
    const agentB = "22222222-2222-4222-8222-222222222222";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta(agentA, parent, root, "2026-06-21T10:00:00.000Z")]);
    writeJsonl(join(dir, "rollout-b.jsonl"), [subagentSessionMeta(agentB, parent, root, "2026-06-21T10:01:00.000Z")]);
    mkdirSync(agentProfileDir, { recursive: true });
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__11111111.jsonl"), [
      { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "unknown", value: "necessary_overhead", event_type: "tool_call_start", commands: ["node --test tools/agent.test.mjs"], session_id: agentA },
      { ts: "2026-06-21T10:02:02+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/agent.test.mjs"], session_id: agentA },
    ]);
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__22222222.jsonl"), [
      { ts: "2026-06-21T10:03:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "unknown", value: "necessary_overhead", event_type: "tool_call_start", commands: ["node --test tools/agent.test.mjs"], session_id: agentB },
      { ts: "2026-06-21T10:03:01+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/agent.test.mjs"], session_id: agentB, exit_code: 1 },
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-profile-dir", agentProfileDir,
      "--min-agents", "2",
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.enabled, true);
    assert.equal(status.agent_rollup.ok, true);
    assert.equal(status.agent_rollup.strict_ok, false);
    assert.deepEqual(status.agent_rollup.strict_problems, ["unresolved agent failures: 1"]);
    assert.equal(status.agent_rollup.source, "parent-thread");
    assert.equal(status.agent_rollup.subagent_session_count, 2);
    assert.deepEqual(status.agent_rollup.roles, [{ role: "test verifier", count: 2 }]);
    assert.equal(status.agent_rollup.profile_rollup.profiled_agent_count, 2);
    assert.equal(status.agent_rollup.profile_rollup.telemetry_agent_count, 2);
    assert.equal(status.agent_rollup.profile_rollup.transcript_agent_count, 0);
    assert.equal(status.agent_rollup.profile_rollup.records, 2);
    assert.equal(status.agent_rollup.profile_rollup.recorded_ms, 3000);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 1);
    assert.deepEqual(status.agent_rollup.profile_rollup.unresolved_failure_samples.map((sample) => ({
      agent_id: sample.agent_id,
      role: sample.role,
      source: sample.source,
      command_key: sample.command_key,
      command: sample.command,
      exit_code: sample.exit_code,
      line: sample.line,
    })), [{
      agent_id: agentB,
      role: "test verifier",
      source: "profile",
      command_key: "node agent.test.mjs",
      command: "node --test tools/agent.test.mjs",
      exit_code: 1,
      line: 2,
    }]);
    assert.equal(status.agent_rollup.profile_rollup.command_rollup.by_time[0].key, "node agent.test.mjs");
    assert.match(result.stdout, /## Agent Rollup/);
    assert.match(result.stdout, /subagent sessions: 2/);
    assert.match(result.stdout, /## Agent Profile Rollup/);
    assert.match(result.stdout, /telemetry agents: 2\/2/);
    assert.match(result.stdout, /sources: profiles=2, transcripts=0/);
    assert.match(result.stdout, /unresolved: agent-22222222-2222-4222-8222-222222222222 \[test verifier\] profile:2 node agent\.test\.mjs exit 1 - node --test tools\/agent\.test\.mjs/);
    assert.match(result.stdout, /strict problem: unresolved agent failures: 1/);
    assert.match(result.stdout, /node agent\.test\.mjs: 3\.0s total over 2 run\(s\)/);
    assert.match(status.next_action, /unresolved agent failure samples/);
    assert.match(result.stdout, /Inspect unresolved agent failure samples/);

    const strict = spawnSync(process.execPath, [
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-profile-dir", agentProfileDir,
      "--min-agents", "2",
    ], {
      cwd: root,
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    });
    assert.equal(strict.status, 1);
    assert.match(strict.stdout, /strict problem: unresolved agent failures: 1/);
  } finally {
    cleanup(dir);
  }
});

test("status writes compact agent rollup evidence artifact", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const agentProfileDir = join(dir, "profiles");
    const evidenceJson = join(dir, "status-evidence.json");
    const parent = "parent-thread-1";
    const agentA = "33333333-3333-4333-8333-333333333333";
    const agentB = "44444444-4444-4444-8444-444444444444";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta(agentA, parent, root, "2026-06-21T10:00:00.000Z")]);
    writeJsonl(join(dir, "rollout-b.jsonl"), [subagentSessionMeta(agentB, parent, root, "2026-06-21T10:01:00.000Z")]);
    mkdirSync(agentProfileDir, { recursive: true });
    for (const agent of [agentA, agentB]) {
      writeJsonl(join(agentProfileDir, `2026-06-21__codex__${agent.slice(0, 8)}.jsonl`), [
        { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "unknown", value: "necessary_overhead", event_type: "tool_call_start", commands: ["node --test tools/agent.test.mjs"], session_id: agent },
        { ts: "2026-06-21T10:02:01+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/agent.test.mjs"], session_id: agent },
      ]);
    }

    run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-profile-dir", agentProfileDir,
      "--min-agents", "2",
      "--agent-rollup-evidence",
      "--json-output", evidenceJson,
    ]);
    const evidence = readJson(evidenceJson);
    assert.equal(evidence.kind, "status-agent-rollup-evidence");
    assert.equal(evidence.schema_version, 2);
    assert.match(evidence.generated_at, /^\d{4}-\d{2}-\d{2}T/);
    assert.equal(evidence.valid, true);
    assert.deepEqual(evidence.errors, []);
    assert.equal(evidence.agent_rollup.ok, true);
    assert.equal(evidence.agent_rollup.strict_ok, true);
    assert.equal(evidence.agent_rollup.parent_thread_id, parent);
    assert.equal(evidence.agent_rollup.min_agents, 2);
    assert.equal(evidence.agent_rollup.subagent_session_count, 2);
    assert.equal(evidence.agent_rollup.profile_rollup.missing_agent_telemetry_count, 0);
    assert.equal(evidence.agent_rollup.profile_rollup.unresolved_failed_records, 0);
    assert.equal(Object.hasOwn(evidence, "profile"), false);
    assert.equal(Object.hasOwn(evidence, "profile_files"), false);
    assert.equal(Object.hasOwn(evidence, "command_rollup"), false);
    assert.equal(Object.hasOwn(evidence, "slowest_record"), false);
    assert.equal(Object.hasOwn(evidence.agent_rollup, "agents"), false);
  } finally {
    cleanup(dir);
  }
});

test("orchestration evidence dry-run uses current task command and writes nothing", () => {
  const dir = tempDir();
  try {
    const sessionRoot = join(dir, "sessions");
    const artifact = join(dir, "tasks", "evidence", "T0001-status-rollup.json");
    const evidenceCommand = `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id parent-thread-1 --session-root "${sessionRoot}" --agent-cwd "${dir}" --agent-rollup-evidence --json-output tasks/evidence/T0001-status-rollup.json`;
    writeTaskboardTask(dir, { id: "T0001", evidenceCommand });

    const result = spawnSync(process.execPath, ["tools/ai_profile/orchestration_evidence.mjs", "--current", "--json"], {
      cwd: root,
      env: { ...process.env, TASKBOARD_ROOT: dir, CODEX_SESSION_FILE: "" },
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    });

    assert.equal(result.status, 0, result.stderr || result.stdout);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, true);
    assert.equal(parsed.mode, "dry-run");
    assert.equal(parsed.task_id, "T0001");
    assert.equal(parsed.inference_source, "task-command");
    assert.equal(parsed.artifact.replaceAll("\\", "/"), "tasks/evidence/T0001-status-rollup.json");
    assert.equal(parsed.artifact_source, "task-command");
    assert.match(parsed.command, /node tools\/ai\.mjs status --agent-rollup --require-agent-rollup-ok/);
    assert.match(parsed.command, /--agent-rollup-evidence/);
    assert.match(parsed.command, /--json-output/);
    assert.equal(existsSync(artifact), false);
  } finally {
    cleanup(dir);
  }
});

test("orchestration evidence run writes compact status artifact", () => {
  const dir = tempDir();
  try {
    const sessionRoot = join(dir, "sessions");
    const agentProfileDir = join(dir, "agent-profiles");
    const parentProfile = join(dir, "parent-profile.jsonl");
    const parent = "parent-thread-1";
    const agentA = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    const agentB = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
    mkdirSync(sessionRoot, { recursive: true });
    mkdirSync(agentProfileDir, { recursive: true });
    writeJsonl(parentProfile, [
      { ts: "2026-06-21T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: parent },
    ]);
    writeJsonl(join(sessionRoot, "rollout-a.jsonl"), [subagentSessionMeta(agentA, parent, dir, "2026-06-21T10:00:00.000Z")]);
    writeJsonl(join(sessionRoot, "rollout-b.jsonl"), [subagentSessionMeta(agentB, parent, dir, "2026-06-21T10:01:00.000Z")]);
    for (const agent of [agentA, agentB]) {
      writeJsonl(join(agentProfileDir, `2026-06-21__codex__${agent.slice(0, 8)}.jsonl`), [
        { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "unknown", value: "necessary_overhead", event_type: "tool_call_start", commands: ["node --test tools/agent.test.mjs"], session_id: agent },
        { ts: "2026-06-21T10:02:01+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/agent.test.mjs"], session_id: agent },
      ]);
    }
    const evidenceCommand = `node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id ${parent} --session-root "${sessionRoot}" --agent-cwd "${dir}" --profile "${parentProfile}" --agent-rollup-evidence --json-output tasks/evidence/T0001-status-rollup.json`;
    writeTaskboardTask(dir, { id: "T0001", evidenceCommand });

    const result = spawnSync(process.execPath, [
      "tools/ai_profile/orchestration_evidence.mjs",
      "--current",
      "--run",
      "--json",
      "--agent-profile-dir", agentProfileDir,
    ], {
      cwd: root,
      env: { ...process.env, TASKBOARD_ROOT: dir, CODEX_SESSION_FILE: "" },
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    });

    assert.equal(result.status, 0, result.stderr || result.stdout);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, true);
    assert.equal(parsed.status_ok, true);
    assert.equal(parsed.mode, "run");
    assert.equal(parsed.stdout.includes("strict problem:"), false);
    const evidence = readJson(join(dir, "tasks", "evidence", "T0001-status-rollup.json"));
    assert.equal(evidence.kind, "status-agent-rollup-evidence");
    assert.equal(evidence.valid, true);
    assert.equal(evidence.agent_rollup.strict_ok, true);
    assert.equal(evidence.agent_rollup.subagent_session_count, 2);
    assert.equal(evidence.agent_rollup.profile_rollup.unresolved_failed_records, 0);
  } finally {
    cleanup(dir);
  }
});

test("orchestration evidence fails closed when source cannot be inferred", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "plain-profile.jsonl");
    writeTaskboardTask(dir, { id: "T0001" });
    writeJsonl(profile, [
      { ts: "2026-06-21T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "parent" },
    ]);

    const result = spawnSync(process.execPath, [
      "tools/ai_profile/orchestration_evidence.mjs",
      "--current",
      "--profile", profile,
      "--json",
    ], {
      cwd: root,
      env: { ...process.env, TASKBOARD_ROOT: dir, CODEX_SESSION_FILE: "" },
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    });

    assert.equal(result.status, 1);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, false);
    assert.equal(parsed.task_id, "T0001");
    assert.match(parsed.problem, /could not infer parent thread id/);
    assert.match(parsed.next_action, /--parent-thread-id/);
  } finally {
    cleanup(dir);
  }
});

test("orchestration evidence fails closed when explicit task is missing", () => {
  const dir = tempDir();
  try {
    const result = spawnSync(process.execPath, [
      "tools/ai_profile/orchestration_evidence.mjs",
      "--task", "T9999",
      "--parent-thread-id", "parent-thread-1",
      "--session-root", dir,
      "--json",
    ], {
      cwd: root,
      env: { ...process.env, TASKBOARD_ROOT: dir, CODEX_SESSION_FILE: "" },
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    });

    assert.equal(result.status, 1);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, false);
    assert.match(parsed.problem, /task not found: T9999/);
    assert.doesNotMatch(parsed.stdout || "", /T9999-status-rollup/);
  } finally {
    cleanup(dir);
  }
});

test("status agent rollup classifies parent-recovered failures by later parent pass", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const agentProfileDir = join(dir, "profiles");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const recoveredAgent = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    const unresolvedAgent = "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb";
    const environmentAgent = "cccccccc-cccc-4ccc-8ccc-cccccccccccc";
    writeJsonl(profile, [
      { ts: "2026-06-21T10:01:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/early.test.mjs"], session_id: "parent" },
      { ts: "2026-06-21T10:05:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/recovered.test.mjs"], session_id: "parent" },
      { ts: "2026-06-21T10:06:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node tools/ai.mjs validate --full"], session_id: "parent" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta(recoveredAgent, parent, root, "2026-06-21T10:00:00.000Z")]);
    writeJsonl(join(dir, "rollout-b.jsonl"), [subagentSessionMeta(unresolvedAgent, parent, root, "2026-06-21T10:00:01.000Z")]);
    writeJsonl(join(dir, "rollout-c.jsonl"), [subagentSessionMeta(environmentAgent, parent, root, "2026-06-21T10:00:02.000Z")]);
    mkdirSync(agentProfileDir, { recursive: true });
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__aaaaaaaa.jsonl"), [
      { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/recovered.test.mjs"], session_id: recoveredAgent, exit_code: 1 },
    ]);
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__bbbbbbbb.jsonl"), [
      { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/early.test.mjs"], session_id: unresolvedAgent, exit_code: 1 },
    ]);
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__cccccccc.jsonl"), [
      {
        ts: "2026-06-21T10:02:00+05:00",
        phase: "session",
        category: "validation",
        intent: "auto:Bash",
        result: "fail",
        value: "necessary_overhead",
        event_type: "tool_call_result",
        commands: ["node tools/ai.mjs validate --full"],
        session_id: environmentAgent,
        exit_code: 1,
        failure_kind: "environment_blocked",
        blocked_by: "missing full-gate Python modules",
      },
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-profile-dir", agentProfileDir,
      "--min-agents", "3",
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    const profileRollup = status.agent_rollup.profile_rollup;
    assert.equal(profileRollup.parent_recovered_failed_records, 1);
    assert.equal(profileRollup.total_recovered_failed_records, 1);
    assert.equal(profileRollup.unresolved_failed_records, 1);
    assert.equal(profileRollup.environment_blocked_failed_records, 1);
    assert.deepEqual(profileRollup.unresolved_failure_samples.map((sample) => ({
      agent_id: sample.agent_id,
      command_key: sample.command_key,
      command: sample.command,
    })), [{
      agent_id: unresolvedAgent,
      command_key: "node early.test.mjs",
      command: "node --test tools/early.test.mjs",
    }]);
    assert.match(result.stdout, /parent-recovered agent failures: 1/);
    assert.match(result.stdout, /unresolved agent failures: 1/);
    assert.match(result.stdout, /node early\.test\.mjs exit 1/);
    assert.doesNotMatch(result.stdout, /node recovered\.test\.mjs exit 1/);
  } finally {
    cleanup(dir);
  }
});

test("status agent rollup recovers single-file node tests from later parent supersets", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const agentProfileDir = join(dir, "profiles");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const recoveredAgent = "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
    const earlyAgent = "ffffffff-ffff-4fff-8fff-ffffffffffff";
    const multiAgent = "abababab-abab-4bab-8bab-abababababab";
    const filteredAgent = "cdcdcdcd-cdcd-4dcd-8dcd-cdcdcdcdcdcd";
    writeJsonl(profile, [
      { ts: "2026-06-21T10:01:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/early.test.mjs tools/other.test.mjs"], session_id: "parent" },
      { ts: "2026-06-21T10:04:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test --test-name-pattern smoke tools/filtered.test.mjs"], session_id: "parent" },
      { ts: "2026-06-21T10:05:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/recovered.test.mjs tools/ai_profile/test.mjs"], session_id: "parent" },
      { ts: "2026-06-21T10:06:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/multi-a.test.mjs"], session_id: "parent" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta(recoveredAgent, parent, root, "2026-06-21T10:00:00.000Z")]);
    writeJsonl(join(dir, "rollout-b.jsonl"), [subagentSessionMeta(earlyAgent, parent, root, "2026-06-21T10:00:01.000Z")]);
    writeJsonl(join(dir, "rollout-c.jsonl"), [subagentSessionMeta(multiAgent, parent, root, "2026-06-21T10:00:02.000Z")]);
    writeJsonl(join(dir, "rollout-d.jsonl"), [subagentSessionMeta(filteredAgent, parent, root, "2026-06-21T10:00:03.000Z")]);
    mkdirSync(agentProfileDir, { recursive: true });
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__eeeeeeee.jsonl"), [
      { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/recovered.test.mjs"], session_id: recoveredAgent, exit_code: 1 },
    ]);
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__ffffffff.jsonl"), [
      { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/early.test.mjs"], session_id: earlyAgent, exit_code: 1 },
    ]);
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__abababab.jsonl"), [
      { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/multi-a.test.mjs tools/multi-b.test.mjs"], session_id: multiAgent, exit_code: 1 },
    ]);
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__cdcdcdcd.jsonl"), [
      { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/filtered.test.mjs"], session_id: filteredAgent, exit_code: 1 },
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-profile-dir", agentProfileDir,
      "--min-agents", "4",
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    const profileRollup = status.agent_rollup.profile_rollup;
    assert.equal(profileRollup.parent_recovered_failed_records, 1);
    assert.equal(profileRollup.parent_node_test_file_recovered_failed_records, 1);
    assert.equal(profileRollup.parent_exact_recovered_failed_records, 0);
    assert.equal(profileRollup.unresolved_failed_records, 3);
    assert.deepEqual(profileRollup.unresolved_failure_samples.map((sample) => sample.command), [
      "node --test tools/early.test.mjs",
      "node --test tools/multi-a.test.mjs tools/multi-b.test.mjs",
      "node --test tools/filtered.test.mjs",
    ]);
    assert.match(result.stdout, /parent-recovered agent failures: 1/);
    assert.match(result.stdout, /unresolved agent failures: 3/);
    assert.doesNotMatch(result.stdout, /node recovered\.test\.mjs exit 1/);
  } finally {
    cleanup(dir);
  }
});

test("status next action stays generic for clean agent rollup", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const agentProfileDir = join(dir, "profiles");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agent = "66666666-6666-4666-8666-666666666666";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta(agent, parent, root, "2026-06-21T10:00:00.000Z")]);
    mkdirSync(agentProfileDir, { recursive: true });
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__66666666.jsonl"), [
      { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/agent.test.mjs"], session_id: agent },
    ]);

    run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-profile-dir", agentProfileDir,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 0);
    assert.match(status.next_action, /No profiling action needed/);
  } finally {
    cleanup(dir);
  }
});

test("status reports hidden unresolved agent failure sample count", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const agentProfileDir = join(dir, "profiles");
    const statusJson = join(dir, "status.json");
    const verboseStatusJson = join(dir, "status-verbose.json");
    const parent = "parent-thread-1";
    const agent = "88888888-8888-4888-8888-888888888888";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta(agent, parent, root, "2026-06-21T10:00:00.000Z")]);
    mkdirSync(agentProfileDir, { recursive: true });
    const records = [];
    for (let index = 1; index <= 12; index += 1) {
      records.push({
        ts: `2026-06-21T10:${String(index).padStart(2, "0")}:00+05:00`,
        phase: "session",
        category: "validation",
        intent: "auto:Bash",
        result: "fail",
        value: "rework",
        event_type: "tool_call_result",
        commands: [`node --test tools/agent-${index}.test.mjs`],
        session_id: agent,
        exit_code: 1,
      });
    }
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__88888888.jsonl"), records);

    const args = [
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-profile-dir", agentProfileDir,
    ];
    const normal = run([...args, "--json-output", statusJson]);
    const verbose = run([...args, "--verbose", "--json-output", verboseStatusJson]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 12);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failure_samples.length, 10);
    assert.equal((normal.stdout.match(/^- unresolved:/gm) || []).length, 3);
    assert.match(normal.stdout, /\.\.\. 9 more unresolved agent failure\(s\) not shown/);
    assert.equal((verbose.stdout.match(/^- unresolved:/gm) || []).length, 10);
    assert.match(verbose.stdout, /\.\.\. 2 more unresolved agent failure\(s\) not shown/);
  } finally {
    cleanup(dir);
  }
});

test("status falls back to subagent transcripts when profile logs are absent", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agentA = "33333333-3333-4333-8333-333333333333";
    const agentB = "44444444-4444-4444-8444-444444444444";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [
      subagentSessionMeta(agentA, parent, root, "2026-06-21T10:00:00.000Z"),
      shellCall("call_a", "node --test tools/same.test.mjs", "2026-06-21T10:00:01.000Z"),
      shellOutput("call_a", "Exit code: 1\nWall time: 2.4 seconds\nOutput:\nfail\n", "2026-06-21T10:00:04.000Z"),
    ]);
    writeJsonl(join(dir, "rollout-b.jsonl"), [
      subagentSessionMeta(agentB, parent, root, "2026-06-21T10:01:00.000Z"),
      shellCall("call_b", "node --test tools/same.test.mjs", "2026-06-21T10:01:01.000Z"),
      shellOutput("call_b", "Exit code: 0\nWall time: 1.1 seconds\nOutput:\nok\n", "2026-06-21T10:01:03.000Z"),
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--min-agents", "2",
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.ok, true);
    assert.equal(status.agent_rollup.profile_rollup.telemetry_agent_count, 2);
    assert.equal(status.agent_rollup.profile_rollup.profile_agent_count, 0);
    assert.equal(status.agent_rollup.profile_rollup.transcript_agent_count, 2);
    assert.equal(status.agent_rollup.profile_rollup.recorded_ms, 3500);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 1);
    assert.deepEqual(status.agent_rollup.profile_rollup.unresolved_failure_samples.map((sample) => ({
      agent_id: sample.agent_id,
      nickname: sample.nickname,
      role: sample.role,
      command_key: sample.command_key,
      command: sample.command,
      exit_code: sample.exit_code,
      source: sample.source,
      line: sample.line,
    })), [{
      agent_id: agentA,
      nickname: `agent-${agentA}`,
      role: "test verifier",
      command_key: "node same.test.mjs",
      command: "node --test tools/same.test.mjs",
      exit_code: 1,
      source: "transcript",
      line: 3,
    }]);
    assert.equal(status.agent_rollup.profile_rollup.command_rollup.by_time[0].key, "node same.test.mjs");
    assert.match(status.next_action, /unresolved agent failure samples/);
    assert.match(result.stdout, /sources: profiles=0, transcripts=2/);
    assert.match(result.stdout, /unresolved agent failures: 1/);
    assert.match(result.stdout, new RegExp(`unresolved: agent-${agentA} \\[test verifier\\] transcript:3 node same\\.test\\.mjs exit 1 - node --test tools/same\\.test\\.mjs`));
    assert.match(result.stdout, /Inspect unresolved agent failure samples/);
  } finally {
    cleanup(dir);
  }
});

test("status agent rollup separates transcript tool-usage failures from unresolved failures", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agent = "dddddddd-dddd-4ddd-8ddd-dddddddddddd";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [
      subagentSessionMeta(agent, parent, root, "2026-06-21T10:00:00.000Z"),
      shellCall("call_missing_path", "Get-Content C:\\projects\\game-67-idle\\src\\missing_state.c", "2026-06-21T10:00:01.000Z"),
      shellOutput("call_missing_path", "Exit code: 1\nWall time: 0.3 seconds\nOutput:\nGet-Content : Cannot find path 'C:\\projects\\game-67-idle\\src\\missing_state.c' because it does not exist.\nFullyQualifiedErrorId : PathNotFound,Microsoft.PowerShell.Commands.GetContentCommand\nItemNotFoundException\n", "2026-06-21T10:00:02.000Z"),
      shellCall("call_bad_trace", "node tools/ai.mjs orchestration-trace --json", "2026-06-21T10:00:03.000Z"),
      shellOutput("call_bad_trace", "Exit code: 1\nWall time: 0.4 seconds\nOutput:\n{\"ok\":false,\"problems\":[\"missing evidence source: pass --session or --parent-thread-id\"]}\n", "2026-06-21T10:00:04.000Z"),
      shellCall("call_bad_range", "Get-Content docs\\ai-pipeline\\subagent-protocol.md | Select-Object -Index 96..114", "2026-06-21T10:00:05.000Z"),
      shellOutput("call_bad_range", "Exit code: 1\nWall time: 0.4 seconds\nOutput:\nSelect-Object : Cannot bind parameter 'Index'. Cannot convert value \"96..114\" to type \"System.Int32\".\nFullyQualifiedErrorId : CannotConvertArgumentNoMessage,Microsoft.PowerShell.Commands.SelectObjectCommand\n", "2026-06-21T10:00:06.000Z"),
      shellCall("call_test_fail", "node --test tools/real.test.mjs", "2026-06-21T10:00:07.000Z"),
      shellOutput("call_test_fail", "Exit code: 1\nWall time: 0.5 seconds\nOutput:\nnot ok 1 real validation failure\n", "2026-06-21T10:00:08.000Z"),
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    const profileRollup = status.agent_rollup.profile_rollup;
    assert.equal(profileRollup.agent_tool_usage_failed_records, 3);
    assert.equal(profileRollup.agent_tool_usage_failure_samples.length, 3);
    assert.deepEqual(profileRollup.agent_tool_usage_reasons, [
      { reason: "invalid shell command/parameter", count: 1 },
      { reason: "missing orchestration evidence source", count: 1 },
      { reason: "missing local file/path", count: 1 },
    ]);
    assert.deepEqual(profileRollup.agent_tool_usage_prevention_hints.map((item) => item.reason), [
      "missing local file/path",
      "invalid shell command/parameter",
      "missing orchestration evidence source",
    ]);
    const evidenceHint = profileRollup.agent_tool_usage_prevention_hints.find((item) => item.reason === "missing orchestration evidence source");
    assert.ok(evidenceHint);
    assert.match(evidenceHint.hint, /node tools\/ai\.mjs orchestration-evidence --current --run --json/);
    assert.match(evidenceHint.hint, /orchestration-trace --parent-thread-id parent-thread-1/);
    const fallback = evidenceHint.hint.match(/`(node tools\/ai\.mjs orchestration-trace[^`]+)`/)?.[1] || "";
    assert.equal(isMachineEvidenceCommand(fallback), true);
    assert.equal(profileRollup.unresolved_failed_records, 1);
    assert.equal(profileRollup.unresolved_failure_samples[0].command, "node --test tools/real.test.mjs");
    assert.match(result.stdout, /agent tool-usage failures: 3/);
    assert.match(result.stdout, /tool-usage: agent-dddddddd-dddd-4ddd-8ddd-dddddddddddd \[test verifier\] transcript:3 Get-Content \(missing local file\/path\)/);
    assert.match(result.stdout, /prevent missing local file\/path: Verify paths with `rg --files <scope>` or `Test-Path -LiteralPath <path>` before reads\./);
    assert.match(result.stdout, /prevent invalid shell command\/parameter: Avoid unsupported PowerShell shapes/);
    assert.match(result.stdout, /prevent missing orchestration evidence source: Prefer task-scoped evidence with `node tools\/ai\.mjs orchestration-evidence --current --run --json`/);
    assert.match(result.stdout, /for raw trace fallback use an evidence source/);
    assert.match(result.stdout, /orchestration-trace --parent-thread-id parent-thread-1 --session-root .* --cwd .* --json-output tmp\/orchestration-trace\.json --json/);
    assert.match(result.stdout, /unresolved agent failures: 1/);
    assert.match(result.stdout, /unresolved: agent-dddddddd-dddd-4ddd-8ddd-dddddddddddd \[test verifier\] transcript:9 node real\.test\.mjs exit 1/);
  } finally {
    cleanup(dir);
  }
});

test("status next action applies prevention hints for classified agent tool-usage failures", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agent = "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [
      subagentSessionMeta(agent, parent, root, "2026-06-21T10:00:00.000Z"),
      shellCall("call_missing_path", "Get-Content C:\\projects\\game-67-idle\\src\\missing_state.c", "2026-06-21T10:00:01.000Z"),
      shellOutput("call_missing_path", "Exit code: 1\nWall time: 0.3 seconds\nOutput:\nGet-Content : Cannot find path 'C:\\projects\\game-67-idle\\src\\missing_state.c' because it does not exist.\nFullyQualifiedErrorId : PathNotFound,Microsoft.PowerShell.Commands.GetContentCommand\nItemNotFoundException\n", "2026-06-21T10:00:02.000Z"),
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.strict_ok, true);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 0);
    assert.equal(status.agent_rollup.profile_rollup.agent_tool_usage_failed_records, 1);
    assert.match(status.next_action, /Apply the printed agent tool-use prevention hints/);
    assert.match(result.stdout, /prevent missing local file\/path: Verify paths with `rg --files <scope>` or `Test-Path -LiteralPath <path>` before reads\./);
    assert.match(result.stdout, /Apply the printed agent tool-use prevention hints/);
  } finally {
    cleanup(dir);
  }
});

test("strict status agent rollup fails missing usable subagent telemetry", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agent = "66666666-6666-4666-8666-666666666666";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [
      subagentSessionMeta(agent, parent, root, "2026-06-21T10:00:00.000Z"),
    ]);

    const diagnostic = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
      "--verbose",
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.ok, true);
    assert.equal(status.agent_rollup.strict_ok, false);
    assert.deepEqual(status.agent_rollup.strict_problems, ["missing telemetry for 1 subagent session(s)"]);
    assert.equal(status.agent_rollup.profile_rollup.missing_agent_telemetry_count, 1);
    assert.match(diagnostic.stdout, /strict problem: missing telemetry for 1 subagent session\(s\)/);

    const strict = spawnSync(process.execPath, [
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
    ], {
      cwd: root,
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    });
    assert.equal(strict.status, 1);
    assert.match(strict.stdout, /strict problem: missing telemetry for 1 subagent session\(s\)/);
  } finally {
    cleanup(dir);
  }
});

test("strict status agent rollup ignores failed diagnostic strict-status probes", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agent = "99999999-9999-4999-8999-999999999999";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node tools/ai.mjs validate --review"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [
      subagentSessionMeta(agent, parent, root, "2026-06-21T10:00:00.000Z"),
      shellCall("call_strict_probe", "node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 2 --parent-thread-id parent-thread-1", "2026-06-21T10:00:01.000Z"),
      shellOutput("call_strict_probe", "Exit code: 1\nWall time: 0.4 seconds\nOutput:\n## Agent Rollup\n- strict problem: unresolved agent failures: 1\n", "2026-06-21T10:00:02.000Z"),
      shellCall("call_read", "rg --files tools/ai_profile", "2026-06-21T10:00:03.000Z"),
      shellOutput("call_read", "Exit code: 0\nWall time: 0.1 seconds\nOutput:\ntools/ai_profile/status.mjs\n", "2026-06-21T10:00:04.000Z"),
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.ok, true);
    assert.equal(status.agent_rollup.strict_ok, true);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 0);
    assert.equal(status.agent_rollup.profile_rollup.agent_evidence_probe_failed_records, 1);
    assert.match(result.stdout, /agent evidence-probe failures: 1/);
    assert.match(result.stdout, /evidence-probe: agent-99999999-9999-4999-8999-999999999999 \[test verifier\] transcript:3 node ai\.mjs \(failed strict agent rollup probe\) exit 1/);
    assert.doesNotMatch(result.stdout, /unresolved: agent-99999999-9999-4999-8999-999999999999 \[test verifier\] transcript:3 node ai\.mjs/);
  } finally {
    cleanup(dir);
  }
});

test("strict status agent rollup classifies profile diagnostic failures outside unresolved", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const agentProfileDir = join(dir, "profiles");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agent = "99999999-9999-4999-8999-999999999999";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node tools/ai.mjs validate --review"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [
      subagentSessionMeta(agent, parent, root, "2026-06-21T10:00:00.000Z"),
    ]);
    mkdirSync(agentProfileDir, { recursive: true });
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__99999999.jsonl"), [
      { ts: "2026-06-21T10:00:01+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node tools/ai.mjs status --agent-rollup --require-agent-rollup-ok --min-agents 1 --json"], session_id: agent, exit_code: 1 },
      { ts: "2026-06-21T10:00:02+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node tools/ai.mjs --help"], session_id: agent, exit_code: 1 },
      { ts: "2026-06-21T10:00:03+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["Get-ChildItem -Path tasks\\active,tasks\\review -File -ErrorAction SilentlyContinue | Select-Object FullName"], session_id: agent, exit_code: 1 },
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-profile-dir", agentProfileDir,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.ok, true);
    assert.equal(status.agent_rollup.strict_ok, true);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 0);
    assert.equal(status.agent_rollup.profile_rollup.agent_evidence_probe_failed_records, 2);
    assert.equal(status.agent_rollup.profile_rollup.agent_tool_usage_failed_records, 1);
    assert.match(result.stdout, /agent evidence-probe failures: 2/);
    assert.match(result.stdout, /evidence-probe: agent-99999999-9999-4999-8999-999999999999 \[test verifier\] profile:1 node ai\.mjs \(failed strict agent rollup probe\) exit 1/);
    assert.doesNotMatch(result.stdout, /unresolved: agent-99999999-9999-4999-8999-999999999999 \[test verifier\] profile:1 node ai\.mjs/);
    assert.match(result.stdout, /agent tool-usage failures: 1/);
  } finally {
    cleanup(dir);
  }
});

test("status reports evidence-probe clean tail after historical probe failures", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeEvidenceProbeAgentRollout(dir, { file: "rollout-probe.jsonl", agent: "abababab-abab-4aba-8aba-ababababab01", parent, timestamp: "2026-06-21T10:00:00.000Z" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-1.jsonl", agent: "abababab-abab-4aba-8aba-ababababab02", parent, timestamp: "2026-06-21T10:01:00.000Z", callId: "clean_one" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-2.jsonl", agent: "abababab-abab-4aba-8aba-ababababab03", parent, timestamp: "2026-06-21T10:02:00.000Z", callId: "clean_two" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-3.jsonl", agent: "abababab-abab-4aba-8aba-ababababab04", parent, timestamp: "2026-06-21T10:03:00.000Z", callId: "clean_three" });

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.ok, true);
    assert.equal(status.agent_rollup.strict_ok, true);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 0);
    assert.equal(status.agent_rollup.profile_rollup.agent_evidence_probe_failed_records, 1);
    assert.equal(status.agent_rollup.profile_rollup.agent_evidence_probe_clean_tail_agents, 3);
    assert.match(result.stdout, /agent evidence-probe failures: 1/);
    assert.match(result.stdout, /agent evidence-probe clean tail: 3 agent\(s\)/);
  } finally {
    cleanup(dir);
  }
});

test("status compact evidence includes evidence-probe clean tail", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const evidenceJson = join(dir, "status-evidence.json");
    const parent = "parent-thread-1";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeEvidenceProbeAgentRollout(dir, { file: "rollout-probe.jsonl", agent: "acacacac-acac-4aca-8aca-acacacacac01", parent, timestamp: "2026-06-21T10:00:00.000Z" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-1.jsonl", agent: "acacacac-acac-4aca-8aca-acacacacac02", parent, timestamp: "2026-06-21T10:01:00.000Z", callId: "clean_one" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-2.jsonl", agent: "acacacac-acac-4aca-8aca-acacacacac03", parent, timestamp: "2026-06-21T10:02:00.000Z", callId: "clean_two" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-3.jsonl", agent: "acacacac-acac-4aca-8aca-acacacacac04", parent, timestamp: "2026-06-21T10:03:00.000Z", callId: "clean_three" });

    run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-rollup-evidence",
      "--json-output", evidenceJson,
    ]);
    const evidence = readJson(evidenceJson);
    assert.equal(evidence.kind, "status-agent-rollup-evidence");
    assert.equal(evidence.agent_rollup.strict_ok, true);
    assert.equal(evidence.agent_rollup.profile_rollup.unresolved_failed_records, 0);
    assert.equal(evidence.agent_rollup.profile_rollup.agent_evidence_probe_failed_records, 1);
    assert.equal(evidence.agent_rollup.profile_rollup.agent_evidence_probe_clean_tail_agents, 3);
  } finally {
    cleanup(dir);
  }
});

test("status evidence-probe clean tail resets on later probe failure", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeEvidenceProbeAgentRollout(dir, { file: "rollout-probe-1.jsonl", agent: "babababa-baba-4bab-8bab-bababababa01", parent, timestamp: "2026-06-21T10:00:00.000Z", callId: "probe_one" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-1.jsonl", agent: "babababa-baba-4bab-8bab-bababababa02", parent, timestamp: "2026-06-21T10:01:00.000Z", callId: "clean_one" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-2.jsonl", agent: "babababa-baba-4bab-8bab-bababababa03", parent, timestamp: "2026-06-21T10:02:00.000Z", callId: "clean_two" });
    writeEvidenceProbeAgentRollout(dir, { file: "rollout-probe-2.jsonl", agent: "babababa-baba-4bab-8bab-bababababa04", parent, timestamp: "2026-06-21T10:03:00.000Z", callId: "probe_two" });

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.ok, true);
    assert.equal(status.agent_rollup.strict_ok, true);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 0);
    assert.equal(status.agent_rollup.profile_rollup.agent_evidence_probe_failed_records, 2);
    assert.equal(status.agent_rollup.profile_rollup.agent_evidence_probe_clean_tail_agents, 0);
    assert.match(result.stdout, /agent evidence-probe failures: 2/);
    assert.doesNotMatch(result.stdout, /agent evidence-probe clean tail:/);
  } finally {
    cleanup(dir);
  }
});

test("status clean-tail next action guides task creation when no current preflight task exists", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const badAgent = "eeeeeeee-eeee-4eee-8eee-eeeeeeeeeeee";
    const cleanAgents = [
      "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa1",
      "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa2",
      "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaa3",
    ];
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-bad.jsonl"), [
      subagentSessionMeta(badAgent, parent, root, "2026-06-21T10:00:00.000Z"),
      shellCall("call_missing_path", "Get-Content C:\\projects\\game-67-idle\\src\\missing_state.c", "2026-06-21T10:00:01.000Z"),
      shellOutput("call_missing_path", "Exit code: 1\nWall time: 0.3 seconds\nOutput:\nGet-Content : Cannot find path 'C:\\projects\\game-67-idle\\src\\missing_state.c' because it does not exist.\nFullyQualifiedErrorId : PathNotFound,Microsoft.PowerShell.Commands.GetContentCommand\nItemNotFoundException\n", "2026-06-21T10:00:02.000Z"),
    ]);
    writeJsonl(join(dir, "rollout-clean-1.jsonl"), [
      subagentSessionMeta(cleanAgents[0], parent, root, "2026-06-21T10:01:00.000Z"),
      shellCall("clean_one", "rg --files tools/ai_profile", "2026-06-21T10:01:01.000Z"),
      shellOutput("clean_one", "Exit code: 0\nWall time: 0.1 seconds\nOutput:\ntools/ai_profile/status.mjs\n", "2026-06-21T10:01:02.000Z"),
    ]);
    writeJsonl(join(dir, "rollout-clean-2.jsonl"), [
      subagentSessionMeta(cleanAgents[1], parent, root, "2026-06-21T10:02:00.000Z"),
      shellCall("clean_two", "Get-Content -Path tools/ai_profile/status.mjs", "2026-06-21T10:02:01.000Z"),
      shellOutput("clean_two", "Exit code: 0\nWall time: 0.1 seconds\nOutput:\nstatus source\n", "2026-06-21T10:02:02.000Z"),
    ]);
    writeJsonl(join(dir, "rollout-clean-3.jsonl"), [
      subagentSessionMeta(cleanAgents[2], parent, root, "2026-06-21T10:03:00.000Z"),
      shellCall("clean_three", "node --test tools/ai_profile/test.mjs", "2026-06-21T10:03:01.000Z"),
      shellOutput("clean_three", "Exit code: 0\nWall time: 0.1 seconds\nOutput:\npass\n", "2026-06-21T10:03:02.000Z"),
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ], { env: { TASKBOARD_ROOT: dir } });
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.profile_rollup.agent_tool_usage_failed_records, 1);
    assert.equal(status.agent_rollup.profile_rollup.agent_tool_usage_clean_tail_agents, 3);
    assert.match(status.next_action, /Recent subagents are clean of classified tool-use failures/);
    assert.match(status.next_action, /create one current orchestration task with `node tools\/ai\.mjs orchestration-bootstrap` using bounded packet fields/);
    assert.doesNotMatch(status.next_action, /--objective "\.\.\."/);
    assert.doesNotMatch(status.next_action, /--tool-use-guard/);
    assert.doesNotMatch(status.next_action, /parent-thread-id/);
    assert.match(status.next_action, /node tools\/ai\.mjs orchestration-check --current --json/);
    assert.match(result.stdout, /agent tool-usage clean tail: 3 agent\(s\)/);
    assert.match(result.stdout, /Recent subagents are clean of classified tool-use failures/);
    assert.match(result.stdout, /node tools\/ai\.mjs orchestration-bootstrap/);
    assert.doesNotMatch(result.stdout, /--tool-use-guard/);
    assert.match(result.stdout, /node tools\/ai\.mjs orchestration-check --current --json/);
  } finally {
    cleanup(dir);
  }
});

test("status clean-tail next action preflights the current task without copying its id", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    writeTaskboardTask(dir, { id: "T1234", status: "doing", tags: ["pipeline", "orchestration"] });
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeCleanTailRollouts(dir, parent);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ], { env: { TASKBOARD_ROOT: dir } });
    const status = readJson(statusJson);
    assert.match(status.next_action, /node tools\/ai\.mjs orchestration-check --current --json/);
    assert.doesNotMatch(status.next_action, /<task-id>/);
    assert.doesNotMatch(status.next_action, /orchestration-check T1234 --json/);
    assert.doesNotMatch(status.next_action, /orchestration-bootstrap/);
    assert.doesNotMatch(status.next_action, /--tool-use-guard/);
    assert.match(result.stdout, /node tools\/ai\.mjs orchestration-check --current --json/);
  } finally {
    cleanup(dir);
  }
});

test("status clean-tail next action keeps placeholder when current task id is ambiguous", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    writeTaskboardTask(dir, { id: "T1234", status: "doing", tags: ["pipeline", "orchestration"] });
    writeTaskboardTask(dir, { id: "T1235", status: "doing", tags: ["pipeline", "orchestration"] });
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeCleanTailRollouts(dir, parent, "abababab-abab-4aba-8aba-ababababab");

    run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ], { env: { TASKBOARD_ROOT: dir } });
    const status = readJson(statusJson);
    assert.match(status.next_action, /resolve multiple current `doing` pipeline\/orchestration tasks to exactly one, then run `node tools\/ai\.mjs orchestration-check --current --json`/);
    assert.match(status.next_action, /node tools\/ai\.mjs orchestration-check --current --json/);
    assert.doesNotMatch(status.next_action, /<task-id>/);
    assert.doesNotMatch(status.next_action, /orchestration-check T1234 --json/);
    assert.doesNotMatch(status.next_action, /create or refine one/);
  } finally {
    cleanup(dir);
  }
});

test("status clean-tail next action ignores non-current or non-orchestration tasks", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    writeTaskboardTask(dir, { id: "T1234", status: "review", tags: ["pipeline", "orchestration"] });
    writeTaskboardTask(dir, { id: "T1235", status: "doing", tags: ["gameplay"] });
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeCleanTailRollouts(dir, parent, "bcbcbcbc-bcbc-4bcb-8bcb-bcbcbcbcbc");

    run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ], { env: { TASKBOARD_ROOT: dir } });
    const status = readJson(statusJson);
    assert.match(status.next_action, /node tools\/ai\.mjs orchestration-bootstrap/);
    assert.doesNotMatch(status.next_action, /--tool-use-guard/);
    assert.doesNotMatch(status.next_action, /parent-thread-id/);
    assert.match(status.next_action, /node tools\/ai\.mjs orchestration-check --current --json/);
    assert.doesNotMatch(status.next_action, /orchestration-check T1234 --json/);
    assert.doesNotMatch(status.next_action, /orchestration-check T1235 --json/);
  } finally {
    cleanup(dir);
  }
});

test("status next action keeps prevention advice for short clean tail", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeToolUsageAgentRollout(dir, { file: "rollout-bad.jsonl", agent: "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb", parent, timestamp: "2026-06-21T10:00:00.000Z" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-1.jsonl", agent: "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb1", parent, timestamp: "2026-06-21T10:01:00.000Z", callId: "clean_one" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-2.jsonl", agent: "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbb2", parent, timestamp: "2026-06-21T10:02:00.000Z", callId: "clean_two" });

    run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.profile_rollup.agent_tool_usage_clean_tail_agents, 2);
    assert.match(status.next_action, /Apply the printed agent tool-use prevention hints/);
  } finally {
    cleanup(dir);
  }
});

test("status clean tail resets on later agent tool-usage failure", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeToolUsageAgentRollout(dir, { file: "rollout-bad-1.jsonl", agent: "cccccccc-cccc-4ccc-8ccc-ccccccccccc1", parent, timestamp: "2026-06-21T10:00:00.000Z", callId: "bad_one" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-1.jsonl", agent: "cccccccc-cccc-4ccc-8ccc-ccccccccccc2", parent, timestamp: "2026-06-21T10:01:00.000Z", callId: "clean_one" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-2.jsonl", agent: "cccccccc-cccc-4ccc-8ccc-ccccccccccc3", parent, timestamp: "2026-06-21T10:02:00.000Z", callId: "clean_two" });
    writeToolUsageAgentRollout(dir, { file: "rollout-bad-2.jsonl", agent: "cccccccc-cccc-4ccc-8ccc-ccccccccccc4", parent, timestamp: "2026-06-21T10:03:00.000Z", callId: "bad_two" });

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.profile_rollup.agent_tool_usage_clean_tail_agents, 0);
    assert.match(status.next_action, /Apply the printed agent tool-use prevention hints/);
    assert.doesNotMatch(result.stdout, /agent tool-usage clean tail:/);
  } finally {
    cleanup(dir);
  }
});

test("status unresolved agent failures still outrank clean tool-use tail", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeToolUsageAgentRollout(dir, { file: "rollout-bad.jsonl", agent: "dddddddd-dddd-4ddd-8ddd-dddddddddd01", parent, timestamp: "2026-06-21T10:00:00.000Z" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-1.jsonl", agent: "dddddddd-dddd-4ddd-8ddd-dddddddddd02", parent, timestamp: "2026-06-21T10:01:00.000Z", callId: "clean_one" });
    writeCleanAgentRollout(dir, { file: "rollout-clean-2.jsonl", agent: "dddddddd-dddd-4ddd-8ddd-dddddddddd03", parent, timestamp: "2026-06-21T10:02:00.000Z", callId: "clean_two" });
    writeUnresolvedAgentRollout(dir, { file: "rollout-unresolved.jsonl", agent: "dddddddd-dddd-4ddd-8ddd-dddddddddd04", parent, timestamp: "2026-06-21T10:03:00.000Z" });

    run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.profile_rollup.agent_tool_usage_clean_tail_agents, 3);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 1);
    assert.match(status.next_action, /Inspect unresolved agent failure samples/);
  } finally {
    cleanup(dir);
  }
});

test("status next action prioritizes unresolved agent failures over environment blockers", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const agentProfileDir = join(dir, "profiles");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agent = "77777777-7777-4777-8777-777777777777";
    writeJsonl(profile, [
      {
        ts: "2026-06-13T10:00:00+05:00",
        phase: "session",
        category: "validation",
        intent: "auto:Bash",
        result: "fail",
        value: "necessary_overhead",
        event_type: "tool_call_result",
        commands: ["node tools/ai.mjs validate --full"],
        session_id: "s1",
        failure_kind: "environment_blocked",
        blocked_by: "missing full-gate Python modules",
      },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta(agent, parent, root, "2026-06-21T10:00:00.000Z")]);
    mkdirSync(agentProfileDir, { recursive: true });
    writeJsonl(join(agentProfileDir, "2026-06-21__codex__77777777.jsonl"), [
      { ts: "2026-06-21T10:02:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/agent.test.mjs"], session_id: agent, exit_code: 1 },
    ]);

    run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--agent-profile-dir", agentProfileDir,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.environment_blocked_failed_records, 1);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 1);
    assert.match(status.next_action, /unresolved agent failure samples/);
    assert.doesNotMatch(status.next_action, /Environment blockers/);
  } finally {
    cleanup(dir);
  }
});

test("status transcript fallback treats search no-match as pass", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parent = "parent-thread-1";
    const agent = "55555555-5555-4555-8555-555555555555";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [
      subagentSessionMeta(agent, parent, root, "2026-06-21T10:00:00.000Z"),
      shellCall("call_rg_nomatch", "$i=0; rg definitely-not-present", "2026-06-21T10:00:01.000Z"),
      shellOutput("call_rg_nomatch", "Exit code: 1\nWall time: 0.5 seconds\nOutput:\n", "2026-06-21T10:00:02.000Z"),
      shellCall("call_select_nomatch", "Select-String -Path tools/ai_profile/status.mjs -Pattern definitely-not-present", "2026-06-21T10:00:03.000Z"),
      shellOutput("call_select_nomatch", "Exit code: 1\nWall time: 0.6 seconds\nOutput:\n", "2026-06-21T10:00:04.000Z"),
      shellCall("call_rg_error", "rg --badflag", "2026-06-21T10:00:05.000Z"),
      shellOutput("call_rg_error", "Exit code: 2\nWall time: 0.7 seconds\nOutput:\nerror: unknown option\n", "2026-06-21T10:00:06.000Z"),
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.profile_rollup.telemetry_agent_count, 1);
    assert.equal(status.agent_rollup.profile_rollup.records, 3);
    assert.equal(status.agent_rollup.profile_rollup.unresolved_failed_records, 1);
    assert.deepEqual({
      source: status.agent_rollup.profile_rollup.unresolved_failure_samples[0].source,
      command_key: status.agent_rollup.profile_rollup.unresolved_failure_samples[0].command_key,
      command: status.agent_rollup.profile_rollup.unresolved_failure_samples[0].command,
      exit_code: status.agent_rollup.profile_rollup.unresolved_failure_samples[0].exit_code,
      line: status.agent_rollup.profile_rollup.unresolved_failure_samples[0].line,
    }, {
      source: "transcript",
      command_key: "rg",
      command: "rg --badflag",
      exit_code: 2,
      line: 7,
    });
    assert.equal(status.agent_rollup.profile_rollup.command_rollup.by_time.find((entry) => entry.key === "rg")?.fails, 1);
    assert.match(result.stdout, /unresolved agent failures: 1/);
    assert.match(result.stdout, /unresolved: agent-55555555-5555-4555-8555-555555555555 \[test verifier\] transcript:7 rg exit 2 - rg --badflag/);
  } finally {
    cleanup(dir);
  }
});

test("status hints when agent rollup is omitted but parent session id is available", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parentSession = join(dir, "parent-session.jsonl");
    const parent = "parent-thread-1";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(parentSession, [{ type: "session_meta", payload: { id: parent } }]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta("subagent-a", parent, root, "2026-06-21T10:00:00.000Z")]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ], { env: { CODEX_SESSION_FILE: parentSession } });
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup.enabled, false);
    assert.equal(status.agent_rollup_hint.parent_thread_id, parent);
    assert.equal(status.agent_rollup_hint.subagent_session_count, 1);
    assert.ok(
      status.agent_rollup_hint.command.includes(`--session-root ${dir}`)
        || status.agent_rollup_hint.command.includes(`--session-root "${dir}"`),
      status.agent_rollup_hint.command,
    );
    assert.match(status.agent_rollup_hint.command, /--agent-cwd/);
    assert.match(result.stdout, /not included in this status run/);
    assert.match(result.stdout, /status .* --agent-rollup/);
    assert.match(result.stdout, new RegExp(`--parent-thread-id ${parent}`));
  } finally {
    cleanup(dir);
  }
});

test("status does not hint agent rollup when no matching subagent sessions exist", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parentSession = join(dir, "parent-session.jsonl");
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(parentSession, [{ type: "session_meta", payload: { id: "parent-thread-1" } }]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ], { env: { CODEX_SESSION_FILE: parentSession } });
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup_hint, null);
    assert.doesNotMatch(result.stdout, /not included in this status run/);
  } finally {
    cleanup(dir);
  }
});

test("status does not hint agent rollup for another cwd", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const statusJson = join(dir, "status.json");
    const parentSession = join(dir, "parent-session.jsonl");
    const parent = "parent-thread-1";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(parentSession, [{ type: "session_meta", payload: { id: parent } }]);
    writeJsonl(join(dir, "rollout-a.jsonl"), [subagentSessionMeta("subagent-a", parent, join(dir, "other-project"))]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--session-root", dir,
      "--agent-cwd", root,
      "--json-output", statusJson,
    ], { env: { CODEX_SESSION_FILE: parentSession } });
    const status = readJson(statusJson);
    assert.equal(status.agent_rollup_hint, null);
    assert.doesNotMatch(result.stdout, /not included in this status run/);
  } finally {
    cleanup(dir);
  }
});

test("status agent rollup reports missing parent id without failing status", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);

    const result = run(["tools/ai_profile/status.mjs", "--profile", profile, "--agent-rollup", "--session-root", dir, "--agent-cwd", root]);
    assert.equal(result.status, 0);
    assert.match(result.stdout, /problem: missing parent thread id for agent rollup/);

    const strict = spawnSync(process.execPath, ["tools/ai_profile/status.mjs", "--profile", profile, "--agent-rollup", "--require-agent-rollup-ok", "--session-root", dir, "--agent-cwd", root], {
      cwd: root,
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    });
    assert.equal(strict.status, 1);
    assert.match(strict.stdout, /problem: missing parent thread id for agent rollup/);
  } finally {
    cleanup(dir);
  }
});

test("status reports incomplete trace-session rollup without failing status", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const session = join(dir, "codex-session.jsonl");
    const statusJson = join(dir, "status.json");
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);
    writeJsonl(session, [
      multiAgentCall("call_spawn", "spawn_agent", {}),
      multiAgentOutput("call_spawn", { agent_id: "agent-1" }),
    ]);

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--trace-session", session,
      "--json-output", statusJson,
    ]);
    const status = readJson(statusJson);
    assert.equal(result.status, 0);
    assert.equal(status.agent_rollup.enabled, true);
    assert.equal(status.agent_rollup.ok, false);
    assert.equal(status.agent_rollup.source, "trace-session");
    assert.equal(status.agent_rollup.calls_count, 1);
    assert.match(result.stdout, /problem: missing wait for agent-1/);

    const strict = spawnSync(process.execPath, [
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--trace-session", session,
    ], {
      cwd: root,
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    });
    assert.equal(strict.status, 1);
    assert.match(strict.stdout, /problem: missing wait for agent-1/);
  } finally {
    cleanup(dir);
  }
});

test("strict status agent rollup requires at least one matching subagent by default", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "session.jsonl");
    const parent = "parent-thread-1";
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["git status --short"], session_id: "s1" },
    ]);

    const diagnostic = run([
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
    ]);
    assert.equal(diagnostic.status, 0);
    assert.match(diagnostic.stdout, /subagent sessions: 0/);

    const strict = spawnSync(process.execPath, [
      "tools/ai_profile/status.mjs",
      "--profile", profile,
      "--agent-rollup",
      "--require-agent-rollup-ok",
      "--parent-thread-id", parent,
      "--session-root", dir,
      "--agent-cwd", root,
    ], {
      cwd: root,
      encoding: "utf8",
      stdio: ["ignore", "pipe", "pipe"],
    });
    assert.equal(strict.status, 1);
    assert.match(strict.stdout, /expected at least 1 subagent session\(s\), found 0/);
  } finally {
    cleanup(dir);
  }
});

test("status classifies recovered vs unresolved failures", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "failures.jsonl");
    const statusJson = join(dir, "status.json");
    writeJsonl(profile, [
      // command A: fails then passes -> recovered
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/a.test.mjs"], session_id: "s1" },
      { ts: "2026-06-13T10:00:05+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", commands: ["node --test tools/a.test.mjs"], session_id: "s1" },
      // command B: fails and never passes -> unresolved
      { ts: "2026-06-13T10:00:10+05:00", phase: "session", category: "validation", intent: "auto:Bash", result: "fail", value: "rework", event_type: "tool_call_result", commands: ["node --test tools/b.test.mjs"], session_id: "s1" },
    ]);

    const result = run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson]);
    const status = readJson(statusJson);
    assert.equal(status.resolved_later_failed_records, 1);
    assert.equal(status.recovered_failed_records, 1);
    assert.equal(status.unresolved_failed_records, 1);
    assert.match(result.stdout, /Unresolved failures: 1/);
    assert.match(result.stdout, /Resolved later failures: 1/);
    assert.match(status.next_action, /unresolved failed commands/);
  } finally {
    cleanup(dir);
  }
});

test("status separates environment-blocked failures from unresolved failures", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "environment-blocked.jsonl");
    const statusJson = join(dir, "status.json");
    writeJsonl(profile, [
      {
        ts: "2026-06-13T10:00:00+05:00",
        phase: "session",
        category: "validation",
        intent: "auto:Bash",
        result: "fail",
        value: "necessary_overhead",
        event_type: "tool_call_result",
        commands: ["node tools/ai.mjs validate --full"],
        session_id: "s1",
        failure_kind: "environment_blocked",
        blocked_by: "missing full-gate Python modules; install tools/requirements/ai-pipeline-full.txt or set AI_PIPELINE_PYTHON",
      },
    ]);

    const result = run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson, "--verbose"]);
    const status = readJson(statusJson);
    assert.equal(status.unresolved_failed_records, 0);
    assert.equal(status.environment_blocked_failed_records, 1);
    assert.equal(status.environment_blocked_reasons[0].count, 1);
    assert.match(result.stdout, /Environment-blocked failures: 1/);
    assert.match(result.stdout, /Environment Blockers/);
    assert.match(status.next_action, /Environment blockers remain/);
  } finally {
    cleanup(dir);
  }
});

test("status flags low wall-clock coverage and gaps in verbose mode", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "sparse.jsonl");
    const statusJson = join(dir, "status.json");
    // two short events 31 min apart -> effective span > 30 min, near-zero active
    // time -> low coverage + one large gap.
    writeJsonl(profile, [
      { ts: "2026-06-13T10:00:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", duration_ms: 100, commands: ["ls"], session_id: "s1" },
      { ts: "2026-06-13T10:31:00+05:00", phase: "session", category: "tooling", intent: "auto:Bash", result: "pass", value: "unknown", event_type: "tool_call_result", duration_ms: 100, commands: ["ls"], session_id: "s1" },
    ]);

    const result = run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson, "--verbose"]);
    const status = readJson(statusJson);
    assert.equal(status.low_profile_coverage, true);
    assert.equal(status.wall_clock_coverage.largest_gaps.length, 1);
    assert.match(result.stdout, /Largest Coverage Gaps/);
  } finally {
    cleanup(dir);
  }
});

// PARITY: the native hot path (hook_record_fast.c) and the JS fallback
// (hook_record.mjs) duplicate the result/category logic. This test feeds
// identical payloads to BOTH and asserts they agree, so the two sources of
// truth cannot silently drift (e.g. a fix applied to only one of them).
test("hook_record_fast (C) and hook_record.mjs (JS) agree on result + category", {
  skip: !existsSync(join(root, "tools", "ai_profile", process.platform === "win32" ? "hook_record_fast.exe" : "hook_record_fast")),
}, () => {
  const dir = tempDir();
  try {
    const cases = [
      { cmd: "rg nomatch_xyz", exit: 1, want: "pass" },   // search no-match -> pass
      { cmd: "rg --badflag", exit: 2, want: "fail" },     // search real error -> fail
      { cmd: "ninja -C build", exit: 1, want: "fail" },   // real build failure -> fail
      { cmd: "git commit -m x", exit: 0, want: "pass" },  // task_status
      { cmd: "cmake --build .", exit: 0, want: "pass" },  // validation
      { cmd: "node tools/x.mjs", exit: 0, want: "pass" }, // tooling
    ];
    for (const item of cases) {
      const payload = { hook_event_name: "PostToolUse", tool_name: "Bash", tool_input: { command: item.cmd }, tool_response: { exit_code: item.exit } };
      const pc = join(dir, "c.jsonl");
      const pj = join(dir, "j.jsonl");
      rmSync(pc, { force: true });
      rmSync(pj, { force: true });
      runFastHook(payload, pc, "claude");
      runHook(payload, pj, "claude");
      const recC = readJsonl(pc).find((r) => r.event_type === "tool_call_result");
      const recJ = readJsonl(pj).find((r) => r.event_type === "tool_call_result");
      assert.ok(recC && recJ, `"${item.cmd}": missing result record (C=${!!recC} JS=${!!recJ})`);
      assert.equal(recC.result, item.want, `C result for "${item.cmd}"`);
      assert.equal(recJ.result, recC.result, `C/JS RESULT diverge for "${item.cmd}": C=${recC.result} JS=${recJ.result}`);
      assert.equal(recJ.category, recC.category, `C/JS CATEGORY diverge for "${item.cmd}": C=${recC.category} JS=${recJ.category}`);
    }
  } finally {
    cleanup(dir);
  }
});

// The two harness hook configs are hand-mirrored; this guards them from drifting
// (e.g. adding PreToolUse to one but not the other). They must register the same
// events and invoke hook_record_fast for each (modulo the harness name token).
test("hook configs (.claude/settings.json and .codex/hooks.json) stay in sync", () => {
  const claude = JSON.parse(readFileSync(join(root, ".claude", "settings.json"), "utf8"));
  const codex = JSON.parse(readFileSync(join(root, ".codex", "hooks.json"), "utf8"));
  const events = (cfg) => Object.keys(cfg.hooks || {}).sort();
  assert.deepEqual(events(claude), events(codex), "hook EVENTS differ between .claude/settings.json and .codex/hooks.json");
  const norm = (cfg, ev, harness) =>
    (cfg.hooks[ev] || [])
      .flatMap((group) => (group.hooks || []).map((h) => `${h.command}|${h.commandWindows}`))
      .map((s) => s.split(harness).join("<H>"))
      .sort();
  for (const ev of events(claude)) {
    assert.deepEqual(
      norm(claude, ev, "claude"),
      norm(codex, ev, "codex"),
      `hook commands for "${ev}" differ between harnesses (beyond the harness name)`,
    );
  }
});
