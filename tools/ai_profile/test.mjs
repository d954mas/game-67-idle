import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

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

function runRaw(args, options = {}) {
  return spawnSync(process.execPath, args, {
    cwd: root,
    env: { ...process.env, ...(options.env || {}) },
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
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

test("scope file supplies metadata after CLI and env fallbacks", () => {
  const dir = tempDir();
  try {
    const scope = join(dir, "scope.json");
    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "SCOPE", "--iteration", "scope-default"]);

    const scopeOnly = join(dir, "scope-only.jsonl");
    run([
      "tools/ai_profile/event.mjs",
      "--profile", scopeOnly,
      "--phase", "test",
      "--category", "tooling",
      "--intent", "scope only",
      "--result", "pass",
      "--value", "productive",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    assert.deepEqual(
      { work_item: readJsonl(scopeOnly)[0].work_item, iteration: readJsonl(scopeOnly)[0].iteration },
      { work_item: "SCOPE", iteration: "scope-default" },
    );

    const envOverride = join(dir, "env-override.jsonl");
    run([
      "tools/ai_profile/event.mjs",
      "--profile", envOverride,
      "--phase", "test",
      "--category", "tooling",
      "--intent", "env override",
      "--result", "pass",
      "--value", "productive",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope, AI_PROFILE_WORK_ITEM: "ENV", AI_PROFILE_ITERATION: "env-default" } });
    assert.deepEqual(
      { work_item: readJsonl(envOverride)[0].work_item, iteration: readJsonl(envOverride)[0].iteration },
      { work_item: "ENV", iteration: "env-default" },
    );

    const cliOverride = join(dir, "cli-override.jsonl");
    run([
      "tools/ai_profile/event.mjs",
      "--profile", cliOverride,
      "--phase", "test",
      "--category", "tooling",
      "--intent", "cli override",
      "--result", "pass",
      "--value", "productive",
      "--work-item", "CLI",
      "--iteration", "cli-default",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope, AI_PROFILE_WORK_ITEM: "ENV", AI_PROFILE_ITERATION: "env-default" } });
    assert.deepEqual(
      { work_item: readJsonl(cliOverride)[0].work_item, iteration: readJsonl(cliOverride)[0].iteration },
      { work_item: "CLI", iteration: "cli-default" },
    );
  } finally {
    cleanup(dir);
  }
});

test("start writes scope and phase_start event", () => {
  const dir = tempDir();
  try {
    const scope = join(dir, "scope.json");
    const profile = join(dir, "started.jsonl");
    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      scope,
      "--profile",
      profile,
      "--work-item",
      "START",
      "--iteration",
      "helper",
      "--phase",
      "test_start",
      "--category",
      "planning",
      "--notes",
      "start helper smoke",
    ]);

    const savedScope = readJson(scope);
    assert.equal(savedScope.work_item, "START");
    assert.equal(savedScope.iteration, "helper");

    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].event_type, "phase_start");
    assert.equal(records[0].phase, "test_start");
    assert.equal(records[0].category, "planning");
    assert.equal(records[0].result, "pass");
    assert.equal(records[0].value, "necessary_overhead");
    assert.equal(records[0].work_item, "START");
    assert.equal(records[0].iteration, "helper");
    assert.equal(records[0].notes, "start helper smoke");
    assert.equal(records[0].scope_path, resolve(scope));
    assert.deepEqual(records[0].tools, ["ai_profile/start.mjs"]);
  } finally {
    cleanup(dir);
  }
});

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
      tool_input: { command: "git status --short" },
      tool_response: { exit_code: 0 },
    }, profile, "codex", { CODEX_SESSION_FILE: session });
    runHook({
      hook_event_name: "PostToolUse",
      tool_name: "Bash",
      tool_input: { command: "git status --short" },
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

test("status reports scope, coverage fields, and next action", () => {
  const dir = tempDir();
  try {
    const scope = join(dir, "scope.json");
    const profile = join(dir, "profile.jsonl");
    const statusJson = join(dir, "status.json");
    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "STATUS", "--iteration", "status-test"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "test",
      "--category", "tooling",
      "--intent", "status event",
      "--result", "pass",
      "--value", "productive",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    const status = readJson(statusJson);
    assert.equal(status.scope.work_item, "STATUS");
    assert.equal(status.scope.iteration, "status-test");
    assert.equal(status.work_item_coverage.missing_records, 0);
    assert.equal(status.closeout_seen, false);
    assert.match(status.next_action, /reflect/);
  } finally {
    cleanup(dir);
  }
});

test("status recommends start helper for missing profiles", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "missing.jsonl");
    const scope = join(dir, "missing-scope.json");
    const statusJson = join(dir, "status.json");
    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });

    const status = readJson(statusJson);
    assert.equal(status.exists, false);
    assert.match(status.next_action, /ai\.mjs start/);
    assert.doesNotMatch(status.next_action, /scope\.mjs/);
  } finally {
    cleanup(dir);
  }
});

test("status flags archived current scope as stale", () => {
  const dir = tempDir();
  try {
    const taskRoot = join(dir, "task-root");
    mkdirSync(join(taskRoot, "tasks", "archive", "E999"), { recursive: true });
    writeFileSync(join(taskRoot, "tasks", "archive", "E999", "T9999-old-scope.md"), `---
id: T9999
title: Old scope
status: done
epic: E999
priority: P1
tags: []
created: 2026-06-15
updated: 2026-06-15
---

## What
`, "utf8");
    const scope = join(dir, "scope.json");
    const profile = join(dir, "stale-scope.jsonl");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      scope,
      "--profile",
      profile,
      "--work-item",
      "T9999",
      "--iteration",
      "old",
    ]);
    const result = run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope, AI_PROFILE_TASK_ROOT: taskRoot },
    });
    const status = readJson(statusJson);
    assert.equal(status.stale_scope, true);
    assert.equal(status.scope_task.status, "done");
    assert.match(status.next_action, /Reset profiling scope/);
    assert.match(result.stdout, /Current scope task: done/);
    assert.match(result.stdout, /stale/);
  } finally {
    cleanup(dir);
  }
});

test("status current-scope guard fails for slash-suffixed archived task ids", () => {
  const dir = tempDir();
  try {
    const taskRoot = join(dir, "task-root");
    mkdirSync(join(taskRoot, "tasks", "archive", "E999"), { recursive: true });
    writeFileSync(join(taskRoot, "tasks", "archive", "E999", "T9998-old-scope.md"), `---
id: T9998
title: Old slash scope
status: done
epic: E999
priority: P1
tags: []
created: 2026-06-15
updated: 2026-06-15
---

## What
`, "utf8");
    const scope = join(dir, "scope.json");
    const profile = join(dir, "stale-slash-scope.jsonl");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      scope,
      "--profile",
      profile,
      "--work-item",
      "T9998/old-slice",
      "--iteration",
      "old",
      "--ts",
      "2026-06-13T10:00:00+05:00",
    ]);
    writeFileSync(scope, `${JSON.stringify({
      schema_version: 1,
      work_item: "T9998/old-slice",
      iteration: "old",
      updated_at: "2026-06-13T10:00:00+05:00",
    })}\n`, "utf8");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "validation",
      "--category",
      "validation",
      "--intent",
      "second scoped event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--duration-ms",
      "1000",
      "--ts",
      "2026-06-13T10:00:01+05:00",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });

    const result = runRaw([
      "tools/ai_profile/status.mjs",
      "--profile",
      profile,
      "--json-output",
      statusJson,
      "--require-current-scope-usable",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope, AI_PROFILE_TASK_ROOT: taskRoot } });

    assert.equal(result.status, 3);
    assert.match(result.stderr, /scope_stale/);
    const status = readJson(statusJson);
    assert.equal(status.stale_scope, true);
    assert.equal(status.scope_task.status, "done");
    assert.match(status.scope_task.path, /T9998-old-scope\.md/);
    assert.ok(status.current_scope_review_confidence.blocking_reasons.includes("scope_stale"));
  } finally {
    cleanup(dir);
  }
});

test("status current-scope guard allows slash-suffixed active task ids", () => {
  const dir = tempDir();
  try {
    const taskRoot = join(dir, "task-root");
    mkdirSync(join(taskRoot, "tasks", "active"), { recursive: true });
    writeFileSync(join(taskRoot, "tasks", "active", "T9997-active-scope.md"), `---
id: T9997
title: Active slash scope
status: doing
epic: E999
priority: P1
tags: []
created: 2026-06-15
updated: 2026-06-15
---

## What
`, "utf8");
    const scope = join(dir, "scope.json");
    const profile = join(dir, "active-slash-scope.jsonl");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      scope,
      "--profile",
      profile,
      "--work-item",
      "T9997/active-slice",
      "--iteration",
      "active",
      "--ts",
      "2026-06-13T10:00:00+05:00",
    ]);
    writeFileSync(scope, `${JSON.stringify({
      schema_version: 1,
      work_item: "T9997/active-slice",
      iteration: "active",
      updated_at: "2026-06-13T10:00:00+05:00",
    })}\n`, "utf8");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "validation",
      "--category",
      "validation",
      "--intent",
      "measured active event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--duration-ms",
      "1000",
      "--ts",
      "2026-06-13T10:00:01+05:00",
      "--command",
      "node tools/taskboard/cli.mjs validate",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile",
      profile,
      "--json-output",
      statusJson,
      "--require-current-scope-usable",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope, AI_PROFILE_TASK_ROOT: taskRoot } });

    const status = readJson(statusJson);
    assert.equal(status.stale_scope, false);
    assert.equal(status.scope_task.status, "doing");
    assert.equal(status.current_scope_review_confidence.level, "usable");
    assert.match(result.stdout, /Current scope review confidence: usable/);
  } finally {
    cleanup(dir);
  }
});

test("status does not ask for scope or context fixes when only historical records are incomplete", () => {
  const dir = tempDir();
  try {
    const missingScope = join(dir, "missing-scope.json");
    const currentScope = join(dir, "current-scope.json");
    const profile = join(dir, "historical-unscoped.jsonl");
    const statusJson = join(dir, "status.json");

    for (let index = 0; index < 6; index += 1) {
      const args = [
        "tools/ai_profile/event.mjs",
        "--profile",
        profile,
        "--phase",
        "test",
        "--category",
        "context",
        "--intent",
        `historical unscoped ${index}`,
        "--result",
        "pass",
        "--value",
        "productive",
      ];
      if (index === 0) args.push("--context-risk", "medium");
      run(args, { env: { AI_PROFILE_SCOPE_FILE: missingScope } });
    }

    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      currentScope,
      "--profile",
      profile,
      "--work-item",
      "STATUS2",
      "--iteration",
      "current",
    ]);
    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: currentScope },
    });

    const status = readJson(statusJson);
    assert.equal(status.scope.work_item, "STATUS2");
    assert.equal(status.work_item_coverage.missing_records, 6);
    assert.equal(status.current_scope.records, 1);
    assert.equal(status.current_scope.missing_context_inputs, 0);
    assert.equal(status.current_scope.missing_work_item_records, 0);
    assert.match(status.next_action, /reflect/);
    assert.doesNotMatch(status.next_action, /scope\.mjs/);
    assert.doesNotMatch(status.next_action, /start\.mjs/);
    assert.doesNotMatch(status.next_action, /ai\.mjs context/);
  } finally {
    cleanup(dir);
  }
});

test("status still flags missing context inputs in the current scope", () => {
  const dir = tempDir();
  try {
    const scope = join(dir, "scope.json");
    const profile = join(dir, "current-missing-context.jsonl");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      scope,
      "--profile",
      profile,
      "--work-item",
      "STATUS3",
      "--iteration",
      "current",
    ]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "context",
      "--category",
      "context",
      "--intent",
      "current unmeasured context",
      "--result",
      "pass",
      "--value",
      "necessary_overhead",
      "--context-risk",
      "medium",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });

    const status = readJson(statusJson);
    assert.equal(status.current_scope.records, 2);
    assert.equal(status.current_scope.missing_context_inputs, 1);
    assert.match(status.next_action, /ai\.mjs context --path/);
  } finally {
    cleanup(dir);
  }
});

test("status recommends checkpoint helper for low wall-clock coverage", () => {
  const dir = tempDir();
  try {
    const scope = join(dir, "scope.json");
    const profile = join(dir, "low-coverage.jsonl");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      scope,
      "--profile",
      profile,
      "--work-item",
      "STATUS4",
      "--iteration",
      "coverage",
    ]);
    const futureTs = new Date(Date.now() + 31 * 60 * 1000).toISOString();
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "planning",
      "--category",
      "planning",
      "--intent",
      "future sparse checkpoint",
      "--result",
      "pass",
      "--value",
      "productive",
      "--ts",
      futureTs,
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    const result = run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });

    const status = readJson(statusJson);
    assert.equal(status.low_profile_coverage, true);
    assert.equal(status.current_scope.low_profile_coverage, true);
    assert.equal(status.wall_clock_coverage.largest_gaps.length, 1);
    assert.equal(status.current_scope.wall_clock_coverage.largest_gaps.length, 1);
    assert.match(status.wall_clock_coverage.largest_gaps[0].previous_intent, /phase start|Start/i);
    assert.equal(status.wall_clock_coverage.largest_gaps[0].next_intent, "future sparse checkpoint");
    assert.equal(status.review_confidence.level, "broken");
    assert.ok(status.review_confidence.blocking_reasons.includes("current_scope_low_wall_clock_coverage"));
    assert.match(status.next_action, /ai\.mjs checkpoint/);
    assert.doesNotMatch(status.next_action, /event\.mjs/);
    assert.match(result.stdout, /Largest Coverage Gaps/);
    assert.match(result.stdout, /current scope/);
    assert.match(result.stdout, /whole profile/);
    assert.match(result.stdout, /future sparse checkpoint|lines 1-2/);
  } finally {
    cleanup(dir);
  }
});

test("status does not recommend checkpoint for historical low coverage only", () => {
  const dir = tempDir();
  try {
    const oldScope = join(dir, "old-scope.json");
    const scope = join(dir, "scope.json");
    const profile = join(dir, "historical-low-coverage.jsonl");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "planning",
      "--category",
      "planning",
      "--intent",
      "old sparse event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--ts",
      "2026-06-13T10:00:00+05:00",
    ], { env: { AI_PROFILE_SCOPE_FILE: oldScope } });
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "planning",
      "--category",
      "planning",
      "--intent",
      "old future sparse event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--ts",
      "2026-06-13T10:31:00+05:00",
    ], { env: { AI_PROFILE_SCOPE_FILE: oldScope } });
    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      scope,
      "--profile",
      profile,
      "--work-item",
      "STATUS5",
      "--iteration",
      "current",
      "--ts",
      "2026-06-13T10:32:00+05:00",
    ]);
    writeFileSync(scope, `${JSON.stringify({
      schema_version: 1,
      work_item: "STATUS5",
      iteration: "current",
      updated_at: "2026-06-13T10:32:00+05:00",
    })}\n`, "utf8");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "planning",
      "--category",
      "planning",
      "--intent",
      "current measured event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--duration-ms",
      "1000",
      "--ts",
      "2026-06-13T10:32:01+05:00",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    const result = run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });

    const status = readJson(statusJson);
    assert.equal(status.low_profile_coverage, true);
    assert.equal(status.current_scope.low_profile_coverage, false);
    assert.equal(status.wall_clock_coverage.largest_gaps.length, 1);
    assert.equal(status.current_scope.wall_clock_coverage.largest_gaps.length, 0);
    assert.equal(status.review_confidence.level, "partial");
    assert.ok(status.review_confidence.partial_reasons.includes("whole_profile_low_wall_clock_coverage"));
    assert.doesNotMatch(status.next_action, /checkpoint\.mjs/);
    assert.match(result.stdout, /Largest Coverage Gaps/);
    assert.match(result.stdout, /whole profile/);
    assert.doesNotMatch(status.passive_summary.normal_work_next_action, /No profiling maintenance needed/);
    assert.match(status.passive_summary.normal_work_next_action, /Historical wall-clock coverage is incomplete/);
  } finally {
    cleanup(dir);
  }
});

test("status exposes usable current scope when historical coverage is partial", () => {
  const dir = tempDir();
  try {
    const oldScope = join(dir, "old-scope.json");
    const scope = join(dir, "scope.json");
    const profile = join(dir, "historical-partial-current-usable.jsonl");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "planning",
      "--category",
      "planning",
      "--intent",
      "old sparse event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--ts",
      "2026-06-13T10:00:00+05:00",
    ], { env: { AI_PROFILE_SCOPE_FILE: oldScope } });
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "planning",
      "--category",
      "planning",
      "--intent",
      "old future sparse event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--ts",
      "2026-06-13T10:31:00+05:00",
    ], { env: { AI_PROFILE_SCOPE_FILE: oldScope } });
    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      scope,
      "--profile",
      profile,
      "--work-item",
      "STATUS6",
      "--iteration",
      "current",
      "--ts",
      "2026-06-13T10:32:00+05:00",
    ]);
    writeFileSync(scope, `${JSON.stringify({
      schema_version: 1,
      work_item: "STATUS6",
      iteration: "current",
      updated_at: "2026-06-13T10:32:00+05:00",
    })}\n`, "utf8");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "validation",
      "--category",
      "validation",
      "--intent",
      "current measured command",
      "--result",
      "pass",
      "--value",
      "productive",
      "--duration-ms",
      "5000",
      "--ts",
      "2026-06-13T10:32:05+05:00",
      "--command",
      "node tools/taskboard/cli.mjs validate",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });

    const result = run([
      "tools/ai_profile/status.mjs",
      "--profile",
      profile,
      "--json-output",
      statusJson,
      "--require-current-scope-usable",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });

    const status = readJson(statusJson);
    assert.equal(status.review_confidence.level, "partial");
    assert.equal(status.current_scope_review_confidence.level, "usable");
    assert.match(result.stdout, /Current scope review confidence: usable/);
  } finally {
    cleanup(dir);
  }
});

test("status current-scope guard fails when only start was recorded", () => {
  const dir = tempDir();
  try {
    const scope = join(dir, "scope.json");
    const profile = join(dir, "too-shallow-current-scope.jsonl");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/start.mjs",
      "--scope",
      scope,
      "--profile",
      profile,
      "--work-item",
      "STATUS7",
      "--iteration",
      "start-only",
    ]);

    const result = runRaw([
      "tools/ai_profile/status.mjs",
      "--profile",
      profile,
      "--json-output",
      statusJson,
      "--require-current-scope-usable",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });

    assert.equal(result.status, 3);
    assert.match(result.stdout, /Current scope review confidence: broken/);
    assert.match(result.stderr, /current_scope_too_shallow/);
    assert.match(result.stderr, /node tools\/ai\.mjs checkpoint/);
    const status = readJson(statusJson);
    assert.equal(status.current_scope_review_confidence.level, "broken");
    assert.ok(status.current_scope_review_confidence.blocking_reasons.includes("current_scope_too_shallow"));
  } finally {
    cleanup(dir);
  }
});

test("context profiler warns on oversized context inputs", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "context-budget.jsonl");
    const largeFile = join(dir, "large-context.txt");
    writeFileSync(largeFile, "x".repeat(240), "utf8");

    const result = runRaw([
      "tools/ai_profile/context.mjs",
      "--profile",
      profile,
      "--phase",
      "context",
      "--intent",
      "measure oversized context",
      "--path",
      largeFile,
      "--warn-file-chars",
      "100",
      "--warn-total-chars",
      "200",
    ]);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /context budget: total=240 chars, risk=low/);
    assert.match(result.stdout, /warning: large context input/);
    assert.match(result.stdout, /warning: context batch is large/);

    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].context_inputs[0].chars, 240);
    assert.deepEqual(records[0].tools, ["ai_profile/context.mjs"]);
  } finally {
    cleanup(dir);
  }
});

test("ai facade can fully profile taskboard summary shortcut", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "ai-summary.jsonl");
    const scope = join(dir, "scope.json");
    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "SUMMARY", "--iteration", "shortcut"]);

    const result = runRaw(["tools/ai.mjs", "summary", "--profile-mode", "full", "--profile", profile], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /# Taskboard Summary/);

    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].work_item, "SUMMARY");
    assert.equal(records[0].iteration, "shortcut");
    assert.equal(records[0].category, "context");
    assert.deepEqual(records[0].tools, ["ai_profile/context_command.mjs"]);
    assert.match(records[0].commands[0], /tools\/taskboard\/cli\.mjs summary|tools\\taskboard\\cli\.mjs summary/);
    assert.match(records[0].context_inputs[0].path, /^command:/);
    assert.ok(records[0].context_inputs[0].chars > 0);
  } finally {
    cleanup(dir);
  }
});

test("context_command records command output as measured context", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "context-command.jsonl");
    const scope = join(dir, "scope.json");
    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "CTXCMD", "--iteration", "success"]);
    const result = runRaw([
      "tools/ai_profile/context_command.mjs",
      "--profile",
      profile,
      "--phase",
      "context",
      "--intent",
      "read command context",
      "--reason",
      "test output",
      "--",
      process.execPath,
      "-e",
      "console.log('context command output')",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });

    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /context command output/);
    const record = readJsonl(profile)[0];
    assert.equal(record.result, "pass");
    assert.equal(record.value, "necessary_overhead");
    assert.equal(record.command_exit_code, 0);
    assert.deepEqual(record.tools, ["ai_profile/context_command.mjs"]);
    assert.equal(record.work_item, "CTXCMD");
    assert.equal(record.iteration, "success");
    assert.equal(record.context_inputs.length, 1);
    assert.match(record.context_inputs[0].path, /^command:/);
    assert.ok(record.context_inputs[0].chars >= "context command output\n".length);
    assert.equal(record.context_inputs[0].reason, "test output");
    assert.equal(record.context_risk, "low");
  } finally {
    cleanup(dir);
  }
});

test("context_command records failing command and preserves exit code", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "context-command-fail.jsonl");
    const result = runRaw([
      "tools/ai_profile/context_command.mjs",
      "--profile",
      profile,
      "--phase",
      "context",
      "--intent",
      "read failing command context",
      "--",
      process.execPath,
      "-e",
      "console.error('context command failed'); process.exit(7)",
    ]);

    assert.equal(result.status, 7);
    assert.match(result.stderr, /context command failed/);
    const record = readJsonl(profile)[0];
    assert.equal(record.result, "fail");
    assert.equal(record.command_exit_code, 7);
    assert.equal(record.context_inputs.length, 1);
    assert.ok(record.context_inputs[0].chars >= "context command failed\n".length);
  } finally {
    cleanup(dir);
  }
});

test("checkpoint infers duration from previous profile record", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "checkpoint.jsonl");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "planning",
      "--category",
      "planning",
      "--intent",
      "previous event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--ts",
      "2026-06-13T10:00:00+05:00",
    ]);
    run([
      "tools/ai_profile/checkpoint.mjs",
      "--profile",
      profile,
      "--intent",
      "manual planning checkpoint",
      "--ts",
      "2026-06-13T10:05:00+05:00",
    ]);

    const records = readJsonl(profile);
    const checkpoint = records[1];
    assert.equal(checkpoint.event_type, "checkpoint");
    assert.equal(checkpoint.duration_ms, 300000);
    assert.equal(checkpoint.duration_source, "since_previous_record");
    assert.equal(checkpoint.duration_capped, false);
    assert.equal(checkpoint.previous_profile_line, 1);
    assert.equal(checkpoint.previous_profile_intent, "previous event");
    assert.deepEqual(checkpoint.tools, ["ai_profile/checkpoint.mjs"]);
  } finally {
    cleanup(dir);
  }
});

test("checkpoint caps inferred duration", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "checkpoint-capped.jsonl");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "planning",
      "--category",
      "planning",
      "--intent",
      "previous event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--ts",
      "2026-06-13T10:00:00+05:00",
    ]);
    run([
      "tools/ai_profile/checkpoint.mjs",
      "--profile",
      profile,
      "--intent",
      "capped checkpoint",
      "--ts",
      "2026-06-13T12:00:00+05:00",
      "--max-duration-min",
      "10",
    ]);

    const checkpoint = readJsonl(profile)[1];
    assert.equal(checkpoint.duration_ms, 600000);
    assert.equal(checkpoint.duration_capped, true);
  } finally {
    cleanup(dir);
  }
});

test("checkpoint explicit duration overrides inferred duration", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "checkpoint-explicit.jsonl");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "planning",
      "--category",
      "planning",
      "--intent",
      "previous event",
      "--result",
      "pass",
      "--value",
      "productive",
      "--ts",
      "2026-06-13T10:00:00+05:00",
    ]);
    run([
      "tools/ai_profile/checkpoint.mjs",
      "--profile",
      profile,
      "--intent",
      "explicit checkpoint",
      "--ts",
      "2026-06-13T12:00:00+05:00",
      "--duration-ms",
      "1234",
    ]);

    const checkpoint = readJsonl(profile)[1];
    assert.equal(checkpoint.duration_ms, 1234);
    assert.equal(checkpoint.duration_source, "explicit");
    assert.equal(checkpoint.duration_capped, undefined);
  } finally {
    cleanup(dir);
  }
});

test("gap checkpoint appends only when gap exceeds threshold", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "gap-checkpoint.jsonl");
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "planning",
      "--category", "planning",
      "--intent", "previous planning",
      "--result", "pass",
      "--value", "productive",
      "--ts", "2026-06-13T10:00:00+05:00",
    ]);
    const result = run([
      "tools/ai_profile/gap_checkpoint.mjs",
      "--profile", profile,
      "--intent", "manual review gap",
      "--min-gap-min", "5",
      "--max-duration-min", "10",
      "--ts", "2026-06-13T10:12:00+05:00",
      "--work-item", "GAP",
    ]);
    assert.match(result.stdout, /profile gap checkpoint appended/);
    const records = readJsonl(profile);
    const checkpoint = records[1];
    assert.equal(checkpoint.event_type, "gap_checkpoint");
    assert.equal(checkpoint.duration_ms, 600000);
    assert.equal(checkpoint.raw_gap_ms, 720000);
    assert.equal(checkpoint.min_gap_ms, 300000);
    assert.equal(checkpoint.duration_capped, true);
    assert.equal(checkpoint.previous_profile_line, 1);
    assert.equal(checkpoint.work_item, "GAP");
    assert.deepEqual(checkpoint.tools, ["ai_profile/gap_checkpoint.mjs"]);
  } finally {
    cleanup(dir);
  }
});

test("gap checkpoint skips short gaps without writing a record", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "gap-checkpoint-skip.jsonl");
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "planning",
      "--category", "planning",
      "--intent", "previous planning",
      "--result", "pass",
      "--value", "productive",
      "--ts", "2026-06-13T10:00:00+05:00",
    ]);
    const result = run([
      "tools/ai_profile/gap_checkpoint.mjs",
      "--profile", profile,
      "--intent", "short pause",
      "--min-gap-min", "5",
      "--ts", "2026-06-13T10:02:00+05:00",
    ]);
    assert.match(result.stdout, /gap checkpoint skipped/);
    assert.equal(readJsonl(profile).length, 1);
  } finally {
    cleanup(dir);
  }
});

test("status recovers failed validation checks when command changes", () => {
  const dir = tempDir();
  try {
    const scope = join(dir, "scope.json");
    const profile = join(dir, "validation-check-recovered.jsonl");
    const statusJson = join(dir, "status.json");
    writeFileSync(scope, `${JSON.stringify({
      schema_version: 1,
      work_item: "RECOVER",
      iteration: "validation-check",
      updated_at: "2026-06-13T10:00:00+05:00",
    })}\n`, "utf8");
    writeFileSync(profile, [
      {
        ts: "2026-06-13T10:00:01+05:00",
        phase: "validation",
        category: "validation",
        intent: "old runner command",
        result: "fail",
        value: "productive",
        work_item: "RECOVER",
        iteration: "validation-check",
        commands: ["py -3.12 -m unittest tools.assets.atomic_io_test"],
        validation_check_id: "asset-source-preprocess-tests",
      },
      {
        ts: "2026-06-13T10:00:02+05:00",
        phase: "validation",
        category: "validation",
        intent: "fixed runner command",
        result: "pass",
        value: "productive",
        work_item: "RECOVER",
        iteration: "validation-check",
        commands: ["python -m unittest tools.assets.atomic_io_test"],
        validation_check_id: "asset-source-preprocess-tests",
      },
    ].map((record) => JSON.stringify(record)).join("\n") + "\n", "utf8");

    const statusResult = run([
      "tools/ai_profile/status.mjs",
      "--profile",
      profile,
      "--json-output",
      statusJson,
      "--require-current-scope-usable",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    assert.match(statusResult.stdout, /Current scope review confidence: usable/);
    const status = readJson(statusJson);
    assert.equal(status.unresolved_failed_records, 0);
    assert.equal(status.recovered_failed_records, 1);
    assert.equal(status.current_scope.unresolved_failed_records, 0);
    assert.equal(status.current_scope.recovered_failed_records, 1);
  } finally {
    cleanup(dir);
  }
});
