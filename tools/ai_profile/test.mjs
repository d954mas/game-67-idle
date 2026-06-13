import test from "node:test";
import assert from "node:assert/strict";
import { appendFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, utimesSync, writeFileSync } from "node:fs";
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

function writeValidReflectionBundle(profile) {
  const base = profile.replace(/\.jsonl$/i, "");
  writeFileSync(`${base}.summary.md`, "summary\n", "utf8");
  writeFileSync(`${base}.review.md`, "review\n", "utf8");
  writeFileSync(`${base}.review.json`, `${JSON.stringify({
    profile,
    findings: [],
    current_scope: {
      enabled: true,
      records: 1,
      findings: [],
      suggested_actions: ["Use current scope as clean baseline."],
    },
    repeated_commands: [],
    repeated_commands_by_scope: [],
    repeated_broad_final_commands: [],
    repeated_broad_final_by_work_item: [],
  })}\n`, "utf8");
  writeFileSync(`${base}.followups.md`, "followups\n", "utf8");
  writeFileSync(`${base}.followups.json`, `${JSON.stringify({
    suggestions: [],
    suppressed_historical_findings: [],
  })}\n`, "utf8");
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
    assert.match(status.next_action, /ai\.mjs start/);
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
    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });

    const status = readJson(statusJson);
    assert.equal(status.low_profile_coverage, true);
    assert.equal(status.current_scope.low_profile_coverage, true);
    assert.match(status.next_action, /ai\.mjs checkpoint/);
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

test("status recommends baseline capture when clean profile has no baseline", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "clean-no-baseline.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "BASE", "--iteration", "missing"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "session_closeout",
      "--category",
      "reflection",
      "--intent",
      "clean closeout",
      "--result",
      "pass",
      "--value",
      "necessary_overhead",
      "--work-item",
      "BASE",
      "--iteration",
      "missing",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeBundleArtifacts(profile);

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const status = readJson(statusJson);
    assert.equal(status.baselines.count, 0);
    assert.equal(status.baselines.latest_manifest, null);
    assert.match(status.next_action, /capture_baseline\.mjs/);
  } finally {
    cleanup(dir);
  }
});

test("status reports latest captured baseline when one exists", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "clean-with-baseline.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "BASE", "--iteration", "present"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile",
      profile,
      "--phase",
      "session_closeout",
      "--category",
      "reflection",
      "--intent",
      "clean closeout",
      "--result",
      "pass",
      "--value",
      "necessary_overhead",
      "--work-item",
      "BASE",
      "--iteration",
      "present",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeBundleArtifacts(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({
      schema_version: 1,
      label: "clean",
      captured_at: "2026-06-13T10:05:00+05:00",
      source_review: profile.replace(/\.jsonl$/i, ".review.json"),
      baseline_review: resolve(baselineReview),
      compare_command: `node tools/ai_profile/compare_reviews.mjs ${resolve(baselineReview)} current.review.json`,
      summary: { current_scope_findings: 0 },
    })}\n`, "utf8");

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const status = readJson(statusJson);
    assert.equal(status.baselines.count, 1);
    assert.equal(status.baselines.latest_manifest.label, "clean");
    assert.equal(status.baselines.latest_manifest.baseline_review, resolve(baselineReview));
    assert.equal(status.comparison.status, "missing");
    assert.match(status.next_action, /compare_reviews\.mjs/);
    assert.doesNotMatch(status.next_action, /capture_baseline\.mjs/);
  } finally {
    cleanup(dir);
  }
});

test("status reports stale baseline comparison", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "stale-compare.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compareJson = join(dir, "clean.compare.json");
    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "BASE", "--iteration", "stale"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "session_closeout",
      "--category", "reflection",
      "--intent", "clean closeout",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--work-item", "BASE",
      "--iteration", "stale",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeBundleArtifacts(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({
      schema_version: 1,
      label: "clean",
      captured_at: "2026-06-13T10:05:00+05:00",
      baseline_review: resolve(baselineReview),
    })}\n`, "utf8");
    writeFileSync(compareJson, `${JSON.stringify({ verdict: "stable", current_regressions: [] })}\n`, "utf8");
    setMtime(profile, "2026-06-13T05:00:00Z");
    for (const suffix of ["summary.md", "review.md", "review.json", "followups.md", "followups.json"]) {
      setMtime(profile.replace(/\.jsonl$/i, `.${suffix}`), "2026-06-13T05:10:00Z");
    }
    setMtime(compareJson, "2026-06-13T05:00:00Z");

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const status = readJson(statusJson);
    assert.equal(status.comparison.status, "stale");
    assert.match(status.comparison.reason, /older than current review/);
    assert.match(status.next_action, /Run baseline comparison/);
  } finally {
    cleanup(dir);
  }
});

test("status reports current-scope regressions from baseline comparison", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "regressed-compare.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compareJson = join(dir, "clean.compare.json");
    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "BASE", "--iteration", "regressed"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "session_closeout",
      "--category", "reflection",
      "--intent", "clean closeout",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--work-item", "BASE",
      "--iteration", "regressed",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeBundleArtifacts(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({
      schema_version: 1,
      label: "clean",
      captured_at: "2026-06-13T10:05:00+05:00",
      baseline_review: resolve(baselineReview),
    })}\n`, "utf8");
    writeFileSync(compareJson, `${JSON.stringify({
      verdict: "regressed",
      current_regressions: [{ key: "current_missing_context_inputs" }],
    })}\n`, "utf8");

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const status = readJson(statusJson);
    assert.equal(status.comparison.status, "regressed");
    assert.equal(status.comparison.current_regressions, 1);
    assert.match(status.next_action, /Inspect current-scope regressions/);
  } finally {
    cleanup(dir);
  }
});

test("status reports fresh baseline comparison without regressions", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "fresh-compare.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compareJson = join(dir, "clean.compare.json");
    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "BASE", "--iteration", "fresh"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "session_closeout",
      "--category", "reflection",
      "--intent", "clean closeout",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--work-item", "BASE",
      "--iteration", "fresh",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeBundleArtifacts(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({
      schema_version: 1,
      label: "clean",
      captured_at: "2026-06-13T10:05:00+05:00",
      baseline_review: resolve(baselineReview),
    })}\n`, "utf8");
    writeFileSync(compareJson, `${JSON.stringify({
      verdict: "stable",
      current_regressions: [],
    })}\n`, "utf8");

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const status = readJson(statusJson);
    assert.equal(status.comparison.status, "fresh");
    assert.equal(status.comparison.verdict, "stable");
    assert.equal(status.reflection.packet.status, "missing");
    assert.match(status.next_action, /Generate reflection packet/);
    assert.doesNotMatch(status.next_action, /Run baseline comparison/);
  } finally {
    cleanup(dir);
  }
});

test("status reports stale reflection draft after fresh packet", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "stale-draft.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compareJson = join(dir, "clean.compare.json");
    const packetMd = join(dir, "stale-draft.reflection_packet.md");
    const packetJson = join(dir, "stale-draft.reflection_packet.json");
    const draftMd = join(dir, "stale-draft.reflection_draft.md");
    const draftJson = join(dir, "stale-draft.reflection_draft.json");

    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "BASE", "--iteration", "draft-stale"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "session_closeout",
      "--category", "reflection",
      "--intent", "clean closeout",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--work-item", "BASE",
      "--iteration", "draft-stale",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeBundleArtifacts(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({ schema_version: 1, label: "clean", baseline_review: resolve(baselineReview) })}\n`, "utf8");
    writeFileSync(compareJson, `${JSON.stringify({ verdict: "stable", current_regressions: [] })}\n`, "utf8");
    writeFileSync(packetMd, "packet\n", "utf8");
    writeFileSync(packetJson, "{}\n", "utf8");
    writeFileSync(draftMd, "draft\n", "utf8");
    writeFileSync(draftJson, "{}\n", "utf8");
    setMtime(profile, "2026-06-13T04:59:00Z");
    for (const suffix of ["summary.md", "review.md", "review.json", "followups.md", "followups.json"]) {
      setMtime(profile.replace(/\.jsonl$/i, `.${suffix}`), "2026-06-13T05:00:00Z");
    }
    setMtime(compareJson, "2026-06-13T05:00:00Z");
    setMtime(baselineReview, "2026-06-13T04:59:00Z");
    setMtime(draftMd, "2026-06-13T05:00:00Z");
    setMtime(draftJson, "2026-06-13T05:00:00Z");
    setMtime(packetMd, "2026-06-13T05:10:00Z");
    setMtime(packetJson, "2026-06-13T05:10:00Z");

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const status = readJson(statusJson);
    assert.equal(status.reflection.packet.status, "fresh");
    assert.equal(status.reflection.draft.status, "stale");
    assert.equal(status.reflection.review.status, "waiting");
    assert.match(status.next_action, /Generate reflection draft/);
  } finally {
    cleanup(dir);
  }
});

test("status reports missing reflection review after fresh draft", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "fresh-draft.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compareJson = join(dir, "clean.compare.json");
    const packetMd = join(dir, "fresh-draft.reflection_packet.md");
    const packetJson = join(dir, "fresh-draft.reflection_packet.json");
    const draftMd = join(dir, "fresh-draft.reflection_draft.md");
    const draftJson = join(dir, "fresh-draft.reflection_draft.json");

    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "BASE", "--iteration", "draft-fresh"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "session_closeout",
      "--category", "reflection",
      "--intent", "clean closeout",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--work-item", "BASE",
      "--iteration", "draft-fresh",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeBundleArtifacts(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({ schema_version: 1, label: "clean", baseline_review: resolve(baselineReview) })}\n`, "utf8");
    writeFileSync(compareJson, `${JSON.stringify({ verdict: "stable", current_regressions: [] })}\n`, "utf8");
    writeFileSync(packetMd, "packet\n", "utf8");
    writeFileSync(packetJson, "{}\n", "utf8");
    writeFileSync(draftMd, "draft\n", "utf8");
    writeFileSync(draftJson, "{}\n", "utf8");

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const status = readJson(statusJson);
    assert.equal(status.reflection.packet.status, "fresh");
    assert.equal(status.reflection.draft.status, "fresh");
    assert.equal(status.reflection.review.status, "missing");
    assert.match(status.next_action, /Generate reflection review/);
  } finally {
    cleanup(dir);
  }
});

test("status reports fresh reflection review as first retrospective decision artifact", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "fresh-review.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compareJson = join(dir, "clean.compare.json");
    const packetMd = join(dir, "fresh-review.reflection_packet.md");
    const packetJson = join(dir, "fresh-review.reflection_packet.json");
    const draftMd = join(dir, "fresh-review.reflection_draft.md");
    const draftJson = join(dir, "fresh-review.reflection_draft.json");
    const reviewMd = join(dir, "fresh-review.reflection_review.md");
    const reviewJson = join(dir, "fresh-review.reflection_review.json");

    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "BASE", "--iteration", "review-fresh"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "session_closeout",
      "--category", "reflection",
      "--intent", "clean closeout",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--work-item", "BASE",
      "--iteration", "review-fresh",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeBundleArtifacts(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({ schema_version: 1, label: "clean", baseline_review: resolve(baselineReview) })}\n`, "utf8");
    writeFileSync(compareJson, `${JSON.stringify({ verdict: "stable", current_regressions: [] })}\n`, "utf8");
    writeFileSync(packetMd, "packet\n", "utf8");
    writeFileSync(packetJson, "{}\n", "utf8");
    writeFileSync(draftMd, "draft\n", "utf8");
    writeFileSync(draftJson, "{}\n", "utf8");
    writeFileSync(reviewMd, "review\n", "utf8");
    writeFileSync(reviewJson, "{}\n", "utf8");

    run(["tools/ai_profile/status.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const status = readJson(statusJson);
    assert.equal(status.reflection.packet.status, "fresh");
    assert.equal(status.reflection.draft.status, "fresh");
    assert.equal(status.reflection.review.status, "fresh");
    assert.match(status.next_action, /Use fresh reflection review/);
  } finally {
    cleanup(dir);
  }
});

test("prepare reflection is no-op when handoff artifacts are fresh", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "prep-fresh.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compareJson = join(dir, "clean.compare.json");
    const packetMd = join(dir, "prep-fresh.reflection_packet.md");
    const packetJson = join(dir, "prep-fresh.reflection_packet.json");
    const draftMd = join(dir, "prep-fresh.reflection_draft.md");
    const draftJson = join(dir, "prep-fresh.reflection_draft.json");
    const reviewMd = join(dir, "prep-fresh.reflection_review.md");
    const reviewJson = join(dir, "prep-fresh.reflection_review.json");

    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "PREP", "--iteration", "fresh"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "session_closeout",
      "--category", "reflection",
      "--intent", "clean closeout",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--work-item", "PREP",
      "--iteration", "fresh",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeValidReflectionBundle(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({ schema_version: 1, label: "clean", baseline_review: resolve(baselineReview) })}\n`, "utf8");
    writeFileSync(compareJson, `${JSON.stringify({ verdict: "stable", current_regressions: [] })}\n`, "utf8");
    writeFileSync(packetMd, "packet\n", "utf8");
    writeFileSync(packetJson, `${JSON.stringify({ artifacts: { review_json: `${profile.replace(/\.jsonl$/i, "")}.review.json` } })}\n`, "utf8");
    writeFileSync(draftMd, "draft\n", "utf8");
    writeFileSync(draftJson, "{}\n", "utf8");
    writeFileSync(reviewMd, "review\n", "utf8");
    writeFileSync(reviewJson, "{}\n", "utf8");

    const result = run(["tools/ai_profile/prepare_reflection.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    assert.match(result.stdout, /Steps: none/);
    const status = readJson(statusJson);
    assert.equal(status.reflection.packet.status, "fresh");
    assert.equal(status.reflection.draft.status, "fresh");
    assert.equal(status.reflection.review.status, "fresh");
  } finally {
    cleanup(dir);
  }
});

test("prepare reflection generates missing packet draft and review", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "prep-missing.jsonl");
    const scope = join(dir, "scope.json");
    const statusJson = join(dir, "status.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compareJson = join(dir, "clean.compare.json");

    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "PREP", "--iteration", "missing"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "session_closeout",
      "--category", "reflection",
      "--intent", "clean closeout",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--work-item", "PREP",
      "--iteration", "missing",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeValidReflectionBundle(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({ schema_version: 1, label: "clean", baseline_review: resolve(baselineReview) })}\n`, "utf8");
    writeFileSync(compareJson, `${JSON.stringify({ verdict: "stable", current_regressions: [] })}\n`, "utf8");

    const result = run(["tools/ai_profile/prepare_reflection.mjs", "--profile", profile, "--json-output", statusJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    assert.match(result.stdout, /Steps: packet, draft, review/);
    assert.equal(existsSync(join(dir, "prep-missing.reflection_packet.json")), true);
    assert.equal(existsSync(join(dir, "prep-missing.reflection_draft.json")), true);
    assert.equal(existsSync(join(dir, "prep-missing.reflection_review.json")), true);
    const status = readJson(statusJson);
    assert.equal(status.reflection.packet.status, "fresh");
    assert.equal(status.reflection.draft.status, "fresh");
    assert.equal(status.reflection.review.status, "fresh");
  } finally {
    cleanup(dir);
  }
});

test("prepare reflection refuses current-scope regressions by default", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "prep-regressed.jsonl");
    const scope = join(dir, "scope.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compareJson = join(dir, "clean.compare.json");

    run(["tools/ai_profile/scope.mjs", "set", "--scope", scope, "--work-item", "PREP", "--iteration", "regressed"]);
    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "session_closeout",
      "--category", "reflection",
      "--intent", "clean closeout",
      "--result", "pass",
      "--value", "necessary_overhead",
      "--work-item", "PREP",
      "--iteration", "regressed",
    ], { env: { AI_PROFILE_SCOPE_FILE: scope } });
    writeValidReflectionBundle(profile);
    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({ schema_version: 1, label: "clean", baseline_review: resolve(baselineReview) })}\n`, "utf8");
    writeFileSync(compareJson, `${JSON.stringify({
      verdict: "regressed",
      current_regressions: [{ key: "current_missing_context_inputs", label: "Current missing context", baseline: 0, current: 1 }],
    })}\n`, "utf8");

    const result = runRaw(["tools/ai_profile/prepare_reflection.mjs", "--profile", profile], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    assert.equal(result.status, 1);
    assert.match(result.stderr, /current-scope regressions present/);
    assert.equal(existsSync(join(dir, "prep-regressed.reflection_packet.json")), false);
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

test("validation planner defers broad final checks by default", () => {
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
    const jsSyntaxCheck = plan.checks_by_tier.preflight.find((check) => check.id === "js-syntax-touched");
    assert.equal(jsSyntaxCheck.command, "node tools/ai_profile/check_touched_js.mjs");
    assert.equal(jsSyntaxCheck.placeholder, undefined);
    assert.ok(plan.checks_by_tier.scoped.length > 0);
    assert.ok(plan.checks_by_tier.scoped.some((check) => check.id === "ai-profile-tests"));
    assert.equal(plan.broad_final_count, 0);
    assert.ok(plan.skipped_final.some((check) => check.id === "portable-pipeline"));
    assert.match(plan.next_action, /deferred/);
    assert.match(result.stdout, /Validation Ladder/);
  } finally {
    cleanup(dir);
  }
});

test("validation planner includes broad final checks only when requested", () => {
  const dir = tempDir();
  try {
    const output = join(dir, "validation-plan-final.json");
    run([
      "tools/ai_profile/plan_validation.mjs",
      "--change",
      "pipeline",
      "--risk",
      "medium",
      "--include-final",
      "--json-output",
      output,
    ]);

    const plan = readJson(output);
    assert.ok(plan.broad_final_count > 0);
    assert.ok(plan.broad_final_checks.some((check) => check.id === "portable-pipeline"));
    assert.match(plan.next_action, /broad\/final checks once/);
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

test("validation runner profiles checks and stops final after failure", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "validation-run.jsonl");
    const planJson = join(dir, "plan.json");
    const summaryJson = join(dir, "summary.json");
    const passCommand = `${JSON.stringify(process.execPath)} -e ${JSON.stringify("process.exit(0)")}`;
    const failCommand = `${JSON.stringify(process.execPath)} -e ${JSON.stringify("process.exit(3)")}`;
    writeFileSync(planJson, `${JSON.stringify({
      schema_version: 1,
      risk: "medium",
      changes: ["profiling"],
      checks: [
        { id: "preflight-pass", tier: "preflight", command: passCommand, why: "cheap pass", broad: false },
        { id: "scoped-fail", tier: "scoped", command: failCommand, why: "expected fail", broad: false },
        { id: "final-skipped", tier: "final", command: passCommand, why: "should not run", broad: true },
      ],
      next_action: "test plan",
    })}\n`, "utf8");

    const result = runRaw([
      "tools/ai_profile/validation_run.mjs",
      "--plan", planJson,
      "--profile", profile,
      "--work-item", "VALRUN",
      "--iteration", "failure",
      "--batch-id", "batch-test",
      "--json-output", summaryJson,
    ]);
    assert.equal(result.status, 3, result.stderr);
    assert.match(result.stdout, /Executed: 2/);
    assert.match(result.stdout, /Skipped: 1/);

    const summary = readJson(summaryJson);
    assert.equal(summary.batch_id, "batch-test");
    assert.equal(summary.executed.length, 2);
    assert.equal(summary.skipped[0].id, "final-skipped");
    assert.equal(summary.skipped[0].reason, "previous check failed");
    const records = readJsonl(profile);
    assert.equal(records.length, 2);
    assert.equal(records[0].validation_check_id, "preflight-pass");
    assert.equal(records[0].validation_batch_id, "batch-test");
    assert.equal(records[0].validation_plan_risk, "medium");
    assert.deepEqual(records[0].validation_plan_changes, ["profiling"]);
    assert.equal(records[0].result, "pass");
    assert.equal(records[0].work_item, "VALRUN");
    assert.equal(records[1].validation_check_id, "scoped-fail");
    assert.equal(records[1].result, "fail");
    assert.equal(records[1].command_exit_code, 3);
  } finally {
    cleanup(dir);
  }
});

test("validation runner compacts passing output but prints failures", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "validation-output.jsonl");
    const planJson = join(dir, "plan-output.json");
    const summaryJson = join(dir, "summary-output.json");
    const passCommand = `${JSON.stringify(process.execPath)} -e ${JSON.stringify("console.log('PASS_OUTPUT')")}`;
    const failCommand = `${JSON.stringify(process.execPath)} -e ${JSON.stringify("console.error('FAIL_OUTPUT'); process.exit(4)")}`;
    writeFileSync(planJson, `${JSON.stringify({
      schema_version: 1,
      risk: "medium",
      changes: ["profiling"],
      checks: [
        { id: "pass-output", tier: "preflight", command: passCommand, why: "passing output", broad: false },
        { id: "fail-output", tier: "scoped", command: failCommand, why: "failing output", broad: false },
      ],
      next_action: "test output",
    })}\n`, "utf8");

    const result = runRaw([
      "tools/ai_profile/validation_run.mjs",
      "--plan", planJson,
      "--profile", profile,
      "--json-output", summaryJson,
    ]);
    assert.equal(result.status, 4);
    assert.doesNotMatch(result.stdout, /PASS_OUTPUT/);
    assert.match(result.stdout, /pass preflight pass-output \(\d+ms, output \d+ chars suppressed\)/);
    assert.match(result.stderr, /FAIL_OUTPUT/);

    const summary = readJson(summaryJson);
    assert.equal(summary.executed[0].output_suppressed, true);
    assert.equal(summary.executed[1].output_suppressed, false);
    const records = readJsonl(profile);
    assert.equal(records[0].command_output_suppressed, true);
    assert.equal(records[1].command_output_suppressed, undefined);
  } finally {
    cleanup(dir);
  }
});

test("profile review summarizes validation batches", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "validation-batches.jsonl");
    const reviewMd = join(dir, "review.md");
    const reviewJson = join(dir, "review.json");
    for (const [index, tier] of ["preflight", "scoped", "final"].entries()) {
      appendFileSync(profile, `${JSON.stringify({
        ts: `2026-06-13T10:00:0${index}+05:00`,
        phase: "validation",
        category: "validation",
        intent: `check ${tier}`,
        result: "pass",
        value: tier === "final" ? "necessary_overhead" : "productive",
        duration_ms: 1000 + index,
        commands: [tier === "final" ? "node tools/pipeline_validate.mjs" : "node tools/skills_eval.mjs"],
        validation_batch_id: "batch-review",
        validation_check_id: `check-${tier}`,
        validation_tier: tier,
        validation_plan_risk: "medium",
        validation_plan_changes: ["profiling", "pipeline"],
        command_exit_code: 0,
      })}\n`, "utf8");
    }

    const result = run(["tools/ai_profile/review.mjs", profile, "--output", reviewMd, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: join(dir, "scope.json") },
    });
    assert.match(result.stdout, /Validation Batches/);
    assert.match(result.stdout, /batch-review/);
    const review = readJson(reviewJson);
    assert.equal(review.validation_batches.length, 1);
    assert.equal(review.validation_batches[0].batch_id, "batch-review");
    assert.equal(review.validation_batches[0].records, 3);
    assert.equal(review.validation_batches[0].broad_final_commands, 1);
    assert.deepEqual(review.validation_batches[0].changes, ["profiling", "pipeline"]);
    assert.equal(existsSync(reviewMd), true);
  } finally {
    cleanup(dir);
  }
});

test("profile review summarizes tool use", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "tool-use.jsonl");
    const reviewJson = join(dir, "review.json");
    appendFileSync(profile, `${JSON.stringify({
      ts: "2026-06-13T10:00:00+05:00",
      phase: "art_generation",
      category: "art",
      intent: "Generate art candidate",
      result: "pass",
      value: "productive",
      duration_ms: 2000,
      tools: ["imagegen"],
    })}\n`, "utf8");
    appendFileSync(profile, `${JSON.stringify({
      ts: "2026-06-13T10:00:02+05:00",
      phase: "validation",
      category: "validation",
      intent: "Run failing check",
      result: "fail",
      value: "rework",
      duration_ms: 3000,
      tools: ["shell_command"],
      commands: ["node tools/skills_eval.mjs"],
    })}\n`, "utf8");
    appendFileSync(profile, `${JSON.stringify({
      ts: "2026-06-13T10:05:02+05:00",
      event_type: "gap_checkpoint",
      phase: "checkpoint",
      category: "reflection",
      intent: "Capture manual review gap",
      result: "pass",
      value: "necessary_overhead",
      duration_ms: 300000,
      tools: ["ai_profile/gap_checkpoint.mjs"],
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: join(dir, "scope.json") },
    });
    assert.match(result.stdout, /Tool Use Summary/);
    assert.match(result.stdout, /Tool Runtime Summary/);
    assert.match(result.stdout, /Captured Elapsed Summary/);
    assert.match(result.stdout, /imagegen/);
    assert.match(result.stdout, /shell_command/);
    assert.match(result.stdout, /captured elapsed/);
    const review = readJson(reviewJson);
    assert.equal(review.tool_use_summary[0].tool, "ai_profile/gap_checkpoint.mjs");
    assert.equal(review.tool_use_summary[0].duration_kind, "captured_elapsed");
    assert.equal(review.tool_use_summary[0].captured_elapsed_ms, 300000);
    assert.equal(review.tool_use_summary[0].runtime_ms, 0);
    const shellTool = review.tool_use_summary.find((item) => item.tool === "shell_command");
    assert.equal(shellTool.failed, 1);
    assert.equal(shellTool.waste_or_rework, 1);
    assert.equal(shellTool.duration_kind, "runtime");
    assert.equal(shellTool.command_runtime_ms, 3000);
    assert.equal(review.tool_runtime_summary[0].tool, "shell_command");
    assert.equal(review.captured_elapsed_summary[0].tool, "ai_profile/gap_checkpoint.mjs");
    assert.ok(review.tool_use_summary.some((item) => item.tool === "imagegen"));
  } finally {
    cleanup(dir);
  }
});

test("profile review summarizes current scope tool and context use", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "current-scope-use.jsonl");
    const scopeFile = join(dir, "scope.json");
    const reviewJson = join(dir, "review.json");
    writeFileSync(scopeFile, `${JSON.stringify({
      work_item: "T-CURRENT",
      iteration: "tool-context",
      updated_at: "2026-06-13T10:00:00+05:00",
    })}\n`, "utf8");
    appendFileSync(profile, `${JSON.stringify({
      ts: "2026-06-13T09:59:59+05:00",
      phase: "art_generation",
      category: "art",
      intent: "Generate older art before current scope",
      result: "pass",
      value: "productive",
      duration_ms: 99000,
      tools: ["imagegen"],
    })}\n`, "utf8");
    appendFileSync(profile, `${JSON.stringify({
      ts: "2026-06-13T10:00:01+05:00",
      phase: "context",
      category: "context",
      intent: "Load current task context",
      result: "pass",
      value: "necessary_overhead",
      duration_ms: 2000,
      tools: ["ai_profile/context_command.mjs", "shell_command"],
      commands: ["node tools/taskboard/cli.mjs context"],
      context_risk: "medium",
      context_inputs: [{ path: "tasks/STATUS.md", chars: 1234 }],
      work_item: "T-CURRENT",
      iteration: "tool-context",
    })}\n`, "utf8");
    appendFileSync(profile, `${JSON.stringify({
      ts: "2026-06-13T10:00:04+05:00",
      phase: "validation",
      category: "validation",
      intent: "Run current scoped check",
      result: "pass",
      value: "productive",
      duration_ms: 3000,
      tools: ["shell_command"],
      commands: ["node tools/skills_eval.mjs"],
      validation_batch_id: "current-batch",
      validation_check_id: "skills-current",
      validation_tier: "scoped",
      validation_plan_risk: "medium",
      validation_plan_changes: ["profiling"],
      work_item: "T-CURRENT",
      iteration: "tool-context",
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: scopeFile },
    });
    assert.match(result.stdout, /Current Scope Tool Use/);
    assert.match(result.stdout, /Current Scope Context Use/);
    assert.match(result.stdout, /Current Scope Snapshot/);
    assert.match(result.stdout, /Current Scope Validation/);
    assert.match(result.stdout, /current-batch: 1 record\(s\), 3\.0s, pass, risk=medium, changes=profiling, broad\/final=0/);
    assert.match(result.stdout, /profiled\/wall-clock: 5\.0s \/ 5\.0s \(100\.0%\)/);
    assert.match(result.stdout, /telemetry gaps: context=0, work_item=0, tools=0/);
    assert.match(result.stdout, /tasks\/STATUS\.md: 1234 chars/);
    const review = readJson(reviewJson);
    assert.equal(review.current_scope.records, 2);
    assert.equal(review.current_scope.wall_clock_coverage.merged_profiled_ms, 5000);
    assert.equal(review.current_scope.wall_clock_coverage.wall_clock_span_ms, 5000);
    assert.equal(review.current_scope.tool_use_summary[0].tool, "shell_command");
    assert.equal(review.current_scope.tool_use_summary[0].records, 2);
    assert.equal(review.current_scope.context_use_summary.hotspots[0].path, "tasks/STATUS.md");
    assert.equal(review.current_scope.context_use_summary.hotspots[0].chars, 1234);
    assert.equal(review.current_scope.validation_batches[0].batch_id, "current-batch");
    assert.equal(review.current_scope.validation_batches[0].records, 1);
    assert.equal(review.current_scope.tool_use_summary.some((item) => item.tool === "imagegen"), false);
  } finally {
    cleanup(dir);
  }
});

test("profile review and followups flag missing tool metadata", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "missing-tools.jsonl");
    const reviewJson = join(dir, "review.json");
    const followupsJson = join(dir, "followups.json");
    for (let index = 0; index < 20; index += 1) {
      appendFileSync(profile, `${JSON.stringify({
        ts: `2026-06-13T10:00:${String(index).padStart(2, "0")}+05:00`,
        phase: "validation",
        category: "validation",
        intent: `tool metadata ${index}`,
        result: "pass",
        value: "productive",
        duration_ms: 100,
        ...(index === 0 ? {} : { tools: ["shell_command"] }),
      })}\n`, "utf8");
    }

    const result = run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: join(dir, "scope.json") },
    });
    assert.match(result.stdout, /Missing Tool Metadata/);
    assert.match(result.stdout, /tool metadata 0/);
    const review = readJson(reviewJson);
    assert.ok(review.findings.some((finding) => finding.type === "missing_tool_metadata"));
    assert.equal(review.missing_tool_records.length, 1);
    assert.ok(review.suggested_pipeline_actions.some((action) => action.includes("populate `tools`")));

    run(["tools/ai_profile/followups.mjs", reviewJson, "--json-output", followupsJson]);
    const followups = readJson(followupsJson);
    assert.ok(followups.suggestions.some((suggestion) => suggestion.source === "missing_tool_metadata"));
  } finally {
    cleanup(dir);
  }
});

test("profile review separates batched and unbatched broad final repeats", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "broad-final-classification.jsonl");
    const reviewJson = join(dir, "review.json");
    const command = "node tools/pipeline_validate.mjs";
    for (const [index, batchId] of ["batch-a", "batch-b", ""].entries()) {
      appendFileSync(profile, `${JSON.stringify({
        ts: `2026-06-13T10:00:0${index}+05:00`,
        phase: "validation",
        category: "validation",
        intent: `pipeline validation ${index}`,
        result: "pass",
        value: "necessary_overhead",
        duration_ms: 1000,
        commands: [command],
        ...(batchId ? {
          validation_batch_id: batchId,
          validation_check_id: "portable-pipeline",
          validation_tier: "final",
          validation_plan_risk: "medium",
          validation_plan_changes: ["pipeline"],
        } : {}),
      })}\n`, "utf8");
    }
    appendFileSync(profile, `${JSON.stringify({
      ts: "2026-06-13T10:00:03+05:00",
      phase: "validation",
      category: "validation",
      intent: "pipeline validation unbatched second",
      result: "pass",
      value: "necessary_overhead",
      duration_ms: 1000,
      commands: [command],
    })}\n`, "utf8");

    run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: join(dir, "scope.json") },
    });
    const review = readJson(reviewJson);
    assert.equal(review.repeated_broad_final_commands[0].count, 4);
    assert.equal(review.batched_broad_final_commands[0].count, 2);
    assert.equal(review.repeated_unbatched_broad_final_commands[0].count, 2);
    assert.equal(review.repeated_unbatched_broad_final_occurrences, 2);
    assert.match(review.findings.find((finding) => finding.type === "repeated_broad_final").message, /2 unbatched broad\/final occurrence/);
    assert.equal(review.repeated_command_classification[0].classification, "validation_waste_risk");
    assert.equal(review.repeated_command_classification[0].batched, 2);
    assert.equal(review.repeated_command_classification[0].unbatched, 2);
    assert.ok(review.findings.some((finding) => finding.type === "repeated_broad_final"));
  } finally {
    cleanup(dir);
  }
});

test("profile review does not flag repeated broad final when repeats are batched", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "batched-only-broad-final.jsonl");
    const reviewJson = join(dir, "review.json");
    const command = "node tools/pipeline_validate.mjs";
    for (const [index, batchId] of ["batch-a", "batch-b"].entries()) {
      appendFileSync(profile, `${JSON.stringify({
        ts: `2026-06-13T10:00:0${index}+05:00`,
        phase: "validation",
        category: "validation",
        intent: `batched final ${index}`,
        result: "pass",
        value: "necessary_overhead",
        duration_ms: 1000,
        commands: [command],
        validation_batch_id: batchId,
        validation_check_id: "portable-pipeline",
        validation_tier: "final",
        validation_plan_risk: "medium",
        validation_plan_changes: ["pipeline"],
      })}\n`, "utf8");
    }

    run(["tools/ai_profile/review.mjs", profile, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: join(dir, "scope.json") },
    });
    const review = readJson(reviewJson);
    assert.equal(review.repeated_broad_final_commands[0].count, 2);
    assert.equal(review.batched_broad_final_commands[0].count, 2);
    assert.deepEqual(review.repeated_unbatched_broad_final_commands, []);
    assert.equal(review.repeated_command_classification[0].classification, "planned_validation");
    assert.equal(review.findings.some((finding) => finding.type === "repeated_broad_final"), false);
  } finally {
    cleanup(dir);
  }
});

test("validation runner dry run writes summary without profile records", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "validation-dry-run.jsonl");
    const planJson = join(dir, "plan.json");
    const summaryJson = join(dir, "summary.json");
    const passCommand = `${JSON.stringify(process.execPath)} -e ${JSON.stringify("process.exit(0)")}`;
    writeFileSync(planJson, `${JSON.stringify({
      schema_version: 1,
      risk: "low",
      changes: ["docs"],
      checks: [
        { id: "dry-pass", tier: "preflight", command: passCommand, why: "dry pass", broad: false },
        { id: "placeholder", tier: "scoped", command: "<fill me>", why: "placeholder", broad: false, placeholder: true },
      ],
    })}\n`, "utf8");

    const result = run([
      "tools/ai_profile/validation_run.mjs",
      "--plan", planJson,
      "--profile", profile,
      "--dry-run",
      "--json-output", summaryJson,
    ]);
    assert.match(result.stdout, /Executed: 0/);
    const summary = readJson(summaryJson);
    assert.equal(summary.dry_run, true);
    assert.equal(summary.executed.length, 0);
    assert.equal(summary.skipped.length, 2);
    assert.equal(existsSync(profile), false);
  } finally {
    cleanup(dir);
  }
});

test("compare review baseline reports improvement", () => {
  const dir = tempDir();
  try {
    const baseline = join(dir, "baseline.review.json");
    const current = join(dir, "current.review.json");
    const compareJson = join(dir, "compare.json");
    writeFileSync(baseline, `${JSON.stringify({
      schema_version: 1,
      profile: "baseline.jsonl",
      findings: [{ type: "missing_context_inputs" }],
      missing_context_inputs: [{ line: 1 }],
      repeated_broad_final_commands: [],
      recovered_failed_records: [],
      unresolved_failed_records: [],
      current_scope: {
        enabled: true,
        records: 2,
        findings: [{ type: "current_missing_context_inputs" }],
        missing_context_inputs: 1,
        missing_work_item_records: 0,
        repeated_broad_final_commands: [],
        recovered_failed_records: [],
        unresolved_failed_records: [],
        low_profile_coverage: false,
      },
    })}\n`, "utf8");
    writeFileSync(current, `${JSON.stringify({
      schema_version: 1,
      profile: "current.jsonl",
      findings: [],
      missing_context_inputs: [],
      repeated_broad_final_commands: [],
      recovered_failed_records: [],
      unresolved_failed_records: [],
      current_scope: {
        enabled: true,
        records: 2,
        findings: [],
        missing_context_inputs: 0,
        missing_work_item_records: 0,
        repeated_broad_final_commands: [],
        recovered_failed_records: [],
        unresolved_failed_records: [],
        low_profile_coverage: false,
      },
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/compare_reviews.mjs", baseline, current, "--json-output", compareJson]);
    assert.match(result.stdout, /Verdict: improved/);
    const compare = readJson(compareJson);
    assert.equal(compare.verdict, "improved");
    assert.equal(compare.current_regressions.length, 0);
    assert.ok(compare.improvements.some((item) => item.key === "current_missing_context_inputs"));
  } finally {
    cleanup(dir);
  }
});

test("compare review baseline fails on current-scope regression", () => {
  const dir = tempDir();
  try {
    const baseline = join(dir, "baseline.review.json");
    const current = join(dir, "current.review.json");
    const compareJson = join(dir, "compare.json");
    writeFileSync(baseline, `${JSON.stringify({
      schema_version: 1,
      profile: "baseline.jsonl",
      findings: [],
      missing_context_inputs: [],
      repeated_broad_final_commands: [],
      recovered_failed_records: [],
      unresolved_failed_records: [],
      current_scope: {
        enabled: true,
        records: 2,
        findings: [],
        missing_context_inputs: 0,
        missing_work_item_records: 0,
        repeated_broad_final_commands: [],
        recovered_failed_records: [],
        unresolved_failed_records: [],
        low_profile_coverage: false,
      },
    })}\n`, "utf8");
    writeFileSync(current, `${JSON.stringify({
      schema_version: 1,
      profile: "current.jsonl",
      findings: [{ type: "missing_context_inputs" }],
      missing_context_inputs: [{ line: 2 }],
      repeated_broad_final_commands: [],
      recovered_failed_records: [],
      unresolved_failed_records: [],
      current_scope: {
        enabled: true,
        records: 2,
        findings: [{ type: "current_missing_context_inputs" }],
        missing_context_inputs: 2,
        missing_work_item_records: 0,
        repeated_broad_final_commands: [],
        recovered_failed_records: [],
        unresolved_failed_records: [],
        low_profile_coverage: false,
      },
    })}\n`, "utf8");

    const result = runRaw([
      "tools/ai_profile/compare_reviews.mjs",
      baseline,
      current,
      "--json-output",
      compareJson,
      "--fail-on-regression",
    ]);
    assert.equal(result.status, 1, result.stdout);
    assert.match(result.stdout, /Verdict: regressed/);
    const compare = readJson(compareJson);
    assert.equal(compare.verdict, "regressed");
    assert.ok(compare.current_regressions.some((item) => item.key === "current_missing_context_inputs"));
  } finally {
    cleanup(dir);
  }
});

test("compare review baseline ignores batched broad final repeats", () => {
  const dir = tempDir();
  try {
    const baseline = join(dir, "baseline.review.json");
    const current = join(dir, "current.review.json");
    const compareJson = join(dir, "compare.json");
    const baseReview = {
      schema_version: 1,
      profile: "baseline.jsonl",
      findings: [],
      missing_context_inputs: [],
      repeated_broad_final_commands: [],
      repeated_unbatched_broad_final_commands: [],
      batched_broad_final_commands: [],
      recovered_failed_records: [],
      unresolved_failed_records: [],
      current_scope: {
        enabled: true,
        records: 2,
        findings: [],
        missing_context_inputs: 0,
        missing_work_item_records: 0,
        repeated_broad_final_commands: [],
        repeated_unbatched_broad_final_commands: [],
        recovered_failed_records: [],
        unresolved_failed_records: [],
        low_profile_coverage: false,
      },
    };
    const currentReview = {
      ...baseReview,
      profile: "current.jsonl",
      repeated_broad_final_commands: [{ command: "node tools/pipeline_validate.mjs", count: 2 }],
      repeated_unbatched_broad_final_commands: [],
      batched_broad_final_commands: [{ command: "node tools/pipeline_validate.mjs", count: 2 }],
      current_scope: {
        ...baseReview.current_scope,
        records: 4,
        repeated_broad_final_commands: [{ command: "node tools/pipeline_validate.mjs", count: 2 }],
        repeated_unbatched_broad_final_commands: [],
      },
    };
    writeFileSync(baseline, `${JSON.stringify(baseReview)}\n`, "utf8");
    writeFileSync(current, `${JSON.stringify(currentReview)}\n`, "utf8");

    const result = run(["tools/ai_profile/compare_reviews.mjs", baseline, current, "--json-output", compareJson]);
    assert.match(result.stdout, /Verdict: stable/);
    const compare = readJson(compareJson);
    assert.equal(compare.verdict, "stable");
    assert.equal(compare.current_regressions.length, 0);
    assert.equal(compare.deltas.find((item) => item.key === "current_repeated_broad_final_commands").current, 0);
  } finally {
    cleanup(dir);
  }
});

test("capture baseline writes stable review copy and manifest", () => {
  const dir = tempDir();
  try {
    const review = join(dir, "clean.review.json");
    const baseline = join(dir, "baseline.review.json");
    const manifest = join(dir, "baseline.manifest.json");
    writeFileSync(review, `${JSON.stringify({
      schema_version: 1,
      profile: "clean.jsonl",
      records: 12,
      findings: [],
      current_scope: {
        enabled: true,
        records: 3,
        findings: [],
        missing_context_inputs: 0,
        missing_work_item_records: 0,
        repeated_broad_final_commands: [],
        recovered_failed_records: [],
        unresolved_failed_records: [],
      },
    })}\n`, "utf8");

    const result = run([
      "tools/ai_profile/capture_baseline.mjs",
      review,
      "--label",
      "Clean Baseline",
      "--output",
      baseline,
      "--manifest",
      manifest,
    ]);
    assert.match(result.stdout, /Label: clean-baseline/);
    assert.deepEqual(readJson(baseline).profile, "clean.jsonl");
    const captured = readJson(manifest);
    assert.equal(captured.label, "clean-baseline");
    assert.equal(captured.baseline_review, resolve(baseline));
    assert.equal(captured.summary.current_scope_findings, 0);
    assert.match(captured.compare_command, /compare_reviews\.mjs/);
  } finally {
    cleanup(dir);
  }
});

test("capture baseline refuses overwrite unless forced", () => {
  const dir = tempDir();
  try {
    const review = join(dir, "clean.review.json");
    const baseline = join(dir, "baseline.review.json");
    const manifest = join(dir, "baseline.manifest.json");
    writeFileSync(review, `${JSON.stringify({
      schema_version: 1,
      profile: "clean.jsonl",
      records: 1,
      findings: [],
      current_scope: { enabled: true, records: 1, findings: [] },
    })}\n`, "utf8");
    run([
      "tools/ai_profile/capture_baseline.mjs",
      review,
      "--label",
      "guard",
      "--output",
      baseline,
      "--manifest",
      manifest,
    ]);

    const refused = runRaw([
      "tools/ai_profile/capture_baseline.mjs",
      review,
      "--label",
      "guard",
      "--output",
      baseline,
      "--manifest",
      manifest,
    ]);
    assert.equal(refused.status, 1);
    assert.match(refused.stderr, /refused to overwrite/);

    run([
      "tools/ai_profile/capture_baseline.mjs",
      review,
      "--label",
      "guard",
      "--output",
      baseline,
      "--manifest",
      manifest,
      "--force",
    ]);
  } finally {
    cleanup(dir);
  }
});

test("reflection packet summarizes clean profile artifacts", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "packet.jsonl");
    const review = join(dir, "packet.review.json");
    const followups = join(dir, "packet.followups.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compare = join(dir, "clean.compare.json");
    const packetJson = join(dir, "packet.out.json");

    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(profile, "", "utf8");
    writeFileSync(review, `${JSON.stringify({
      current_scope: {
        enabled: true,
        records: 2,
        findings: [],
        suggested_actions: ["Use current scope as clean baseline."],
      },
    })}\n`, "utf8");
    writeFileSync(followups, `${JSON.stringify({
      suggestions: [{ priority: "P3", title: "Use clean AI profile as baseline", next_action: "Compare later." }],
      suppressed_historical_findings: ["low_profile_coverage"],
    })}\n`, "utf8");
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({
      label: "clean",
      captured_at: "2026-06-13T10:05:00+05:00",
      baseline_review: resolve(baselineReview),
    })}\n`, "utf8");
    writeFileSync(compare, `${JSON.stringify({
      verdict: "stable",
      current_regressions: [],
      improvements: [],
      historical_regressions: [],
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/reflection_packet.mjs", profile, "--json-output", packetJson]);
    assert.match(result.stdout, /Readiness: ready/);
    assert.match(result.stdout, /Baseline comparison: stable/);
    const packet = readJson(packetJson);
    assert.deepEqual(packet.readiness, ["ready"]);
    assert.equal(packet.current_scope.findings.length, 0);
    assert.equal(packet.followups.suggestions.length, 1);
    assert.equal(packet.followups.pending_suggestions.length, 0);
    assert.equal(packet.followups.satisfied_suggestions.length, 1);
    assert.equal(packet.comparison.verdict, "stable");
    assert.equal(packet.comparison.current_regressions.length, 0);
  } finally {
    cleanup(dir);
  }
});

test("reflection packet keeps baseline follow-up pending when comparison is missing", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "packet-pending.jsonl");
    const review = join(dir, "packet-pending.review.json");
    const followups = join(dir, "packet-pending.followups.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const packetJson = join(dir, "packet.out.json");

    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(profile, "", "utf8");
    writeFileSync(review, `${JSON.stringify({ current_scope: { enabled: true, records: 2, findings: [] } })}\n`, "utf8");
    writeFileSync(followups, `${JSON.stringify({
      suggestions: [{ priority: "P3", title: "Use clean AI profile as baseline", source: "clean_profile", next_action: "Compare later." }],
      suppressed_historical_findings: [],
    })}\n`, "utf8");
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({
      label: "clean",
      captured_at: "2026-06-13T10:05:00+05:00",
      baseline_review: resolve(baselineReview),
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/reflection_packet.mjs", profile, "--json-output", packetJson]);
    assert.match(result.stdout, /Readiness: comparison_missing/);
    assert.match(result.stdout, /Pending Follow-ups/);
    const packet = readJson(packetJson);
    assert.equal(packet.followups.pending_suggestions.length, 1);
    assert.equal(packet.followups.satisfied_suggestions.length, 0);
  } finally {
    cleanup(dir);
  }
});

test("reflection packet flags current-scope comparison regressions", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "packet-regressed.jsonl");
    const review = join(dir, "packet-regressed.review.json");
    const followups = join(dir, "packet-regressed.followups.json");
    const baselineDir = join(dir, "baselines");
    const baselineReview = join(baselineDir, "clean.review.json");
    const manifest = join(baselineDir, "clean.manifest.json");
    const compare = join(dir, "clean.compare.json");
    const packetJson = join(dir, "packet.out.json");

    mkdirSync(baselineDir, { recursive: true });
    writeFileSync(profile, "", "utf8");
    writeFileSync(review, `${JSON.stringify({ current_scope: { enabled: true, records: 2, findings: [] } })}\n`, "utf8");
    writeFileSync(followups, `${JSON.stringify({ suggestions: [], suppressed_historical_findings: [] })}\n`, "utf8");
    writeFileSync(baselineReview, "{}\n", "utf8");
    writeFileSync(manifest, `${JSON.stringify({
      label: "clean",
      captured_at: "2026-06-13T10:05:00+05:00",
      baseline_review: resolve(baselineReview),
    })}\n`, "utf8");
    writeFileSync(compare, `${JSON.stringify({
      verdict: "regressed",
      current_regressions: [{ key: "current_missing_context_inputs", label: "Current missing context", baseline: 0, current: 1 }],
      improvements: [],
      historical_regressions: [],
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/reflection_packet.mjs", profile, "--json-output", packetJson]);
    assert.match(result.stdout, /Readiness: current_regressions/);
    assert.match(result.stdout, /regression: Current missing context 0 -> 1/);
    const packet = readJson(packetJson);
    assert.ok(packet.readiness.includes("current_regressions"));
    assert.equal(packet.comparison.current_regressions.length, 1);
  } finally {
    cleanup(dir);
  }
});

test("reflection draft summarizes clean packet artifacts", () => {
  const dir = tempDir();
  try {
    const review = join(dir, "clean.review.json");
    const packetJson = join(dir, "clean.packet.json");
    const draftJson = join(dir, "clean.draft.json");
    const draftMd = join(dir, "clean.draft.md");

    writeFileSync(review, `${JSON.stringify({
      findings: [{ type: "low_profile_coverage", message: "Profile coverage was low before the clean current scope." }],
      current_scope: { enabled: true, findings: [], suggested_actions: [] },
      suggested_pipeline_actions: ["Use node tools/ai.mjs checkpoint for long manual stretches."],
    })}\n`, "utf8");
    writeFileSync(packetJson, `${JSON.stringify({
      profile: "clean.jsonl",
      artifacts: { review_json: review, comparison_json: join(dir, "clean.compare.json") },
      readiness: ["ready"],
      current_scope: { findings: [], suggested_actions: [] },
      followups: {
        pending_suggestions: [],
        satisfied_suggestions: [{ priority: "P3", title: "Use clean AI profile as baseline", packet_reason: "baseline comparison is stable" }],
        suppressed_historical_findings: ["low_profile_coverage"],
      },
      comparison: { verdict: "stable", current_regressions: [] },
    })}\n`, "utf8");

    const result = run([
      "tools/ai_profile/reflection_draft.mjs",
      packetJson,
      "--output",
      draftMd,
      "--json-output",
      draftJson,
    ]);
    assert.match(result.stdout, /Draft status: generated starter/);
    assert.match(result.stdout, /Current reflection state is clean/);
    assert.match(result.stdout, /low_profile_coverage/);
    assert.equal(existsSync(draftMd), true);
    const draft = readJson(draftJson);
    assert.equal(draft.current_state.pending_followups.length, 0);
    assert.equal(draft.current_state.satisfied_followups.length, 1);
    assert.equal(draft.historical_lessons[0].type, "low_profile_coverage");
    assert.equal(draft.review, review);
  } finally {
    cleanup(dir);
  }
});

test("reflection draft keeps pending followups and regressions visible", () => {
  const dir = tempDir();
  try {
    const review = join(dir, "regressed.review.json");
    const packetJson = join(dir, "regressed.packet.json");
    const draftJson = join(dir, "regressed.draft.json");

    writeFileSync(review, `${JSON.stringify({ findings: [] })}\n`, "utf8");
    writeFileSync(packetJson, `${JSON.stringify({
      profile: "regressed.jsonl",
      artifacts: { review_json: review },
      readiness: ["current_regressions"],
      current_scope: {
        findings: [{ type: "current_missing_context_inputs", message: "Current scope has unmeasured context reads." }],
        suggested_actions: ["Use node tools/ai.mjs context --path <file> for the next context-heavy read."],
      },
      followups: {
        pending_suggestions: [{ priority: "P1", title: "Fix current context capture", next_action: "Run node tools/ai.mjs context -- <command> for context commands." }],
        satisfied_suggestions: [],
        suppressed_historical_findings: [],
      },
      comparison: {
        verdict: "regressed",
        current_regressions: [{ key: "current_missing_context_inputs", label: "Current missing context", baseline: 0, current: 1 }],
      },
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/reflection_draft.mjs", packetJson, "--json-output", draftJson]);
    assert.match(result.stdout, /Current-scope regressions: 1/);
    assert.match(result.stdout, /pending \[P1\] Fix current context capture/);
    assert.match(result.stdout, /regression: Current missing context/);
    const draft = readJson(draftJson);
    assert.equal(draft.current_state.current_scope_findings.length, 1);
    assert.equal(draft.current_state.current_regressions.length, 1);
    assert.equal(draft.current_state.pending_followups.length, 1);
    assert.ok(draft.next_cycle_actions.some((action) => action.includes("current-scope regressions")));
  } finally {
    cleanup(dir);
  }
});

test("reflection draft classifies repeated command evidence by scope", () => {
  const dir = tempDir();
  try {
    const review = join(dir, "repeated.review.json");
    const packetJson = join(dir, "repeated.packet.json");
    const draftJson = join(dir, "repeated.draft.json");

    writeFileSync(review, `${JSON.stringify({
      findings: [{ type: "repeated_commands", message: "3 repeated command(s) may need batching or narrower gates." }],
      repeated_commands: [
        { command: "node tools/pipeline_validate.mjs", count: 4, scope: "broad/final" },
        { command: "git diff --check", count: 3, scope: "preflight" },
        { command: "node tools/skills_eval.mjs", count: 2, scope: "scoped" },
      ],
      repeated_commands_by_scope: [
        { scope: "broad/final", count: 4 },
        { scope: "preflight", count: 3 },
        { scope: "scoped", count: 2 },
      ],
      repeated_command_classification: [
        { command: "node tools/pipeline_validate.mjs", count: 4, scope: "broad/final", classification: "validation_waste_risk", reason: "Broad/final command repeated outside a validation batch.", next_action: "Use ai validation facade.", batched: 2, unbatched: 2, failed: 0 },
        { command: "git diff --check", count: 3, scope: "preflight", classification: "guardrail_rerun_review", reason: "Preflight guardrail rerun.", next_action: "Keep only after fresh edits.", batched: 0, unbatched: 3, failed: 0 },
      ],
      repeated_broad_final_commands: [
        { command: "node tools/pipeline_validate.mjs", count: 4, scope: "broad/final" },
      ],
      repeated_unbatched_broad_final_commands: [
        { command: "node tools/pipeline_validate.mjs", count: 2, scope: "broad/final" },
      ],
      repeated_unbatched_broad_final_occurrences: 2,
      batched_broad_final_commands: [
        { command: "node tools/pipeline_validate.mjs", count: 2, scope: "broad/final" },
      ],
      repeated_broad_final_by_work_item: [
        { work_item: "T0099", command: "node tools/pipeline_validate.mjs", count: 2 },
      ],
      validation_batches: [
        { batch_id: "batch-draft", records: 4, duration_ms: 12000, failed: 0, broad_final_commands: 1, risk: "medium", changes: ["profiling"] },
      ],
      tool_use_summary: [
        { tool: "shell_command", records: 5, duration_ms: 9000, failed: 1, waste_or_rework: 1, contexts: 0, commands: 5 },
        { tool: "imagegen", records: 1, duration_ms: 3000, failed: 0, waste_or_rework: 0, contexts: 0, commands: 0 },
      ],
      context_hotspots: [
        { path: "AI_PIPELINE_SESSION_PROFILING.md", chars: 27455 },
        { path: "command:node tools/taskboard/cli.mjs context", chars: 36514 },
      ],
      missing_context_inputs: [
        { line: 12, intent: "Validate portable AI pipeline with profile wrappers", context_risk: "medium" },
      ],
      current_scope: {
        enabled: true,
        work_item: "T0099",
        iteration: "reflection-slice",
        records: 2,
        missing_context_inputs: 0,
        missing_work_item_records: 0,
        missing_tool_records: 0,
        recovered_failed_records: [],
        unresolved_failed_records: [],
        wall_clock_coverage: {
          merged_profiled_ms: 5000,
          wall_clock_span_ms: 305000,
          coverage_ratio: 5000 / 305000,
          largest_gaps: [
            { start_ts: "2026-06-13T05:00:00.000Z", end_ts: "2026-06-13T05:05:00.000Z", duration_ms: 300000 },
          ],
        },
        tool_use_summary: [
          { tool: "shell_command", records: 2, duration_ms: 5000, failed: 0, waste_or_rework: 0, contexts: 1, commands: 2 },
        ],
        context_use_summary: {
          hotspots: [{ path: "tasks/STATUS.md", chars: 1234 }],
          high_context: [],
          missing_inputs: [],
        },
        validation_batches: [
          { batch_id: "current-batch", records: 1, duration_ms: 3000, failed: 0, broad_final_commands: 0, risk: "medium", changes: ["profiling"] },
        ],
      },
    })}\n`, "utf8");
    writeFileSync(packetJson, `${JSON.stringify({
      profile: "repeated.jsonl",
      artifacts: { review_json: review },
      readiness: ["ready"],
      current_scope: { findings: [], suggested_actions: [] },
      followups: { pending_suggestions: [], satisfied_suggestions: [], suppressed_historical_findings: [] },
      comparison: { verdict: "stable", current_regressions: [] },
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/reflection_draft.mjs", packetJson, "--json-output", draftJson]);
    assert.match(result.stdout, /Repeated Command Evidence/);
    assert.match(result.stdout, /Tool Use Summary/);
    assert.match(result.stdout, /shell_command/);
    assert.match(result.stdout, /Current Scope Snapshot/);
    assert.match(result.stdout, /profiled\/wall-clock: 5\.0s \/ 5\.1m \(1\.6%\)/);
    assert.match(result.stdout, /largest gaps/);
    assert.match(result.stdout, /5\.0m from 2026-06-13T05:00:00\.000Z to 2026-06-13T05:05:00\.000Z/);
    assert.match(result.stdout, /Current Scope Tool Use/);
    assert.match(result.stdout, /Current Scope Context Use/);
    assert.match(result.stdout, /Current Scope Validation/);
    assert.match(result.stdout, /current-batch: 1 record\(s\), 3\.0s, pass, risk=medium, changes=profiling, broad\/final=0/);
    assert.match(result.stdout, /tasks\/STATUS\.md/);
    assert.match(result.stdout, /Context Use Evidence/);
    assert.match(result.stdout, /AI_PIPELINE_SESSION_PROFILING\.md/);
    assert.match(result.stdout, /classification/);
    assert.match(result.stdout, /validation_waste_risk/);
    assert.match(result.stdout, /unbatched broad\/final repeated: 2 occurrence/);
    assert.match(result.stdout, /broad\/final: 4/);
    assert.match(result.stdout, /preflight: 3/);
    assert.match(result.stdout, /scoped: 2/);
    assert.match(result.stdout, /node tools\/pipeline_validate\.mjs/);
    assert.match(result.stdout, /validation batches/);
    assert.match(result.stdout, /batch-draft/);
    assert.doesNotMatch(result.stdout, /Cause needs human review/);
    const draft = readJson(draftJson);
    assert.equal(draft.repeated_commands.total_distinct, 3);
    assert.equal(draft.current_state.current_scope_snapshot.records, 2);
    assert.equal(draft.current_state.current_scope_snapshot.coverage_ratio, 5000 / 305000);
    assert.equal(draft.current_state.current_scope_snapshot.largest_gaps[0].duration_ms, 300000);
    assert.equal(draft.tool_use_summary[0].tool, "shell_command");
    assert.equal(draft.current_state.current_scope_tool_use_summary[0].tool, "shell_command");
    assert.equal(draft.current_state.current_scope_context_use_summary.hotspots[0].path, "tasks/STATUS.md");
    assert.equal(draft.current_state.current_scope_validation_batches[0].batch_id, "current-batch");
    assert.equal(draft.context_use_summary.hotspots[0].path, "AI_PIPELINE_SESSION_PROFILING.md");
    assert.equal(draft.context_use_summary.missing_inputs[0].line, 12);
    assert.equal(draft.repeated_commands.classification[0].classification, "validation_waste_risk");
    assert.equal(draft.repeated_commands.unbatched_broad_final_occurrences, 2);
    assert.equal(draft.repeated_commands.broad_final_commands.length, 1);
    assert.equal(draft.repeated_commands.unbatched_broad_final_commands[0].count, 2);
    assert.equal(draft.repeated_commands.batched_broad_final_commands[0].count, 2);
    assert.equal(draft.repeated_commands.broad_final_by_work_item[0].work_item, "T0099");
    assert.equal(draft.repeated_commands.validation_batches[0].batch_id, "batch-draft");
    assert.match(draft.historical_lessons[0].cause, /generated classification/);
  } finally {
    cleanup(dir);
  }
});

test("reflection review separates clean current scope from historical lessons", () => {
  const dir = tempDir();
  try {
    const draftJson = join(dir, "clean.draft.json");
    const reviewJson = join(dir, "clean.review.json");
    const reviewMd = join(dir, "clean.review.md");

    writeFileSync(draftJson, `${JSON.stringify({
      historical_lessons: [
        { type: "missing_context_inputs", symptom: "Missing context inputs.", cause: "Unmeasured reads.", fix: "Use node tools/ai.mjs context --path <file>." },
        { type: "repeated_commands", symptom: "Repeated commands.", cause: "Reruns.", fix: "Classify reruns." },
        { type: "repeated_broad_final", symptom: "Repeated final gates.", cause: "Unbatched.", fix: "Use ai facade validation." },
        { type: "missing_work_item_metadata", symptom: "Missing work item.", cause: "Wide scope.", fix: "Use ai facade focus." },
      ],
      suppressed_historical_findings: ["missing_context_inputs"],
      repeated_commands: {
        by_scope: [{ scope: "scoped", count: 3 }, { scope: "broad/final", count: 1 }],
        classification: [{ command: "node tools/skills_eval.mjs", count: 3, scope: "scoped", classification: "guardrail_rerun_review" }],
        validation_batches: [{ batch_id: "batch-review", records: 5, broad_final_commands: 1, failed: 0 }],
        unbatched_broad_final_occurrences: 2,
      },
      tool_use_summary: [
        { tool: "ai_profile/gap_checkpoint.mjs", records: 1, duration_ms: 300000, captured_elapsed_ms: 300000, runtime_ms: 0, duration_kind: "captured_elapsed", failed: 0, waste_or_rework: 0 },
        { tool: "shell_command", records: 5, duration_ms: 9000, runtime_ms: 9000, command_runtime_ms: 9000, duration_kind: "runtime", failed: 1, waste_or_rework: 1 },
      ],
      context_use_summary: {
        hotspots: [{ path: "AI_PIPELINE_SESSION_PROFILING.md", chars: 27455 }],
        missing_inputs: [{ line: 12, intent: "Validate portable AI pipeline with profile wrappers", context_risk: "medium" }],
      },
      current_state: {
        current_scope_findings: [],
        current_regressions: [],
        pending_followups: [],
        satisfied_followups: [{ title: "Baseline captured" }],
        current_scope_snapshot: {
          enabled: true,
          work_item: "T0099",
          iteration: "reflection-slice",
          records: 2,
          profiled_ms: 5000,
          wall_clock_ms: 5000,
          coverage_ratio: 1,
          missing_context_inputs: 0,
          missing_work_item_records: 0,
          missing_tool_records: 0,
          recovered_failed_records: 0,
          unresolved_failed_records: 0,
        },
        current_scope_tool_use_summary: [
          { tool: "ai_profile/gap_checkpoint.mjs", records: 1, duration_ms: 300000, captured_elapsed_ms: 300000, runtime_ms: 0, duration_kind: "captured_elapsed", failed: 0, waste_or_rework: 0 },
          { tool: "shell_command", records: 2, duration_ms: 5000, runtime_ms: 5000, command_runtime_ms: 5000, duration_kind: "runtime", failed: 0, waste_or_rework: 0 },
        ],
        current_scope_context_use_summary: {
          hotspots: [{ path: "tasks/STATUS.md", chars: 1234 }],
          high_context: [],
          missing_inputs: [],
        },
        current_scope_validation_batches: [
          { batch_id: "current-batch", records: 1, duration_ms: 3000, failed: 0, broad_final_commands: 0, risk: "medium", changes: ["profiling"] },
        ],
      },
    })}\n`, "utf8");

    const result = run([
      "tools/ai_profile/reflection_review.mjs",
      draftJson,
      "--output",
      reviewMd,
      "--json-output",
      reviewJson,
    ]);
    assert.match(result.stdout, /Verdict: current_clean/);
    assert.match(result.stdout, /Current actions: 0/);
    assert.match(result.stdout, /historical_only/);
    assert.match(result.stdout, /No current action items/);
    assert.match(result.stdout, /Current Scope Readout/);
    assert.match(result.stdout, /Current scope T0099\/reflection-slice is clean: 2 record\(s\), 5\.0s profiled over 5\.0s wall-clock \(100\.0%\)/);
    assert.match(result.stdout, /Current coverage confidence is usable for rough time-spend claims/);
    assert.match(result.stdout, /Current telemetry has no context, work-item, or tool metadata gaps/);
    assert.match(result.stdout, /Largest current tool runtime: shell_command \(5\.0s, 2 record\(s\)\)/);
    assert.match(result.stdout, /Largest current captured elapsed checkpoint: ai_profile\/gap_checkpoint\.mjs \(5\.0m, 1 record\(s\)\)/);
    assert.match(result.stdout, /Current validation was batched: 1 batch\(es\), 1 record\(s\), broad\/final=0, failed=0/);
    assert.match(result.stdout, /Current Scope Snapshot/);
    assert.match(result.stdout, /profiled\/wall-clock: 5\.0s \/ 5\.0s \(100\.0%\)/);
    assert.match(result.stdout, /Current Scope Tool Use/);
    assert.match(result.stdout, /total current runtime: 5\.0s/);
    assert.match(result.stdout, /total current captured elapsed: 5\.0m/);
    assert.match(result.stdout, /ai_profile\/gap_checkpoint\.mjs: 1 record\(s\), 5\.0m captured elapsed, share=100\.0%/);
    assert.match(result.stdout, /shell_command: 2 record\(s\), 5\.0s command\/runtime, share=100\.0%/);
    assert.match(result.stdout, /Current Scope Context Use/);
    assert.match(result.stdout, /total hotspot chars: 1234/);
    assert.match(result.stdout, /tasks\/STATUS\.md: 1234 chars, share=100\.0%/);
    assert.match(result.stdout, /Current Scope Validation/);
    assert.match(result.stdout, /current-batch: 1 record\(s\), 3\.0s, pass, risk=medium, changes=profiling, broad\/final=0/);
    assert.match(result.stdout, /tasks\/STATUS\.md/);
    assert.match(result.stdout, /Tool Runtime Review/);
    assert.match(result.stdout, /Captured Elapsed Review/);
    assert.match(result.stdout, /shell_command/);
    assert.match(result.stdout, /total runtime: 9\.0s/);
    assert.match(result.stdout, /shell_command: 5 record\(s\), 9\.0s command\/runtime, share=100\.0%/);
    assert.match(result.stdout, /total captured elapsed: 5\.0m/);
    assert.match(result.stdout, /ai_profile\/gap_checkpoint\.mjs: 1 record\(s\), 5\.0m captured elapsed, share=100\.0%/);
    assert.match(result.stdout, /Context Use Review/);
    assert.match(result.stdout, /total hotspot chars: 27455/);
    assert.match(result.stdout, /AI_PIPELINE_SESSION_PROFILING\.md: 27455 chars, share=100\.0%/);
    assert.match(result.stdout, /guardrail_rerun_review/);
    assert.match(result.stdout, /total repeated occurrences: 4/);
    assert.match(result.stdout, /scoped: 3, share=75\.0%/);
    assert.match(result.stdout, /broad\/final: 1, share=25\.0%/);
    assert.match(result.stdout, /classified repeated occurrences: 3/);
    assert.match(result.stdout, /3x guardrail_rerun_review, share=100\.0% \[scoped\]/);
    assert.match(result.stdout, /validation batches/);
    assert.match(result.stdout, /batch-review/);
    assert.match(result.stdout, /Top Improvements/);
    const review = readJson(reviewJson);
    assert.equal(review.verdict, "current_clean");
    assert.equal(review.current.actions.length, 0);
    assert.match(review.current.status_message, /No current action items/);
    assert.ok(review.historical_lessons.every((lesson) => lesson.current_action === "historical_only"));
    assert.ok(review.top_improvements.some((item) => item.includes("node tools/ai.mjs context --path")));
    assert.ok(review.top_improvements.some((item) => item.includes("validation batch evidence")));
    assert.ok(review.top_improvements.some((item) => item.includes("Repeated Command Review shares")));
    assert.ok(review.top_improvements.some((item) => item.includes("largest scope is scoped (75.0%")));
    assert.ok(review.top_improvements.some((item) => item.includes("Tool Runtime Review")));
    assert.ok(review.top_improvements.some((item) => item.includes("Captured Elapsed Review")));
    assert.ok(review.top_improvements.some((item) => item.includes("top runtime is shell_command (100.0%)")));
    assert.ok(review.top_improvements.some((item) => item.includes("top captured elapsed is ai_profile/gap_checkpoint.mjs (100.0%)")));
    assert.equal(review.top_improvements.some((item) => item.includes("tool_use_summary")), false);
    assert.ok(review.top_improvements.some((item) => item.includes("context_use_summary")));
    assert.ok(review.top_improvements.some((item) => item.includes("100.0% of hotspot chars")));
    assert.ok(review.top_improvements.some((item) => item.includes("historical whole-profile review shows 2")));
    assert.equal(review.top_improvements.some((item) => item.includes("current review shows")), false);
    assert.ok(review.current.readout.some((item) => item.includes("Current scope T0099/reflection-slice is clean")));
    assert.ok(review.current.readout.some((item) => item.includes("coverage confidence is usable")));
    assert.ok(review.current.readout.some((item) => item.includes("Largest current tool runtime: shell_command")));
    assert.ok(review.current.readout.some((item) => item.includes("Largest current captured elapsed checkpoint")));
    assert.ok(review.current.readout.some((item) => item.includes("Current validation was batched")));
    assert.equal(review.current.snapshot.records, 2);
    assert.equal(review.current.snapshot.coverage_ratio, 1);
    assert.equal(review.current.validation_batches[0].batch_id, "current-batch");
    assert.equal(review.current.tool_use_summary[0].tool, "ai_profile/gap_checkpoint.mjs");
    assert.equal(review.current.tool_use_summary[0].duration_kind, "captured_elapsed");
    assert.equal(review.current.tool_runtime_total_ms, 5000);
    assert.equal(review.current.captured_elapsed_total_ms, 300000);
    assert.equal(review.current.context_use_summary.hotspots[0].path, "tasks/STATUS.md");
    assert.equal(review.current.context_use_summary.total_hotspot_chars, 1234);
    assert.equal(review.tool_runtime_summary[0].tool, "shell_command");
    assert.equal(review.tool_runtime_total_ms, 9000);
    assert.equal(review.captured_elapsed_summary[0].tool, "ai_profile/gap_checkpoint.mjs");
    assert.equal(review.captured_elapsed_total_ms, 300000);
    assert.equal(review.tool_use_summary[0].tool, "ai_profile/gap_checkpoint.mjs");
    assert.equal(review.context_use_summary.hotspots[0].path, "AI_PIPELINE_SESSION_PROFILING.md");
    assert.equal(review.context_use_summary.total_hotspot_chars, 27455);
    assert.equal(review.repeated_commands.repeated_total_occurrences, 4);
    assert.equal(review.repeated_commands.classification_total_occurrences, 3);
    assert.ok(review.top_improvements.some((item) => item.includes("node tools/ai.mjs validate")));
    assert.ok(review.top_improvements.some((item) => item.includes("node tools/ai.mjs start")));
    assert.ok(review.top_improvements.some((item) => item.includes("node tools/ai.mjs focus")));
    assert.equal(existsSync(reviewMd), true);
  } finally {
    cleanup(dir);
  }
});

test("reflection review keeps dirty current scope actionable", () => {
  const dir = tempDir();
  try {
    const draftJson = join(dir, "dirty.draft.json");
    const reviewJson = join(dir, "dirty.review.json");

    writeFileSync(draftJson, `${JSON.stringify({
      current_state: {
        current_scope_findings: [{ type: "current_missing_context_inputs", message: "Current context is unmeasured." }],
        current_regressions: [{ key: "current_missing_context_inputs", label: "Current missing context" }],
        pending_followups: [{ title: "Fix context capture", next_action: "Run node tools/ai.mjs context -- <command>." }],
        satisfied_followups: [],
      },
      historical_lessons: [
        {
          type: "low_profile_coverage",
          symptom: "Low coverage.",
          cause: "No checkpoints.",
          fix: "Use node tools/ai.mjs checkpoint.",
        },
      ],
      repeated_commands: { by_scope: [] },
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/reflection_review.mjs", draftJson, "--json-output", reviewJson]);
    assert.match(result.stdout, /Verdict: current_action_required/);
    assert.match(result.stdout, /Current context is unmeasured/);
    const review = readJson(reviewJson);
    assert.equal(review.verdict, "current_action_required");
    assert.match(review.current.status_message, /Resolve current action items/);
    assert.ok(review.current.actions.some((action) => action.includes("node tools/ai.mjs context --")));
    assert.ok(review.historical_lessons.every((lesson) => lesson.current_action === "review_after_current_items"));
  } finally {
    cleanup(dir);
  }
});

test("reflection review flags partial current coverage confidence", () => {
  const dir = tempDir();
  try {
    const draftJson = join(dir, "partial-coverage.draft.json");
    const reviewJson = join(dir, "partial-coverage.review.json");
    writeFileSync(draftJson, `${JSON.stringify({
      current_state: {
        current_scope_findings: [],
        current_regressions: [],
        pending_followups: [],
        satisfied_followups: [],
        current_scope_snapshot: {
          enabled: true,
          work_item: "T0099",
          iteration: "partial-coverage",
          records: 10,
          profiled_ms: 60_000,
          wall_clock_ms: 10 * 60_000,
          coverage_ratio: 0.1,
          missing_context_inputs: 0,
          missing_work_item_records: 0,
          missing_tool_records: 0,
          recovered_failed_records: 0,
          unresolved_failed_records: 0,
          largest_gaps: [
            { start_ts: "2026-06-13T05:00:00.000Z", end_ts: "2026-06-13T05:07:00.000Z", duration_ms: 7 * 60_000 },
          ],
        },
        current_scope_tool_use_summary: [{ tool: "shell_command", records: 3, duration_ms: 60_000 }],
      },
      historical_lessons: [],
      repeated_commands: { by_scope: [] },
    })}\n`, "utf8");

    const result = run(["tools/ai_profile/reflection_review.mjs", draftJson, "--json-output", reviewJson]);
    assert.match(result.stdout, /Current coverage confidence is partial/);
    assert.match(result.stdout, /Largest current wall-clock gap: 7\.0m from 2026-06-13T05:00:00\.000Z to 2026-06-13T05:07:00\.000Z/);
    assert.match(result.stdout, /largest gaps/);
    const review = readJson(reviewJson);
    assert.ok(review.current.readout.some((item) => item.includes("coverage confidence is partial")));
    assert.ok(review.current.readout.some((item) => item.includes("Largest current wall-clock gap")));
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
    assert.deepEqual(closeout.tools, [
      "ai_profile/closeout.mjs",
      "ai_profile/summarize_session_profile.mjs",
      "ai_profile/review.mjs",
      "ai_profile/followups.mjs",
    ]);
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
    assert.equal(review.recovered_failure_classification.length, 1);
    assert.equal(review.recovered_failure_classification[0].classification, "avoidable_rework");
    assert.match(review.recovered_failure_classification[0].next_action, /preflight|rule|validation/);

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
    assert.ok(followups.suggestions.some((suggestion) => suggestion.next_action.includes("capture_baseline.mjs")));
  } finally {
    cleanup(dir);
  }
});

test("review reports clean current scope before historical findings", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "current-clean-review.jsonl");
    const scope = join(dir, "scope.json");
    const reviewMd = join(dir, "review.md");
    const reviewJson = join(dir, "review.json");

    run([
      "tools/ai_profile/event.mjs",
      "--profile", profile,
      "--phase", "validation",
      "--category", "validation",
      "--intent", "old unmeasured validation",
      "--result", "pass",
      "--value", "productive",
      "--context-risk", "medium",
      "--ts", "2026-06-13T10:00:00+05:00",
    ]);
    run([
      "tools/ai_profile/start.mjs",
      "--scope", scope,
      "--profile", profile,
      "--work-item", "CUR",
      "--iteration", "clean-review",
      "--phase", "test",
      "--intent", "clean current scope",
    ]);

    run(["tools/ai_profile/review.mjs", profile, "--output", reviewMd, "--json-output", reviewJson], {
      env: { AI_PROFILE_SCOPE_FILE: scope },
    });
    const markdown = readFileSync(reviewMd, "utf8");
    const currentIndex = markdown.indexOf("## Current Scope Findings");
    const historicalIndex = markdown.indexOf("## Historical Whole-Profile Findings");
    assert.ok(currentIndex > 0);
    assert.ok(historicalIndex > currentIndex);
    assert.match(markdown, /Current scope has no urgent review findings/);

    const review = readJson(reviewJson);
    assert.equal(review.current_scope.findings.length, 0);
    assert.ok(review.current_scope.suggested_actions.some((action) => action.includes("clean baseline")));
    assert.ok(review.findings.some((finding) => finding.type === "missing_context_inputs"));
  } finally {
    cleanup(dir);
  }
});

test("review reports current-scope issues first", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "current-dirty-review.jsonl");
    const scope = join(dir, "scope.json");
    const reviewJson = join(dir, "review.json");

    run([
      "tools/ai_profile/start.mjs",
      "--scope", scope,
      "--profile", profile,
      "--work-item", "CUR",
      "--iteration", "dirty-review",
      "--phase", "test",
      "--intent", "dirty current scope",
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
    assert.ok(review.current_scope.findings.some((finding) => finding.type === "current_missing_context_inputs"));
    assert.ok(review.current_scope.suggested_actions.some((action) => action.includes("node tools/ai.mjs context --path")));
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

test("followups suppress historical broad final when current scope has only batched repeats", () => {
  const dir = tempDir();
  try {
    const reviewJson = join(dir, "batched-current.review.json");
    const followupsJson = join(dir, "batched-current.followups.json");
    writeFileSync(reviewJson, `${JSON.stringify({
      schema_version: 1,
      profile: "batched-current.jsonl",
      findings: [
        {
          type: "repeated_broad_final",
          message: "1 unbatched repeated broad/final command(s) are historical validation waste.",
        },
      ],
      repeated_broad_final_commands: [
        { command: "node tools/pipeline_validate.mjs", count: 4, scope: "broad/final" },
      ],
      repeated_unbatched_broad_final_commands: [
        { command: "node tools/pipeline_validate.mjs", count: 2, scope: "broad/final" },
      ],
      batched_broad_final_commands: [
        { command: "node tools/pipeline_validate.mjs", count: 2, scope: "broad/final" },
      ],
      current_scope: {
        enabled: true,
        records: 4,
        findings: [],
        suggested_actions: ["Use current scope as clean baseline."],
        repeated_broad_final_commands: [
          { command: "node tools/pipeline_validate.mjs", count: 2, scope: "broad/final" },
        ],
        repeated_unbatched_broad_final_commands: [],
        missing_context_inputs: 0,
        missing_work_item_records: 0,
        recovered_failed_records: [],
        unresolved_failed_records: [],
        low_profile_coverage: false,
      },
    })}\n`, "utf8");

    run(["tools/ai_profile/followups.mjs", reviewJson, "--json-output", followupsJson]);
    const followups = readJson(followupsJson);
    assert.ok(followups.suppressed_historical_findings.includes("repeated_broad_final_commands"));
    assert.equal(
      followups.suggestions.some((suggestion) => suggestion.source === "current_scope.repeated_unbatched_broad_final_commands"),
      false,
    );
    assert.equal(
      followups.suggestions.some((suggestion) => suggestion.title === "Reduce repeated broad/final validation"),
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
