import assert from "node:assert/strict";
import test from "node:test";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../../../../", import.meta.url)));

function tempDir() {
  return mkdtempSync(join(tmpdir(), "doc-reference-check-test-"));
}

function cleanup(dir) {
  rmSync(dir, { recursive: true, force: true });
}

function run(args) {
  return spawnSync(process.execPath, ["ai_studio/core_harness/validation/doc_reference_check.mjs", ...args], {
    cwd: root,
    encoding: "utf8",
    stdio: "pipe",
  });
}

function writeMinimalRoot(dir) {
  mkdirSync(join(dir, "ai_studio", "core_harness", "workflow"), { recursive: true });
  writeFileSync(join(dir, "AGENTS.md"), "# Agents\n\nSee `ai_studio/README.md` and `CLAUDE.md`.\n", "utf8");
  writeFileSync(join(dir, "CLAUDE.md"), "# Claude\n\nSee `AGENTS.md`.\n", "utf8");
  writeFileSync(join(dir, "GAME_PROJECT.md"), "# Game Project\n\nNo active game.\n", "utf8");
  writeFileSync(join(dir, "ai_studio", "README.md"), "# AI Studio\n\nSee `ai_studio/core_harness/README.md`.\n", "utf8");
  writeFileSync(
    join(dir, "ai_studio", "core_harness", "README.md"),
    "# Core Harness\n\nSee `ai_studio/core_harness/workflow/README.md`.\n",
    "utf8",
  );
  writeFileSync(join(dir, "ai_studio", "core_harness", "workflow", "README.md"), "# Workflow\n", "utf8");
}

test("doc reference check passes core harness markdown references", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    const result = run(["--root", dir]);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /markdown file\(s\) checked/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check fails missing core markdown references", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(join(dir, "ai_studio", "core_harness", "README.md"), "See `ai_studio/core_harness/missing.md`.\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /ai_studio\/core_harness\/README\.md -> ai_studio\/core_harness\/missing\.md/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check ignores bare backticked template names", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(join(dir, "AGENTS.md"), "Template names: `gdd.md`, `GAME_PROJECT.md`.\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 0, result.stderr);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check fails missing markdown links", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(join(dir, "AGENTS.md"), "See [missing](ai_studio/core_harness/missing.md).\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /AGENTS\.md -> ai_studio\/core_harness\/missing\.md/);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check allows direct core doc reference command in docs", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(
      join(dir, "ai_studio", "README.md"),
      "Current command:\n\n```powershell\nnode ai_studio/core_harness/validation/doc_reference_check.mjs\n```\n",
      "utf8",
    );
    const result = run(["--root", dir]);
    assert.equal(result.status, 0, result.stderr);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check validates non-markdown AI Studio references in core docs", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    mkdirSync(join(dir, "ai_studio", "core_harness", "validation"), { recursive: true });
    writeFileSync(join(dir, "ai_studio", "core_harness", "validation", "sample_check.mjs"), "// sample\n", "utf8");
    writeFileSync(join(dir, "AGENTS.md"), "Run `ai_studio/core_harness/validation/sample_check.mjs`.\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 0, result.stderr);
  } finally {
    cleanup(dir);
  }
});

test("doc reference check fails missing non-markdown AI Studio references in core docs", () => {
  const dir = tempDir();
  try {
    writeMinimalRoot(dir);
    writeFileSync(join(dir, "AGENTS.md"), "Run `ai_studio/core_harness/validation/missing_check.mjs`.\n", "utf8");
    const result = run(["--root", dir]);
    assert.equal(result.status, 1);
    assert.match(result.stderr, /ai_studio\/core_harness\/validation\/missing_check\.mjs/);
  } finally {
    cleanup(dir);
  }
});
