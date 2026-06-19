import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));

function tempDir() {
  return mkdtempSync(join(tmpdir(), "context-budget-test-"));
}

function cleanup(dir) {
  rmSync(dir, { recursive: true, force: true });
}

function writeFixture(dir, skillBody = "short skill\n", agentBody = "# AGENTS\n") {
  mkdirSync(join(dir, ".codex", "skills", "sample"), { recursive: true });
  mkdirSync(join(dir, "tasks"), { recursive: true });
  writeFileSync(join(dir, ".codex", "skills", "sample", "SKILL.md"), skillBody, "utf8");
  writeFileSync(join(dir, "AGENTS.md"), agentBody, "utf8");
  writeFileSync(join(dir, "AI_PIPELINE.md"), "# Pipeline\n", "utf8");
  writeFileSync(join(dir, "tasks", "STATUS.md"), "# Status\n", "utf8");
  writeFileSync(join(dir, "tasks", "README.md"), "# Tasks\n", "utf8");
}

function run(args) {
  return spawnSync(process.execPath, ["tools/context_budget.mjs", ...args], {
    cwd: root,
    encoding: "utf8",
    stdio: "pipe",
  });
}

test("context budget passes compact hot docs and skill entrypoints", () => {
  const dir = tempDir();
  try {
    writeFixture(dir);
    const result = run(["--root", dir, "--max-skill-chars", "100", "--max-doc-chars", "100"]);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /ok: context budgets pass/);
    assert.match(result.stdout, /sample\/SKILL.md/);
  } finally {
    cleanup(dir);
  }
});

test("context budget fails oversized skill entrypoints", () => {
  const dir = tempDir();
  try {
    writeFixture(dir, "x".repeat(120));
    const result = run(["--root", dir, "--max-skill-chars", "100", "--max-doc-chars", "1000"]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /sample\/SKILL.md: 120 chars > 100/);
  } finally {
    cleanup(dir);
  }
});

test("context budget supports json output", () => {
  const dir = tempDir();
  try {
    writeFixture(dir);
    const result = run(["--root", dir, "--json"]);
    assert.equal(result.status, 0, result.stderr);
    const parsed = JSON.parse(result.stdout);
    assert.equal(parsed.ok, true);
    assert.ok(parsed.records.some((record) => record.path === ".codex/skills/sample/SKILL.md"));
  } finally {
    cleanup(dir);
  }
});

test("context budget default hot doc limit is tight enough for live docs", () => {
  const dir = tempDir();
  try {
    writeFixture(dir, "short skill\n", "x".repeat(6600));
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /AGENTS\.md: 6600 chars > 6500/);
  } finally {
    cleanup(dir);
  }
});
