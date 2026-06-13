import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
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

    const result = run(["context", "--path", file, "--profile", profile, "--intent", "Measure temp context"]);
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
