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

test("unknown command prints usage and exits non-zero", () => {
  const result = run(["definitely-not-a-command"]);
  assert.equal(result.status, 2);
  assert.match(result.stderr, /usage:/);
});

test("retired profiler ceremony commands are gone", () => {
  for (const command of ["start", "focus", "checkpoint", "run", "context", "reflect", "summary"]) {
    const result = run([command, "--dry-run"]);
    assert.equal(result.status, 2, `${command} should no longer be a command`);
    assert.match(result.stderr, /usage:/);
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

test("validate forwards review pipeline option", () => {
  const result = run(["validate", "--review", "--dry-run"]);

  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /mode: quick\+review \(dry-run\)/);
  assert.match(result.stdout, /== context budget review/);
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
    const shot = join(dir, "screen.png");
    writeFileSync(shot, "png", "utf8");
    writeFileSync(gate, `${JSON.stringify({ verdict: "pass", surface: "desktop", screenshot: shot, markdown: "gate.md", next: "Next slice" })}\n`, "utf8");
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
