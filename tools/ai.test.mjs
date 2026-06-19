import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import test from "node:test";
import assert from "node:assert/strict";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));

function tempDir() {
  return mkdtempSync(join(tmpdir(), "ai-facade-"));
}

function cleanup(dir) {
  rmSync(dir, { recursive: true, force: true });
}

function run(args, options = {}) {
  const result = spawnSync(process.execPath, ["tools/ai.mjs", ...args], {
    cwd: root,
    encoding: "utf8",
    stdio: "pipe",
    shell: false,
    ...options,
  });
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

test("start forwards scope and profile options", () => {
  const dir = tempDir();
  try {
    const scope = join(dir, "scope.json");
    const profile = join(dir, "profile.jsonl");
    const result = run(["start", "TSTART", "first", "--scope", scope, "--profile", profile]);

    assert.equal(result.status, 0, result.stderr);
    assert.equal(readJson(scope).work_item, "TSTART");
    assert.equal(readJson(scope).iteration, "first");
    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].work_item, "TSTART");
    assert.equal(records[0].iteration, "first");
  } finally {
    cleanup(dir);
  }
});

test("focus reuses current work item from selected scope", () => {
  const dir = tempDir();
  try {
    const scope = join(dir, "scope.json");
    const profile = join(dir, "profile.jsonl");
    assert.equal(run(["start", "TFOCUS", "first", "--scope", scope, "--profile", profile]).status, 0);

    const result = run(["focus", "second", "--scope", scope, "--profile", profile]);
    assert.equal(result.status, 0, result.stderr);
    assert.equal(readJson(scope).work_item, "TFOCUS");
    assert.equal(readJson(scope).iteration, "second");
    const records = readJsonl(profile);
    assert.equal(records.length, 2);
    assert.equal(records[1].work_item, "TFOCUS");
    assert.equal(records[1].iteration, "second");
  } finally {
    cleanup(dir);
  }
});

test("focus fails clearly without an existing work item scope", () => {
  const dir = tempDir();
  try {
    const missingScope = join(dir, "missing.json");
    const result = run(["focus", "second", "--scope", missingScope, "--profile", join(dir, "profile.jsonl")]);

    assert.equal(result.status, 2);
    assert.match(result.stderr, /requires an existing work item scope/);
    assert.equal(existsSync(missingScope), false);
  } finally {
    cleanup(dir);
  }
});

test("context records measured file inputs", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "profile.jsonl");
    const file = join(dir, "notes.md");
    writeFileSync(file, "reference notes\n", "utf8");

    const result = run(["context", "--profile-mode", "full", "--path", file, "--profile", profile, "--intent", "Measure temp context"]);
    assert.equal(result.status, 0, result.stderr);
    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].phase, "context");
    assert.equal(records[0].context_inputs.length, 1);
    assert.equal(records[0].context_inputs[0].chars, "reference notes\n".length);
    assert.equal(records[0].files_read[0], file);
  } finally {
    cleanup(dir);
  }
});

test("context command records measured command output", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "profile.jsonl");
    const result = run([
      "context",
      "--profile-mode",
      "full",
      "--profile",
      profile,
      "--intent",
      "Measure command context",
      "--",
      process.execPath,
      "-e",
      "console.log('context-output')",
    ]);

    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /context-output/);
    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].phase, "context");
    assert.equal(records[0].commands.length, 1);
    assert.match(records[0].context_inputs[0].path, /command:/);
    assert.ok(records[0].context_inputs[0].chars >= "context-output\n".length);
  } finally {
    cleanup(dir);
  }
});

test("passive run skips short successful commands", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "profile.jsonl");
    const result = run([
      "run",
      "--profile",
      profile,
      "--profile-slow-ms",
      "60000",
      "--",
      process.execPath,
      "-e",
      "console.log('fast-ok')",
    ]);

    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /fast-ok/);
    assert.equal(existsSync(profile), false);
  } finally {
    cleanup(dir);
  }
});

test("passive run records failed commands", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "profile.jsonl");
    const result = run([
      "run",
      "--profile",
      profile,
      "--",
      process.execPath,
      "-e",
      "process.exit(7)",
    ]);

    assert.equal(result.status, 7);
    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].result, "fail");
    assert.equal(records[0].passive_reason, "failed_command");
  } finally {
    cleanup(dir);
  }
});

test("passive run records slow commands", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "profile.jsonl");
    const result = run([
      "run",
      "--profile",
      profile,
      "--profile-slow-ms",
      "0",
      "--",
      process.execPath,
      "-e",
      "console.log('slow-enough')",
    ]);

    assert.equal(result.status, 0, result.stderr);
    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].result, "pass");
    assert.equal(records[0].passive_reason, "slow_command");
  } finally {
    cleanup(dir);
  }
});

test("gate forwards product-read review options", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    const output = join(dir, "gate.md");
    const json = join(dir, "gate.json");
    writeFileSync(screenshot, "png", "utf8");

    const result = run([
      "gate",
      "--project", "rune-marches",
      "--task", "T0006",
      "--screenshot", screenshot,
      "--verdict", "fail",
      "--where", "A fantasy road screen.",
      "--action", "Click the primary Scout button.",
      "--response", "The route advances into combat.",
      "--reward", "Coins and upgrade progress become visible.",
      "--game-look", "Map art and game controls replace debug widgets.",
      "--problem", "The first screen still has too many controls.",
      "--next", "Reduce the HUD and rebuild the primary action group.",
      "--output", output,
      "--json-output", json,
      "--index-output", join(dir, "latest.json"),
      "--strict",
    ]);

    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /Product Read Gate/);
    assert.equal(existsSync(output), true);
    assert.equal(readJson(json).verdict, "fail");
  } finally {
    cleanup(dir);
  }
});

test("critic forwards visual critique packet options", () => {
  const dir = tempDir();
  try {
    const screenshot = join(dir, "screen.png");
    const output = join(dir, "critic.md");
    const json = join(dir, "critic.json");
    writeFileSync(screenshot, "png", "utf8");

    const result = run([
      "critic",
      "--project", "rune-marches",
      "--task", "T0006",
      "--surface", "desktop",
      "--screenshot", screenshot,
      "--target", "gamedesign/projects/rune-marches/art/fake.png",
      "--brief", "Bright casual screen with readable controls.",
      "--output", output,
      "--json-output", json,
    ]);

    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /Visual Critic Packet/);
    assert.equal(existsSync(output), true);
    const packet = readJson(json);
    assert.equal(packet.schema, "game.visual_critique_packet");
    assert.match(packet.gate_command, /--visual-strict/);
  } finally {
    cleanup(dir);
  }
});

test("validate forwards to the reusable pipeline validator", () => {
  const result = run(["validate", "--dry-run"]);

  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /mode: quick \(dry-run\)/);
});

test("validate forwards supported pipeline options", () => {
  const result = run(["validate", "--full", "--reexport-tests", "--keep-exports", "2", "--no-prune", "--dry-run"]);

  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /mode: full \(dry-run\)/);
  assert.match(result.stdout, /== exported ai profile tests/);
});

test("validate rejects stale file-scoped option instead of ignoring it", () => {
  const result = run(["validate", "--file", "AI_PIPELINE.md", "--dry-run"]);

  assert.equal(result.status, 2);
  assert.match(result.stderr, /validate no longer supports --file/);
});

test("close-slice forwards product gate closeout options", () => {
  const dir = tempDir();
  try {
    const taskDir = join(dir, "tasks", "active");
    const gate = join(dir, "gate.json");
    mkdirSync(taskDir, { recursive: true });
    writeFileSync(join(taskDir, "T0096-test.md"), `---
id: T0096
title: Test task
status: doing
priority: P0
tags: [test]
created: 2026-06-14
updated: 2026-06-14
---

## What

Test task.

## Done when

- [ ] evidence exists

## Open questions

## Log
`, "utf8");
    writeFileSync(gate, `${JSON.stringify({ verdict: "pass", surface: "desktop", screenshot: "tmp/screen.png", markdown: "gate.md", next: "Next slice" })}\n`, "utf8");
    const result = run([
      "close-slice",
      "--project", "rune-marches",
      "--task", "T0096",
      "--gate", gate,
      "--evidence", "product gate test evidence",
      "--strict",
    ], { env: { ...process.env, TASKBOARD_ROOT: dir } });

    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /Close Slice/);
    const task = readFileSync(join(taskDir, "T0096-test.md"), "utf8");
    assert.match(task, /close-slice PASS gate/);
  } finally {
    cleanup(dir);
  }
});

function seedOldWorkRecord(profile) {
  writeFileSync(profile, `${JSON.stringify({
    ts: "2026-06-13T10:00:00+05:00",
    phase: "implementation",
    category: "implementation",
    intent: "Seed old work record",
    result: "pass",
    value: "productive",
    tools: ["shell_command"],
  })}\n`, "utf8");
}

function writeFailedCodexSession(file, callId = "call_facade_import") {
  writeFileSync(file, [
    JSON.stringify({
      type: "response_item",
      payload: {
        type: "function_call",
        call_id: callId,
        arguments: JSON.stringify({ command: "node --test tools/ai_profile/test.mjs" }),
      },
    }),
    JSON.stringify({
      type: "response_item",
      payload: {
        type: "function_call_output",
        call_id: callId,
        output: "Exit code: 1\n",
      },
    }),
  ].join("\n") + "\n", "utf8");
}

test("import-codex-session forwards profile and session options", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "profile.jsonl");
    const session = join(dir, "codex-session.jsonl");
    writeFailedCodexSession(session);

    const result = run(["import-codex-session", "--profile", profile, "--session", session]);

    assert.equal(result.status, 0, result.stderr);
    const records = readJsonl(profile);
    assert.equal(records.length, 1);
    assert.equal(records[0].event_type, "tool_call_result_recovered");
    assert.equal(records[0].source_call_id, "call_facade_import");
  } finally {
    cleanup(dir);
  }
});

test("status imports Codex session before analysis", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "profile.jsonl");
    const session = join(dir, "codex-session.jsonl");
    writeFailedCodexSession(session, "call_status_import");

    const result = run(["status", "--profile", profile, "--session", session]);

    assert.equal(result.status, 0, result.stderr);
    const records = readJsonl(profile);
    assert.equal(records[0].event_type, "tool_call_result_recovered");
    assert.equal(records[0].source_call_id, "call_status_import");
    assert.match(result.stdout, /Unresolved failures: 1/);
  } finally {
    cleanup(dir);
  }
});

test("reflect is passive by default: no forced gap checkpoint", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "profile.jsonl");
    seedOldWorkRecord(profile);

    const result = run(["reflect", "--quick", "--profile", profile, "--no-import-codex-session"]);

    assert.equal(result.status, 0, result.stderr);
    const records = readJsonl(profile);
    const gap = records.find((record) => record.event_type === "gap_checkpoint");
    const closeout = records.find((record) => record.phase === "session_closeout");
    assert.equal(gap, undefined, "default reflect must not force a gap checkpoint");
    assert.ok(closeout);
    assert.ok(closeout.tools.includes("ai_profile/closeout.mjs"));
  } finally {
    cleanup(dir);
  }
});

test("reflect --gap-checkpoint records the pre-reflection gap before closeout", () => {
  const dir = tempDir();
  try {
    const profile = join(dir, "profile.jsonl");
    seedOldWorkRecord(profile);

    const result = run(["reflect", "--gap-checkpoint", "--quick", "--profile", profile, "--no-import-codex-session"]);

    assert.equal(result.status, 0, result.stderr);
    const records = readJsonl(profile);
    const gap = records.find((record) => record.event_type === "gap_checkpoint");
    const closeout = records.find((record) => record.phase === "session_closeout");
    assert.ok(gap);
    assert.equal(gap.intent, "Capture pre-reflection unprofiled work gap");
    assert.deepEqual(gap.tools, ["ai_profile/gap_checkpoint.mjs"]);
    assert.ok(closeout);
    assert.ok(closeout.tools.includes("ai_profile/closeout.mjs"));
  } finally {
    cleanup(dir);
  }
});
