import test from "node:test";
import assert from "node:assert/strict";
import { appendFileSync, existsSync, mkdtempSync, readFileSync, rmSync, utimesSync, writeFileSync } from "node:fs";
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

function writeBundleArtifacts(profile, text = "artifact\n") {
  const base = profile.replace(/\.jsonl$/i, "");
  for (const suffix of ["summary.md", "review.md", "review.json", "followups.md", "followups.json"]) {
    writeFileSync(`${base}.${suffix}`, text, "utf8");
  }
}

function setMtime(path, iso) {
  const date = new Date(iso);
  utimesSync(path, date, date);
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
    assert.match(status.next_action, /closeout\.mjs/);
    assert.doesNotMatch(status.next_action, /scope\.mjs/);
    assert.doesNotMatch(status.next_action, /start\.mjs/);
    assert.doesNotMatch(status.next_action, /context\.mjs/);
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
    assert.match(status.next_action, /context\.mjs/);
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
    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });

    const status = readJson(statusJson);
    assert.equal(status.low_profile_coverage, true);
    assert.equal(status.current_scope.low_profile_coverage, true);
    assert.match(status.next_action, /checkpoint\.mjs/);
    assert.doesNotMatch(status.next_action, /event\.mjs/);
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
    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });

    const status = readJson(statusJson);
    assert.equal(status.low_profile_coverage, true);
    assert.equal(status.current_scope.low_profile_coverage, false);
    assert.doesNotMatch(status.next_action, /checkpoint\.mjs/);
  } finally {
    cleanup(dir);
  }
});

test("status reports complete fresh closeout bundle", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "fresh-bundle.jsonl");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "session_closeout",
      "--category",
      "reflection",
      "--intent",
      "closeout",
      "--result",
      "pass",
      "--value",
      "necessary_overhead",
      "--ts",
      "2026-06-13T10:00:00+05:00",
    ]);
    writeBundleArtifacts(profile);
    setMtime(profile, "2026-06-13T05:00:00Z");
    for (const suffix of ["summary.md", "review.md", "review.json", "followups.md", "followups.json"]) {
      setMtime(profile.replace(/\.jsonl$/i, `.${suffix}`), "2026-06-13T05:01:00Z");
    }

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson]);
    const status = readJson(statusJson);
    assert.equal(status.bundle.complete, true);
    assert.equal(status.bundle.fresh, true);
    assert.deepEqual(status.bundle.stale_artifacts, []);
  } finally {
    cleanup(dir);
  }
});

test("status reports complete stale closeout bundle", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "stale-bundle.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "session_closeout",
      "--category",
      "reflection",
      "--intent",
      "closeout",
      "--result",
      "pass",
      "--value",
      "necessary_overhead",
      "--duration-ms",
      "600000",
      "--ts",
      "2026-06-13T10:00:00+05:00",
    ]);
    writeBundleArtifacts(profile);
    setMtime(profile, "2026-06-13T05:02:00Z");
    for (const suffix of ["summary.md", "review.md", "review.json", "followups.md", "followups.json"]) {
      setMtime(profile.replace(/\.jsonl$/i, `.${suffix}`), "2026-06-13T05:01:00Z");
    }
    writeFileSync(scope, `${JSON.stringify({ schema_version: 1, work_item: "STALE", iteration: "bundle" })}\n`, "utf8");

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const status = readJson(statusJson);
    assert.equal(status.bundle.complete, true);
    assert.equal(status.bundle.fresh, false);
    assert.deepEqual(status.bundle.stale_artifacts, ["summary", "review", "review_json", "followups", "followups_json"]);
    assert.match(status.next_action, /closeout\.mjs/);
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

test("observability gate stays local without concrete external needs", () => {
  const dir = tempDir();
  try {
    const output = join(dir, "observability-local.json");
    run([
      "tools/ai_profile/observability_gate.mjs",
      "--setup-cost",
      "high",
      "--sensitivity",
      "high",
      "--json-output",
      output,
    ]);

    const decision = readJson(output);
    assert.equal(decision.recommendation, "local_jsonl_only");
    assert.equal(decision.keep_local_jsonl, true);
    assert.deepEqual(decision.matched_external_needs, []);
    assert.ok(decision.required_capture_fields.includes("context_inputs"));
  } finally {
    cleanup(dir);
  }
});

test("observability gate recommends bounded pilot for shared eval needs", () => {
  const dir = tempDir();
  try {
    const output = join(dir, "observability-pilot.json");
    const result = run([
      "tools/ai_profile/observability_gate.mjs",
      "--need",
      "human-review",
      "--need",
      "datasets",
      "--team",
      "small",
      "--setup-cost",
      "medium",
      "--sensitivity",
      "medium",
      "--self-host-ok",
      "--json-output",
      output,
    ]);

    const decision = readJson(output);
    assert.equal(decision.recommendation, "external_pilot");
    assert.equal(decision.keep_local_jsonl, true);
    assert.ok(result.stdout.includes("run a bounded pilot beside local JSONL"));
  } finally {
    cleanup(dir);
  }
});

test("validation planner writes json output with broad final decision", () => {
  const dir = tempDir();
  try {
    const output = join(dir, "validation-plan.json");
    const result = run([
      "tools/ai_profile/plan_validation.mjs",
      "--change",
      "profiling",
      "--change",
      "pipeline",
      "--risk",
      "medium",
      "--json-output",
      output,
    ]);

    const plan = readJson(output);
    assert.equal(plan.schema_version, 1);
    assert.equal(plan.risk, "medium");
    assert.ok(plan.checks_by_tier.preflight.length > 0);
    assert.ok(plan.checks_by_tier.scoped.length > 0);
    assert.ok(plan.broad_final_count > 0);
    assert.ok(plan.broad_final_checks.some((check) => check.id === "portable-pipeline"));
    assert.match(plan.next_action, /broad\/final checks once/);
    assert.match(result.stdout, /Validation Ladder/);
  } finally {
    cleanup(dir);
  }
});

test("validation planner json output marks low risk broad checks as deferred", () => {
  const dir = tempDir();
  try {
    const output = join(dir, "validation-plan-low.json");
    run([
      "tools/ai_profile/plan_validation.mjs",
      "--change",
      "pipeline",
      "--risk",
      "low",
      "--json-output",
      output,
      "--json",
    ]);

    const plan = readJson(output);
    assert.equal(plan.risk, "low");
    assert.equal(plan.broad_final_count, 0);
    assert.ok(plan.deferred_broad_count > 0);
    assert.ok(plan.skipped_final.some((check) => check.id === "portable-pipeline"));
    assert.match(plan.next_action, /deferred/);
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
    run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: join(dir, "no-scope.json") },
    });
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

test("followups suppress historical-only issues when current scope is clean", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "historical-noise.jsonl");
    const scope = join(dir, "scope.json");
    const reviewJson = join(dir, "review.json");
    const followupsJson = join(dir, "followups.json");
    const broadCommand = "node tools/pipeline_validate.mjs";

    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "validation",
      "--category", "validation",
      "--intent", "old broad validation one",
      "--result", "pass",
      "--value", "productive",
      "--command", broadCommand,
      "--context-risk", "medium",
      "--ts", "2026-06-13T10:00:00+05:00",
    ]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "validation",
      "--category", "validation",
      "--intent", "old broad validation two",
      "--result", "pass",
      "--value", "productive",
      "--command", broadCommand,
      "--duration-ms", "1000",
      "--ts", "2026-06-13T10:45:00+05:00",
    ]);
    for (let index = 0; index < 18; index += 1) {
      appendFileSync(profile, `${JSON.stringify({
        ts: `2026-06-13T10:46:${String(index).padStart(2, "0")}+05:00`,
        phase: "context",
        category: "context",
        intent: `old unscoped context ${index}`,
        result: "pass",
        value: "necessary_overhead",
      })}\n`, "utf8");
    }
    run([
      "tools/ai_profile/start.mjs",
      "--scope", scope,
      "--profile", profile,
      "--work-item", "CUR",
      "--iteration", "clean",
      "--phase", "test",
      "--intent", "current clean scope",
    ]);

    run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const review = readJson(reviewJson);
    assert.equal(review.current_scope.enabled, true);
    assert.equal(review.current_scope.missing_context_inputs, 0);
    assert.equal(review.current_scope.missing_work_item_records, 0);
    assert.equal(review.current_scope.repeated_broad_final_commands.length, 0);

    run(["tools/ai_profile/followups.mjs", reviewJson, "--json-output", followupsJson]);
    const followups = readJson(followupsJson);
    assert.ok(followups.suppressed_historical_findings.includes("repeated_broad_final_commands"));
    assert.ok(followups.suppressed_historical_findings.includes("missing_context_inputs"));
    assert.ok(followups.suppressed_historical_findings.includes("missing_work_item_metadata"));
    assert.ok(followups.suppressed_historical_findings.includes("low_profile_coverage"));
    assert.equal(
      followups.suggestions.some((suggestion) => suggestion.priority === "P1"),
      false,
    );
  } finally {
    cleanup(dir);
  }
});

test("followups preserve current-scope issues", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "current-issue.jsonl");
    const scope = join(dir, "scope.json");
    const reviewJson = join(dir, "review.json");
    const followupsJson = join(dir, "followups.json");

    run([
      "tools/ai_profile/start.mjs",
      "--scope", scope,
      "--profile", profile,
      "--work-item", "CUR",
      "--iteration", "dirty",
      "--phase", "test",
      "--intent", "current dirty scope",
    ]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "context",
      "--category", "context",
      "--intent", "current unmeasured context",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--context-risk", "medium",
    ], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });

    run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const review = readJson(reviewJson);
    assert.equal(review.current_scope.enabled, true);
    assert.equal(review.current_scope.missing_context_inputs, 1);

    run(["tools/ai_profile/followups.mjs", reviewJson, "--json-output", followupsJson]);
    const followups = readJson(followupsJson);
    assert.ok(followups.suggestions.some((suggestion) => suggestion.source === "current_scope.missing_context_inputs"));
    assert.equal(followups.suppressed_historical_findings.includes("missing_context_inputs"), false);
  } finally {
    cleanup(dir);
  }
});

test("followups suppress historical recovered failures when current scope is clean", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "historical-recovered.jsonl");
    const scope = join(dir, "scope.json");
    const reviewJson = join(dir, "review.json");
    const followupsJson = join(dir, "followups.json");
    const command = "node tools/skills_eval.mjs";

    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "validation",
      "--category", "validation",
      "--intent", "old failing eval",
      "--result", "fail",
      "--value", "rework",
      "--command", command,
      "--ts", "2026-06-13T10:00:00+05:00",
    ]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "validation",
      "--category", "validation",
      "--intent", "old passing eval",
      "--result", "pass",
      "--value", "productive",
      "--command", command,
      "--ts", "2026-06-13T10:01:00+05:00",
    ]);
    run([
      "tools/ai_profile/start.mjs",
      "--scope", scope,
      "--profile", profile,
      "--work-item", "CUR",
      "--iteration", "clean",
      "--phase", "test",
      "--intent", "clean current scope",
    ]);

    run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const review = readJson(reviewJson);
    assert.equal(review.recovered_failed_records.length, 1);
    assert.equal(review.current_scope.recovered_failed_records.length, 0);

    run(["tools/ai_profile/followups.mjs", reviewJson, "--json-output", followupsJson]);
    const followups = readJson(followupsJson);
    assert.ok(followups.suppressed_historical_findings.includes("recovered_failed_records"));
    assert.equal(
      followups.suggestions.some((suggestion) => suggestion.source === "recovered_failed_records"),
      false,
    );
  } finally {
    cleanup(dir);
  }
});

test("followups preserve current-scope recovered failures", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "current-recovered.jsonl");
    const scope = join(dir, "scope.json");
    const reviewJson = join(dir, "review.json");
    const followupsJson = join(dir, "followups.json");
    const command = "node tools/skills_eval.mjs";

    run([
      "tools/ai_profile/start.mjs",
      "--scope", scope,
      "--profile", profile,
      "--work-item", "CUR",
      "--iteration", "recovered",
      "--phase", "test",
      "--intent", "current recovered scope",
    ]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "validation",
      "--category", "validation",
      "--intent", "current failing eval",
      "--result", "fail",
      "--value", "rework",
      "--command", command,
    ], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "validation",
      "--category", "validation",
      "--intent", "current passing eval",
      "--result", "pass",
      "--value", "productive",
      "--command", command,
    ], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });

    run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const review = readJson(reviewJson);
    assert.equal(review.current_scope.recovered_failed_records.length, 1);

    run(["tools/ai_profile/followups.mjs", reviewJson, "--json-output", followupsJson]);
    const followups = readJson(followupsJson);
    assert.ok(followups.suggestions.some((suggestion) => suggestion.source === "current_scope.recovered_failed_records"));
    assert.equal(followups.suppressed_historical_findings.includes("recovered_failed_records"), false);
  } finally {
    cleanup(dir);
  }
});
