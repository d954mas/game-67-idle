import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawn, spawnSync } from "node:child_process";

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

function runFail(args, options = {}) {
  const result = spawnSync(process.execPath, args, {
    cwd: root,
    env: { ...process.env, ...(options.env || {}) },
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  assert.notEqual(result.status, 0, `${args.join(" ")}\nstdout:\n${result.stdout}\nstderr:\n${result.stderr}`);
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
