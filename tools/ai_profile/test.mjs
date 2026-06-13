import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
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
    assert.match(status.next_action, /closeout\.mjs/);
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
    assert.match(status.next_action, /start\.mjs/);
    assert.doesNotMatch(status.next_action, /scope\.mjs/);
  } finally {
    cleanup(dir);
  }
});

test("status does not ask for scope when only historical records lack work item", () => {
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

    run(["tools/ai_profile/scope.mjs", "set", "--scope", currentScope, "--work-item", "STATUS2", "--iteration", "current"]);
    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: currentScope },
    });

    const status = readJson(statusJson);
    assert.equal(status.scope.work_item, "STATUS2");
    assert.equal(status.work_item_coverage.missing_records, 6);
    assert.match(status.next_action, /context\.mjs/);
    assert.doesNotMatch(status.next_action, /scope\.mjs/);
    assert.doesNotMatch(status.next_action, /start\.mjs/);
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

test("closeout writes summary review and follow-up bundle", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "bundle.jsonl");
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "test",
      "--category", "tooling",
      "--intent", "bundle seed",
      "--result", "pass",
      "--value", "productive",
      "--work-item", "BUNDLE",
    ]);
    run(["tools/ai_profile/closeout.mjs", "--profile", profile, "--work-item", "BUNDLE", "--iteration", "closeout"]);
    for (const suffix of ["summary.md", "review.md", "review.json", "followups.md", "followups.json"]) {
      assert.equal(existsSync(join(dir, `bundle.${suffix}`)), true, suffix);
    }
    const records = readJsonl(profile);
    const closeout = records.find((record) => record.phase === "session_closeout");
    assert.ok(closeout);
    assert.equal(closeout.evidence.length, 5);
  } finally {
    cleanup(dir);
  }
});

test("review and followups classify recovered failures", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "recovered.jsonl");
    const reviewJson = join(dir, "review.json");
    const followupsJson = join(dir, "followups.json");
    const command = "node tools/skills_eval.mjs";
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "validation",
      "--category", "validation",
      "--intent", "failing eval",
      "--result", "fail",
      "--value", "rework",
      "--command", command,
    ]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "validation",
      "--category", "validation",
      "--intent", "passing eval",
      "--result", "pass",
      "--value", "productive",
      "--command", command,
    ]);
    run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson]);
    const review = readJson(reviewJson);
    assert.equal(review.recovered_failed_records.length, 1);
    assert.equal(review.unresolved_failed_records.length, 0);

    run(["tools/ai_profile/followups.mjs", reviewJson, "--json-output", followupsJson]);
    const followups = readJson(followupsJson);
    assert.ok(followups.suggestions.some((suggestion) => suggestion.source === "recovered_failed_records"));
  } finally {
    cleanup(dir);
  }
});
