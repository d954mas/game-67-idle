import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { HOT_DOC_BUDGETS, LIVE_STATUS_MAX_CHARS } from "./context_budget_config.mjs";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));

function tempDir() {
  return mkdtempSync(join(tmpdir(), "context-budget-test-"));
}

function cleanup(dir) {
  rmSync(dir, { recursive: true, force: true });
}

function writeFixture(dir, skillBody = "short skill\n", agentBody = "# AGENTS\n") {
  mkdirSync(join(dir, ".codex", "skills", "sample"), { recursive: true });
  mkdirSync(join(dir, "ai_studio", "taskboard"), { recursive: true });
  mkdirSync(join(dir, "docs", "ai-pipeline"), { recursive: true });
  mkdirSync(join(dir, "tasks"), { recursive: true });
  writeFileSync(join(dir, ".codex", "skills", "sample", "SKILL.md"), skillBody, "utf8");
  writeFileSync(join(dir, "AGENTS.md"), agentBody, "utf8");
  writeFileSync(join(dir, "ai_studio", "README.md"), "# AI Studio\n", "utf8");
  writeFileSync(join(dir, "ai_studio", "taskboard", "README.md"), "# Taskboard\n", "utf8");
  writeFileSync(join(dir, "docs", "ai-pipeline", "agent-workflow.md"), "# Agent Workflow\n", "utf8");
  writeFileSync(join(dir, "docs", "ai-pipeline", "quality-validation.md"), "# Quality\n", "utf8");
  writeFileSync(join(dir, "docs", "ai-pipeline", "profiling-reuse.md"), "# Profiling\n", "utf8");
  mkdirSync(join(dir, "tools"), { recursive: true });
  writeFileSync(join(dir, "tools", "README.md"), "# Tools\n", "utf8");
  writeFileSync(join(dir, "tasks", "STATUS.md"), "# Status\n", "utf8");
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
    assert.match(result.stderr, /AGENTS\.md: 6600 chars > 3400/);
  } finally {
    cleanup(dir);
  }
});

test("context budget applies tighter default caps to live status docs", () => {
  const dir = tempDir();
  try {
    writeFixture(dir);
    writeFileSync(join(dir, "tasks", "STATUS.md"), "x".repeat(2500), "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /tasks\/STATUS\.md: 2500 chars > 2400/);
  } finally {
    cleanup(dir);
  }
});

test("live status budget is shared with taskboard validation", () => {
  const statusBudget = HOT_DOC_BUDGETS.find((doc) => doc.path.replaceAll("\\", "/") === "tasks/STATUS.md");
  assert.equal(statusBudget.maxChars, LIVE_STATUS_MAX_CHARS);
});

test("context budget applies the task store hot guide cap", () => {
  const dir = tempDir();
  try {
    writeFixture(dir);
    writeFileSync(join(dir, "ai_studio", "taskboard", "README.md"), "x".repeat(3100), "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /ai_studio\/taskboard\/README\.md: 3100 chars > 3000/);
  } finally {
    cleanup(dir);
  }
});

test("context budget applies the AI Studio map cap", () => {
  const dir = tempDir();
  try {
    writeFixture(dir);
    writeFileSync(join(dir, "ai_studio", "README.md"), "x".repeat(2700), "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /ai_studio\/README\.md: 2700 chars > 2600/);
  } finally {
    cleanup(dir);
  }
});

test("context budget applies the tools README cap", () => {
  const dir = tempDir();
  try {
    writeFixture(dir);
    writeFileSync(join(dir, "tools", "README.md"), "x".repeat(3100), "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /tools\/README\.md: 3100 chars > 3000/);
  } finally {
    cleanup(dir);
  }
});

test("context budget applies pipeline reference caps", () => {
  const dir = tempDir();
  try {
    writeFixture(dir);
    writeFileSync(join(dir, "docs", "ai-pipeline", "agent-workflow.md"), "x".repeat(2700), "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /docs\/ai-pipeline\/agent-workflow\.md: 2700 chars > 2600/);
  } finally {
    cleanup(dir);
  }
});

test("context budget default skill entrypoint cap stays tight", () => {
  const dir = tempDir();
  try {
    writeFixture(dir, "x".repeat(2900));
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /\.codex\/skills\/sample\/SKILL\.md: 2900 chars > 2800/);
  } finally {
    cleanup(dir);
  }
});

test("context budget review mode applies expanded per-file targets", () => {
  const dir = tempDir();
  try {
    writeFixture(dir, "short skill\n", "x".repeat(3700));
    const result = run(["--root", dir, "--review"]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /AGENTS\.md: 3700 chars > 3600/);
  } finally {
    cleanup(dir);
  }
});

test("context budget review mode fails aggregate hot doc growth", () => {
  const dir = tempDir();
  try {
    writeFixture(dir);
    writeFileSync(join(dir, "AGENTS.md"), "x".repeat(3590), "utf8");
    writeFileSync(join(dir, "ai_studio", "README.md"), "x".repeat(2590), "utf8");
    writeFileSync(join(dir, "tasks", "STATUS.md"), "x".repeat(2390), "utf8");
    writeFileSync(join(dir, "ai_studio", "taskboard", "README.md"), "x".repeat(3190), "utf8");
    writeFileSync(join(dir, "tools", "README.md"), "x".repeat(3190), "utf8");
    writeFileSync(join(dir, "docs", "ai-pipeline", "agent-workflow.md"), "x".repeat(3190), "utf8");
    writeFileSync(join(dir, "docs", "ai-pipeline", "quality-validation.md"), "x".repeat(3190), "utf8");
    writeFileSync(join(dir, "docs", "ai-pipeline", "profiling-reuse.md"), "x".repeat(3190), "utf8");
    const result = run(["--root", dir, "--review"]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /<hot-doc-total>: 24520 chars > 24000/);
  } finally {
    cleanup(dir);
  }
});

test("context budget review mode fails aggregate skill entrypoint growth", () => {
  const dir = tempDir();
  try {
    writeFixture(dir);
    for (let i = 0; i < 12; i++) {
      const skillDir = join(dir, ".codex", "skills", `sample-${i}`);
      mkdirSync(skillDir, { recursive: true });
      writeFileSync(join(skillDir, "SKILL.md"), "x".repeat(3190), "utf8");
    }
    const result = run(["--root", dir, "--review"]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /<skill-entrypoint-total>: 38292 chars > 38000/);
  } finally {
    cleanup(dir);
  }
});
